#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"

#include "agents/LeaseManager.hpp"
#include "agents/SubAgentManager.hpp"
#include "config/PlatformSettings.hpp"
#include "db/Database.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace avacli {

namespace {

nlohmann::json recordToJson(const SubAgentManager::TaskRecord& r) {
    nlohmann::json j;
    j["task_id"]          = r.id;
    j["parent_task_id"]   = r.parentTaskId;
    j["session_id"]       = r.sessionId;
    j["goal"]             = r.goal;
    j["allowed_paths"]    = r.allowedPaths;
    j["allowed_tools"]    = r.allowedTools;
    j["depth"]            = r.depth;
    j["status"]           = r.status;
    j["error"]            = r.error;
    j["result"]           = r.result;
    j["prompt_tokens"]    = r.promptTokens;
    j["completion_tokens"] = r.completionTokens;
    j["created_at"]       = r.createdAt;
    j["started_at"]       = r.startedAt;
    j["finished_at"]      = r.finishedAt;
    j["cancel_requested"] = r.cancelRequested;
    return j;
}

} // anonymous namespace

void registerSubAgentRoutes(httplib::Server& svr, ServerContext /*ctx*/) {
    // List recent sub-agent tasks.
    svr.Get("/api/subagents", [](const httplib::Request& req, httplib::Response& res) {
        std::string status = req.has_param("status") ? req.get_param_value("status") : "";
        int limit = 50;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
        }
        if (limit < 1) limit = 1;
        if (limit > 500) limit = 500;

        auto rows = SubAgentManager::instance().list(status, limit);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : rows) arr.push_back(recordToJson(r));

        nlohmann::json j;
        j["subagents"] = arr;
        j["count"]     = arr.size();
        j["max_depth"] = platformSettings::subagentsMaxDepth();
        res.set_content(j.dump(), "application/json");
    });

    // Fetch one task.
    svr.Get(R"(/api/subagents/([A-Za-z0-9_\-]+))",
            [](const httplib::Request& req, httplib::Response& res) {
        const std::string taskId = req.matches[1];
        auto rec = SubAgentManager::instance().get(taskId);
        if (!rec) {
            res.status = 404;
            res.set_content(R"({"error":"not_found"})", "application/json");
            return;
        }
        res.set_content(recordToJson(*rec).dump(), "application/json");
    });

    // Spawn a sub-agent programmatically (master-key authed). Primary use:
    // UI "run task" buttons + acceptance tests. The chat-tool path
    // (spawn_subagent) remains the normal agent-driven entry.
    svr.Post("/api/subagents", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid_json"})", "application/json");
            return;
        }

        SubAgentManager::SpawnOptions opts;
        opts.goal = body.value("goal", "");
        opts.parentTaskId = body.value("parent_task_id", "");
        opts.parentDepth = body.value("parent_depth", 0);
        opts.sessionId = body.value("session_id", "");
        opts.model = body.value("model", "");
        if (body.contains("allowed_paths") && body["allowed_paths"].is_array()) {
            for (const auto& p : body["allowed_paths"])
                if (p.is_string()) opts.allowedPaths.push_back(p.get<std::string>());
        }
        if (body.contains("allowed_tools") && body["allowed_tools"].is_array()) {
            for (const auto& t : body["allowed_tools"])
                if (t.is_string()) opts.allowedTools.push_back(t.get<std::string>());
        }

        auto result = SubAgentManager::instance().spawn(opts);
        nlohmann::json j;
        if (!result.ok) {
            res.status = (result.errorCode == "subagents_disabled" ||
                          result.errorCode == "max_depth_exceeded") ? 409 : 400;
            j["error"] = result.errorCode;
            j["message"] = result.errorMessage;
            j["max_depth"] = result.maxDepth;
            j["attempted_depth"] = result.depth;
            res.set_content(j.dump(), "application/json");
            return;
        }
        j["task_id"] = result.taskId;
        j["depth"] = result.depth;
        j["max_depth"] = result.maxDepth;
        j["status"] = "pending";
        res.status = 201;
        res.set_content(j.dump(), "application/json");
    });

    // Cancel a running task.
    svr.Post(R"(/api/subagents/([A-Za-z0-9_\-]+)/cancel)",
             [](const httplib::Request& req, httplib::Response& res) {
        const std::string taskId = req.matches[1];
        bool signalled = SubAgentManager::instance().cancel(taskId);
        nlohmann::json j;
        j["task_id"]   = taskId;
        j["signalled"] = signalled;
        res.set_content(j.dump(), "application/json");
    });

    // List current leases — useful when debugging a stuck writer.
    svr.Get("/api/subagents-debug/leases", [](const httplib::Request&, httplib::Response& res) {
        auto leases = LeaseManager::list();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& l : leases) {
            nlohmann::json j;
            j["id"]           = l.id;
            j["task_id"]      = l.taskId;
            j["resource_key"] = l.resourceKey;
            j["acquired_at"]  = l.acquiredAt;
            j["expires_at"]   = l.expiresAt;
            arr.push_back(j);
        }
        res.set_content(nlohmann::json{{"leases", arr},
                                       {"count", arr.size()}}.dump(),
                        "application/json");
    });
}

} // namespace avacli
