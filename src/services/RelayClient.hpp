#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace avacli {

class RelayClient {
public:
    static RelayClient& instance();

    void start(const std::string& serverUrl, const std::string& authToken, int localPort);
    void stop();
    bool running() const { return running_; }
    bool connected() const { return connected_; }

private:
    RelayClient() = default;
    ~RelayClient();
    RelayClient(const RelayClient&) = delete;
    RelayClient& operator=(const RelayClient&) = delete;

    void runLoop();
    bool registerWithServer();
    void pollAndProcess();
    void processChat(const std::string& requestId, const std::string& message,
                     const std::string& model, const std::string& session);

    std::string serverUrl_;
    std::string authToken_;
    int localPort_ = 0;
    std::string clientId_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> stopFlag_{false};
};

} // namespace avacli
