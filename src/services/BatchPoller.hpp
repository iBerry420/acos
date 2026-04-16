#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace avacli {

struct ServeConfig;

/// Background poller for xAI batch jobs. Sits in its own thread, wakes every
/// ~30 seconds, selects rows from `batches` with `state='processing'`, calls
/// `BatchClient::get()` on each, updates the row, and when `num_pending == 0`
/// fetches all pages of `listResults()` into the `results_json` column so the
/// row becomes self-contained (survives the upstream batch expiring).
///
/// Singleton, started in HttpServer::start() and stopped in HttpServer::stop().
/// Authentication + endpoint come from the active ServeConfig — we look up
/// the xAI key the same way XAIClient does.
class BatchPoller {
public:
    static BatchPoller& instance();

    /// Must be called before start(); holds a non-owning pointer so the
    /// poller can refresh credentials when Settings rewrite them.
    void setServeConfig(ServeConfig* cfg);

    void start();
    void stop();
    bool running() const { return running_; }

    /// Trigger an immediate poll tick (doesn't wait for the next cycle).
    /// Safe to call concurrently; only one poll runs at a time.
    void pollNow();

private:
    BatchPoller() = default;
    ~BatchPoller();
    BatchPoller(const BatchPoller&) = delete;
    BatchPoller& operator=(const BatchPoller&) = delete;

    void runLoop();
    void pollOnce();

    /// Fetch every page of results for a completed batch and pack them into
    /// the row's `results_json` field. Returns total cost ticks summed from
    /// successful results when the endpoint doesn't surface `cost_breakdown`.
    int64_t snapshotResults(const std::string& batchId, std::string& outJson);

    ServeConfig* config_ = nullptr;
    std::thread  thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
    std::mutex        pollMu_;
};

} // namespace avacli
