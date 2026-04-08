#include "server/LogBuffer.hpp"
#include "db/Database.hpp"

namespace avacli {

LogBuffer& LogBuffer::instance() {
    static LogBuffer buf;
    return buf;
}

void LogBuffer::push(const std::string& level, const std::string& category,
                     const std::string& message, const nlohmann::json& details) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(mu_);
        entries_.push_back({now, level, category, message, details.is_null() ? nlohmann::json::object() : details});
        while (entries_.size() > MAX_ENTRIES)
            entries_.pop_front();
    }

    try {
        auto& db = Database::instance();
        if (db.raw()) {
            std::string det = (details.is_null() || details.empty()) ? "{}" : details.dump();
            db.execute(
                "INSERT INTO event_log (timestamp, level, category, message, details) "
                "VALUES (?1, ?2, ?3, ?4, ?5)",
                {std::to_string(now), level, category, message, det});
        }
    } catch (...) {}
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

nlohmann::json LogBuffer::getEntriesFromDb(int limit, long long sinceTimestamp,
                                           const std::string& category,
                                           const std::string& level,
                                           const std::string& search) const {
    try {
        auto& db = Database::instance();
        std::string sql = "SELECT timestamp, level, category, message, details FROM event_log WHERE 1=1";
        std::vector<std::string> params;
        int paramIdx = 1;

        if (sinceTimestamp > 0) {
            sql += " AND timestamp > ?" + std::to_string(paramIdx++);
            params.push_back(std::to_string(sinceTimestamp));
        }
        if (!category.empty() && category != "all") {
            sql += " AND category = ?" + std::to_string(paramIdx++);
            params.push_back(category);
        }
        if (!level.empty() && level != "all") {
            sql += " AND level = ?" + std::to_string(paramIdx++);
            params.push_back(level);
        }
        if (!search.empty()) {
            sql += " AND message LIKE ?" + std::to_string(paramIdx++);
            params.push_back("%" + search + "%");
        }
        sql += " ORDER BY timestamp DESC LIMIT ?" + std::to_string(paramIdx);
        params.push_back(std::to_string(limit));

        auto rows = db.query(sql, params);
        for (auto& r : rows) {
            if (r.contains("details") && r["details"].is_string()) {
                try { r["details"] = nlohmann::json::parse(r["details"].get<std::string>()); } catch (...) {}
            }
        }
        return rows;
    } catch (...) {
        return getEntries(limit, sinceTimestamp);
    }
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
