#pragma once

#include "core/Types.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace avacli {

struct ServeConfig;

/// Sub-agent task orchestrator (Phase 4).
///
/// Lifecycle of a task:
///   pending → running → (succeeded | failed | cancelled)
///
/// All state is persisted to `agent_tasks` (migration v6). The in-memory
/// registry (`ctx_`) holds the cancel flag + join handle + completion
/// condvar so `wait(task_id)` can block efficiently without polling SQLite.
///
/// xAI-only by design — no provider switch. Resolves apiKey + chatUrl from
/// the shared ServeConfig on every spawn so master-key rotations take
/// effect without a process restart.
class SubAgentManager {
public:
    struct SpawnOptions {
        std::string goal;
        std::vector<std::string> allowedPaths;
        std::vector<std::string> allowedTools;
        std::string parentTaskId;   // "" for top-level
        int parentDepth = 0;         // 0 for root chat; child depth = parent+1
        std::string sessionId;
        std::string model;           // defaults to config->model
        int maxTurns = 12;           // cap tool loop per task (cheap safety)
    };

    struct SpawnResult {
        bool ok = false;
        std::string errorCode;       // e.g. "subagents_disabled", "max_depth_exceeded"
        std::string errorMessage;
        std::string taskId;
        int depth = 0;
        int maxDepth = 0;
    };

    struct TaskRecord {
        std::string id;
        std::string parentTaskId;
        std::string sessionId;
        std::string goal;
        std::vector<std::string> allowedPaths;
        std::vector<std::string> allowedTools;
        int depth = 0;
        std::string status;          // pending|running|succeeded|failed|cancelled
        nlohmann::json result;
        std::string error;
        int64_t promptTokens = 0;
        int64_t completionTokens = 0;
        int64_t createdAt = 0;
        int64_t startedAt = 0;
        int64_t finishedAt = 0;
        bool cancelRequested = false;
    };

    static SubAgentManager& instance();

    /// Call once from HttpServer::start() so spawn() can fetch apiKey +
    /// workspace + default model without extra plumbing through the tools.
    void setServeConfig(ServeConfig* cfg) { config_ = cfg; }

    SpawnResult spawn(const SpawnOptions& opts);

    /// Block until the task reaches a terminal state OR `timeoutMs` elapses.
    /// Returns the latest TaskRecord regardless.
    TaskRecord waitFor(const std::string& taskId, int timeoutMs);

    /// Request cancellation. Safe to call on finished tasks (no-op).
    bool cancel(const std::string& taskId);

    /// DB-backed. Returns nullopt if unknown.
    std::optional<TaskRecord> get(const std::string& taskId);

    /// DB-backed. If `statusFilter` is non-empty, only rows with that status.
    std::vector<TaskRecord> list(const std::string& statusFilter, int limit);

    /// Kick the lease sweeper once. Called from a periodic tick (HttpServer).
    void tickHousekeeping();

private:
    SubAgentManager() = default;
    SubAgentManager(const SubAgentManager&) = delete;
    SubAgentManager& operator=(const SubAgentManager&) = delete;

    struct RunCtx {
        std::string taskId;
        int depth = 0;
        std::shared_ptr<std::atomic<bool>> cancelFlag;
        std::thread thread;
        std::mutex mu;
        std::condition_variable doneCv;
        bool done = false;
    };

    void runTask(std::shared_ptr<RunCtx> ctx, SpawnOptions opts);

    static TaskRecord rowToRecord(const nlohmann::json& row);

    ServeConfig* config_ = nullptr;
    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<RunCtx>> registry_;
};

} // namespace avacli
