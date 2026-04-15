#include "core/AgentEngine.hpp"
#include "client/XAIClient.hpp"
#include "config/ModelRegistry.hpp"
#include "tools/ProjectNotes.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace avacli {

namespace {

namespace fs = std::filesystem;

constexpr int MAX_TOOL_TURNS = 20;
constexpr size_t CONTEXT_WARN_THRESHOLD = 110000;

std::string readFileContents(const std::string& path, size_t maxBytes = 8192) {
    if (!fs::exists(path)) return "";
    std::ifstream f(path);
    if (!f) return "";
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (content.size() > maxBytes)
        content = content.substr(0, maxBytes) + "\n... (truncated)";
    return content;
}

std::string filterActiveTodos(const std::string& raw) {
    try {
        auto arr = nlohmann::json::parse(raw);
        if (!arr.is_array()) return raw;
        auto filtered = nlohmann::json::array();
        for (const auto& t : arr) {
            std::string status = t.value("status", "");
            if (status != "done")
                filtered.push_back(t);
        }
        if (filtered.empty()) return "";
        return filtered.dump(2);
    } catch (...) {
        return raw;
    }
}

std::string loadContextSnapshot(const std::string& workspace, const std::string& sessionId) {
    std::string snapshot;

    // Workspace-level project notes (read-only reference)
    std::string notesPath = (fs::path(workspace) / "GROK_NOTES.md").string();
    std::string notes = readFileContents(notesPath, 4096);
    if (!notes.empty()) {
        snapshot += "\n\n## Project Notes (GROK_NOTES.md)\n```\n" + notes + "\n```\n";
    }

    if (!sessionId.empty()) {
        // Session todos (active only)
        std::vector<TodoItem> todos;
        loadSessionTodos(sessionId, todos);
        pruneDoneTodos(todos);
        if (!todos.empty()) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : todos) {
                if (t.status != "done")
                    arr.push_back({{"id", t.id}, {"title", t.title}, {"status", t.status}});
            }
            if (!arr.empty())
                snapshot += "\n## Active TODOs\n```json\n" + arr.dump(2) + "\n```\n";
        }

        // Session memory (pinned + last 15 entries)
        auto memories = loadAllMemory(sessionId);
        if (!memories.empty()) {
            std::vector<MemoryEntry> contextMemories;
            for (const auto& m : memories)
                if (m.pinned) contextMemories.push_back(m);
            size_t nonPinned = 0;
            for (auto it = memories.rbegin(); it != memories.rend() && nonPinned < 15; ++it) {
                if (!it->pinned) {
                    contextMemories.push_back(*it);
                    ++nonPinned;
                }
            }
            if (!contextMemories.empty()) {
                snapshot += "\n## Session Memory\n";
                for (const auto& m : contextMemories) {
                    snapshot += "- [" + m.category + "] " + m.content;
                    if (m.pinned) snapshot += " (pinned)";
                    snapshot += "\n";
                }
            }
        }

        // Recent edit history (last 15)
        auto edits = loadEditHistory(sessionId, 15);
        if (!edits.empty()) {
            snapshot += "\n## Recent File Edits\n";
            for (const auto& e : edits) {
                snapshot += "- " + e.action + " " + e.file;
                if (!e.summary.empty()) snapshot += " (" + e.summary + ")";
                snapshot += "\n";
            }
        }
    } else {
        // Legacy: load from workspace GROK_TODOS.json
        std::string todosPath = (fs::path(workspace) / "GROK_TODOS.json").string();
        std::string todosRaw = readFileContents(todosPath);
        std::string todos = filterActiveTodos(todosRaw);
        if (!todos.empty()) {
            snapshot += "\n## Active TODOs\n```json\n" + todos + "\n```\n";
        }
    }

    return snapshot;
}

std::string formatToolResultForDisplay(const std::string& toolName, const std::string& result, bool verbose) {
    const size_t capOut = verbose ? (64u * 1024) : 500u;
    const size_t capRead = verbose ? (64u * 1024) : 400u;
    const size_t capMatches = verbose ? 200u : 8u;
    const size_t capEntries = verbose ? 200u : 12u;
    const size_t capGeneric = verbose ? (64u * 1024) : 200u;
    try {
        auto j = nlohmann::json::parse(result);
        std::string out = "[Tool] " + toolName + ": ";
        if (j.contains("error")) {
            auto& err = j["error"];
            if (err.is_string())
                out += "error: " + err.get<std::string>();
            else if (err.is_object())
                out += "error: " + err.value("message", err.dump());
            else
                out += "error: " + err.dump();
            return out;
        }
        if (toolName == "edit_file") {
            if (j.contains("status")) out += "ok";
            if (j.contains("path")) out += " (" + j["path"].get<std::string>() + ")";
        } else if (toolName == "run_shell" || toolName == "run_tests") {
            if (j.contains("exit_code")) out += "exit " + std::to_string(j["exit_code"].get<int>());
            if (j.contains("stdout")) {
                std::string stdout_ = j["stdout"].get<std::string>();
                if (stdout_.size() > capOut) stdout_ = stdout_.substr(0, capOut) + "\n    ...";
                std::string indented;
                size_t lineStart = 0;
                for (size_t i = 0; i <= stdout_.size(); ++i) {
                    if (i == stdout_.size() || stdout_[i] == '\n') {
                        std::string line = stdout_.substr(lineStart, i - lineStart);
                        if (!line.empty()) {
                            if (!indented.empty()) indented += '\n';
                            indented += "    " + line;
                        } else if (i < stdout_.size()) indented += '\n';
                        lineStart = i + 1;
                    }
                }
                if (!indented.empty()) out += "\n" + indented;
            }
        } else if (toolName == "read_file" || toolName == "read_project_notes") {
            if (j.contains("content")) {
                std::string c = j["content"].get<std::string>();
                if (c.size() > capRead) c = c.substr(0, capRead) + "\n...";
                out += "\n" + c;
            }
        } else if (toolName == "search_files" && j.contains("matches")) {
            for (size_t i = 0; i < std::min(capMatches, j["matches"].size()); ++i)
                out += "\n    " + j["matches"][i].get<std::string>();
            if (j["matches"].size() > capMatches) out += "\n    ...";
        } else if (toolName == "list_directory" && j.contains("entries")) {
            for (size_t i = 0; i < std::min(capEntries, j["entries"].size()); ++i)
                out += (i ? " " : "\n    ") + j["entries"][i].get<std::string>();
            if (j["entries"].size() > capEntries) out += " ...";
        } else {
            out += result.substr(0, capGeneric);
            if (result.size() > capGeneric) out += "...";
        }
        return out;
    } catch (...) {
        std::string preview = result.substr(0, capGeneric);
        if (result.size() > capGeneric) preview += "...";
        return "[Tool] " + toolName + ": " + preview;
    }
}

std::string loadPromptFromFile(const std::string& workspace) {
    for (const auto& candidate : {
        workspace + "/GROK_SYSTEM_PROMPT.md",
        fs::path(workspace).parent_path().string() + "/GROK_SYSTEM_PROMPT.md",
    }) {
        if (fs::exists(candidate)) {
            std::ifstream ifs(candidate);
            if (ifs) {
                std::string content((std::istreambuf_iterator<char>(ifs)),
                                     std::istreambuf_iterator<char>());
                if (!content.empty()) {
                    spdlog::info("Loaded system prompt from {}", candidate);
                    return content;
                }
            }
        }
    }
    return "";
}

std::string buildUnifiedPrompt(const std::string& workspace) {
    std::string fromFile = loadPromptFromFile(workspace);
    if (!fromFile.empty()) {
        return fromFile + "\n\n## Environment\n- Workspace: " + workspace + "\n";
    }

    /* Fallback: embedded prompt used when GROK_SYSTEM_PROMPT.md is not found */
    return std::string(R"(You are Grok, an AI coding assistant powered by xAI. You help users with software engineering tasks. You have full access to the workspace and all tools at all times.

## Environment
- Workspace: )") + workspace + R"(
- You have tools for reading, writing, and searching files, running shell commands, and searching the web.
- You operate autonomously. When the user asks you to do something, do it. Don't ask for permission to use tools.
)";
}

} // namespace

AgentEngine::AgentEngine(std::string workspace,
                         Mode mode,
                         ToolExecutor* executor,
                         StreamDeltaFn onStreamDelta,
                         LogLineFn onLogLine,
                         AskUserFn askUser,
                         ModeChangeFn onModeChange)
    : workspace_(std::move(workspace))
    , mode_(mode)
    , executor_(executor)
    , onStreamDelta_(std::move(onStreamDelta))
    , onLogLine_(std::move(onLogLine))
    , askUser_(std::move(askUser))
    , onModeChange_(std::move(onModeChange)) {}

std::string AgentEngine::buildSystemPrompt(Mode /*mode*/, const std::string& workspace) {
    std::string sessionId = executor_ ? executor_->sessionId() : "";
    std::string prompt = buildUnifiedPrompt(workspace);
    prompt += loadContextSnapshot(workspace, sessionId);
    return prompt;
}

bool AgentEngine::run(XAIClient& client,
                      const std::string& model,
                      std::vector<Message>& messagesInOut,
                      const std::vector<nlohmann::json>& tools,
                      UsageFn onUsage) {
    std::vector<Message> apiMessages = messagesInOut;

    if (apiMessages.empty() || apiMessages.front().role != "system") {
        apiMessages.insert(apiMessages.begin(),
            {"system", buildSystemPrompt(mode_, workspace_)});
    } else {
        apiMessages[0].content = buildSystemPrompt(mode_, workspace_);
    }

    ChatOptions opts;
    opts.model = model;
    auto modelCfg = ModelRegistry::get(model);
    opts.maxTokens = modelCfg.defaultMaxTokens;
    if (maxTokensOverride_ > 0) opts.maxTokens = maxTokensOverride_;
    opts.stream = true;
    opts.cancelFlag = cancelFlag_;

    size_t estChars = 0;
    for (const auto& m : apiMessages) {
        estChars += m.content.size();
        for (const auto& tc : m.tool_calls)
            estChars += tc.arguments.size() + tc.name.size();
    }
    if (estChars / 4 > CONTEXT_WARN_THRESHOLD) {
        if (onLogLine_)
            onLogLine_(TranscriptChannel::System,
                       "[Warning] Context may be getting long. Consider using summarize_and_new_chat to continue in a fresh session.");
        Message warnMsg;
        warnMsg.role = "system";
        warnMsg.content = "[Context Warning] The conversation is getting very long (estimated " + std::to_string(estChars / 4) + " tokens). "
            "Consider using the `summarize_and_new_chat` tool to create a new session with a condensed summary of the conversation so far.";
        apiMessages.push_back(warnMsg);
    }

    int turn = 0;
    while (turn < MAX_TOOL_TURNS) {
        ++turn;
        spdlog::info("[AgentEngine] Turn {}", turn);

        std::string fullResponse;
        std::vector<ToolCall> toolCalls;

        auto collectToolCalls = [&](const std::vector<ToolCall>& calls) {
            toolCalls = calls;
        };

        bool ok = client.chatStream(apiMessages, opts, tools, {
            .onContent = [&](const std::string& delta) {
                fullResponse += delta;
                if (onStreamDelta_) onStreamDelta_(delta);
            },
            .onToolCalls = collectToolCalls,
            .onUsage = [&](const Usage& u) {
                if (onUsage) onUsage(u);
            },
            .onDone = [] {}
        });

        if (!ok) {
            spdlog::error("[AgentEngine] Chat failed: {}", client.lastError());
            return false;
        }

        Message assistantMsg{"assistant", fullResponse};
        assistantMsg.tool_calls = toolCalls;
        apiMessages.push_back(std::move(assistantMsg));

        if (toolCalls.empty()) {
            spdlog::info("[AgentEngine] No tool calls - done");
            break;
        }

        spdlog::info("[Tool] Executing {} tool call(s)", toolCalls.size());

        for (const auto& tc : toolCalls) {
            if (onLogLine_) {
                onLogLine_(TranscriptChannel::Tool, "[Tool] " + tc.name + " ...");
            }

            if (onToolStart_) onToolStart_(tc.id, tc.name, tc.arguments);

            std::string result = executor_->execute(tc.name, tc.arguments);

            bool toolSuccess = true;
            try {
                auto jr = nlohmann::json::parse(result);
                if (jr.contains("error")) toolSuccess = false;
            } catch (...) {}
            if (onToolDone_) onToolDone_(tc.id, tc.name, toolSuccess, result);

            if (onLogLine_) {
                std::string display = formatToolResultForDisplay(tc.name, result, verboseToolOutput_);
                onLogLine_(TranscriptChannel::Tool, display);
            }

            Message toolMsg;
            toolMsg.role = "tool";
            toolMsg.tool_call_id = tc.id;
            toolMsg.content = result;
            apiMessages.push_back(std::move(toolMsg));
        }
    }

    if (turn >= MAX_TOOL_TURNS)
        spdlog::warn("[AgentEngine] Reached max tool turns");

    messagesInOut = std::move(apiMessages);
    return true;
}

} // namespace avacli
