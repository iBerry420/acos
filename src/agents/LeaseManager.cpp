#include "agents/LeaseManager.hpp"
#include "db/Database.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <chrono>

namespace avacli {

namespace {

int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // anonymous namespace

namespace LeaseManager {

AcquireResult acquire(const std::string& taskId,
                      const std::string& resourceKey,
                      int ttlSeconds) {
    AcquireResult result;
    if (taskId.empty() || resourceKey.empty() || ttlSeconds <= 0) {
        return result;
    }

    const int64_t now = nowSeconds();
    const int64_t expiresAt = now + static_cast<int64_t>(ttlSeconds);

    auto tryInsert = [&]() -> bool {
        try {
            Database::instance().execute(
                "INSERT INTO agent_leases (task_id, resource_key, acquired_at, expires_at) "
                "VALUES (?1, ?2, ?3, ?4)",
                {taskId, resourceKey, std::to_string(now), std::to_string(expiresAt)});
            return true;
        } catch (const std::exception&) {
            // UNIQUE constraint on resource_key → another holder. Fall
            // through to conflict-resolution below.
            return false;
        }
    };

    if (tryInsert()) {
        result.acquired = true;
        result.newExpiresAt = expiresAt;
        return result;
    }

    // Conflict path: look up the existing holder, reap if stale, retry once.
    nlohmann::json existing;
    try {
        existing = Database::instance().queryOne(
            "SELECT task_id, expires_at FROM agent_leases WHERE resource_key = ?1",
            {resourceKey});
    } catch (const std::exception& ex) {
        spdlog::warn("LeaseManager::acquire lookup failed: {}", ex.what());
    }

    std::string holder;
    int64_t existingExpires = 0;
    if (!existing.is_null()) {
        holder = existing.value("task_id", "");
        existingExpires = existing.value("expires_at", (int64_t)0);
    }

    if (!existing.is_null() && existingExpires <= now) {
        // Stale — reap and retry.
        try {
            Database::instance().execute(
                "DELETE FROM agent_leases WHERE resource_key = ?1 AND expires_at = ?2",
                {resourceKey, std::to_string(existingExpires)});
        } catch (const std::exception& ex) {
            spdlog::warn("LeaseManager::acquire reap failed: {}", ex.what());
        }
        if (tryInsert()) {
            result.acquired = true;
            result.newExpiresAt = expiresAt;
            return result;
        }
        // Another acquirer won the race — fall through to return conflict.
        try {
            existing = Database::instance().queryOne(
                "SELECT task_id, expires_at FROM agent_leases WHERE resource_key = ?1",
                {resourceKey});
            if (!existing.is_null()) {
                holder = existing.value("task_id", "");
                existingExpires = existing.value("expires_at", (int64_t)0);
            }
        } catch (...) {}
    }

    result.acquired = false;
    result.holderTaskId = holder;
    result.existingExpiresAt = existingExpires;
    return result;
}

void release(const std::string& taskId, const std::string& resourceKey) {
    if (taskId.empty() || resourceKey.empty()) return;
    try {
        Database::instance().execute(
            "DELETE FROM agent_leases WHERE resource_key = ?1 AND task_id = ?2",
            {resourceKey, taskId});
    } catch (const std::exception& ex) {
        spdlog::warn("LeaseManager::release failed: {}", ex.what());
    }
}

int releaseAllForTask(const std::string& taskId) {
    if (taskId.empty()) return 0;
    try {
        return static_cast<int>(Database::instance().execute(
            "DELETE FROM agent_leases WHERE task_id = ?1", {taskId}));
    } catch (const std::exception& ex) {
        spdlog::warn("LeaseManager::releaseAllForTask failed: {}", ex.what());
        return 0;
    }
}

int sweepExpired() {
    const int64_t now = nowSeconds();
    try {
        return static_cast<int>(Database::instance().execute(
            "DELETE FROM agent_leases WHERE expires_at < ?1",
            {std::to_string(now)}));
    } catch (const std::exception& ex) {
        spdlog::warn("LeaseManager::sweepExpired failed: {}", ex.what());
        return 0;
    }
}

std::vector<LeaseRow> list() {
    std::vector<LeaseRow> rows;
    try {
        const int64_t now = nowSeconds();
        auto j = Database::instance().query(
            "SELECT id, task_id, resource_key, acquired_at, expires_at FROM agent_leases "
            "WHERE expires_at >= ?1 ORDER BY acquired_at ASC",
            {std::to_string(now)});
        for (const auto& r : j) {
            LeaseRow lr;
            lr.id = r.value("id", (int64_t)0);
            lr.taskId = r.value("task_id", "");
            lr.resourceKey = r.value("resource_key", "");
            lr.acquiredAt = r.value("acquired_at", (int64_t)0);
            lr.expiresAt = r.value("expires_at", (int64_t)0);
            rows.push_back(std::move(lr));
        }
    } catch (const std::exception& ex) {
        spdlog::warn("LeaseManager::list failed: {}", ex.what());
    }
    return rows;
}

} // namespace LeaseManager

} // namespace avacli
