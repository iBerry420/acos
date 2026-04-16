#pragma once
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace avacli {

class ChildProcess;

class ServiceManager {
public:
    static ServiceManager& instance();

    void init();
    void shutdown();

    void startService(const std::string& id);
    void stopService(const std::string& id);
    bool isRunning(const std::string& id) const;

    void logToService(const std::string& serviceId, const std::string& level,
                      const std::string& message);

    /// Live status for a running `type:"process"` service.
    /// Returns null JSON when the service isn't running (or is a tick service).
    nlohmann::json processStatus(const std::string& id) const;

    /// Signal a running process service. `signal` is "term" | "kill" | "restart".
    /// Returns true if delivered, false if not a running process service.
    bool signalProcess(const std::string& id, const std::string& signal);

private:
    ServiceManager() = default;
    ~ServiceManager();
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

    /// `tick` covers rss_feed / scheduled_prompt / custom_script and any
    /// legacy `custom` type. `process` covers the new long-running supervised
    /// runtime (python/node/bin).
    enum class Kind { Tick, Process };

    struct RunningService {
        Kind kind = Kind::Tick;
        std::thread thread;
        std::atomic<bool> shouldStop{false};
        std::atomic<int>  consecutiveFailures{0};

        // Process-only state. Accessed under `mu_` at the manager level, and
        // under `procMu` for intra-thread reads (readAvailable is thread-safe
        // on a single handle, but `child` can be swapped out during restart).
        mutable std::mutex procMu;
        std::unique_ptr<ChildProcess> child;
        long pid = 0;
        long long startedAtMs = 0;
        int  restartCount = 0;
        int  lastExitCode = 0;

        // Signal from API / tool layer: force a respawn at the next loop tick.
        std::atomic<bool> restartRequested{false};
    };

    static constexpr int MAX_CONSECUTIVE_FAILURES = 5;

    Kind kindFromType(const std::string& type) const;

    void runTickLoop(const std::string& id, RunningService& svc);
    bool executeServiceTick(const std::string& id, const nlohmann::json& config,
                            const std::string& type);

    void runProcessLoop(const std::string& id, RunningService& svc);
    void persistSupervisorState(const std::string& id, RunningService& svc);

    mutable std::mutex mu_;
    std::map<std::string, std::unique_ptr<RunningService>> running_;
    std::atomic<bool> shuttingDown_{false};
};

} // namespace avacli
