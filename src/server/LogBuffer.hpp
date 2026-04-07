#pragma once
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <deque>

namespace avacli {

struct LogEntry {
    long long timestamp;
    std::string level;     // "debug", "info", "warn", "error"
    std::string category;  // "chat", "node", "settings", "api", "relay", "system", "error"
    std::string message;
    nlohmann::json details;
};

class LogBuffer {
public:
    static LogBuffer& instance();

    void push(const std::string& level, const std::string& category,
              const std::string& message, const nlohmann::json& details = nullptr);

    void info(const std::string& category, const std::string& message,
              const nlohmann::json& details = nullptr);
    void warn(const std::string& category, const std::string& message,
              const nlohmann::json& details = nullptr);
    void error(const std::string& category, const std::string& message,
               const nlohmann::json& details = nullptr);
    void debug(const std::string& category, const std::string& message,
               const nlohmann::json& details = nullptr);

    nlohmann::json getEntries(int limit = 500, long long sinceTimestamp = 0) const;
    size_t size() const;
    void clear();

private:
    LogBuffer() = default;
    mutable std::mutex mu_;
    std::deque<LogEntry> entries_;
    static constexpr size_t MAX_ENTRIES = 5000;
};

} // namespace avacli
