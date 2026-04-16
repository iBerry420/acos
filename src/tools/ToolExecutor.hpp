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

    virtual ~ToolExecutor() = default;

    /// Execute tool by name with JSON arguments. Returns result or error message.
    /// Virtual so ScopedToolExecutor (sub-agents, Phase 4) can gate on
    /// allowed_paths / allowed_tools and wrap writes in leases.
    virtual std::string execute(const std::string& name, const std::string& arguments);

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

    /// Sub-agent context (Phase 4). Root chat calls leave these at defaults
    /// (empty task id, depth 0); ScopedToolExecutor sets them at construction
    /// so the `spawn_subagent` dispatch can correctly set `parent_task_id`
    /// and `depth+1` on children.
    void setSubAgentContext(std::string taskId, int depth) {
        subAgentTaskId_ = std::move(taskId);
        subAgentDepth_ = depth;
    }
    const std::string& subAgentTaskId() const { return subAgentTaskId_; }
    int subAgentDepth() const { return subAgentDepth_; }

    const std::string& workspace() const { return workspace_; }
    Mode mode() const { return mode_; }

protected:
    std::string workspace_;
    Mode mode_;
    AskUserFn askUser_;
    ModeChangeFn onModeChange_;
    std::string sessionId_;
    std::vector<EditRecord> editHistory_;
    std::string searchModel_;
    std::string vaultPassword_;
    std::string subAgentTaskId_;
    int subAgentDepth_ = 0;
};

} // namespace avacli
