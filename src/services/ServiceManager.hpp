#pragma once
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace avacli {

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

private:
    ServiceManager() = default;
    ~ServiceManager();
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

    struct RunningService {
        std::thread thread;
        std::atomic<bool> shouldStop{false};
        std::atomic<int> consecutiveFailures{0};
    };

    static constexpr int MAX_CONSECUTIVE_FAILURES = 5;

    void runServiceLoop(const std::string& id, std::atomic<bool>& shouldStop);
    bool executeServiceTick(const std::string& id, const nlohmann::json& config,
                            const std::string& type);

    mutable std::mutex mu_;
    std::map<std::string, std::unique_ptr<RunningService>> running_;
    std::atomic<bool> shuttingDown_{false};
};

} // namespace avacli
