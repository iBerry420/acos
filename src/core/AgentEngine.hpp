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
    void setReasoningEffort(const std::string& e) { reasoningEffort_ = e; }

    /// Stable xAI cache-routing key for this session/task. Forwarded to the
    /// XAIClient on every turn — chat-completions sends it as `x-grok-conv-id`,
    /// Responses API sends it as `prompt_cache_key`. Empty = no hint.
    void setConvId(const std::string& id) { convId_ = id; }
    const std::string& convId() const { return convId_; }

    /// Responses API only. The last response id observed from xAI. When set,
    /// the next `runResponses()` call sends `previous_response_id` and slices
    /// `messagesInOut` to only the items appended after `lastSubmittedCount`,
    /// so xAI replays the stored history server-side (reasoning state intact).
    /// AgentEngine updates both values after every successful stream, so
    /// callers just need to persist + restore them across session save/load.
    void setLastResponseId(const std::string& id) { lastResponseId_ = id; }
    const std::string& lastResponseId() const { return lastResponseId_; }
    void setLastSubmittedCount(size_t n) { lastSubmittedCount_ = n; }
    size_t lastSubmittedCount() const { return lastSubmittedCount_; }
    /// When false, AgentEngine always sends full input — useful as a panic
    /// switch if xAI's stored-response feature misbehaves. Default true.
    void setUsePreviousResponseId(bool on) { usePreviousResponseId_ = on; }

    /// Build the full system prompt (static prefix + dynamic context) in a single string.
    /// Prefer the split `buildStaticSystemPrompt` + `buildDynamicContext` on hot paths so
    /// the static portion stays byte-stable across turns and can be served from the xAI
    /// prompt cache (~10x cheaper input tokens on grok-4.20 models).
    std::string buildSystemPrompt(Mode mode, const std::string& workspace);

    /// Cacheable static prefix: GROK_SYSTEM_PROMPT.md + workspace env. Byte-stable across
    /// turns as long as the source file and workspace path don't change.
    std::string buildStaticSystemPrompt(const std::string& workspace);

    /// Volatile per-session context (TODOs, memory, recent edits, project notes). Meant to
    /// be injected as its own message each time `run()` is invoked so it never overwrites
    /// the static prefix. Returns an empty string when there is nothing to include.
    std::string buildDynamicContext(const std::string& workspace);

private:
    bool runChatCompletions(XAIClient& client, const std::string& model,
                            std::vector<Message>& messagesInOut,
                            const std::vector<nlohmann::json>& tools, UsageFn onUsage);
    bool runResponses(XAIClient& client, const std::string& model,
                      std::vector<Message>& messagesInOut,
                      const std::vector<nlohmann::json>& tools, UsageFn onUsage);

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
    std::string reasoningEffort_;
    std::string convId_;
    /// Responses API chain state — persisted across runs via SessionData.
    std::string lastResponseId_;
    size_t lastSubmittedCount_ = 0;
    bool usePreviousResponseId_ = true;
};

} // namespace avacli
