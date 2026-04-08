#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "client/VultrClient.hpp"
#include "infra/VultrDeployUserdata.hpp"
#include "config/VaultStorage.hpp"
#include "config/ServeSettings.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace avacli {

namespace fs = std::filesystem;

void registerInfraRoutes(httplib::Server& svr, ServerContext ctx) {

    // ── Vultr VPS Management (admin only) ────────────────────────────

    svr.Get("/api/vultr/plans", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
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

    svr.Get("/api/vultr/regions", [ctx](const httplib::Request& req, httplib::Response& res) {
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
        VultrClient vc(key);
        auto regions = vc.listRegions();
        res.set_content(regions.dump(), "application/json");
    });

    svr.Get("/api/vultr/account", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string key = getVultrApiKey();
        if (key.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Vultr API key not configured. Add it in Settings."})", "application/json");
            return;
        }
        VultrClient vc(key);
        nlohmann::json out;
        out["account"] = vc.getAccount();
        out["bandwidth"] = vc.getAccountBandwidth();
        out["pending_charges"] = vc.listPendingCharges();
        res.set_content(out.dump(), "application/json");
    });

    svr.Get("/api/vultr/instances", [ctx](const httplib::Request& req, httplib::Response& res) {
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
        VultrClient vc(key);
        auto instances = vc.listInstances();
        res.set_content(instances.dump(), "application/json");
    });

    svr.Get(R"(/api/vultr/instances/([^/]+))", [ctx](const httplib::Request& req, httplib::Response& res) {
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
            res.set_content(R"({"destroyed":true})", "application/json");
        } else {
            LogBuffer::instance().error("deploy", "Failed to destroy VPS instance", {{"instance_id", instanceId}});
            res.status = 500;
            res.set_content(R"({"error":"failed to destroy instance"})", "application/json");
        }
    });

    svr.Get("/api/vultr/os", [ctx](const httplib::Request& req, httplib::Response& res) {
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

    // ── Node Management ──────────────────────────────────────────────

    svr.Get("/api/nodes", [](const httplib::Request&, httplib::Response& res) {
        std::string home;
        const char* h = std::getenv("HOME");
        if (h) home = h; else home = ".";
        auto path = (fs::path(home) / ".avacli" / "nodes.json").string();
        nlohmann::json nodes = nlohmann::json::array();
        if (fs::exists(path)) {
            try { std::ifstream f(path); nodes = nlohmann::json::parse(f); } catch (...) {}
        }
        nlohmann::json j;
        j["nodes"] = nodes;
        res.set_content(j.dump(), "application/json");
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
        std::string home;
        const char* h = std::getenv("HOME");
        if (h) home = h; else home = ".";
        auto dirPath = (fs::path(home) / ".avacli").string();
        fs::create_directories(dirPath);
        auto path = (fs::path(dirPath) / "nodes.json").string();
        nlohmann::json nodes = nlohmann::json::array();
        if (fs::exists(path)) {
            try { std::ifstream f(path); nodes = nlohmann::json::parse(f); } catch (...) {}
        }
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        nlohmann::json node;
        node["id"] = std::to_string(now);
        node["ip"] = ip;
        node["port"] = body.value("port", 8080);
        node["domain"] = body.value("domain", "");
        node["label"] = body.value("label", ip);
        node["added_at"] = now / 1000;
        if (body.contains("username") && !body["username"].get<std::string>().empty())
            node["username"] = body["username"];
        if (body.contains("password") && !body["password"].get<std::string>().empty())
            node["has_password"] = true;
        nodes.push_back(node);
        { std::ofstream f(path); f << nodes.dump(2); }
        nlohmann::json j;
        j["node"] = node;
        res.set_content(j.dump(), "application/json");
    });

    svr.Delete(R"(/api/nodes/(.+))", [ctx](const httplib::Request& req, httplib::Response& res) {
        if (!isAdminRequest(req, ctx.masterKeyMgr)) {
            res.status = 403;
            res.set_content(R"({"error":"admin access required"})", "application/json");
            return;
        }
        std::string nodeId = req.matches[1];
        std::string home;
        const char* h = std::getenv("HOME");
        if (h) home = h; else home = ".";
        auto path = (fs::path(home) / ".avacli" / "nodes.json").string();
        if (!fs::exists(path)) {
            res.set_content(R"({"removed":true})", "application/json");
            return;
        }
        try {
            std::ifstream fi(path);
            auto arr = nlohmann::json::parse(fi);
            nlohmann::json out = nlohmann::json::array();
            for (const auto& n : arr) {
                if (n.value("id", "") != nodeId)
                    out.push_back(n);
            }
            std::ofstream fo(path);
            fo << out.dump(2);
            res.set_content(R"({"removed":true})", "application/json");
        } catch (...) {
            res.status = 500;
            res.set_content(R"({"error":"failed"})", "application/json");
        }
    });
}

} // namespace avacli
