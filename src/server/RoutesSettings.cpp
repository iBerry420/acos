#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "config/ModelRegistry.hpp"
#include "config/ServeSettings.hpp"
#include "services/RelayManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace avacli {

namespace fs = std::filesystem;

void registerSettingsRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Get("/api/status", [ctx](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["status"] = "running";
        j["model"] = ctx.config->model;
        j["workspace"] = ctx.config->workspace;
        j["session"] = ctx.config->session;
        j["port"] = *ctx.actualPort;
        j["requested_port"] = ctx.config->port;
        j["host"] = ctx.config->host;
        j["has_xai_key"] = !ctx.config->apiKey.empty();
        j["extra_model"] = ctx.config->extraModel;
        j["media_model"] = ctx.config->mediaModel;
        j["node_role"] = ctx.config->nodeRole;
        j["relay_server"] = ctx.config->relayServer;
        j["has_relay_token"] = !ctx.config->relayToken.empty();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/models/refresh", [ctx](const httplib::Request&, httplib::Response& res) {
        if (ctx.config->apiKey.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"no API key configured"})", "application/json");
            return;
        }
        ModelRegistry::refreshModels(ctx.config->apiKey);
        const auto& models = ModelRegistry::listAll();
        nlohmann::json j;
        j["refreshed"] = true;
        j["count"] = models.size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/models", [](const httplib::Request&, httplib::Response& res) {
        const auto& models = ModelRegistry::listAll();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& m : models) {
            arr.push_back({
                {"id", m.id},
                {"contextWindow", m.contextWindow},
                {"defaultMaxTokens", m.defaultMaxTokens},
                {"reasoning", m.reasoning},
                {"responsesApi", m.responsesApi},
                {"type", m.type}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    svr.Post("/api/settings", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        // API key changes require admin
        if (body.contains("xai_api_key") && body["xai_api_key"].is_string()) {
            if (!isAdminRequest(req, ctx.masterKeyMgr)) {
                res.status = 403;
                res.set_content(R"({"error":"admin access required to change API keys"})", "application/json");
                return;
            }
        }

        if (body.contains("model") && body["model"].is_string()) {
            std::string newModel = body["model"].get<std::string>();
            if (newModel != ctx.config->model)
                LogBuffer::instance().info("settings", "Model changed");
            ctx.config->model = newModel;
        }
        if (body.contains("extra_model") && body["extra_model"].is_string())
            ctx.config->extraModel = body["extra_model"].get<std::string>();
        if (body.contains("media_model") && body["media_model"].is_string())
            ctx.config->mediaModel = body["media_model"].get<std::string>();
        if (body.contains("node_role") && body["node_role"].is_string()) {
            std::string role = body["node_role"].get<std::string>();
            if (role == "standalone" || role == "server" || role == "client")
                ctx.config->nodeRole = role;
        }
        if (body.contains("relay_server") && body["relay_server"].is_string())
            ctx.config->relayServer = body["relay_server"].get<std::string>();
        if (body.contains("relay_token") && body["relay_token"].is_string())
            ctx.config->relayToken = body["relay_token"].get<std::string>();
        // Sync relay token to RelayManager at runtime
        if (body.contains("relay_token") || body.contains("node_role"))
            RelayManager::instance().setAcceptToken(ctx.config->relayToken);
        if (body.contains("xai_api_key") && body["xai_api_key"].is_string()) {
            std::string newKey = body["xai_api_key"].get<std::string>();
            bool keyChanged = (newKey != ctx.config->apiKey);
            ctx.config->apiKey = newKey;
            if (keyChanged && !newKey.empty())
                ModelRegistry::refreshModels(newKey);
        }

        auto path = serveSettingsFilePath();
        fs::create_directories(fs::path(path).parent_path());

        nlohmann::json settings = loadServeSettingsJson();
        settings["model"] = ctx.config->model;
        settings["workspace"] = ctx.config->workspace;
        settings["port"] = ctx.config->port;
        if (!ctx.config->extraModel.empty())
            settings["extra_model"] = ctx.config->extraModel;
        if (!ctx.config->mediaModel.empty())
            settings["media_model"] = ctx.config->mediaModel;
        else if (body.contains("media_model"))
            settings.erase("media_model");
        settings["node_role"] = ctx.config->nodeRole;
        if (!ctx.config->relayServer.empty())
            settings["relay_server"] = ctx.config->relayServer;
        else if (body.contains("relay_server"))
            settings.erase("relay_server");
        if (!ctx.config->relayToken.empty())
            settings["relay_token"] = ctx.config->relayToken;
        else if (body.contains("relay_token"))
            settings.erase("relay_token");
        if (!ctx.config->apiKey.empty())
            settings["xai_api_key"] = ctx.config->apiKey;
        else if (body.contains("xai_api_key") && body["xai_api_key"].is_string()
                 && body["xai_api_key"].get<std::string>().empty())
            settings.erase("xai_api_key");

        std::ofstream f(path);
        f << settings.dump(2);

        LogBuffer::instance().info("settings", "Settings updated");

        nlohmann::json j;
        j["saved"] = true;
        nlohmann::json pub = settings;
        pub.erase("xai_api_key");
        pub.erase("vultr_api_key");
        pub["has_xai_key"] = !ctx.config->apiKey.empty();
        j["settings"] = pub;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/settings/workspace", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string newWorkspace = body.value("workspace", "");
        if (newWorkspace.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"workspace path required"})", "application/json");
            return;
        }

        try {
            auto resolved = fs::canonical(newWorkspace);
            if (!fs::is_directory(resolved)) {
                res.status = 400;
                res.set_content(R"({"error":"path is not a directory"})", "application/json");
                return;
            }
            ctx.config->workspace = resolved.string();
            LogBuffer::instance().info("settings", "Workspace changed");

            nlohmann::json j;
            j["workspace"] = ctx.config->workspace;
            j["success"] = true;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            nlohmann::json err;
            err["error"] = std::string("invalid path: ") + e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Get("/api/logs", [](const httplib::Request& req, httplib::Response& res) {
        int limit = 500;
        long long since = 0;
        auto it = req.params.find("limit");
        if (it != req.params.end()) try { limit = std::stoi(it->second); } catch (...) {}
        it = req.params.find("since");
        if (it != req.params.end()) try { since = std::stoll(it->second); } catch (...) {}

        auto entries = LogBuffer::instance().getEntries(limit, since);
        nlohmann::json j;
        j["entries"] = entries;
        j["total"] = LogBuffer::instance().size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/logs/clear", [](const httplib::Request&, httplib::Response& res) {
        LogBuffer::instance().clear();
        res.set_content(R"({"cleared":true})", "application/json");
    });
}

} // namespace avacli
