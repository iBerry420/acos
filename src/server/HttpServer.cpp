#include "server/HttpServer.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/Routes.hpp"
#include "server/EmbeddedAssets.hpp"
#include "server/UIFileServer.hpp"
#include "server/LogBuffer.hpp"
#include "config/ModelRegistry.hpp"
#include "tools/ToolRegistry.hpp"
#include "platform/Paths.hpp"
#include "db/Database.hpp"
#include "services/MeshManager.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace avacli {

namespace fs = std::filesystem;

struct HttpServer::Impl {
    httplib::Server svr;
    std::atomic<bool> chatCancelFlag{false};
    std::atomic<size_t> sessionPromptTokens{0};
    std::atomic<size_t> sessionCompletionTokens{0};
};

HttpServer::HttpServer(ServeConfig config)
    : impl_(std::make_unique<Impl>()), config_(std::move(config)) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::start() {
    Database::instance().init();
    masterKeyMgr_.loadSessionsFromDisk();
    setupRoutes();
    running_ = true;

    int port = findAvailablePort(config_.port);
    actualPort_ = port;
    if (port != config_.port) {
        spdlog::warn("Port {} in use — binding to port {} instead", config_.port, port);
    }

    LogBuffer::instance().info("system", "Avacli server started", {{"port", port}, {"workspace", config_.workspace}});
    spdlog::info("Avacli serving at http://localhost:{}", port);
    std::cout << "\nAvacli running at http://localhost:" << port
              << "  (Ctrl+C to stop)\n\n" << std::flush;

    MeshManager::instance().start();

    impl_->svr.listen(config_.host, port);
    MeshManager::instance().stop();
    running_ = false;
}

void HttpServer::stop() {
    if (running_.exchange(false)) {
        impl_->svr.stop();
    }
}

void HttpServer::setupRoutes() {
    mergeServeDiskIntoConfig(config_);

    if (!config_.apiKey.empty())
        ModelRegistry::fetchFromApi(config_.apiKey);

    auto& svr = impl_->svr;

    // Load custom Python tools from disk
    {
        ToolRegistry::registerAll();
        std::string toolsDir = (fs::path(userHomeDirectoryOrFallback()) / ".avacli" / "custom_tools").string();
        if (fs::exists(toolsDir)) {
            for (const auto& entry : fs::directory_iterator(toolsDir)) {
                if (entry.path().extension() == ".json") {
                    try {
                        std::ifstream ifs(entry.path());
                        auto meta = nlohmann::json::parse(ifs);
                        std::string name = meta.value("name", "");
                        if (name.empty() || name.find("custom_") != 0) continue;
                        std::string pyFile = (fs::path(toolsDir) / (name + ".py")).string();
                        if (!fs::exists(pyFile)) continue;

                        nlohmann::json toolDef;
                        toolDef["type"] = "function";
                        toolDef["function"]["name"] = name;
                        toolDef["function"]["description"] = meta.value("description", "");
                        toolDef["function"]["parameters"] = meta.value("parameters", nlohmann::json::object());
                        ToolRegistry::registerCustomTool(toolDef);
                    } catch (...) {}
                }
            }
        }
    }

    // CORS pre-routing + auth middleware
    svr.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res)
                                    -> httplib::Server::HandlerResponse {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }

        if (MasterKeyManager::isConfigured() && MasterKeyManager::hasAnyPasswordSet()
            && req.path.rfind("/api/", 0) == 0) {
            if (req.path != "/api/auth/login" && req.path != "/api/auth/status"
                && req.path.rfind("/api/auth/status", 0) != 0
                && req.path != "/api/auth/accounts"
                && req.path != "/api/health"
                && req.path != "/api/mesh/sync") {
                std::string authHeader = req.get_header_value("Authorization");
                bool authed = false;
                if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
                    authed = masterKeyMgr_.validateSession(authHeader.substr(7));
                }
                if (!authed) {
                    res.status = 401;
                    res.set_content(R"({"error":"authentication required"})", "application/json");
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Build shared context for route handlers
    ServerContext ctx;
    ctx.config = &config_;
    ctx.masterKeyMgr = &masterKeyMgr_;
    ctx.chatCancelFlag = &impl_->chatCancelFlag;
    ctx.sessionPromptTokens = &impl_->sessionPromptTokens;
    ctx.sessionCompletionTokens = &impl_->sessionCompletionTokens;
    ctx.actualPort = &actualPort_;

    // Register all route groups
    registerAuthRoutes(svr, ctx);
    registerChatRoutes(svr, ctx);
    registerFileRoutes(svr, ctx);
    registerToolRoutes(svr, ctx);
    registerDataRoutes(svr, ctx);
    registerSettingsRoutes(svr, ctx);
    registerInfraRoutes(svr, ctx);
    registerDBRoutes(svr, ctx);
    registerKnowledgeRoutes(svr, ctx);
    registerAppRoutes(svr, ctx);
    registerServiceRoutes(svr, ctx);

    // App serving: /apps/:slug/* serves user-created app files from SQLite
    svr.Get(R"(/apps/([^/]+)/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        std::string slug = req.matches[1];
        std::string filepath = req.matches[2];
        if (filepath.empty()) filepath = "index.html";

        try {
            auto& db = Database::instance();
            auto row = db.queryOne(
                "SELECT af.content, af.mime_type FROM app_files af "
                "JOIN apps a ON a.id = af.app_id "
                "WHERE a.slug = ?1 AND af.filename = ?2 AND a.status = 'active'",
                {slug, filepath});

            if (row.is_null()) {
                res.status = 404;
                res.set_content("Not Found", "text/plain");
                return;
            }
            res.set_content(row["content"].get<std::string>(),
                            row["mime_type"].get<std::string>().c_str());
        } catch (...) {
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        }
    });

    // Resolve the UI directory once at startup
    std::string resolvedUiDir;
    bool servingFromDisk = false;
    if (!config_.useEmbeddedUI) {
        resolvedUiDir = UIFileServer::resolveUiDir(config_.uiDir);
        if (UIFileServer::isValidUiDir(resolvedUiDir)) {
            servingFromDisk = true;
            spdlog::info("Serving UI from disk: {}", resolvedUiDir);
        } else {
            spdlog::info("No UI directory at {} — using compiled-in UI", resolvedUiDir);
        }
    } else {
        spdlog::info("Using compiled-in embedded UI (--ui-embedded)");
    }

    std::string uiDir = resolvedUiDir;
    std::string uiTheme = config_.uiTheme;

    // Serve static UI assets from disk: /ui/*
    if (servingFromDisk) {
        svr.Get(R"(/ui/(.+))", [uiDir](const httplib::Request& req, httplib::Response& res) {
            auto result = UIFileServer::serveFile(uiDir, req.path);
            if (result) {
                res.set_content(result->first, result->second.c_str());
            } else {
                res.status = 404;
                res.set_content("Not Found", "text/plain");
            }
        });
    }

    // SPA entry point
    svr.Get("/", [servingFromDisk, uiDir, uiTheme](const httplib::Request&, httplib::Response& res) {
        if (servingFromDisk) {
            auto html = UIFileServer::getIndexFromDisk(uiDir, uiTheme);
            if (html) {
                res.set_content(*html, "text/html");
                return;
            }
        }
        res.set_content(getIndexHtml(), "text/html");
    });

    // Catch-all: serve static files or SPA index for client-side routing
    svr.Get(R"(/.+)", [servingFromDisk, uiDir, uiTheme](const httplib::Request& req, httplib::Response& res) {
        if (req.path.rfind("/api/", 0) == 0) return;
        if (req.path.rfind("/apps/", 0) == 0) return;

        // Try serving a static file from the UI directory
        if (servingFromDisk) {
            auto result = UIFileServer::serveFile(uiDir, req.path);
            if (result) {
                res.set_content(result->first, result->second.c_str());
                return;
            }
        }

        // SPA fallback: serve index.html for client-side routing
        if (servingFromDisk) {
            auto html = UIFileServer::getIndexFromDisk(uiDir, uiTheme);
            if (html) {
                res.set_content(*html, "text/html");
                return;
            }
        }
        res.set_content(getIndexHtml(), "text/html");
    });
}

void HttpServer::handleChat() {}

} // namespace avacli
