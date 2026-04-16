#include "core/Application.hpp"
#include "client/XAIClient.hpp"
#include "config/ModelRegistry.hpp"
#include "server/HttpServer.hpp"
#include "tools/ProjectNotes.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <csignal>
#if defined(_WIN32)
#include <io.h>
#include <cstdio>
#else
#include <unistd.h>
#endif

namespace avacli {

namespace {

#if defined(_WIN32)
inline bool stdinIsTTY() { return _isatty(_fileno(stdin)) != 0; }
#else
inline bool stdinIsTTY() { return isatty(STDIN_FILENO) != 0; }
#endif

class StreamJsonEmitter {
public:
    void emitEvent(const nlohmann::json& j) {
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << j.dump() << "\n" << std::flush;
    }

    void onContentDelta(const std::string& delta) {
        std::lock_guard<std::mutex> lock(mu_);
        buffer_ += delta;
        processBuffer();
    }

    void onToolStart(const std::string& id, const std::string& name, const std::string& args) {
        flushPublic();
        nlohmann::json j;
        j["type"] = "tool_start";
        j["tool_call_id"] = id;
        j["name"] = name;
        try { j["arguments"] = nlohmann::json::parse(args); }
        catch (...) { j["arguments"] = args; }
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << j.dump() << "\n" << std::flush;
    }

    void onToolDone(const std::string& id, const std::string& name, bool success, const std::string& result) {
        nlohmann::json j;
        j["type"] = "tool_done";
        j["tool_call_id"] = id;
        j["name"] = name;
        j["success"] = success;
        std::string truncated = result.size() > 4000 ? result.substr(0, 4000) + "..." : result;
        j["result"] = truncated;
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << j.dump() << "\n" << std::flush;
    }

    void onUsage(const Usage& u) {
        nlohmann::json j;
        j["type"] = "usage";
        j["prompt_tokens"] = u.promptTokens;
        j["completion_tokens"] = u.completionTokens;
        j["cached_tokens"] = u.cachedPromptTokens;
        j["reasoning_tokens"] = u.reasoningTokens;
        j["billable_prompt_tokens"] = u.billablePromptTokens();
        if (u.promptTokens > 0)
            j["cache_hit_rate"] = u.cacheHitRatio();
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << j.dump() << "\n" << std::flush;
    }

    void onDone(bool success) {
        flushPublic();
        nlohmann::json j;
        j["type"] = "done";
        j["success"] = success;
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << j.dump() << "\n" << std::flush;
    }

    void flushPublic() {
        std::lock_guard<std::mutex> lock(mu_);
        flushBuffer();
    }

private:
    std::mutex mu_;
    bool inThought_ = false;
    std::string buffer_;

    static constexpr size_t OPEN_TAG_LEN = 9;
    static constexpr size_t CLOSE_TAG_LEN = 10;

    void emitLocked(const nlohmann::json& j) {
        std::cout << j.dump() << "\n" << std::flush;
    }

    void processBuffer() {
        while (!buffer_.empty()) {
            if (!inThought_) {
                auto pos = buffer_.find("<thought>");
                if (pos != std::string::npos) {
                    if (pos > 0) emitLocked({{"type", "content_delta"}, {"content", buffer_.substr(0, pos)}});
                    inThought_ = true;
                    buffer_.erase(0, pos + OPEN_TAG_LEN);
                } else {
                    size_t safe = (buffer_.size() > OPEN_TAG_LEN - 1) ? buffer_.size() - (OPEN_TAG_LEN - 1) : 0;
                    if (safe > 0) {
                        emitLocked({{"type", "content_delta"}, {"content", buffer_.substr(0, safe)}});
                        buffer_.erase(0, safe);
                    }
                    break;
                }
            } else {
                auto pos = buffer_.find("</thought>");
                if (pos != std::string::npos) {
                    if (pos > 0) emitLocked({{"type", "thinking_delta"}, {"content", buffer_.substr(0, pos)}});
                    emitLocked({{"type", "thinking_done"}});
                    inThought_ = false;
                    buffer_.erase(0, pos + CLOSE_TAG_LEN);
                } else {
                    size_t safe = (buffer_.size() > CLOSE_TAG_LEN - 1) ? buffer_.size() - (CLOSE_TAG_LEN - 1) : 0;
                    if (safe > 0) {
                        emitLocked({{"type", "thinking_delta"}, {"content", buffer_.substr(0, safe)}});
                        buffer_.erase(0, safe);
                    }
                    break;
                }
            }
        }
    }

    void flushBuffer() {
        if (!buffer_.empty()) {
            if (inThought_) {
                emitLocked({{"type", "thinking_delta"}, {"content", buffer_}});
                emitLocked({{"type", "thinking_done"}});
                inThought_ = false;
            } else {
                emitLocked({{"type", "content_delta"}, {"content", buffer_}});
            }
            buffer_.clear();
        }
    }
};

} // namespace

Application::Application(Config config)
    : config_(std::move(config)) {

    if (config_.workspace.empty())
        config_.workspace = ".";
    config_.resolveWorkspace();
    if (config_.nonInteractive)
        config_.tuiVerbose = false;

    if (!config_.session.empty() && sessionMgr_.exists(config_.session)) {
        sessionMgr_.load(config_.session, sessionData_);
    } else {
        sessionData_.mode = config_.mode;
        sessionData_.totalPromptTokens = 0;
        sessionData_.totalCompletionTokens = 0;
    }

    // Mint a stable cache-routing key once per session. saveSession() persists it,
    // so subsequent runs of the same --session reuse the same conv_id and keep
    // hitting xAI's warm KV cache.
    if (sessionData_.convId.empty()) {
        sessionData_.convId = SessionManager::generateConvId();
        spdlog::info("[Session] Minted conv_id {} for session '{}'",
                     sessionData_.convId,
                     config_.session.empty() ? "<ephemeral>" : config_.session);
    } else {
        spdlog::info("[Session] Reusing conv_id {} for session '{}'",
                     sessionData_.convId, config_.session);
    }

    tokenTracker_.addUsage(sessionData_.totalPromptTokens, sessionData_.totalCompletionTokens);
    ensureProjectNotesExist(config_.workspace);
    ensureTodosExist(config_.workspace);

    if (!config_.session.empty()) {
        ensureSessionStorage(config_.session);
    }

    ToolRegistry::registerAll();
    toolExecutor_ = std::make_unique<ToolExecutor>(
        config_.workspace, sessionData_.mode, nullptr);
    toolExecutor_->setSessionId(config_.session);
    toolExecutor_->setSearchModel(config_.model);
    toolExecutor_->setConvId(sessionData_.convId);
    toolExecutor_->setModeChangeCallback([this](Mode m) {
        sessionData_.mode = m;
        if (toolExecutor_) toolExecutor_->setMode(m);
        if (agentEngine_) agentEngine_->setMode(m);
        spdlog::info("[Mode] Switched to {}", modeToString(m));
    });
}

void Application::saveSession() {
    if (config_.session.empty()) return;
    sessionData_.totalPromptTokens = tokenTracker_.promptTokens();
    sessionData_.totalCompletionTokens = tokenTracker_.completionTokens();
    loadTodosFromFile(config_.workspace, sessionData_.todos);
    // Persist Responses API chain state so the next run skips re-sending history.
    if (agentEngine_) {
        sessionData_.lastResponseId = agentEngine_->lastResponseId();
        sessionData_.lastSubmittedCount = agentEngine_->lastSubmittedCount();
    }
    sessionMgr_.save(config_.session, sessionData_);
    spdlog::info("Session saved: {}", config_.session);
}

int Application::runHeadless() {
    std::string task = config_.task;
    if (task.empty()) {
        if (!stdinIsTTY()) {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!task.empty()) task += '\n';
                task += line;
            }
        }
    }

    if (task.empty()) {
        std::cerr << "Error: --non-interactive requires -t <task> or piped stdin\n";
        return 1;
    }

    sessionData_.messages.push_back({"user", task});
    toolExecutor_->setAskUser(nullptr);

    const bool streamJson = (config_.outputFormat == "stream-json");
    auto emitter = streamJson ? std::make_shared<StreamJsonEmitter>() : nullptr;

    agentEngine_ = std::make_unique<AgentEngine>(
        config_.workspace, sessionData_.mode, toolExecutor_.get(),
        [streamJson, emitter](const std::string& delta) {
            if (streamJson) {
                emitter->onContentDelta(delta);
            } else {
                std::cout << delta << std::flush;
            }
        },
        [streamJson](TranscriptChannel, const std::string& line) {
            if (!streamJson) {
                std::cerr << line << "\n";
            }
        },
        nullptr,
        [this](Mode m) {
            sessionData_.mode = m;
            if (toolExecutor_) toolExecutor_->setMode(m);
            if (agentEngine_) agentEngine_->setMode(m);
            spdlog::info("[Mode] Switched to {}", modeToString(m));
        });
    agentEngine_->setVerboseToolOutput(config_.tuiVerbose);
    agentEngine_->setConvId(sessionData_.convId);
    // Seed Responses API chain state from disk so we can keep chaining across
    // multiple headless invocations of the same session.
    agentEngine_->setLastResponseId(sessionData_.lastResponseId);
    agentEngine_->setLastSubmittedCount(sessionData_.lastSubmittedCount);

    if (streamJson) {
        agentEngine_->setToolStartCallback(
            [emitter](const std::string& id, const std::string& name, const std::string& args) {
                emitter->onToolStart(id, name, args);
            });
        agentEngine_->setToolDoneCallback(
            [emitter](const std::string& id, const std::string& name, bool success, const std::string& result) {
                emitter->onToolDone(id, name, success, result);
            });
    }

    XAIClient client(config_.xaiApiKey(), "https://api.x.ai/v1/chat/completions");
    auto tools = ToolRegistry::getToolsForMode(sessionData_.mode);

    auto onUsage = [this, streamJson, emitter](const Usage& u) {
        tokenTracker_.addUsage(u.promptTokens, u.completionTokens,
                               u.cachedPromptTokens, u.reasoningTokens);
        if (streamJson) {
            emitter->onUsage(u);
        }
    };

    bool ok = agentEngine_->run(client, config_.model, sessionData_.messages, tools, onUsage);

    if (streamJson) {
        emitter->onDone(ok);
    } else {
        std::cout << "\n";
    }

    if (!ok && !streamJson) {
        std::cerr << "Error: " << client.lastError() << "\n";
    }

    saveSession();

    if (config_.outputFormat == "json") {
        nlohmann::json j;
        j["success"] = ok;
        j["prompt_tokens"] = tokenTracker_.promptTokens();
        j["completion_tokens"] = tokenTracker_.completionTokens();
        j["cached_tokens"] = tokenTracker_.cachedPromptTokens();
        j["reasoning_tokens"] = tokenTracker_.reasoningTokens();
        j["billable_prompt_tokens"] = tokenTracker_.billablePromptTokens();
        j["cache_hit_percent"] = tokenTracker_.cacheHitPercent();
        j["total_tokens"] = tokenTracker_.totalTokens();
        if (!config_.session.empty()) j["session"] = config_.session;
        std::cerr << j.dump(2) << "\n";
    }

    return ok ? 0 : 1;
}

int Application::runServe() {
    if (!config_.validateServe()) {
        spdlog::error("Invalid serve configuration. Need valid port.");
        return 1;
    }

    ServeConfig sc;
    sc.port = config_.servePort;
    sc.host = config_.serveHost;
    sc.model = config_.model;
    sc.workspace = config_.workspace;
    sc.session = config_.session;
    sc.apiKey = config_.xaiApiKey();
    sc.chatEndpointUrl = "https://api.x.ai/v1/chat/completions";
    sc.uiDir = config_.uiDir;
    sc.useEmbeddedUI = config_.useEmbeddedUI;
    sc.uiTheme = config_.uiTheme;

    HttpServer server(sc);
    server.start();
    return 0;
}

int Application::run() {
    // Serve mode starts without credentials — connect via the web UI settings
    if (config_.serveMode)
        return runServe();

    if (!config_.validate()) {
        spdlog::error("No API key. Set XAI_API_KEY or run: avacli --set-api-key <key>");
        return 1;
    }

    // Default to headless/non-interactive mode (TUI removed)
    config_.nonInteractive = true;
    return runHeadless();
}

} // namespace avacli
