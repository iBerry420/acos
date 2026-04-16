#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace avacli {

/// Advisory lease registry for sub-agent write coordination (Phase 4).
///
/// One task at a time may hold a given `resource_key` (typically
/// `"path:<canonical-abs-path>"`). Rows live in `agent_leases` (migration v6)
/// and are enforced by a UNIQUE index on `resource_key` — so `acquire()` is
/// atomic even under concurrent writers in different threads/processes.
///
/// Stale rows (owner crashed without releasing) are reaped by
/// `sweepExpired()` on a periodic background tick and also opportunistically
/// inside `acquire()` when it sees a stale conflict.
namespace LeaseManager {

struct AcquireResult {
    bool acquired = false;
    /// On failure: task_id of the current holder.
    std::string holderTaskId;
    /// On failure: epoch-seconds when the existing lease expires.
    int64_t existingExpiresAt = 0;
    /// On success: epoch-seconds when THIS lease expires.
    int64_t newExpiresAt = 0;
};

struct LeaseRow {
    int64_t id = 0;
    std::string taskId;
    std::string resourceKey;
    int64_t acquiredAt = 0;
    int64_t expiresAt = 0;
};

/// Attempt to acquire a lease on `resourceKey` for `taskId` for `ttlSeconds`.
/// Opportunistically reaps expired rows on conflict before retrying once.
/// Thread-safe (serialised via the shared Database mutex).
AcquireResult acquire(const std::string& taskId,
                      const std::string& resourceKey,
                      int ttlSeconds);

/// Release a single lease held by `taskId` on `resourceKey`. No-op if not
/// held by this task (prevents a misbehaving child from releasing a
/// sibling's lease).
void release(const std::string& taskId, const std::string& resourceKey);

/// Release every lease held by `taskId`. Called from SubAgentManager when a
/// task terminates (success, failure, cancel) so it can't leak leases.
int releaseAllForTask(const std::string& taskId);

/// Delete rows whose expires_at < now(). Call periodically.
/// Returns count reaped.
int sweepExpired();

/// Debug/admin: list current (non-expired) leases.
std::vector<LeaseRow> list();

} // namespace LeaseManager

} // namespace avacli
