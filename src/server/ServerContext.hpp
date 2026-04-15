#pragma once
#include "server/HttpServer.hpp"
#include "auth/MasterKeyManager.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

namespace avacli {

struct AskUserState {
    std::mutex mu;
    std::condition_variable cv;
    bool pending{false};
    std::string question;
    std::string response;
    bool responded{false};
};

struct ServerContext {
    ServeConfig* config{nullptr};
    MasterKeyManager* masterKeyMgr{nullptr};
    std::atomic<bool>* chatCancelFlag{nullptr};
    std::atomic<bool>* chatBusyFlag{nullptr};
    std::atomic<size_t>* sessionPromptTokens{nullptr};
    std::atomic<size_t>* sessionCompletionTokens{nullptr};
    int* actualPort{nullptr};
    AskUserState* askUserState{nullptr};
};

} // namespace avacli
