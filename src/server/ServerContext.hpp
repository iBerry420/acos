#pragma once
#include "server/HttpServer.hpp"
#include "auth/MasterKeyManager.hpp"
#include <atomic>

namespace avacli {

struct ServerContext {
    ServeConfig* config{nullptr};
    MasterKeyManager* masterKeyMgr{nullptr};
    std::atomic<bool>* chatCancelFlag{nullptr};
    std::atomic<size_t>* sessionPromptTokens{nullptr};
    std::atomic<size_t>* sessionCompletionTokens{nullptr};
    int* actualPort{nullptr};
};

} // namespace avacli
