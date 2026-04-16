#pragma once

#include <cctype>
#include <string>
#include <vector>
#include <optional>

namespace avacli {

enum class Mode {
    Question,
    Plan,
    Agent
};

/// Log line channel for TUI transcript (user input vs assistant/tools/system).
enum class TranscriptChannel {
    UserIn,
    AgentOut,
    Tool,
    System,
    Ask
};

inline const char* modeToString(Mode m) {
    switch (m) {
        case Mode::Question: return "Question";
        case Mode::Plan:     return "Plan";
        case Mode::Agent:    return "Agent";
        default:             return "Unknown";
    }
}

inline std::string modeToStdString(Mode m) {
    return std::string(modeToString(m));
}

inline Mode modeFromString(const std::string& s) {
    std::string lower;
    lower.reserve(s.size());
    for (char c : s) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "question") return Mode::Question;
    if (lower == "plan")     return Mode::Plan;
    if (lower == "agent")    return Mode::Agent;
    return Mode::Question;
}

struct ModelConfig {
    std::string id;
    size_t      contextWindow = 131072;
    size_t      defaultMaxTokens = 65536;
    bool        reasoning = false;
    bool        responsesApi = false;
    std::string type = "chat";  // "chat", "image_gen", "video_gen"
};

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

struct ContentPart {
    std::string type;      // "text" or "image_url"
    std::string text;      // for type=="text"
    std::string image_url; // for type=="image_url"
};

struct Message {
    std::string role;    // "system", "user", "assistant", "tool"
    std::string content;
    std::optional<std::string> tool_call_id;
    std::vector<ToolCall> tool_calls;
    std::vector<ContentPart> content_parts;
    /// Raw `reasoning_content` from xAI reasoning models (assistant turns only).
    /// Captured separately from `content` so it can be echoed back verbatim on
    /// the next turn, keeping xAI's reasoning-cache warm without polluting the
    /// user-visible transcript with chain-of-thought. Empty for non-reasoning
    /// models and for user/tool turns. Non-reasoning models ignore the field
    /// when it's round-tripped in a request body.
    std::string reasoning_content;
};

// Stub: serialize ToolCall result to JSON for API. Expanded in tool execution phase.
inline std::string toolCallToJson(const ToolCall& tc) {
    return "{\"tool_call_id\":\"" + tc.id + "\",\"result\":\"\"}";
}

struct Usage {
    size_t promptTokens = 0;
    size_t completionTokens = 0;
    /// Subset of promptTokens that were served from the xAI prompt cache. Cached tokens
    /// bill at ~10x lower input rate on grok-4.20-* models (4x on grok-4-1-fast).
    /// Parsed from usage.prompt_tokens_details.cached_tokens (chat completions) or
    /// usage.input_tokens_details.cached_tokens (Responses API).
    size_t cachedPromptTokens = 0;
    /// Reasoning tokens for grok-4.20-*-reasoning models. Already summed into
    /// completionTokens above; kept separately so we can bill / display accurately.
    size_t reasoningTokens = 0;

    size_t totalTokens() const { return promptTokens + completionTokens; }
    size_t billablePromptTokens() const {
        return promptTokens > cachedPromptTokens ? promptTokens - cachedPromptTokens : 0;
    }
    double cacheHitRatio() const {
        return promptTokens > 0
            ? static_cast<double>(cachedPromptTokens) / static_cast<double>(promptTokens)
            : 0.0;
    }
};

struct TodoItem {
    std::string id;
    std::string title;
    std::string status;   // pending, in_progress, done
    std::string description;
};

struct MemoryEntry {
    std::string id;
    std::string timestamp;
    std::string category;  // architecture, decision, gotcha, integration, custom
    std::string content;
    std::vector<std::string> tags;
    bool pinned = false;
};

struct EditRecord {
    std::string file;
    std::string action;    // edit, write, undo
    std::string timestamp;
    std::string summary;
};

} // namespace avacli
