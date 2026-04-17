#include "server/HttpServer.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/Routes.hpp"
#include "server/EmbeddedAssets.hpp"
#include "server/UIFileServer.hpp"
#include "server/LogBuffer.hpp"
#include "agents/SubAgentManager.hpp"
#include "config/ModelRegistry.hpp"
#include "tools/ToolRegistry.hpp"
#include "platform/Paths.hpp"
#include "db/Database.hpp"
#include "services/BatchPoller.hpp"
#include "services/MeshManager.hpp"
#include "services/RelayManager.hpp"
#include "services/RelayClient.hpp"

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
    std::atomic<bool> chatBusyFlag{false};
    std::atomic<size_t> sessionPromptTokens{0};
    std::atomic<size_t> sessionCompletionTokens{0};
    AskUserState askUserState;
};

HttpServer::HttpServer(ServeConfig config)
    : impl_(std::make_unique<Impl>()), config_(std::move(config)) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
    Database::instance().init();
    masterKeyMgr_.loadSessionsFromDisk();
    setupRoutes();
    running_ = true;

    // Enable SO_REUSEADDR on the listen socket so a quick systemctl
    // restart doesn't stall for ~60s waiting for prior Apache-upstream
    // connections to drain out of TIME_WAIT.
    //
    // Deliberately NOT setting SO_REUSEPORT: on Linux, SO_REUSEPORT lets
    // multiple processes bind to the same port at once (it's the
    // multi-worker load-balancing flag), which would mean a stray old
    // avacli instance could silently share 9100 with the new one and
    // randomly answer half of incoming requests from stale code. We
    // want the exact opposite here — exclusive ownership, but with the
    // kernel willing to re-bind over stale TIME_WAIT 4-tuples.
    // SO_REUSEADDR alone gives us that on Linux.
    //
    // (socket_t is in the global namespace in httplib.h, not inside
    // `httplib::`, so `auto` keeps this portable between its Windows
    // (SOCKET) and POSIX (int) typedefs without us redeclaring it.)
    impl_->svr.set_socket_options([](auto sock) {
        int opt = 1;
#ifdef _WIN32
        // On Windows we pair SO_REUSEADDR with SO_EXCLUSIVEADDRUSE —
        // REUSEADDR there has different semantics (it *does* allow
        // concurrent listeners), and EXCLUSIVEADDRUSE pins the port to
        // the current socket so we keep the "one live listener" contract.
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&opt), sizeof(opt));
        ::setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    });

    // Resolve the port. Two modes:
    //
    //   (a) Explicit port (systemd service, Docker, or `--port` on the
    //       CLI): bind exactly where we were told. If the port is in use
    //       *even with* SO_REUSEADDR set, something else actually owns it
    //       — falling back silently to port+1 would leave Apache 502-ing
    //       at the configured port. Log an error and return so the
    //       service manager (systemd Restart=always) can back off + retry.
    //
    //   (b) Default/ad-hoc port: keep the old convenience behavior where
    //       we probe for the next free port so two dev instances can run
    //       side-by-side without manual conflict resolution.
    int port = config_.port;
    if (config_.portExplicit) {
        if (!impl_->svr.bind_to_port(config_.host, port)) {
            spdlog::error("Failed to bind to {}:{} — port is in use or permission denied. "
                          "Exiting so the service manager can retry.",
                          config_.host, port);
            LogBuffer::instance().error("system", "Avacli bind failed",
                                        {{"host", config_.host}, {"port", port}});
            running_ = false;
            return false;
        }
    } else {
        port = findAvailablePort(config_.port);
        if (port != config_.port) {
            spdlog::warn("Port {} in use — binding to port {} instead", config_.port, port);
        }
        if (!impl_->svr.bind_to_port(config_.host, port)) {
            spdlog::error("Failed to bind to {}:{} — giving up.", config_.host, port);
            running_ = false;
            return false;
        }
    }
    actualPort_ = port;

    LogBuffer::instance().info("system", "Avacli server started", {{"port", port}, {"workspace", config_.workspace}});
    spdlog::info("Avacli serving at http://localhost:{}", port);
    std::cout << "\nAvacli running at http://localhost:" << port
              << "  (Ctrl+C to stop)\n\n" << std::flush;

    MeshManager::instance().start();

    // Phase 5: background poller for xAI Batch API state. Looks up processing
    // batches every ~30s, persists updates + cached results. The poller
    // tolerates an empty API key (returns silently), so it's safe to start
    // even if Settings haven't been configured yet.
    BatchPoller::instance().setServeConfig(&config_);
    BatchPoller::instance().start();

    // In server mode, set the relay accept token
    if (config_.nodeRole == "server" && !config_.relayToken.empty())
        RelayManager::instance().setAcceptToken(config_.relayToken);

    // In client mode, start the relay client
    if (config_.nodeRole == "client" && !config_.relayServer.empty()) {
        RelayClient::instance().start(config_.relayServer, config_.relayToken, port);
    }

    impl_->svr.listen_after_bind();

    RelayClient::instance().stop();
    BatchPoller::instance().stop();
    MeshManager::instance().stop();
    running_ = false;
    return true;
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
            // Per-app SDK endpoints (phase 3) use their own agent_token
            // instead of the master-key session. Shape:
            //   /api/apps/<slug>/db/...
            //   /api/apps/<slug>/main/...
            //   /api/apps/<slug>/agent[/...]  (phase 3+/4 AI proxy)
            // Any legacy app-admin endpoints (GET/PUT/DELETE /api/apps,
            // /api/apps/:id, /api/apps/:id/files, /api/apps/:id/export,
            // /api/apps/:id/generate-icon) continue to require the master
            // key — we only bypass for the three SDK-facing suffixes.
            bool isAppSdkEndpoint = false;
            if (req.path.rfind("/api/apps/", 0) == 0) {
                // Start scanning after "/api/apps/".
                size_t p = std::string("/api/apps/").size();
                size_t slash = req.path.find('/', p);
                if (slash != std::string::npos) {
                    std::string rest = req.path.substr(slash + 1);
                    if (rest.rfind("db/", 0) == 0 || rest == "db"
                     || rest.rfind("main/", 0) == 0 || rest == "main"
                     || rest.rfind("agent", 0) == 0) {
                        isAppSdkEndpoint = true;
                    }
                }
            }

            if (req.path != "/api/auth/login" && req.path != "/api/auth/status"
                && req.path.rfind("/api/auth/status", 0) != 0
                && req.path != "/api/auth/accounts"
                && req.path != "/api/health"
                && req.path != "/api/mesh/sync"
                && req.path.rfind("/api/relay/", 0) != 0
                && !isAppSdkEndpoint) {
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
    ctx.chatBusyFlag = &impl_->chatBusyFlag;
    ctx.sessionPromptTokens = &impl_->sessionPromptTokens;
    ctx.sessionCompletionTokens = &impl_->sessionCompletionTokens;
    ctx.actualPort = &actualPort_;
    ctx.askUserState = &impl_->askUserState;

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
    registerAppDBRoutes(svr, ctx);
    registerServiceRoutes(svr, ctx);
    registerSystemRoutes(svr, ctx);
    registerRelayRoutes(svr, ctx);
    registerSubAgentRoutes(svr, ctx);
    registerBatchRoutes(svr, ctx);

    // Phase 4: wire SubAgentManager so spawn_subagent can resolve xAI auth
    // + workspace + default model from the shared config. Must happen after
    // mergeServeDiskIntoConfig() above.
    SubAgentManager::instance().setServeConfig(&config_);

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

    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        if (res.status < 400) return;
        if (req.path == "/favicon.ico") return;
        if (req.path.rfind("/ui/", 0) == 0 && res.status == 404) return;
        const std::string& path = req.path;
        std::string msg = req.method + std::string(" ") + path + " -> HTTP " + std::to_string(res.status);
        nlohmann::json details;
        details["status"] = res.status;
        details["method"] = req.method;
        details["path"] = path;
        if (res.status >= 500)
            LogBuffer::instance().error("http", msg, details);
        else if (path.rfind("/api/", 0) == 0 || res.status == 401 || res.status == 403)
            LogBuffer::instance().warn("http", msg, details);
    });
}

void HttpServer::handleChat() {}

} // namespace avacli
