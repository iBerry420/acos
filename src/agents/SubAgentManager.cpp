#include "agents/SubAgentManager.hpp"
#include "agents/ScopedToolExecutor.hpp"
#include "agents/LeaseManager.hpp"
#include "client/XAIClient.hpp"
#include "core/AgentEngine.hpp"
#include "config/PlatformSettings.hpp"
#include "db/Database.hpp"
#include "server/HttpServer.hpp"
#include "server/ServerHelpers.hpp"
#include "tools/ToolRegistry.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <random>
#include <sstream>

namespace avacli {

namespace {

std::string nowSecondsStr() {
    auto n = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return std::to_string(n);
}

int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string generateTaskId() {
    static std::mt19937_64 rng(
        static_cast<unsigned long>(std::chrono::system_clock::now().time_since_epoch().count()));
    std::ostringstream os;
    os << "sa-" << std::hex << rng();
    return os.str();
}

std::vector<std::string> jsonStringArray(const nlohmann::json& j) {
    std::vector<std::string> out;
    if (j.is_array()) {
        for (const auto& v : j) if (v.is_string()) out.push_back(v.get<std::string>());
    }
    return out;
}

std::string arrayToJson(const std::vector<std::string>& v) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& s : v) j.push_back(s);
    return j.dump();
}

std::string buildSubAgentSystemPrompt(const std::string& workspace,
                                       const SubAgentManager::SpawnOptions& opts,
                                       int myDepth,
                                       int maxDepth) {
    std::ostringstream s;
    s << "You are a focused sub-agent spawned by a parent agent. Your entire job is the goal below — finish it, then stop.\n\n"
      << "GOAL:\n" << opts.goal << "\n\n"
      << "SCOPE:\n"
      << "- Workspace: " << workspace << "\n"
      << "- Depth: " << myDepth << " / " << maxDepth;
    if (myDepth >= maxDepth) {
        s << "  (MAX — you may NOT spawn further sub-agents)";
    }
    s << "\n";

    if (!opts.allowedPaths.empty()) {
        s << "- Write access restricted to the following paths (your writes outside these return path_not_allowed):\n";
        for (const auto& p : opts.allowedPaths) s << "  - " << p << "\n";
    } else {
        s << "- No path restriction (full workspace writable).\n";
    }

    if (!opts.allowedTools.empty()) {
        s << "- You may ONLY call these tools: ";
        for (size_t i = 0; i < opts.allowedTools.size(); ++i) {
            if (i) s << ", ";
            s << opts.allowedTools[i];
        }
        s << " (others return tool_not_allowed)\n";
    }

    s << "\n"
      << "COORDINATION:\n"
      << "- Another sub-agent may be working in parallel. If you get resource_locked "
      << "when writing, either (a) wait briefly and retry once, (b) write to a "
      << "different file, or (c) use wait_subagent if you know the holder's task id.\n"
      << "- When the goal is complete, produce a final assistant message that "
      << "summarises what you did. Do NOT keep calling tools after that.\n";

    return s.str();
}

} // anonymous namespace

SubAgentManager& SubAgentManager::instance() {
    static SubAgentManager m;
    return m;
}

SubAgentManager::SpawnResult SubAgentManager::spawn(const SpawnOptions& opts) {
    SpawnResult r;
    r.maxDepth = platformSettings::subagentsMaxDepth();
    r.depth = opts.parentDepth + 1;

    if (opts.goal.empty()) {
        r.errorCode = "invalid_goal";
        r.errorMessage = "goal is required";
        return r;
    }

    if (r.maxDepth <= 0) {
        r.errorCode = "subagents_disabled";
        r.errorMessage = "sub-agents are disabled (subagents_max_depth=0 in platform settings)";
        return r;
    }

    if (r.depth > r.maxDepth) {
        r.errorCode = "max_depth_exceeded";
        r.errorMessage = "spawning at depth " + std::to_string(r.depth)
                       + " would exceed subagents_max_depth=" + std::to_string(r.maxDepth);
        return r;
    }

    if (!config_) {
        r.errorCode = "not_configured";
        r.errorMessage = "SubAgentManager has no ServeConfig; call setServeConfig() during server init";
        return r;
    }

    // Persist the task row.
    const std::string taskId = generateTaskId();
    try {
        Database::instance().execute(
            "INSERT INTO agent_tasks "
            "(id, parent_task_id, session_id, goal, allowed_paths, allowed_tools, depth, status, created_at) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 'pending', ?8)",
            {taskId, opts.parentTaskId, opts.sessionId, opts.goal,
             arrayToJson(opts.allowedPaths), arrayToJson(opts.allowedTools),
             std::to_string(r.depth), nowSecondsStr()});
    } catch (const std::exception& ex) {
        r.errorCode = "db_error";
        r.errorMessage = std::string("failed to persist agent_task: ") + ex.what();
        return r;
    }

    // In-memory run context.
    auto ctx = std::make_shared<RunCtx>();
    ctx->taskId = taskId;
    ctx->depth = r.depth;
    ctx->cancelFlag = std::make_shared<std::atomic<bool>>(false);
    {
        std::lock_guard<std::mutex> lk(mu_);
        registry_[taskId] = ctx;
    }

    SpawnOptions optsCopy = opts;  // runTask keeps a stable copy
    ctx->thread = std::thread([this, ctx, optsCopy]() {
        this->runTask(ctx, optsCopy);
    });
    ctx->thread.detach();

    r.ok = true;
    r.taskId = taskId;
    return r;
}

void SubAgentManager::runTask(std::shared_ptr<RunCtx> ctx, SpawnOptions opts) {
    const std::string taskId = ctx->taskId;
    const int depth = ctx->depth;

    auto markDone = [&](const std::string& status,
                        const nlohmann::json& resultJson,
                        const std::string& errMsg,
                        int64_t promptTokens,
                        int64_t completionTokens) {
        try {
            Database::instance().execute(
                "UPDATE agent_tasks SET status = ?1, result = ?2, error = ?3, "
                "prompt_tokens = ?4, completion_tokens = ?5, finished_at = ?6 "
                "WHERE id = ?7",
                {status, resultJson.dump(), errMsg,
                 std::to_string(promptTokens), std::to_string(completionTokens),
                 nowSecondsStr(), taskId});
        } catch (const std::exception& ex) {
            spdlog::warn("SubAgent markDone failed: {}", ex.what());
        }
        LeaseManager::releaseAllForTask(taskId);

        std::lock_guard<std::mutex> lk(ctx->mu);
        ctx->done = true;
        ctx->doneCv.notify_all();
    };

    nlohmann::json transcript = nlohmann::json::array();
    int64_t promptTokensTotal = 0;
    int64_t completionTokensTotal = 0;
    int64_t cachedTokensTotal = 0;
    int64_t reasoningTokensTotal = 0;

    try {
        // Cancellation checkpoint before we do anything heavy.
        if (ctx->cancelFlag->load()) {
            markDone("cancelled", {{"summary", "cancelled before start"}},
                     "", 0, 0);
            return;
        }

        Database::instance().execute(
            "UPDATE agent_tasks SET status='running', started_at=?1 WHERE id=?2",
            {nowSecondsStr(), taskId});

        std::string apiKey, chatUrl;
        resolveXaiAuth(*config_, apiKey, chatUrl);
        if (apiKey.empty()) {
            markDone("failed", {{"summary", "no xAI API key"}},
                     "XAI_API_KEY not configured", 0, 0);
            return;
        }

        const std::string workspace = config_->workspace;
        const std::string model = !opts.model.empty() ? opts.model : config_->model;

        // Compose a cache-routing key for this sub-agent. Sharing the parent's
        // prefix lets xAI route siblings to the same server pool, where they
        // can reuse each other's cached sub-agent scaffolding. Appending the
        // taskId still distinguishes individual tasks for the local KV cache.
        std::string childConvId = !opts.parentConvId.empty()
            ? (opts.parentConvId + ":" + taskId)
            : taskId;

        // Build the scoped executor.
        ScopedToolExecutor::Scope scope;
        scope.taskId = taskId;
        scope.depth = depth;
        scope.allowedPaths = opts.allowedPaths;
        scope.allowedTools = opts.allowedTools;

        ToolRegistry::registerAll();

        auto executor = std::make_unique<ScopedToolExecutor>(
            workspace, Mode::Agent, scope, nullptr);
        executor->setSessionId(taskId);
        executor->setSearchModel(model);
        executor->setSubAgentContext(taskId, depth);
        // Propagate child conv_id so grandchild sub-agents inherit the routing prefix.
        executor->setConvId(childConvId);

        // Build messages with a sub-agent-tailored system prompt + user goal.
        std::vector<Message> messages;
        messages.push_back(
            {"system",
             buildSubAgentSystemPrompt(workspace, opts, depth,
                                        platformSettings::subagentsMaxDepth())});
        messages.push_back({"user", opts.goal});

        // AgentEngine — no UI callbacks (subagent's transcript is internal).
        // Collect content deltas into a local buffer so we can return a
        // final summary to the parent.
        auto contentBuffer = std::make_shared<std::string>();
        auto engine = std::make_unique<AgentEngine>(
            workspace, Mode::Agent, executor.get(),
            [contentBuffer](const std::string& d) { *contentBuffer += d; },
            [](TranscriptChannel, const std::string&) {},
            nullptr,
            [](Mode) {});
        engine->setCancelFlag(ctx->cancelFlag.get());
        engine->setConvId(childConvId);

        // Cap tool_call iterations via the engine's existing machinery
        // (AgentEngine loops until the model stops emitting tool_calls;
        // the goal + system prompt nudges it to finish quickly).
        engine->setToolStartCallback(
            [&transcript](const std::string& id, const std::string& name, const std::string& args) {
                nlohmann::json e;
                e["event"] = "tool_start";
                e["tool_call_id"] = id;
                e["name"] = name;
                try { e["arguments"] = nlohmann::json::parse(args); }
                catch (...) { e["arguments"] = args; }
                transcript.push_back(e);
            });
        engine->setToolDoneCallback(
            [&transcript](const std::string& id, const std::string& name,
                          bool success, const std::string& result) {
                nlohmann::json e;
                e["event"] = "tool_done";
                e["tool_call_id"] = id;
                e["name"] = name;
                e["success"] = success;
                std::string trunc = result.size() > 4000
                    ? result.substr(0, 4000) + "...(truncated)"
                    : result;
                e["result"] = trunc;
                transcript.push_back(e);
            });

        XAIClient client(apiKey, chatUrl);
        auto tools = ToolRegistry::getToolsForMode(Mode::Agent);

        auto onUsage = [&promptTokensTotal, &completionTokensTotal,
                        &cachedTokensTotal, &reasoningTokensTotal](const Usage& u) {
            promptTokensTotal += static_cast<int64_t>(u.promptTokens);
            completionTokensTotal += static_cast<int64_t>(u.completionTokens);
            cachedTokensTotal += static_cast<int64_t>(u.cachedPromptTokens);
            reasoningTokensTotal += static_cast<int64_t>(u.reasoningTokens);
        };

        bool ok = engine->run(client, model, messages, tools, onUsage);

        if (ctx->cancelFlag->load()) {
            nlohmann::json result;
            result["summary"] = *contentBuffer;
            result["transcript"] = transcript;
            result["cancelled"] = true;
            markDone("cancelled", result, "", promptTokensTotal, completionTokensTotal);
            return;
        }

        nlohmann::json result;
        result["summary"] = *contentBuffer;
        result["transcript"] = transcript;
        int64_t billable = promptTokensTotal > cachedTokensTotal
            ? promptTokensTotal - cachedTokensTotal : 0;
        int cacheHitPct = promptTokensTotal > 0
            ? static_cast<int>((cachedTokensTotal * 100) / promptTokensTotal) : 0;
        result["usage"] = {
            {"prompt_tokens", promptTokensTotal},
            {"completion_tokens", completionTokensTotal},
            {"cached_tokens", cachedTokensTotal},
            {"reasoning_tokens", reasoningTokensTotal},
            {"billable_prompt_tokens", billable},
            {"cache_hit_percent", cacheHitPct}};

        if (ok) {
            markDone("succeeded", result, "", promptTokensTotal, completionTokensTotal);
        } else {
            markDone("failed", result, client.lastError(),
                     promptTokensTotal, completionTokensTotal);
        }
    } catch (const std::exception& ex) {
        nlohmann::json result;
        result["summary"] = "";
        result["transcript"] = transcript;
        markDone("failed", result, ex.what(),
                 promptTokensTotal, completionTokensTotal);
    } catch (...) {
        nlohmann::json result;
        result["summary"] = "";
        result["transcript"] = transcript;
        markDone("failed", result, "unknown exception",
                 promptTokensTotal, completionTokensTotal);
    }
}

SubAgentManager::TaskRecord SubAgentManager::waitFor(const std::string& taskId, int timeoutMs) {
    std::shared_ptr<RunCtx> ctx;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = registry_.find(taskId);
        if (it != registry_.end()) ctx = it->second;
    }

    if (ctx) {
        std::unique_lock<std::mutex> lk(ctx->mu);
        if (timeoutMs <= 0) {
            ctx->doneCv.wait(lk, [&] { return ctx->done; });
        } else {
            ctx->doneCv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                 [&] { return ctx->done; });
        }
    }

    auto rec = get(taskId);
    if (rec) return *rec;

    // Defensive: should never hit — task row is inserted before thread starts.
    TaskRecord empty;
    empty.id = taskId;
    empty.status = "unknown";
    return empty;
}

bool SubAgentManager::cancel(const std::string& taskId) {
    std::shared_ptr<RunCtx> ctx;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = registry_.find(taskId);
        if (it != registry_.end()) ctx = it->second;
    }
    try {
        Database::instance().execute(
            "UPDATE agent_tasks SET cancel_requested=1 "
            "WHERE id=?1 AND status IN ('pending','running')",
            {taskId});
    } catch (...) {}
    if (ctx && ctx->cancelFlag) {
        ctx->cancelFlag->store(true);
        return true;
    }
    return false;
}

std::optional<SubAgentManager::TaskRecord> SubAgentManager::get(const std::string& taskId) {
    try {
        auto row = Database::instance().queryOne(
            "SELECT * FROM agent_tasks WHERE id = ?1", {taskId});
        if (row.is_null()) return std::nullopt;
        return rowToRecord(row);
    } catch (const std::exception& ex) {
        spdlog::warn("SubAgent::get failed: {}", ex.what());
        return std::nullopt;
    }
}

std::vector<SubAgentManager::TaskRecord> SubAgentManager::list(const std::string& statusFilter,
                                                                int limit) {
    std::vector<TaskRecord> out;
    try {
        nlohmann::json rows;
        if (!statusFilter.empty()) {
            rows = Database::instance().query(
                "SELECT * FROM agent_tasks WHERE status = ?1 ORDER BY created_at DESC LIMIT ?2",
                {statusFilter, std::to_string(limit <= 0 ? 100 : limit)});
        } else {
            rows = Database::instance().query(
                "SELECT * FROM agent_tasks ORDER BY created_at DESC LIMIT ?1",
                {std::to_string(limit <= 0 ? 100 : limit)});
        }
        for (const auto& r : rows) out.push_back(rowToRecord(r));
    } catch (const std::exception& ex) {
        spdlog::warn("SubAgent::list failed: {}", ex.what());
    }
    return out;
}

void SubAgentManager::tickHousekeeping() {
    LeaseManager::sweepExpired();
}

SubAgentManager::TaskRecord SubAgentManager::rowToRecord(const nlohmann::json& row) {
    TaskRecord rec;
    rec.id = row.value("id", "");
    rec.parentTaskId = row.value("parent_task_id", "");
    rec.sessionId = row.value("session_id", "");
    rec.goal = row.value("goal", "");
    try { rec.allowedPaths = jsonStringArray(nlohmann::json::parse(row.value("allowed_paths", "[]"))); }
    catch (...) {}
    try { rec.allowedTools = jsonStringArray(nlohmann::json::parse(row.value("allowed_tools", "[]"))); }
    catch (...) {}
    rec.depth = row.value("depth", 0);
    rec.status = row.value("status", "");
    try {
        auto rstr = row.value("result", "");
        if (!rstr.empty()) rec.result = nlohmann::json::parse(rstr);
    } catch (...) {}
    rec.error = row.value("error", "");
    rec.promptTokens = row.value("prompt_tokens", (int64_t)0);
    rec.completionTokens = row.value("completion_tokens", (int64_t)0);
    rec.createdAt = row.value("created_at", (int64_t)0);
    rec.startedAt = row.value("started_at", (int64_t)0);
    rec.finishedAt = row.value("finished_at", (int64_t)0);
    rec.cancelRequested = row.value("cancel_requested", 0) != 0;
    return rec;
}

} // namespace avacli
