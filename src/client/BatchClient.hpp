#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace avacli {

/// Synchronous client for xAI's Batch API (/v1/batches).
///
/// Batches pay a flat 50% discount across all token types (input, output,
/// cached, reasoning). Stacks with prompt caching — a cache-hit input in a
/// batched request pays ~$0.10/1M on grok-4.20 vs ~$2.00/1M real-time cold,
/// a 20x discount. Latency: typically ~seconds to minutes, always within 24h.
///
/// Lifecycle:
///   1. create(name)                        -> batch_id
///   2. addRequests(batch_id, requests)     -> true/false
///   3. poll via get(batch_id) until num_pending == 0
///   4. listResults(batch_id, ...)          -> paginated results
///   (optional) cancel(batch_id) before step 3 finishes
///
/// Docs: https://docs.x.ai/docs/guides/batch-api
class BatchClient {
public:
    explicit BatchClient(const std::string& apiKey,
                         const std::string& baseUrl = "https://api.x.ai/v1");

    /// Aggregate state counters from GET /v1/batches/:id.
    struct BatchState {
        std::string batchId;
        std::string name;
        std::string state;          // "processing" | "complete" | "cancelled" | "error"
        int numRequests  = 0;
        int numPending   = 0;
        int numSuccess   = 0;
        int numError     = 0;
        int numCancelled = 0;
        /// Cost in ticks = 1e-10 USD. Divide by 1e10 for dollars.
        int64_t costUsdTicks = 0;
        /// Unix epoch seconds when the batch results become unreachable.
        int64_t expiresAt    = 0;
        /// Raw payload (for debugging / forward-compat fields).
        nlohmann::json raw;
    };

    /// One element from GET /v1/batches/:id/results.
    struct BatchResult {
        std::string batchRequestId;
        bool succeeded = false;
        std::string errorMessage;
        /// Canonical response envelope — one of:
        ///   response.chat_get_completion  (chat completions)
        ///   response.responses            (Responses API)
        ///   response.image_generation
        ///   response.video_generation
        nlohmann::json response;
        size_t promptTokens     = 0;
        size_t completionTokens = 0;
        size_t cachedTokens     = 0;
        size_t reasoningTokens  = 0;
        int64_t costUsdTicks    = 0;
    };

    struct ResultsPage {
        std::vector<BatchResult> results;
        std::string nextPageToken;     // empty when no more pages
    };

    struct BatchSummary {
        std::string batchId;
        std::string name;
        std::string state;
        int numRequests = 0;
        int numPending  = 0;
        int64_t createdAt = 0;
        int64_t expiresAt = 0;
    };

    struct ListPage {
        std::vector<BatchSummary> batches;
        std::string nextPageToken;
    };

    /// POST /v1/batches
    /// Returns batch_id on success, empty string on failure (see lastError()).
    std::string create(const std::string& name);

    /// POST /v1/batches/:id/requests
    /// Each element must be the full `{batch_request_id, batch_request: {...}}`
    /// envelope. Use helpers below to construct individual requests.
    bool addRequests(const std::string& batchId,
                     const std::vector<nlohmann::json>& requests);

    /// GET /v1/batches/:id
    std::optional<BatchState> get(const std::string& batchId);

    /// GET /v1/batches/:id/results?limit=&pagination_token=
    ResultsPage listResults(const std::string& batchId,
                            int limit = 100,
                            const std::string& pageToken = "");

    /// POST /v1/batches/:id:cancel  (note the colon syntax)
    bool cancel(const std::string& batchId);

    /// GET /v1/batches?limit=
    ListPage list(int limit = 50, const std::string& pageToken = "");

    /// -- Request builders ----------------------------------------------------
    /// Wrap a chat-completions body in the batch_requests envelope.
    /// `chatBody` is what you'd normally POST to /v1/chat/completions.
    static nlohmann::json makeChatRequest(const std::string& batchRequestId,
                                          const nlohmann::json& chatBody);

    /// Wrap a Responses API body (/v1/responses) in the batch_requests envelope.
    static nlohmann::json makeResponsesRequest(const std::string& batchRequestId,
                                               const nlohmann::json& responsesBody);

    std::string lastError() const { return lastError_; }

    /// Exposed for testing / manual inspection.
    std::string lastHttpBody() const { return lastHttpBody_; }

private:
    std::string apiKey_;
    std::string baseUrl_;
    mutable std::string lastError_;
    mutable std::string lastHttpBody_;

    /// Low-level synchronous HTTP helper. Returns HTTP status code; `out`
    /// receives the response body. Sets lastError_ on transport failure.
    long httpRequest(const std::string& method,
                     const std::string& url,
                     const std::string& body,
                     std::string& out);
};

} // namespace avacli
