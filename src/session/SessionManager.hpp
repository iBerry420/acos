#pragma once

#include "core/Types.hpp"
#include "core/TokenTracker.hpp"
#include <string>
#include <vector>

namespace avacli {

struct SessionData {
    std::vector<Message> messages;
    Mode mode = Mode::Question;
    size_t totalPromptTokens = 0;
    size_t totalCompletionTokens = 0;
    std::vector<std::string> projectNotes;
    std::vector<TodoItem> todos;
    /// Stable xAI cache-routing key. Generated once per session and persisted,
    /// so turns N+1, N+2 … always land on the same xAI server pool and
    /// actually hit the warm KV cache. Independent of the user-visible
    /// session name so renames don't evict the cache.
    std::string convId;
    /// Responses API only. The `response.id` from the most recent successful
    /// stream. Next `runResponses()` sends this as `previous_response_id` so
    /// xAI continues the server-stored conversation (reasoning + tool state
    /// intact) instead of us re-sending the full input. Responses are kept
    /// for 30 days; after that, the id expires and AgentEngine transparently
    /// falls back to a full-history resend.
    std::string lastResponseId;
    /// Non-system message count that the server has absorbed. AgentEngine
    /// submits only `messages[lastSubmittedCount..]` when chaining via
    /// `lastResponseId` so bandwidth and cache reuse match the stored state.
    size_t lastSubmittedCount = 0;
};

class SessionManager {
public:
    explicit SessionManager(const std::string& sessionsDir = defaultSessionsDir());

    bool save(const std::string& name, const SessionData& data);
    bool load(const std::string& name, SessionData& data);
    bool exists(const std::string& name) const;
    std::string sessionPath(const std::string& name) const;

    static std::string defaultSessionsDir();

    /// Mint a fresh conv_id for a new session. Format: "conv-<16hex>".
    /// Callers typically invoke this once when SessionData.convId is empty
    /// and persist the result on the next save.
    static std::string generateConvId();

private:
    std::string baseDir_;
};

} // namespace avacli
