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
    std::string host = "0.0.0.0";
    std::string model = "grok-4-1-reasoning";
    std::string workspace = ".";
    std::string session;
    std::string apiKey;
    std::string chatEndpointUrl = "https://api.x.ai/v1/chat/completions";
    std::string extraModel;

    // Detached UI options
    std::string uiDir;              // Custom UI directory (empty = use default ~/.avacli/ui/)
    bool useEmbeddedUI = false;     // Force compiled-in UI, ignore disk files
    std::string uiTheme;            // Active theme name (empty = "default")
};

class HttpServer {
public:
    explicit HttpServer(ServeConfig config);
    ~HttpServer();

    void start();
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
