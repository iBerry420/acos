#pragma once

#include "config/ModelRegistry.hpp"
#include "core/Types.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace avacli {

struct ChatOptions {
    std::string model;
    size_t maxTokens = 65536;
    bool stream = true;
    /// When non-null and loads true, streaming aborts as soon as possible.
    std::atomic<bool>* cancelFlag = nullptr;
    /// Reasoning effort for Responses API models: "low", "medium", "high", "xhigh".
    /// Empty = omit from request.
    std::string reasoningEffort;
    /// Stable routing key for xAI's prompt cache. Emitted as the `x-grok-conv-id`
    /// header on chat-completions requests, or as `prompt_cache_key` in the body
    /// on Responses API requests. Empty = no routing hint (requests get
    /// round-robined by xAI and cache hits become opportunistic). Keeping this
    /// byte-stable across all turns of a logical session is what unlocks the
    /// ~10x cached-input discount on grok-4.20-* models.
    std::string convId;
    /// Responses API only. When non-empty, sent as `previous_response_id` so xAI
    /// chains from the server-side stored conversation (reasoning state + prior
    /// turns). With this set, `input` should contain ONLY new items since the
    /// last response — xAI remembers everything before it. Responses are stored
    /// for 30 days. If the ID has expired or the call returns "response not
    /// found", AgentEngine falls back to a full-history resend.
    std::string previousResponseId;
};

class XAIClient {
public:
    explicit XAIClient(const std::string& apiKey,
                       const std::string& chatUrl = "https://api.x.ai/v1/chat/completions");

    using ContentCallback = std::function<void(const std::string& delta)>;
    using ToolCallsCallback = std::function<void(const std::vector<ToolCall>&)>;
    using UsageCallback = std::function<void(const Usage&)>;
    using DoneCallback = std::function<void()>;
    /// Reasoning-content delta callback (chat-completions reasoning models).
    /// When set, XAIClient routes raw `reasoning_content` deltas here instead
    /// of inlining them as `<thought>…</thought>` through `onContent`. Lets the
    /// caller accumulate the reasoning separately for replay on subsequent
    /// turns — required to keep xAI's reasoning cache warm across the loop.
    using ReasoningCallback = std::function<void(const std::string& delta)>;
    /// Response-ID callback (Responses API only). Fires once per successful
    /// `response.completed` / `response.done` event with the server-assigned
    /// response id. Caller stashes this and passes it as
    /// `ChatOptions::previousResponseId` on the next turn.
    using ResponseIdCallback = std::function<void(const std::string& id)>;

    struct StreamCallbacks {
        ContentCallback onContent;
        ToolCallsCallback onToolCalls;
        UsageCallback onUsage;
        DoneCallback onDone;
        ReasoningCallback onReasoning;
        ResponseIdCallback onResponseId;
    };

    bool chatStream(const std::vector<Message>& messages,
                    const ChatOptions& opts,
                    const StreamCallbacks& callbacks);

    bool chatStream(const std::vector<Message>& messages,
                    const ChatOptions& opts,
                    const std::vector<nlohmann::json>& tools,
                    const StreamCallbacks& callbacks);

    /// Stream via the xAI Responses API (/v1/responses).
    /// Used for multi-agent models (grok-4.20-multi-agent, etc.).
    /// `tools` should already be in Responses API format (flat, not wrapped in "function").
    /// `instructions` is the system/developer prompt (separate from input messages).
    bool responsesStream(const std::string& instructions,
                         const std::vector<nlohmann::json>& input,
                         const ChatOptions& opts,
                         const std::vector<nlohmann::json>& tools,
                         const StreamCallbacks& callbacks);

    std::string lastError() const { return lastError_; }

private:
    std::string apiKey_;
    std::string chatUrl_;
    std::string responsesUrl_ = "https://api.x.ai/v1/responses";
    std::string lastError_;
};

} // namespace avacli
