#pragma once

#include "config/Config.hpp"
#include "core/AgentEngine.hpp"
#include "core/TokenTracker.hpp"
#include "core/Types.hpp"
#include "session/SessionManager.hpp"
#include "tools/ToolExecutor.hpp"
#include "tools/ToolRegistry.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace avacli {

class XAIClient;

class Application {
public:
    explicit Application(Config config);
    ~Application() = default;

    int run();

private:
    int  runHeadless();
    int  runServe();
    void saveSession();

    Config config_;
    SessionManager sessionMgr_;
    SessionData sessionData_;
    TokenTracker tokenTracker_;
    std::atomic<bool> streamCancel_{false};
    std::unique_ptr<ToolExecutor> toolExecutor_;
    std::unique_ptr<AgentEngine> agentEngine_;
};

} // namespace avacli
