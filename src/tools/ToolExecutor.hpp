#pragma once

#include "core/Types.hpp"
#include <functional>
#include <string>

namespace avacli {

/// Executes tools by name with JSON arguments.
/// Returns result as string (typically JSON or plain text).
class ToolExecutor {
public:
    using AskUserFn = std::function<std::string(const std::string& question)>;
    using ModeChangeFn = std::function<void(Mode)>;

    explicit ToolExecutor(const std::string& workspace,
                         Mode mode,
                         AskUserFn askUser = nullptr);

    /// Execute tool by name with JSON arguments. Returns result or error message.
    std::string execute(const std::string& name, const std::string& arguments);

    void setAskUser(AskUserFn fn) { askUser_ = std::move(fn); }
    void setModeChangeCallback(ModeChangeFn fn) { onModeChange_ = std::move(fn); }
    void setMode(Mode m) { mode_ = m; }
    void setSessionId(const std::string& s) { sessionId_ = s; }
    const std::string& sessionId() const { return sessionId_; }
    const std::vector<EditRecord>& editHistory() const { return editHistory_; }

    /// Model id for xAI Responses API live web / X search (usually same as chat model).
    void setSearchModel(std::string model) { searchModel_ = std::move(model); }

    /// Set the vault password for the session (cached in memory only).
    void setVaultPassword(std::string pw) { vaultPassword_ = std::move(pw); }
    const std::string& vaultPassword() const { return vaultPassword_; }

private:
    std::string workspace_;
    Mode mode_;
    AskUserFn askUser_;
    ModeChangeFn onModeChange_;
    std::string sessionId_;
    std::vector<EditRecord> editHistory_;
    std::string searchModel_;
    std::string vaultPassword_;
};

} // namespace avacli
