#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "client/VultrClient.hpp"
#include "client/NodeClient.hpp"
#include "infra/VultrDeployUserdata.hpp"
#include "config/VaultStorage.hpp"
#include "config/ServeSettings.hpp"
#include "services/RelayManager.hpp"
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
#include <unistd.h>

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
        j["version"] = "2.3.4";
        j["workspace"] = ctx.config ? ctx.config->workspace : "";
        j["uptime_s"] = 0; // TODO: track real uptime
        j["port"] = ctx.actualPort ? *ctx.actualPort : 0;
        res.set_content(j.dump(), "application/json");
    });

    // Hostname (pure UX aid for node UIs that want to say "you are connecting to X")
    svr.Get("/api/hostname", [](const httplib::Request&, httplib::Response& res) {
        char buf[256] = {0};
        gethostname(buf, sizeof(buf) - 1);
        nlohmann::json j;
        j["hostname"] = std::string(buf);
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

            // Also include relay-connected clients as virtual nodes
            auto relayClients = RelayManager::instance().getClients();
            for (const auto& rc : relayClients) {
                nlohmann::json rn;
                rn["id"] = rc.id;
                rn["ip"] = "relay";
                rn["port"] = 0;
                rn["domain"] = "";
                rn["label"] = rc.label + " (relay)";
                rn["status"] = rc.connected ? "connected" : "unreachable";
                rn["version"] = rc.version;
                rn["workspace"] = rc.workspace;
                rn["last_seen"] = std::to_string(rc.lastSeen);
                rn["added_at"] = std::to_string(rc.connectedAt);
                rn["is_relay"] = true;
                rows.push_back(rn);
            }

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

        // Check if this is a relay client first
        bool isRelay = RelayManager::instance().isRelayClient(nodeId);

        if (!isRelay) {
            auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
            if (row.is_null()) {
                res.status = 404;
                res.set_content(R"({"error":"node not found"})", "application/json");
                return;
            }
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

        // For relay clients, route through the relay system
        if (isRelay) {
            auto& rm = RelayManager::instance();
            auto relayReq = rm.queueChatRequest(nodeId, message, model, session);
            if (!relayReq) {
                res.status = 502;
                res.set_content(R"({"error":"relay client not connected"})", "application/json");
                return;
            }

            auto rq = relayReq->responseQueue;
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*");

            res.set_chunked_content_provider(
                "text/event-stream",
                [rq](size_t, httplib::DataSink& sink) -> bool {
                    std::unique_lock<std::mutex> lock(rq->mu);
                    rq->cv.wait_for(lock, std::chrono::seconds(60),
                        [&] { return !rq->chunks.empty() || rq->done; });
                    while (!rq->chunks.empty()) {
                        std::string chunk = std::move(rq->chunks.front());
                        rq->chunks.pop();
                        lock.unlock();
                        if (!sink.write(chunk.data(), chunk.size())) return false;
                        lock.lock();
                    }
                    if (rq->done && rq->chunks.empty()) { sink.done(); return false; }
                    return true;
                });
            return;
        }

        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
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
    //
    // Design: show the remote node's UI inside an iframe on this client, transparently
    // routing every XHR/fetch/SSE/WebSocket from the iframe through /api/nodes/:id/proxy/*.
    //
    // The tricky bit: the remote's app.js calls `fetch(path)` where path is a variable,
    // which a server-side text rewriter can't reliably find. We used to only rewrite
    // literal `fetch('/...')` in the HTML — which missed app.js entirely and missed every
    // indirection through helper wrappers like api(). That caused the iframe to render
    // the remote HTML shell but silently fetch *this* client's data (local workspace,
    // local sessions, local models, local auth) — which is what the user was seeing.
    //
    // The fix is a tiny client-side shim injected at the top of the remote's <head>.
    // It monkey-patches fetch/XMLHttpRequest/EventSource/WebSocket so any absolute-path
    // URL starting with `/` is rewritten to `/api/nodes/:id/proxy<path>`. The shim also
    // shadows window.localStorage with a per-iframe in-memory store so the iframe's
    // session/token state doesn't collide with the parent page's.

    auto proxyShim = [](const std::string& nodeId) {
        // Kept inline and minified — this runs before anything else loads in the iframe.
        // Uses only widely-supported JS (IE11+ style) to match the rest of the UI code.
        std::string pfx = "/api/nodes/" + nodeId + "/proxy";
        std::string js;
        js.reserve(3072);
        js += "<script>(function(){";
        js += "var PFX=" + nlohmann::json(pfx).dump() + ";";
        js += "function rw(u){"
              "if(typeof u!=='string')return u;"
              "if(u.charAt(0)!=='/')return u;"
              "if(u.indexOf(PFX)===0)return u;"
              "if(u.indexOf('//')===0)return u;"
              "return PFX+u;"
              "}";
        // fetch
        js += "var _f=window.fetch;"
              "window.fetch=function(i,n){"
              "try{"
              "if(typeof i==='string')i=rw(i);"
              "else if(i&&typeof i.url==='string'){var ru=rw(i.url);if(ru!==i.url)i=new Request(ru,i);}"
              "}catch(e){}"
              "return _f.call(this,i,n);"
              "};";
        // XMLHttpRequest
        js += "var _xo=XMLHttpRequest.prototype.open;"
              "XMLHttpRequest.prototype.open=function(m,u){"
              "try{if(typeof u==='string')arguments[1]=rw(u);}catch(e){}"
              "return _xo.apply(this,arguments);"
              "};";
        // EventSource
        js += "if(window.EventSource){"
              "var _es=window.EventSource;"
              "function _ES(u,c){return new _es(rw(u),c);}"
              "_ES.prototype=_es.prototype;"
              "_ES.CONNECTING=_es.CONNECTING;_ES.OPEN=_es.OPEN;_ES.CLOSED=_es.CLOSED;"
              "window.EventSource=_ES;"
              "}";
        // WebSocket — translate ws://this.host/foo to ws://this.host/PFX/foo if path-absolute
        js += "if(window.WebSocket){"
              "var _ws=window.WebSocket;"
              "function _WS(u,p){"
              "try{if(typeof u==='string'&&/^wss?:\\/\\//.test(u)){"
              "var m=u.match(/^(wss?:\\/\\/[^/]+)(\\/.*)?$/);"
              "if(m&&m[2]&&m[2].indexOf(PFX)!==0)u=m[1]+PFX+m[2];"
              "}}catch(e){}"
              "return p?new _ws(u,p):new _ws(u);"
              "}"
              "_WS.prototype=_ws.prototype;_WS.CONNECTING=0;_WS.OPEN=1;_WS.CLOSING=2;_WS.CLOSED=3;"
              "window.WebSocket=_WS;"
              "}";
        // Shadow localStorage & sessionStorage with per-iframe stores. Seeds a dummy
        // token so the remote UI treats the iframe as "logged in" — the proxy route
        // itself doesn't validate the forwarded token, we authenticate to the remote
        // using the node's stored admin token on the server side.
        js += "(function(){"
              "var _s={avacli_token:'proxy',ava_token:'proxy'};"
              "function mk(s){"
              "var st={"
              "getItem:function(k){return s[k]!==undefined?s[k]:null;},"
              "setItem:function(k,v){s[k]=String(v);},"
              "removeItem:function(k){delete s[k];},"
              "clear:function(){for(var k in s)delete s[k];},"
              "key:function(i){return Object.keys(s)[i]||null;}"
              "};"
              "Object.defineProperty(st,'length',{get:function(){return Object.keys(s).length;}});"
              "return st;"
              "}"
              "try{Object.defineProperty(window,'localStorage',{configurable:true,get:function(){return mk(_s);}});}catch(e){}"
              "try{var _ss={};Object.defineProperty(window,'sessionStorage',{configurable:true,get:function(){return mk(_ss);}});}catch(e){}"
              "})();";
        js += "})();</script>";
        return js;
    };

    svr.Get(R"(/api/nodes/([^/]+)/proxy/?(.*))", [proxyShim](const httplib::Request& req, httplib::Response& res) {
        std::string nodeId = req.matches[1];
        std::string path = "/" + std::string(req.matches[2]);
        // Forward the query string so remote endpoints that paginate / filter still work.
        auto q = req.target.find('?');
        if (q != std::string::npos) path += req.target.substr(q);

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

        if (contentType.find("text/html") != std::string::npos) {
            std::string prefix = "/api/nodes/" + nodeId + "/proxy";

            // Keep rewriting static src/href attributes so non-JS paths (<img>, stylesheet
            // links, favicons) still resolve through the proxy for browsers/tools that
            // don't execute the shim (e.g. initial paint, view-source).
            auto rewrite = [&](const std::string& attr) {
                std::string needle = attr + "=\"/";
                size_t pos = 0;
                while ((pos = body.find(needle, pos)) != std::string::npos) {
                    pos += attr.size() + 2;
                    if (body.substr(pos, prefix.size() - 1) == prefix.substr(1)) continue;
                    body.insert(pos, prefix);
                    pos += prefix.size();
                }
            };
            rewrite("src");
            rewrite("href");

            // Inject the shim as the very first child of <head>. It MUST run before any
            // other script on the page for the fetch/XHR/EventSource wrappers to take effect.
            std::string shim = proxyShim(nodeId);
            size_t headPos = body.find("<head");
            if (headPos != std::string::npos) {
                size_t gt = body.find('>', headPos);
                if (gt != std::string::npos) body.insert(gt + 1, shim);
                else body.insert(0, shim);
            } else {
                // No <head>? Prepend and hope the browser tolerates a script before <html>.
                body.insert(0, shim);
            }
        }

        if (contentType.empty()) contentType = "application/octet-stream";
        res.set_content(body, contentType.c_str());
    });

    // Transparent streaming POST proxy. Does NOT buffer-and-JSON-parse the upstream
    // response — that's what broke chat streaming previously (a text/event-stream reply
    // was silently replaced with `{"error":"invalid JSON"}` after 30s). Instead we run
    // the upstream request in a worker thread and pipe raw bytes to the browser via
    // set_chunked_content_provider so SSE tokens arrive live.
    svr.Post(R"(/api/nodes/([^/]+)/proxy/?(.*))", [](const httplib::Request& req, httplib::Response& res) {
        std::string nodeId = req.matches[1];
        std::string path = "/" + std::string(req.matches[2]);
        auto q = req.target.find('?');
        if (q != std::string::npos) path += req.target.substr(q);

        auto row = Database::instance().queryOne("SELECT * FROM nodes WHERE id = ?1", {nodeId});
        if (row.is_null()) {
            res.status = 404;
            res.set_content(R"({"error":"node not found"})", "application/json");
            return;
        }

        NodeInfo ni = nodeFromRow(row);

        struct ChunkQueue {
            std::mutex mu;
            std::condition_variable cv;
            std::queue<std::string> chunks;
            bool done = false;
            bool headersReady = false;
            long status = 0;
            std::string contentType;
        };
        auto cq = std::make_shared<ChunkQueue>();
        std::string bodyCopy = req.body;
        std::string reqCt = req.get_header_value("Content-Type");
        if (reqCt.empty()) reqCt = "application/json";

        std::thread([cq, ni, path, bodyCopy, reqCt]() {
            NodeClient client(ni);
            client.proxyRawPost(path, bodyCopy, reqCt,
                [cq](long status, const std::string& ct) {
                    {
                        std::lock_guard<std::mutex> lock(cq->mu);
                        cq->status = status;
                        cq->contentType = ct;
                        cq->headersReady = true;
                    }
                    cq->cv.notify_all();
                },
                [cq](const std::string& chunk) {
                    {
                        std::lock_guard<std::mutex> lock(cq->mu);
                        cq->chunks.push(chunk);
                    }
                    cq->cv.notify_all();
                });
            {
                std::lock_guard<std::mutex> lock(cq->mu);
                cq->done = true;
                if (!cq->headersReady) cq->headersReady = true;
            }
            cq->cv.notify_all();
        }).detach();

        // Block up to 30s for upstream headers so we can set our status + content-type
        // before the browser starts reading. Past that, fall through with whatever we got.
        {
            std::unique_lock<std::mutex> lock(cq->mu);
            cq->cv.wait_for(lock, std::chrono::seconds(30),
                [&] { return cq->headersReady || cq->done; });
        }

        std::string ct = cq->contentType.empty() ? std::string("application/octet-stream") : cq->contentType;
        if (cq->status > 0) res.status = (int)cq->status;

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_chunked_content_provider(
            ct.c_str(),
            [cq](size_t, httplib::DataSink& sink) -> bool {
                std::unique_lock<std::mutex> lock(cq->mu);
                cq->cv.wait_for(lock, std::chrono::seconds(60),
                    [&] { return !cq->chunks.empty() || cq->done; });
                while (!cq->chunks.empty()) {
                    std::string c = std::move(cq->chunks.front());
                    cq->chunks.pop();
                    lock.unlock();
                    if (!sink.write(c.data(), c.size())) return false;
                    lock.lock();
                }
                if (cq->done && cq->chunks.empty()) { sink.done(); return false; }
                return true;
            });
    });
}

} // namespace avacli
