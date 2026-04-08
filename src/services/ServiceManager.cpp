#include "services/ServiceManager.hpp"
#include "db/Database.hpp"
#include "server/LogBuffer.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace avacli {

namespace {
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
}

ServiceManager& ServiceManager::instance() {
    static ServiceManager mgr;
    return mgr;
}

ServiceManager::~ServiceManager() {
    shutdown();
}

void ServiceManager::init() {
    try {
        auto& db = Database::instance();
        auto rows = db.query("SELECT id FROM services WHERE status = 'running'");
        for (const auto& r : rows) {
            std::string id = r["id"].get<std::string>();
            try {
                startService(id);
            } catch (...) {
                spdlog::warn("Failed to auto-start service {}", id);
            }
        }
    } catch (...) {}
}

void ServiceManager::shutdown() {
    shuttingDown_ = true;
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [id, svc] : running_) {
        svc->shouldStop = true;
        if (svc->thread.joinable())
            svc->thread.join();
    }
    running_.clear();
}

void ServiceManager::startService(const std::string& id) {
    try {
        auto& db = Database::instance();
        auto row = db.queryOne("SELECT type, config FROM services WHERE id = ?1", {id});
        if (row.is_null()) {
            spdlog::warn("Cannot start service {}: not found in database", id);
            return;
        }
        std::string type = row["type"].get<std::string>();
        nlohmann::json config;
        if (row["config"].is_string()) {
            try { config = nlohmann::json::parse(row["config"].get<std::string>()); } catch (...) {}
        } else {
            config = row["config"];
        }
        if (type == "scheduled_prompt" && config.value("prompt", "").empty()) {
            logToService(id, "error", "Refused to start: scheduled_prompt requires 'prompt' in config");
            return;
        }
        if (type == "rss_feed" && config.value("url", "").empty()) {
            logToService(id, "error", "Refused to start: rss_feed requires 'url' in config");
            return;
        }
        if (type == "custom_script" && config.value("script", "").empty()) {
            logToService(id, "error", "Refused to start: custom_script requires 'script' in config");
            return;
        }
    } catch (const std::exception& e) {
        spdlog::warn("Cannot validate service {} config: {}", id, e.what());
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (running_.count(id)) return;

    auto svc = std::make_unique<RunningService>();
    auto* svcPtr = svc.get();
    svc->thread = std::thread([this, id, svcPtr]() {
        runServiceLoop(id, svcPtr->shouldStop);
    });
    running_[id] = std::move(svc);
    logToService(id, "info", "Service started");
}

void ServiceManager::stopService(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = running_.find(id);
    if (it == running_.end()) return;
    it->second->shouldStop = true;
    if (it->second->thread.joinable())
        it->second->thread.join();
    running_.erase(it);
    logToService(id, "info", "Service stopped");
}

bool ServiceManager::isRunning(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return running_.count(id) > 0;
}

void ServiceManager::logToService(const std::string& serviceId, const std::string& level,
                                  const std::string& message) {
    try {
        Database::instance().execute(
            "INSERT INTO service_logs (service_id, timestamp, level, message) VALUES (?1, ?2, ?3, ?4)",
            {serviceId, std::to_string(nowMs()), level, message});
    } catch (...) {}

    LogBuffer::instance().info("services", "[" + serviceId.substr(0, 8) + "] " + message);
}

void ServiceManager::runServiceLoop(const std::string& id, std::atomic<bool>& shouldStop) {
    int consecutiveFailures = 0;

    while (!shouldStop && !shuttingDown_) {
        try {
            auto& db = Database::instance();
            auto row = db.queryOne("SELECT type, config, next_run FROM services WHERE id = ?1", {id});
            if (row.is_null()) break;

            std::string type = row["type"].get<std::string>();
            nlohmann::json config;
            if (row["config"].is_string()) {
                try { config = nlohmann::json::parse(row["config"].get<std::string>()); } catch (...) {}
            } else {
                config = row["config"];
            }

            int intervalSec = config.value("interval_seconds", 60);
            long long nextRun = row.value("next_run", (long long)0);
            auto now = nowMs();

            if (now >= nextRun) {
                bool tickOk = executeServiceTick(id, config, type);
                long long next = now + (intervalSec * 1000LL);
                db.execute("UPDATE services SET last_run = ?1, next_run = ?2 WHERE id = ?3",
                    {std::to_string(now), std::to_string(next), id});

                if (tickOk) {
                    consecutiveFailures = 0;
                } else {
                    consecutiveFailures++;
                    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                        logToService(id, "error",
                            "Auto-stopped after " + std::to_string(MAX_CONSECUTIVE_FAILURES) +
                            " consecutive failures. Fix the service config and restart.");
                        db.execute("UPDATE services SET status = 'error' WHERE id = ?1", {id});
                        break;
                    }
                }
            }
        } catch (const std::exception& e) {
            logToService(id, "error", std::string("Service error: ") + e.what());
            consecutiveFailures++;
            if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                logToService(id, "error", "Auto-stopped after " +
                    std::to_string(MAX_CONSECUTIVE_FAILURES) + " consecutive exceptions.");
                try {
                    Database::instance().execute("UPDATE services SET status = 'error' WHERE id = ?1", {id});
                } catch (...) {}
                break;
            }
        }

        for (int i = 0; i < 50 && !shouldStop && !shuttingDown_; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool ServiceManager::executeServiceTick(const std::string& id, const nlohmann::json& config,
                                         const std::string& type) {
    if (type == "rss_feed") {
        std::string url = config.value("url", "");
        if (url.empty()) {
            logToService(id, "error", "RSS feed URL not configured -- service cannot run without 'url' in config");
            return false;
        }
        logToService(id, "info", "RSS feed tick: " + url);
        return true;
    } else if (type == "scheduled_prompt") {
        std::string prompt = config.value("prompt", "");
        if (prompt.empty()) {
            logToService(id, "error", "No prompt configured -- service cannot run without 'prompt' in config");
            return false;
        }
        logToService(id, "info", "Scheduled prompt tick");
        return true;
    } else if (type == "custom_script") {
        std::string script = config.value("script", "");
        if (script.empty()) {
            logToService(id, "error", "No script configured -- service cannot run without 'script' in config");
            return false;
        }
        logToService(id, "info", "Custom script tick");
        return true;
    } else {
        logToService(id, "debug", "Service tick (type: " + type + ")");
        return true;
    }
}

} // namespace avacli
