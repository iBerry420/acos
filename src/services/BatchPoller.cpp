#include "services/BatchPoller.hpp"

#include "client/BatchClient.hpp"
#include "db/Database.hpp"
#include "server/HttpServer.hpp"
#include "server/ServerHelpers.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>

namespace avacli {

namespace {

int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// Seconds between poll cycles. Short enough that interactive users see their
/// small batches complete in near-real-time, long enough to not spam xAI.
constexpr int POLL_INTERVAL_SEC = 30;
/// Initial backoff before the first poll so the server finishes booting.
constexpr int INITIAL_DELAY_SEC = 5;

} // namespace

BatchPoller& BatchPoller::instance() {
    static BatchPoller p;
    return p;
}

BatchPoller::~BatchPoller() {
    stop();
}

void BatchPoller::setServeConfig(ServeConfig* cfg) {
    config_ = cfg;
}

void BatchPoller::start() {
    if (running_) return;
    stopFlag_ = false;
    running_ = true;
    thread_ = std::thread(&BatchPoller::runLoop, this);
    spdlog::info("[batch] poller started");
}

void BatchPoller::stop() {
    if (!running_) return;
    stopFlag_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
    spdlog::info("[batch] poller stopped");
}

void BatchPoller::pollNow() {
    if (!running_) return;
    std::thread([this] {
        try { pollOnce(); }
        catch (const std::exception& ex) {
            spdlog::warn("[batch] pollNow error: {}", ex.what());
        }
    }).detach();
}

void BatchPoller::runLoop() {
    for (int i = 0; i < INITIAL_DELAY_SEC && !stopFlag_; ++i)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    while (!stopFlag_) {
        try {
            pollOnce();
        } catch (const std::exception& ex) {
            spdlog::warn("[batch] poll cycle error: {}", ex.what());
        }
        for (int i = 0; i < POLL_INTERVAL_SEC && !stopFlag_; ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void BatchPoller::pollOnce() {
    std::lock_guard<std::mutex> lk(pollMu_);
    if (!config_) return;

    std::string apiKey, chatUrl;
    resolveXaiAuth(*config_, apiKey, chatUrl);
    if (apiKey.empty()) return;  // silent — no key configured yet

    // Derive the batch base URL from the chat URL so that callers who point
    // us at a proxy or alternate region keep working. We strip the trailing
    // /chat/completions segment to get back to `/v1`.
    std::string baseUrl = chatUrl;
    auto pos = baseUrl.find("/chat/completions");
    if (pos != std::string::npos) baseUrl.erase(pos);
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
    if (baseUrl.empty()) baseUrl = "https://api.x.ai/v1";

    nlohmann::json rows;
    try {
        rows = Database::instance().query(
            "SELECT id FROM batches WHERE state = 'processing' "
            "ORDER BY created_at ASC LIMIT 100");
    } catch (const std::exception& ex) {
        spdlog::warn("[batch] select processing: {}", ex.what());
        return;
    }
    if (rows.empty()) return;

    BatchClient client(apiKey, baseUrl);
    int polled = 0, completed = 0;

    for (const auto& row : rows) {
        if (stopFlag_) break;
        std::string id = row.value("id", "");
        if (id.empty()) continue;

        auto st = client.get(id);
        ++polled;
        if (!st) {
            spdlog::debug("[batch] get({}) failed: {}", id, client.lastError());
            try {
                Database::instance().execute(
                    "UPDATE batches SET last_polled = ?1 WHERE id = ?2",
                    {std::to_string(nowSec()), id});
            } catch (...) {}
            continue;
        }

        bool finished = (st->numRequests > 0 && st->numPending == 0);
        std::string newState = finished
            ? (st->numCancelled > 0 && st->numSuccess == 0 ? "cancelled" : "complete")
            : "processing";

        std::string resultsJson;
        int64_t costTicks = st->costUsdTicks;
        if (finished) {
            int64_t derivedCost = snapshotResults(id, resultsJson);
            if (costTicks == 0) costTicks = derivedCost;
            ++completed;
            spdlog::info("[batch] {} complete: success={} error={} cost=${:.4f}",
                         id, st->numSuccess, st->numError,
                         static_cast<double>(costTicks) / 1e10);
        }

        try {
            if (finished) {
                Database::instance().execute(
                    "UPDATE batches SET state = ?1, num_requests = ?2, num_pending = ?3, "
                    "num_success = ?4, num_error = ?5, num_cancelled = ?6, "
                    "cost_usd_ticks = ?7, expires_at = ?8, last_polled = ?9, "
                    "results_json = ?10, updated_at = ?11 WHERE id = ?12",
                    {
                        newState,
                        std::to_string(st->numRequests),
                        std::to_string(st->numPending),
                        std::to_string(st->numSuccess),
                        std::to_string(st->numError),
                        std::to_string(st->numCancelled),
                        std::to_string(costTicks),
                        std::to_string(st->expiresAt),
                        std::to_string(nowSec()),
                        resultsJson,
                        std::to_string(nowSec()),
                        id,
                    });
            } else {
                Database::instance().execute(
                    "UPDATE batches SET state = ?1, num_requests = ?2, num_pending = ?3, "
                    "num_success = ?4, num_error = ?5, num_cancelled = ?6, "
                    "cost_usd_ticks = ?7, expires_at = ?8, last_polled = ?9, "
                    "updated_at = ?10 WHERE id = ?11",
                    {
                        newState,
                        std::to_string(st->numRequests),
                        std::to_string(st->numPending),
                        std::to_string(st->numSuccess),
                        std::to_string(st->numError),
                        std::to_string(st->numCancelled),
                        std::to_string(costTicks),
                        std::to_string(st->expiresAt),
                        std::to_string(nowSec()),
                        std::to_string(nowSec()),
                        id,
                    });
            }
        } catch (const std::exception& ex) {
            spdlog::warn("[batch] update {}: {}", id, ex.what());
        }
    }

    if (polled > 0)
        spdlog::debug("[batch] cycle: polled={} completed={}", polled, completed);
}

int64_t BatchPoller::snapshotResults(const std::string& batchId, std::string& outJson) {
    if (!config_) { outJson.clear(); return 0; }

    std::string apiKey, chatUrl;
    resolveXaiAuth(*config_, apiKey, chatUrl);
    if (apiKey.empty()) { outJson.clear(); return 0; }

    std::string baseUrl = chatUrl;
    auto pos = baseUrl.find("/chat/completions");
    if (pos != std::string::npos) baseUrl.erase(pos);
    while (!baseUrl.empty() && baseUrl.back() == '/') baseUrl.pop_back();
    if (baseUrl.empty()) baseUrl = "https://api.x.ai/v1";

    BatchClient client(apiKey, baseUrl);
    nlohmann::json all = nlohmann::json::array();
    int64_t totalTicks = 0;

    std::string token;
    while (true) {
        auto page = client.listResults(batchId, 100, token);
        if (page.results.empty() && page.nextPageToken.empty()) break;

        for (const auto& r : page.results) {
            nlohmann::json j;
            j["batch_request_id"] = r.batchRequestId;
            j["succeeded"]        = r.succeeded;
            if (!r.errorMessage.empty())
                j["error_message"] = r.errorMessage;
            j["response"]         = r.response;
            j["prompt_tokens"]    = r.promptTokens;
            j["completion_tokens"] = r.completionTokens;
            j["cached_tokens"]    = r.cachedTokens;
            j["reasoning_tokens"] = r.reasoningTokens;
            j["cost_usd_ticks"]   = r.costUsdTicks;
            all.push_back(j);
            totalTicks += r.costUsdTicks;
        }

        if (page.nextPageToken.empty()) break;
        token = page.nextPageToken;
    }

    outJson = all.dump();
    return totalTicks;
}

} // namespace avacli
