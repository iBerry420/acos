#include "services/RelayManager.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <random>

namespace avacli {

static int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string genId(const std::string& prefix) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    static const char* hx = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string id = prefix + std::to_string(ms) + "_";
    for (int i = 0; i < 6; ++i) id += hx[dist(gen)];
    return id;
}

RelayManager& RelayManager::instance() {
    static RelayManager rm;
    return rm;
}

void RelayManager::setAcceptToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    acceptToken_ = token;
}

bool RelayManager::validateToken(const std::string& token) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (acceptToken_.empty()) return true;
    return token == acceptToken_;
}

std::string RelayManager::registerClient(const std::string& label, const std::string& version,
                                          const std::string& workspace, const std::string& authToken) {
    std::lock_guard<std::mutex> lock(mu_);

    // Check if this client already exists (reconnect by label)
    for (auto& [id, ci] : clients_) {
        if (ci.label == label) {
            ci.connected = true;
            ci.version = version;
            ci.workspace = workspace;
            ci.lastSeen = nowSec();
            spdlog::info("Relay client reconnected: {} ({})", label, id);
            return id;
        }
    }

    std::string id = genId("rc_");
    RelayClientInfo ci;
    ci.id = id;
    ci.label = label;
    ci.version = version;
    ci.workspace = workspace;
    ci.connectedAt = nowSec();
    ci.lastSeen = nowSec();
    ci.connected = true;
    clients_[id] = ci;

    spdlog::info("Relay client registered: {} ({})", label, id);
    return id;
}

void RelayManager::disconnectClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        it->second.connected = false;
        spdlog::info("Relay client disconnected: {}", it->second.label);
    }
}

void RelayManager::heartbeat(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = clients_.find(clientId);
    if (it != clients_.end()) {
        it->second.lastSeen = nowSec();
        it->second.connected = true;
    }
}

std::vector<RelayClientInfo> RelayManager::getClients() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<RelayClientInfo> result;
    auto now = nowSec();
    for (const auto& [id, ci] : clients_) {
        auto copy = ci;
        if (copy.connected && (now - copy.lastSeen) > 90)
            copy.connected = false;
        result.push_back(copy);
    }
    return result;
}

bool RelayManager::isRelayClient(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(mu_);
    return clients_.count(nodeId) > 0;
}

std::shared_ptr<RelayRequest> RelayManager::queueChatRequest(
    const std::string& clientId, const std::string& message,
    const std::string& model, const std::string& session) {

    std::lock_guard<std::mutex> lock(mu_);

    auto cit = clients_.find(clientId);
    if (cit == clients_.end() || !cit->second.connected)
        return nullptr;

    auto req = std::make_shared<RelayRequest>();
    req->requestId = genId("rr_");
    req->clientId = clientId;
    req->type = "chat";
    req->payload = {{"message", message}, {"model", model}, {"session", session}};

    pendingRequests_[clientId].push(req);
    activeRequests_[req->requestId] = req;
    pollCv_.notify_all();

    spdlog::info("Relay chat request queued: {} → {}", req->requestId, cit->second.label);
    return req;
}

std::shared_ptr<RelayRequest> RelayManager::pollRequest(const std::string& clientId, int timeoutSec) {
    std::unique_lock<std::mutex> lock(mu_);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);

    pollCv_.wait_until(lock, deadline, [&] {
        auto it = pendingRequests_.find(clientId);
        return it != pendingRequests_.end() && !it->second.empty();
    });

    auto it = pendingRequests_.find(clientId);
    if (it == pendingRequests_.end() || it->second.empty())
        return nullptr;

    auto req = it->second.front();
    it->second.pop();
    return req;
}

void RelayManager::pushResponseChunk(const std::string& requestId, const std::string& chunk) {
    std::shared_ptr<RelayRequest> req;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = activeRequests_.find(requestId);
        if (it == activeRequests_.end()) return;
        req = it->second;
    }
    {
        std::lock_guard<std::mutex> lock(req->responseQueue->mu);
        req->responseQueue->chunks.push(chunk);
        req->responseQueue->cv.notify_one();
    }
}

void RelayManager::finishResponse(const std::string& requestId) {
    std::shared_ptr<RelayRequest> req;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = activeRequests_.find(requestId);
        if (it == activeRequests_.end()) return;
        req = it->second;
        activeRequests_.erase(it);
    }
    {
        std::lock_guard<std::mutex> lock(req->responseQueue->mu);
        req->responseQueue->done = true;
        req->responseQueue->cv.notify_one();
    }
    spdlog::info("Relay response finished: {}", requestId);
}

} // namespace avacli
