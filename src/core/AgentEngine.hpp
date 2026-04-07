#pragma once

#include "core/Types.hpp"
#include "tools/ToolExecutor.hpp"
#include "tools/ToolRegistry.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace avacli {

class XAIClient;
class TerminalUI;

/// Drives the full autonomous agent workflow with multi-turn tool loop.
/// Think → Update Notes → TODOs → Plan → Execute tools → Verify → Reflect → Repeat.
class AgentEngine {
public:
    using StreamDeltaFn = std::function<void(const std::string&)>;
    using LogLineFn = std::function<void(TranscriptChannel, const std::string&)>;
    using AskUserFn = std::function<std::string(const std::string&)>;
    using ModeChangeFn = std::function<void(Mode)>;
    using ToolStartFn = std::function<void(const std::string& id, const std::string& name, const std::string& arguments)>;
    using ToolDoneFn = std::function<void(const std::string& id, const std::string& name, bool success, const std::string& result)>;

    AgentEngine(std::string workspace,
                Mode mode,
                ToolExecutor* executor,
                StreamDeltaFn onStreamDelta,
                LogLineFn onLogLine,
                AskUserFn askUser,
                ModeChangeFn onModeChange);

    using UsageFn = std::function<void(const Usage&)>;

    /// Run chat loop with tools until completion (no more tool calls).
    /// Returns true on success, false on error.
    bool run(XAIClient& client,
             const std::string& model,
             std::vector<Message>& messagesInOut,
             const std::vector<nlohmann::json>& tools,
             UsageFn onUsage = nullptr);

    void setMode(Mode m) { mode_ = m; }
    Mode mode() const { return mode_; }
    void setWorkspace(const std::string& w) { workspace_ = w; }

    void setToolStartCallback(ToolStartFn fn) { onToolStart_ = std::move(fn); }
    void setToolDoneCallback(ToolDoneFn fn) { onToolDone_ = std::move(fn); }
    void setVerboseToolOutput(bool v) { verboseToolOutput_ = v; }
    void setCancelFlag(std::atomic<bool>* p) { cancelFlag_ = p; }
    void setMaxTokensOverride(int n) { maxTokensOverride_ = n; }

    /// Build rich system prompt for the current mode.
    std::string buildSystemPrompt(Mode mode, const std::string& workspace);

private:
    std::string workspace_;
    Mode mode_;
    ToolExecutor* executor_;
    StreamDeltaFn onStreamDelta_;
    LogLineFn onLogLine_;
    AskUserFn askUser_;
    ModeChangeFn onModeChange_;
    ToolStartFn onToolStart_;
    ToolDoneFn onToolDone_;
    bool verboseToolOutput_ = false;
    std::atomic<bool>* cancelFlag_ = nullptr;
    int maxTokensOverride_ = 0;
};

} // namespace avacli
