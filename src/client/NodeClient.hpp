#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <functional>

namespace avacli {

struct NodeInfo {
    std::string id;
    std::string ip;
    int port = 8080;
    std::string domain;
    std::string label;
    std::string username;
    std::string password;
    std::string status;         // connected, unreachable, auth_failed, not_avacli
    int latencyMs = -1;
    std::string version;
    std::string workspace;
};

struct NodeHealthResult {
    bool reachable = false;
    bool isAvacli = false;
    bool authOk = true;
    int latencyMs = -1;
    std::string version;
    std::string workspace;
    std::string error;
};

class NodeClient {
public:
    explicit NodeClient(const NodeInfo& node);

    NodeHealthResult checkHealth();

    nlohmann::json proxyGet(const std::string& path);
    nlohmann::json proxyPost(const std::string& path, const nlohmann::json& body);

    using SseCallback = std::function<void(const std::string& data)>;
    bool proxyChatStream(const std::string& message, const std::string& model,
                         const std::string& session, SseCallback onData,
                         std::atomic<bool>* cancelFlag);

    std::string proxyRawGet(const std::string& path, std::string& contentType);

private:
    struct HttpResult {
        long status = 0;
        std::string body;
        std::string contentType;
        bool ok() const { return status >= 200 && status < 300; }
    };

    HttpResult get(const std::string& path, int timeoutSec = 10);
    HttpResult post(const std::string& path, const std::string& body, int timeoutSec = 30);

    std::string baseUrl() const;
    std::string authToken() const;

    NodeInfo node_;
};

} // namespace avacli
