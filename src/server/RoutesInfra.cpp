#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "client/VultrClient.hpp"
#include "client/NodeClient.hpp"
#include "infra/VultrDeployUserdata.hpp"
#include "config/VaultStorage.hpp"
#include "config/ServeSettings.hpp"
#include "db/Database.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

namespace avacli {

namespace fs = std::filesystem;

static int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string genNodeId() {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "n" + std::to_string(ms);
}

static NodeInfo nodeFromRow(const nlohmann::json& row) {
    NodeInfo ni;
    ni.id = row.value("id", "");
    ni.ip = row.value("ip", "");
    ni.port = row.value("port", 8080);
    ni.domain = row.value("domain", "");
    ni.label = row.value("label", "");
    ni.username = row.value("username", "");
    ni.password = row.value("auth_token", "");
    ni.status = row.value("status", "unknown");
    ni.latencyMs = row.value("latency_ms", -1);
    ni.version = row.value("version", "");
    ni.workspace = row.value("workspace", "");
    return ni;
}

void registerInfraRoutes(httplib::Server& svr, ServerContext ctx) {

    // ── Health endpoint (public, no auth required) ──────────────
    svr.Get("/api/health", [ctx](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["status"] = "ok";
        j["version"] = "2.1.0";
        j["workspace"] = ctx.config ? ctx.config->workspace : "";
        j["uptime_s"] = 0; // TODO: track real uptime
        j["port"] = ctx.actualPort ? *ctx.actualPort : 0;
        res.set_content(j.dump(), "application/json");
    });

    // ── Vultr VPS Management ────────────────────────────────────

    svr.Get("/api/vultr/plans", [ctx](const httplib::Request&, httplib::Response& res) {
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured. Add it in Settings."})", "application/json");
            return;
        }
        VultrClient vc(key);
        auto plans = vc.listPlans();
        res.set_content(plans.dump(), "application/json");
    });

    svr.Get("/api/vultr/regions", [ctx](const httplib::Request&, httplib::Response& res) {
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured"})", "application/json");
            return;
        }
        VultrClient vc(key);
        auto regions = vc.listRegions();
        res.set_content(regions.dump(), "application/json");
    });

    svr.Get("/api/vultr/account", [ctx](const httplib::Request&, httplib::Response& res) {
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured. Add it in Settings."})", "application/json");
            return;
        }
        try {
            VultrClient vc(key);
            nlohmann::json out;
            out["account"] = vc.getAccount();
            try { out["bandwidth"] = vc.getAccountBandwidth(); } catch (...) { out["bandwidth"] = nlohmann::json::object(); }
            try { out["pending_charges"] = vc.listPendingCharges(); } catch (...) { out["pending_charges"] = nlohmann::json::object(); }
            res.set_content(out.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 502;
            res.set_content(nlohmann::json({{"error", std::string("Vultr API error: ") + e.what()}}).dump(), "application/json");
        }
    });

    svr.Get("/api/vultr/instances", [ctx](const httplib::Request&, httplib::Response& res) {
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured"})", "application/json");
            return;
        }
        VultrClient vc(key);
        auto instances = vc.listInstances();
        res.set_content(instances.dump(), "application/json");
    });

    svr.Get(R"(/api/vultr/instances/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured"})", "application/json");
            return;
        }
        std::string instanceId = req.matches[1];
        VultrClient vc(key);
        auto instance = vc.getInstance(instanceId);
        res.set_content(instance.dump(), "application/json");
    });

    svr.Post("/api/vultr/instances", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured"})", "application/json");
            return;
        }
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string region = body.value("region", "");
        std::string plan = body.value("plan", "");
        std::string osId = body.value("os_id", "");
        std::string label = body.value("label", "");
        std::string deployConfig = body.value("deploy_config", "");
        if (region.empty() || plan.empty() || osId.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"region, plan, and os_id required"})", "application/json");
            return;
        }
        VultrClient vc(key);
        std::string userData = vultrDeployUserDataForProfile(deployConfig);
        auto result = vc.createInstance(region, plan, osId, label, "", userData);
        if (result.contains("error")) {
            res.status = 400;
            LogBuffer::instance().error("deploy", "VPS deploy failed", {{"region", region}, {"plan", plan}, {"error", result["error"]}});
        } else {
            LogBuffer::instance().info("deploy", "VPS instance deployed", {{"region", region}, {"plan", plan}, {"label", label}, {"config", deployConfig}});

            // Auto-register as node if deploy succeeded and we have an instance ID
            if (result.contains("instance") && result["instance"].contains("id")) {
                std::string instId = result["instance"].value("id", "");
                std::string instIp = result["instance"].value("main_ip", "");
                std::string instLabel = label.empty() ? ("vultr-" + region) : label;
                if (!instId.empty()) {
                    auto& db = Database::instance();
                    auto nodeId = genNodeId();
                    try {
                        db.execute(
                            "INSERT INTO nodes (id, ip, port, label, status, vultr_instance_id, added_at) "
                            "VALUES (?1, ?2, 8080, ?3, 'provisioning', ?4, ?5)",
                            {nodeId, instIp.empty() ? "0.0.0.0" : instIp, instLabel, instId, std::to_string(nowSec())});
                        db.execute(
                            "INSERT INTO mesh_events (event_type, node_id, payload, created_at) "
                            "VALUES ('node_added', ?1, ?2, ?3)",
                            {nodeId, result.dump(), std::to_string(nowSec())});
                    } catch (const std::exception& ex) {
                        spdlog::warn("Auto-register node failed: {}", ex.what());
                    }
                }
            }
        }
        res.set_content(result.dump(), "application/json");
    });

    svr.Delete(R"(/api/vultr/instances/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured"})", "application/json");
            return;
        }
        std::string instanceId = req.matches[1];
        VultrClient vc(key);
        if (vc.destroyInstance(instanceId)) {
            LogBuffer::instance().info("deploy", "VPS instance destroyed", {{"instance_id", instanceId}});
            // Remove any node linked to this Vultr instance
            try {
                Database::instance().execute(
                    "DELETE FROM nodes WHERE vultr_instance_id = ?1", {instanceId});
            } catch (...) {}
            res.set_content(R"({"destroyed":true})", "application/json");
        } else {
            LogBuffer::instance().error("deploy", "Failed to destroy VPS instance", {{"instance_id", instanceId}});
            res.status = 500;
            res.set_content(R"({"error":"failed to destroy instance"})", "application/json");
        }
    });

    svr.Get("/api/vultr/os", [ctx](const httplib::Request&, httplib::Response& res) {
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured"})", "application/json");
            return;
        }
        VultrClient vc(key);
        auto os = vc.listOSImages();
        res.set_content(os.dump(), "application/json");
    });

    svr.Post("/api/vultr/key", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string apiKey = body.value("api_key", "");
        if (apiKey.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"api_key required"})", "application/json");
            return;
        }
        VaultStorage::store("avacli_default", "vultr_api_key", "vultr", apiKey);
        auto path = serveSettingsFilePath();
        auto settings = loadServeSettingsJson();
        settings["vultr_api_key"] = apiKey;
        std::ofstream f(path);
        f << settings.dump(2);
        LogBuffer::instance().info("settings", "Vultr API key configured");
        res.set_content(R"({"saved":true})", "application/json");
    });

    svr.Get("/api/vultr/key/status", [](const httplib::Request&, httplib::Response& res) {
        std::string key = getVultrApiKey();
        nlohmann::json j;
        j["configured"] = !key.empty();
        res.set_content(j.dump(), "application/json");
    });

    svr.Delete("/api/vultr/key", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        VaultStorage::remove("avacli_default", "vultr_api_key");
        auto settings = loadServeSettingsJson();
        if (settings.contains("vultr_api_key")) {
            settings.erase("vultr_api_key");
            auto path = serveSettingsFilePath();
            std::ofstream f(path);
            f << settings.dump(2);
        }
        res.set_content(R"({"removed":true})", "application/json");
    });

    // ── Node Management (SQLite-backed) ──────────────────────────

    svr.Get("/api/nodes", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto rows = Database::instance().query(
                "SELECT * FROM nodes ORDER BY added_at DESC");
            nlohmann::json j;
            j["nodes"] = rows;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", ex.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/nodes", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string ip = body.value("ip", "");
        if (ip.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"ip required"})", "application/json");
            return;
        }
        int port = body.value("port", 8080);
        std::string label = body.value("label", ip);
        std::string domain = body.value("domain", "");
        std::string username = body.value("username", "");
        std::string authToken = body.value("password", "");
        auto nodeId = genNodeId();

        try {
            Database::instance().execute(
                "INSERT INTO nodes (id, ip, port, domain, label, username, auth_token, status, added_at) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 'testing', ?8)",
                {nodeId, ip, std::to_string(port), domain, label, username, authToken, std::to_string(nowSec())});
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", ex.what()}}).dump(), "application/json");
            return;
        }

        // Test connection in background
        NodeInfo ni;
        ni.id = nodeId; ni.ip = ip; ni.port = port;
        ni.domain = domain; ni.label = label;
        ni.username = username; ni.password = authToken;

        std::thread([ni]() {
            NodeClient client(ni);
            auto hr = client.checkHealth();
            std::string status = "unreachable";
            if (hr.reachable && hr.isAvacli && hr.authOk) status = "connected";
            else if (hr.reachable && !hr.authOk) status = "auth_failed";
            else if (hr.reachable && !hr.isAvacli) status = "not_avacli";

            try {
                Database::instance().execute(
                    "UPDATE nodes SET status = ?1, latency_ms = ?2, version = ?3, workspace = ?4, last_seen = ?5 "
                    "WHERE id = ?6",
                    {status, std::to_string(hr.latencyMs), hr.version, hr.workspace,
                     std::to_string(nowSec()), ni.id});
                Database::instance().execute(
                    "INSERT INTO mesh_events (event_type, node_id, payload, created_at) "
                    "VALUES ('node_added', ?1, ?2, ?3)",
                    {ni.id, nlohmann::json({{"status", status}, {"latency_ms", hr.latencyMs}}).dump(),
                     std::to_string(nowSec())});
            } catch (...) {}
            spdlog::info("Node {} connection test: {} ({}ms)", ni.label, status, hr.latencyMs);
        }).detach();

        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
        nlohmann::json j;
        j["node"] = row;
        res.set_content(j.dump(), "application/json");
    });

    svr.Delete(R"(/api/nodes/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string nodeId = req.matches[1];
        try {
            Database::instance().execute("DELETE FROM nodes WHERE id = ?1", {nodeId});
            Database::instance().execute(
                "INSERT INTO mesh_events (event_type, node_id, created_at) VALUES ('node_removed', ?1, ?2)",
                {nodeId, std::to_string(nowSec())});
        } catch (...) {}
        res.set_content(R"({"removed":true})", "application/json");
    });

    // ── Node Connection Testing ─────────────────────────────────

    svr.Post(R"(/api/nodes/([^/]+)/test)", [](const httplib::Request& req, httplib::Response& res) {
        std::string nodeId = req.matches[1];
        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
        if (row.is_null()) {
            res.status = 404;
            res.set_content(R"({"error":"node not found"})", "application/json");
            return;
        }
        NodeInfo ni = nodeFromRow(row);
        NodeClient client(ni);
        auto hr = client.checkHealth();

        std::string status = "unreachable";
        if (hr.reachable && hr.isAvacli && hr.authOk) status = "connected";
        else if (hr.reachable && !hr.authOk) status = "auth_failed";
        else if (hr.reachable && !hr.isAvacli) status = "not_avacli";

        try {
            Database::instance().execute(
                "UPDATE nodes SET status = ?1, latency_ms = ?2, version = ?3, workspace = ?4, last_seen = ?5 "
                "WHERE id = ?6",
                {status, std::to_string(hr.latencyMs), hr.version, hr.workspace,
                 std::to_string(nowSec()), nodeId});
        } catch (...) {}

        nlohmann::json j;
        j["status"] = status;
        j["latency_ms"] = hr.latencyMs;
        j["version"] = hr.version;
        j["workspace"] = hr.workspace;
        j["reachable"] = hr.reachable;
        j["is_avacli"] = hr.isAvacli;
        j["auth_ok"] = hr.authOk;
        if (!hr.error.empty()) j["error"] = hr.error;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/nodes/health", [](const httplib::Request&, httplib::Response& res) {
        auto rows = Database::instance().query("SELECT * FROM nodes");
        nlohmann::json results = nlohmann::json::array();

        for (const auto& row : rows) {
            NodeInfo ni = nodeFromRow(row);
            NodeClient client(ni);
            auto hr = client.checkHealth();

            std::string status = "unreachable";
            if (hr.reachable && hr.isAvacli && hr.authOk) status = "connected";
            else if (hr.reachable && !hr.authOk) status = "auth_failed";
            else if (hr.reachable && !hr.isAvacli) status = "not_avacli";

            try {
                Database::instance().execute(
                    "UPDATE nodes SET status = ?1, latency_ms = ?2, version = ?3, workspace = ?4, last_seen = ?5 "
                    "WHERE id = ?6",
                    {status, std::to_string(hr.latencyMs), hr.version, hr.workspace,
                     std::to_string(nowSec()), ni.id});
            } catch (...) {}

            nlohmann::json r;
            r["id"] = ni.id;
            r["label"] = ni.label;
            r["status"] = status;
            r["latency_ms"] = hr.latencyMs;
            r["version"] = hr.version;
            results.push_back(r);
        }

        nlohmann::json j;
        j["results"] = results;
        res.set_content(j.dump(), "application/json");
    });

    // ── Node Chat Proxy (forward messages to remote node) ──────

    svr.Post(R"(/api/nodes/([^/]+)/chat)", [ctx](const httplib::Request& req, httplib::Response& res) {
        std::string nodeId = req.matches[1];
        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
        if (row.is_null()) {
            res.status = 404;
            res.set_content(R"({"error":"node not found"})", "application/json");
            return;
        }

        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string message = body.value("message", "");
        std::string model = body.value("model", "");
        std::string session = body.value("session", "");
        if (message.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"message required"})", "application/json");
            return;
        }

        NodeInfo ni = nodeFromRow(row);

        struct EventQueue {
            std::mutex mu;
            std::condition_variable cv;
            std::queue<std::string> events;
            bool done = false;
        };
        auto eq = std::make_shared<EventQueue>();

        auto cancelFlag = ctx.chatCancelFlag;

        std::thread([eq, ni, message, model, session, cancelFlag, nodeId]() {
            NodeClient client(ni);
            bool ok = client.proxyChatStream(message, model, session,
                [eq, nodeId](const std::string& chunk) {
                    // Inject node_id into SSE events so the UI knows which node responded
                    std::lock_guard<std::mutex> lock(eq->mu);
                    eq->events.push(chunk);
                    eq->cv.notify_one();
                },
                cancelFlag);

            // Send a final done marker with node_id
            std::string doneEvt = "data: " + nlohmann::json({
                {"type", "node_done"}, {"node_id", nodeId}, {"success", ok}
            }).dump() + "\n\n";

            std::lock_guard<std::mutex> lock(eq->mu);
            eq->events.push(doneEvt);
            eq->done = true;
            eq->cv.notify_one();
        }).detach();

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider(
            "text/event-stream",
            [eq](size_t, httplib::DataSink& sink) -> bool {
                std::unique_lock<std::mutex> lock(eq->mu);
                eq->cv.wait(lock, [&] { return !eq->events.empty() || eq->done; });
                while (!eq->events.empty()) {
                    std::string event = std::move(eq->events.front());
                    eq->events.pop();
                    lock.unlock();
                    if (!sink.write(event.data(), event.size())) return false;
                    lock.lock();
                }
                if (eq->done && eq->events.empty()) { sink.done(); return false; }
                return true;
            });
    });

    // ── Mesh Sync ───────────────────────────────────────────────

    svr.Post("/api/mesh/sync", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        auto& db = Database::instance();
        auto incomingNodes = body.value("nodes", nlohmann::json::array());
        std::string sourceId = body.value("source_id", "");

        int merged = 0;
        for (const auto& n : incomingNodes) {
            std::string id = n.value("id", "");
            std::string ip = n.value("ip", "");
            if (id.empty() || ip.empty()) continue;

            auto existing = db.queryOne("SELECT id, last_seen FROM nodes WHERE id = ?1", {id});
            if (existing.is_null()) {
                try {
                    db.execute(
                        "INSERT INTO nodes (id, ip, port, domain, label, status, added_at, last_seen) "
                        "VALUES (?1, ?2, ?3, ?4, ?5, 'unknown', ?6, ?7)",
                        {id, ip, std::to_string(n.value("port", 8080)),
                         n.value("domain", ""), n.value("label", ip),
                         std::to_string(n.value("added_at", nowSec())),
                         std::to_string(nowSec())});
                    merged++;
                } catch (...) {}
            }
        }

        db.execute(
            "INSERT INTO mesh_events (event_type, source_node_id, payload, created_at) "
            "VALUES ('sync', ?1, ?2, ?3)",
            {sourceId, nlohmann::json({{"merged", merged}, {"received", incomingNodes.size()}}).dump(),
             std::to_string(nowSec())});

        auto localNodes = db.query("SELECT id, ip, port, domain, label, added_at FROM nodes");
        nlohmann::json j;
        j["nodes"] = localNodes;
        j["merged"] = merged;
        res.set_content(j.dump(), "application/json");
    });

    // ── Remote UI Proxy ─────────────────────────────────────────

    svr.Get(R"(/api/nodes/([^/]+)/proxy/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        std::string nodeId = req.matches[1];
        std::string path = "/" + std::string(req.matches[2]);
        if (path == "/") path = "/";

        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
        if (row.is_null()) {
            res.status = 404;
            res.set_content("Node not found", "text/plain");
            return;
        }

        NodeInfo ni = nodeFromRow(row);
        NodeClient client(ni);
        std::string contentType;
        std::string body = client.proxyRawGet(path, contentType);

        if (body.empty() && contentType.empty()) {
            res.status = 502;
            res.set_content("Could not reach node", "text/plain");
            return;
        }

        // Rewrite URLs in HTML responses to go through the proxy
        if (contentType.find("text/html") != std::string::npos) {
            std::string prefix = "/api/nodes/" + nodeId + "/proxy";
            // Rewrite absolute paths in src/href attributes
            auto rewrite = [&](const std::string& attr) {
                std::string needle = attr + "=\"/";
                size_t pos = 0;
                while ((pos = body.find(needle, pos)) != std::string::npos) {
                    pos += attr.size() + 2; // move past attr="
                    if (body.substr(pos, prefix.size()) == prefix.substr(1))
                        continue; // already rewritten
                    body.insert(pos, prefix);
                    pos += prefix.size();
                }
            };
            rewrite("src");
            rewrite("href");
            // Rewrite fetch('/api/...' calls
            size_t pos = 0;
            std::string fetchNeedle = "fetch('/";
            while ((pos = body.find(fetchNeedle, pos)) != std::string::npos) {
                pos += 7; // move past fetch('
                body.insert(pos, prefix);
                pos += prefix.size();
            }
            pos = 0;
            fetchNeedle = "fetch(\"/";
            while ((pos = body.find(fetchNeedle, pos)) != std::string::npos) {
                pos += 7;
                body.insert(pos, prefix);
                pos += prefix.size();
            }
        }

        if (contentType.empty()) contentType = "application/octet-stream";
        res.set_content(body, contentType.c_str());
    });

    // Also proxy POST requests for API calls from the remote UI
    svr.Post(R"(/api/nodes/([^/]+)/proxy/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        std::string nodeId = req.matches[1];
        std::string path = "/" + std::string(req.matches[2]);

        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
        if (row.is_null()) {
            res.status = 404;
            res.set_content(R"({"error":"node not found"})", "application/json");
            return;
        }

        NodeInfo ni = nodeFromRow(row);
        NodeClient client(ni);
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            body = nlohmann::json::object();
        }
        auto result = client.proxyPost(path, body);
        res.set_content(result.dump(), "application/json");
    });
}

} // namespace avacli
