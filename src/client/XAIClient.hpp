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
};

class XAIClient {
public:
    explicit XAIClient(const std::string& apiKey,
                       const std::string& chatUrl = "https://api.x.ai/v1/chat/completions");

    using ContentCallback = std::function<void(const std::string& delta)>;
    using ToolCallsCallback = std::function<void(const std::vector<ToolCall>&)>;
    using UsageCallback = std::function<void(const Usage&)>;
    using DoneCallback = std::function<void()>;

    struct StreamCallbacks {
        ContentCallback onContent;
        ToolCallsCallback onToolCalls;
        UsageCallback onUsage;
        DoneCallback onDone;
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
