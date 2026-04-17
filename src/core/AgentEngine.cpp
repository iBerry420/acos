#include "core/AgentEngine.hpp"
#include "client/XAIClient.hpp"
#include "config/ModelRegistry.hpp"
#include "server/DefaultSystemPrompt.hpp"
#include "tools/ProjectNotes.hpp"
#include "tools/ToolRegistry.hpp"
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

    /* Fallback: full embedded default (baked in from GROK_SYSTEM_PROMPT.md) */
    /* when the workspace has no GROK_SYSTEM_PROMPT.md on disk. Keeps agent */
    /* and UI in perfect agreement on what "default" means. */
    return getDefaultSystemPrompt() +
           "\n\n## Environment\n- Workspace: " + workspace + "\n";
}

/// Convert chat-completions-style messages to Responses API `input` array.
/// System messages are excluded (handled via `instructions` parameter).
std::vector<nlohmann::json> messagesToResponsesInput(const std::vector<Message>& messages) {
    std::vector<nlohmann::json> input;
    for (const auto& m : messages) {
        if (m.role == "system") continue;

        if (m.role == "user") {
            if (!m.content_parts.empty()) {
                auto parts = nlohmann::json::array();
                for (const auto& p : m.content_parts) {
                    nlohmann::json part;
                    if (p.type == "text") {
                        part["type"] = "input_text";
                        part["text"] = p.text;
                    } else if (p.type == "image_url") {
                        part["type"] = "input_image";
                        part["image_url"] = p.image_url;
                    }
                    parts.push_back(part);
                }
                input.push_back({{"role", "user"}, {"content", parts}});
            } else {
                input.push_back({{"role", "user"}, {"content", m.content}});
            }
        }
        else if (m.role == "assistant") {
            if (!m.tool_calls.empty()) {
                for (const auto& tc : m.tool_calls) {
                    input.push_back({
                        {"type", "function_call"},
                        {"call_id", tc.id},
                        {"name", tc.name},
                        {"arguments", tc.arguments}
                    });
                }
            }
            if (!m.content.empty()) {
                input.push_back({
                    {"type", "message"},
                    {"role", "assistant"},
                    {"content", nlohmann::json::array({{{"type", "output_text"}, {"text", m.content}}})}
                });
            }
        }
        else if (m.role == "tool") {
            input.push_back({
                {"type", "function_call_output"},
                {"call_id", m.tool_call_id.value_or("")},
                {"output", m.content}
            });
        }
    }
    return input;
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
    std::string prompt = buildStaticSystemPrompt(workspace);
    std::string dyn = buildDynamicContext(workspace);
    if (!dyn.empty())
        prompt += dyn;
    return prompt;
}

std::string AgentEngine::buildStaticSystemPrompt(const std::string& workspace) {
    return buildUnifiedPrompt(workspace);
}

std::string AgentEngine::buildDynamicContext(const std::string& workspace) {
    std::string sessionId = executor_ ? executor_->sessionId() : "";
    return loadContextSnapshot(workspace, sessionId);
}

bool AgentEngine::run(XAIClient& client,
                      const std::string& model,
                      std::vector<Message>& messagesInOut,
                      const std::vector<nlohmann::json>& tools,
                      UsageFn onUsage) {
    auto modelCfg = ModelRegistry::get(model);

    if (modelCfg.responsesApi)
        return runResponses(client, model, messagesInOut, tools, onUsage);

    return runChatCompletions(client, model, messagesInOut, tools, onUsage);
}

bool AgentEngine::runChatCompletions(XAIClient& client,
                                     const std::string& model,
                                     std::vector<Message>& messagesInOut,
                                     const std::vector<nlohmann::json>& tools,
                                     UsageFn onUsage) {
    // Build apiMessages = [static system prompt] [prior turns] [dynamic ctx refresh] [latest user turn]
    // The static system prompt is set once and NEVER rewritten inside the tool loop so the xAI
    // prompt cache can serve the prefix on every turn. Volatile session state (TODOs, memory,
    // recent edits) is injected as a separate user message right before the latest user turn —
    // new each run(), but only at the end so earlier turns stay byte-stable and remain cached.
    std::vector<Message> apiMessages;
    apiMessages.reserve(messagesInOut.size() + 2);

    apiMessages.push_back({"system", buildStaticSystemPrompt(workspace_)});

    // Carry over prior non-system messages from the session. Any stale system message in
    // messagesInOut is dropped — the fresh static prompt above is authoritative.
    size_t tailUserIdx = std::string::npos;
    for (const auto& m : messagesInOut) {
        if (m.role == "system") continue;
        apiMessages.push_back(m);
    }
    for (size_t i = apiMessages.size(); i > 1; --i) {
        if (apiMessages[i - 1].role == "user") {
            tailUserIdx = i - 1;
            break;
        }
    }

    std::string dynCtx = buildDynamicContext(workspace_);
    bool dynCtxInjected = false;
    size_t dynCtxIdx = 0;
    if (!dynCtx.empty()) {
        Message ctxMsg{"user",
            "## Current Session State (refreshed at the start of each run)\n" + dynCtx};
        size_t insertPos = (tailUserIdx != std::string::npos) ? tailUserIdx : apiMessages.size();
        apiMessages.insert(apiMessages.begin() + static_cast<std::ptrdiff_t>(insertPos),
                           std::move(ctxMsg));
        dynCtxIdx = insertPos;
        dynCtxInjected = true;
    }

    ChatOptions opts;
    opts.model = model;
    auto modelCfg = ModelRegistry::get(model);
    opts.maxTokens = modelCfg.defaultMaxTokens;
    if (maxTokensOverride_ > 0) opts.maxTokens = maxTokensOverride_;
    opts.stream = true;
    opts.cancelFlag = cancelFlag_;
    opts.convId = convId_;

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

    spdlog::info("[AgentEngine] Chat completions run (model={}, conv_id={})",
                 model, opts.convId.empty() ? "<none>" : opts.convId);

    int turn = 0;
    while (turn < MAX_TOOL_TURNS) {
        ++turn;
        spdlog::info("[AgentEngine] Turn {}", turn);

        std::string fullResponse;
        std::string reasoningBuf;
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
            .onDone = [] {},
            // Capture reasoning_content raw so we can echo it back on the next
            // turn (keeps xAI's reasoning-cache warm) without burying it in the
            // assistant `content` as the legacy <thought> wrapping did.
            .onReasoning = [&](const std::string& delta) {
                reasoningBuf += delta;
                if (onStreamDelta_) {
                    // Preserve the UI-visible thought stream the web/TUI already
                    // expects: they split on <thought>…</thought>.
                    if (reasoningBuf.size() == delta.size())
                        onStreamDelta_("<thought>");
                    onStreamDelta_(delta);
                }
            }
        });

        if (!ok) {
            spdlog::error("[AgentEngine] Chat failed: {}", client.lastError());
            return false;
        }

        // Close the thought block for the UI if we actually streamed reasoning.
        if (!reasoningBuf.empty() && onStreamDelta_)
            onStreamDelta_("</thought>");

        Message assistantMsg{"assistant", fullResponse};
        assistantMsg.tool_calls = toolCalls;
        assistantMsg.reasoning_content = std::move(reasoningBuf);
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

    // Strip our injections before handing the transcript back to the caller so they don't
    // accumulate in the persisted session: the system prompt is always re-generated at the
    // next run() entry, and the dynamic-context block is re-freshed from live state.
    if (dynCtxInjected && dynCtxIdx < apiMessages.size()) {
        apiMessages.erase(apiMessages.begin() + static_cast<std::ptrdiff_t>(dynCtxIdx));
    }
    if (!apiMessages.empty() && apiMessages.front().role == "system") {
        apiMessages.erase(apiMessages.begin());
    }

    messagesInOut = std::move(apiMessages);
    return true;
}

bool AgentEngine::runResponses(XAIClient& client,
                               const std::string& model,
                               std::vector<Message>& messagesInOut,
                               const std::vector<nlohmann::json>& tools,
                               UsageFn onUsage) {
    std::vector<Message> apiMessages;
    apiMessages.reserve(messagesInOut.size() + 1);
    for (const auto& m : messagesInOut) {
        if (m.role == "system") continue;  // drop any stale system message
        apiMessages.push_back(m);
    }

    bool isMultiAgent = model.find("multi-agent") != std::string::npos;

    // `instructions` carries only the cacheable static prefix; volatile context goes inline
    // as a user message so the prefix + all preceding turns stay byte-stable across turns.
    std::string systemPrompt = buildStaticSystemPrompt(workspace_);

    // Phase 4a: chain via `previous_response_id` when we've seen one. In that mode we only
    // send the slice of messages appended since the last server response — xAI replays the
    // stored history (including encrypted reasoning) for us. Falls back to full resend if
    // the stored response has expired (>30 d) or the id is otherwise rejected.
    size_t baseCount = 0;
    std::string currentPrevId;
    if (usePreviousResponseId_ && !lastResponseId_.empty() && !isMultiAgent) {
        baseCount = std::min(lastSubmittedCount_, apiMessages.size());
        currentPrevId = lastResponseId_;
        spdlog::info("[AgentEngine] Responses chain: prev_id={} slice_from={}/{} msgs",
                     currentPrevId, baseCount, apiMessages.size());
    }

    std::string dynCtx = buildDynamicContext(workspace_);
    bool dynCtxInjected = false;
    size_t dynCtxIdx = 0;
    if (!dynCtx.empty()) {
        size_t insertPos = apiMessages.size();
        for (size_t i = apiMessages.size(); i > 0; --i) {
            if (apiMessages[i - 1].role == "user") {
                insertPos = i - 1;
                break;
            }
        }
        // Keep the refresh inside the slice that actually goes to the server, otherwise
        // xAI never sees it when we're chaining via previous_response_id.
        if (insertPos < baseCount) insertPos = baseCount;
        Message ctxMsg{"user",
            "## Current Session State (refreshed at the start of each run)\n" + dynCtx};
        apiMessages.insert(apiMessages.begin() + static_cast<std::ptrdiff_t>(insertPos),
                           std::move(ctxMsg));
        dynCtxIdx = insertPos;
        dynCtxInjected = true;
    }

    std::vector<nlohmann::json> responsesTools;
    if (isMultiAgent) {
        responsesTools.push_back({{"type", "web_search"}});
        responsesTools.push_back({{"type", "x_search"}});
    } else {
        responsesTools = ToolRegistry::toResponsesApiFormat(tools);
    }

    ChatOptions opts;
    opts.model = model;
    auto modelCfg = ModelRegistry::get(model);
    if (!isMultiAgent) {
        opts.maxTokens = modelCfg.defaultMaxTokens;
        if (maxTokensOverride_ > 0) opts.maxTokens = maxTokensOverride_;
    } else {
        opts.maxTokens = 0;
    }
    opts.stream = true;
    opts.cancelFlag = cancelFlag_;
    opts.convId = convId_;
    if (!reasoningEffort_.empty())
        opts.reasoningEffort = reasoningEffort_;
    else if (opts.reasoningEffort.empty())
        opts.reasoningEffort = "high";

    spdlog::info("[AgentEngine] Using Responses API for model '{}' (effort={}, multi_agent={}, conv_id={}, prev_id={})",
                 model, opts.reasoningEffort, isMultiAgent,
                 opts.convId.empty() ? "<none>" : opts.convId,
                 currentPrevId.empty() ? "<none>" : currentPrevId);

    auto isStalePrevIdError = [](const std::string& err) {
        std::string lower;
        lower.reserve(err.size());
        for (char c : err) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.find("previous_response_id") != std::string::npos) return true;
        if (lower.find("previous response") != std::string::npos) return true;
        return lower.find("response") != std::string::npos &&
               (lower.find("not found") != std::string::npos ||
                lower.find("expired") != std::string::npos ||
                lower.find("invalid") != std::string::npos);
    };

    bool prevIdFallbackUsed = false;
    int turn = 0;
    while (turn < MAX_TOOL_TURNS) {
        ++turn;
        spdlog::info("[AgentEngine] Responses turn {} (slice {} → {})",
                     turn, baseCount, apiMessages.size());

        // Slice only the items the server hasn't seen yet. On the first call of a fresh
        // chain baseCount==0 → full input; on chained calls baseCount points past the
        // last assistant, so the slice is [new tool results | new user msg | dynCtx].
        std::vector<Message> slice;
        slice.reserve(apiMessages.size() - baseCount);
        for (size_t i = baseCount; i < apiMessages.size(); ++i)
            slice.push_back(apiMessages[i]);
        auto input = messagesToResponsesInput(slice);

        // When chaining, skip `instructions`: the server already has them cached from the
        // original turn. Sending fresh instructions on chained calls would either be
        // ignored or trigger a re-encode.
        const std::string& turnInstructions = currentPrevId.empty() ? systemPrompt : std::string();
        opts.previousResponseId = currentPrevId;

        std::string fullResponse;
        std::string reasoningBuf;
        std::string newResponseId;
        std::vector<ToolCall> toolCalls;

        auto collectToolCalls = [&](const std::vector<ToolCall>& calls) {
            toolCalls = calls;
        };

        bool ok = client.responsesStream(turnInstructions, input, opts, responsesTools, {
            .onContent = [&](const std::string& delta) {
                fullResponse += delta;
                if (onStreamDelta_) onStreamDelta_(delta);
            },
            .onToolCalls = collectToolCalls,
            .onUsage = [&](const Usage& u) {
                if (onUsage) onUsage(u);
            },
            .onDone = [] {},
            .onReasoning = [&](const std::string& delta) {
                reasoningBuf += delta;
                if (onStreamDelta_) {
                    if (reasoningBuf.size() == delta.size())
                        onStreamDelta_("<thought>");
                    onStreamDelta_(delta);
                }
            },
            .onResponseId = [&](const std::string& id) {
                newResponseId = id;
            }
        });

        if (!ok) {
            std::string err = client.lastError();
            if (!currentPrevId.empty() && !prevIdFallbackUsed && isStalePrevIdError(err)) {
                spdlog::warn("[AgentEngine] previous_response_id rejected ({}), falling back "
                             "to full-history resend for this run", err);
                prevIdFallbackUsed = true;
                currentPrevId.clear();
                lastResponseId_.clear();
                baseCount = 0;
                lastSubmittedCount_ = 0;
                --turn;  // replay this iteration from scratch
                continue;
            }
            spdlog::error("[AgentEngine] Responses API failed: {}", err);
            return false;
        }

        // Close the UI thought block for streamed reasoning.
        if (!reasoningBuf.empty() && onStreamDelta_)
            onStreamDelta_("</thought>");

        if (!newResponseId.empty()) {
            currentPrevId = newResponseId;
            lastResponseId_ = newResponseId;
        }

        Message assistantMsg{"assistant", fullResponse};
        assistantMsg.tool_calls = toolCalls;
        assistantMsg.reasoning_content = std::move(reasoningBuf);
        apiMessages.push_back(std::move(assistantMsg));

        // Server now has everything through this response's output. Anything we append
        // from here on (tool results) is "new input" for the next turn.
        baseCount = apiMessages.size();

        if (toolCalls.empty()) {
            spdlog::info("[AgentEngine] No tool calls - done");
            break;
        }

        spdlog::info("[Tool] Executing {} tool call(s) from Responses API", toolCalls.size());

        for (const auto& tc : toolCalls) {
            if (onLogLine_)
                onLogLine_(TranscriptChannel::Tool, "[Tool] " + tc.name + " ...");

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

    if (turn >= MAX_TOOL_TURNS) {
        spdlog::warn("[AgentEngine] Reached max tool turns (Responses) — clearing chain state");
        // There are un-submitted tool messages after baseCount that the server never saw.
        // Next run() will do a full resend to re-sync rather than continue from a torn chain.
        lastResponseId_.clear();
        lastSubmittedCount_ = 0;
    } else {
        // baseCount tracked non-persisted indices (with the injected dynCtx counted in).
        // After we erase dynCtx, indices before it are unchanged but indices after it
        // shift down by one — adjust the persisted submitted count accordingly.
        size_t persistedCount = baseCount;
        if (dynCtxInjected && baseCount > dynCtxIdx && persistedCount > 0)
            persistedCount -= 1;
        lastSubmittedCount_ = persistedCount;
    }

    // Strip the injected dynamic-context refresh so it is re-built from live state next run().
    if (dynCtxInjected && dynCtxIdx < apiMessages.size()) {
        apiMessages.erase(apiMessages.begin() + static_cast<std::ptrdiff_t>(dynCtxIdx));
    }

    messagesInOut = std::move(apiMessages);
    return true;
}

} // namespace avacli
