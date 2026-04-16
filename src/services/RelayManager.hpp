#pragma once

#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

namespace avacli {

struct RelayClientInfo {
    std::string id;
    std::string label;
    std::string version;
    std::string workspace;
    int64_t connectedAt = 0;
    int64_t lastSeen = 0;
    bool connected = false;
};

struct RelayRequest {
    std::string requestId;
    std::string clientId;
    std::string type;           // "chat", "api"
    nlohmann::json payload;

    // Response streaming: the originating handler writes chunks here,
    // and the waiting SSE handler reads them.
    struct ResponseQueue {
        std::mutex mu;
        std::condition_variable cv;
        std::queue<std::string> chunks;
        bool done = false;
    };
    std::shared_ptr<ResponseQueue> responseQueue = std::make_shared<ResponseQueue>();
};

class RelayManager {
public:
    static RelayManager& instance();

    // Server-side: register/unregister relay clients
    std::string registerClient(const std::string& label, const std::string& version,
                               const std::string& workspace, const std::string& authToken);
    void disconnectClient(const std::string& clientId);
    void heartbeat(const std::string& clientId);
    std::vector<RelayClientInfo> getClients() const;
    bool isRelayClient(const std::string& nodeId) const;

    // Server-side: queue a request for a relay client (returns nullptr if client not connected)
    std::shared_ptr<RelayRequest> queueChatRequest(const std::string& clientId,
                                                    const std::string& message,
                                                    const std::string& model,
                                                    const std::string& session);

    // Client-side poll: blocks until a request is available or timeout
    std::shared_ptr<RelayRequest> pollRequest(const std::string& clientId, int timeoutSec = 30);

    // Client-side: push response chunk back for a request
    void pushResponseChunk(const std::string& requestId, const std::string& chunk);
    void finishResponse(const std::string& requestId);

    // Auth token validation
    void setAcceptToken(const std::string& token);
    bool validateToken(const std::string& token) const;

private:
    RelayManager() = default;
    ~RelayManager() = default;
    RelayManager(const RelayManager&) = delete;
    RelayManager& operator=(const RelayManager&) = delete;

    mutable std::mutex mu_;
    std::string acceptToken_;
    std::unordered_map<std::string, RelayClientInfo> clients_;
    // pending requests per client
    std::unordered_map<std::string, std::queue<std::shared_ptr<RelayRequest>>> pendingRequests_;
    std::condition_variable pollCv_;
    // active requests by request ID (for response routing)
    std::unordered_map<std::string, std::shared_ptr<RelayRequest>> activeRequests_;
};

} // namespace avacli
