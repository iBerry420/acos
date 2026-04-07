#include "server/LogBuffer.hpp"

namespace avacli {

LogBuffer& LogBuffer::instance() {
    static LogBuffer buf;
    return buf;
}

void LogBuffer::push(const std::string& level, const std::string& category,
                     const std::string& message, const nlohmann::json& details) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(mu_);
    entries_.push_back({now, level, category, message, details.is_null() ? nlohmann::json::object() : details});
    while (entries_.size() > MAX_ENTRIES)
        entries_.pop_front();
}

void LogBuffer::info(const std::string& cat, const std::string& msg, const nlohmann::json& d) { push("info", cat, msg, d); }
void LogBuffer::warn(const std::string& cat, const std::string& msg, const nlohmann::json& d) { push("warn", cat, msg, d); }
void LogBuffer::error(const std::string& cat, const std::string& msg, const nlohmann::json& d) { push("error", cat, msg, d); }
void LogBuffer::debug(const std::string& cat, const std::string& msg, const nlohmann::json& d) { push("debug", cat, msg, d); }

nlohmann::json LogBuffer::getEntries(int limit, long long sinceTimestamp) const {
    std::lock_guard<std::mutex> lock(mu_);
    nlohmann::json arr = nlohmann::json::array();
    int count = 0;
    for (auto it = entries_.rbegin(); it != entries_.rend() && count < limit; ++it) {
        if (sinceTimestamp > 0 && it->timestamp <= sinceTimestamp) continue;
        nlohmann::json entry;
        entry["timestamp"] = it->timestamp;
        entry["level"] = it->level;
        entry["category"] = it->category;
        entry["message"] = it->message;
        if (!it->details.empty() && !it->details.is_null())
            entry["details"] = it->details;
        arr.push_back(entry);
        count++;
    }
    return arr;
}

size_t LogBuffer::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return entries_.size();
}

void LogBuffer::clear() {
    std::lock_guard<std::mutex> lock(mu_);
    entries_.clear();
}

} // namespace avacli
