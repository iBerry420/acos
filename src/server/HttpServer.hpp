#pragma once
#include "auth/MasterKeyManager.hpp"
#include "core/Types.hpp"
#include <functional>
#include <memory>
#include <string>
#include <atomic>

namespace avacli {

struct ServeConfig {
    int port = 8080;
    // True when --port was passed explicitly on the command line (or the
    // caller otherwise wants the port treated as a hard requirement).
    // When set, HttpServer::start() will fail fast if the port is already
    // in use instead of silently falling back to port+1 — upstream
    // proxies (Apache/nginx/systemd sockets) always expect the configured
    // port, so a fallback corrupts the deployment contract.
    bool portExplicit = false;
    std::string host = "0.0.0.0";
    std::string model = "grok-4-1-reasoning";
    std::string workspace = ".";
    std::string session;
    std::string apiKey;
    std::string chatEndpointUrl = "https://api.x.ai/v1/chat/completions";
    std::string extraModel;
    std::string mediaModel;

    // Node role: "standalone" (default), "server" (accept relay clients), "client" (connect to server)
    std::string nodeRole = "standalone";
    std::string relayServer;        // URL of the relay server (client mode), e.g. "https://devacos.avalynn.ai"
    std::string relayToken;         // Auth token for relay connection

    // Detached UI options
    std::string uiDir;              // Custom UI directory (empty = use default ~/.avacli/ui/)
    bool useEmbeddedUI = false;     // Force compiled-in UI, ignore disk files
    std::string uiTheme;            // Active theme name (empty = "default")
};

class HttpServer {
public:
    explicit HttpServer(ServeConfig config);
    ~HttpServer();

    // Returns true on clean shutdown, false if the server failed to bind
    // (so the caller can propagate a non-zero exit status to the service
    // manager and let it retry rather than running on the wrong port).
    bool start();
    void stop();

private:
    void setupRoutes();
    void handleChat();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    ServeConfig config_;
    MasterKeyManager masterKeyMgr_;
    int actualPort_{0};
    std::atomic<bool> running_{false};
};

} // namespace avacli
