#pragma once

#include "tools/ToolExecutor.hpp"
#include <string>
#include <unordered_set>
#include <vector>

namespace avacli {

/// Sub-agent wrapper around ToolExecutor (Phase 4).
///
/// Policies applied BEFORE delegating to the base ToolExecutor:
///
///   1. allowed_tools  — if non-empty, reject tool names not in the set with
///                       `{"error":"tool_not_allowed", ...}`. Empty = no
///                       restriction (mode-based gating still applies).
///
///   2. allowed_paths  — if non-empty, write-path tools (write_file,
///                       edit_file, insert_line, append_file, delete_file)
///                       are rejected unless the target canonical path is
///                       under at least one allowed prefix. Empty = full
///                       workspace (no path restriction).
///
///   3. leases         — write-path tools acquire a lease on
///                       `path:<canonical>` before executing; sibling tasks
///                       writing the same file get
///                       `{"error":"resource_locked","holder":...}`. Leases
///                       auto-release on tool completion AND on task exit
///                       (belt + suspenders).
///
/// Reads are never leased or path-gated — only writes. This matches the
/// intent: parallel reads are fine, parallel writes to the same file are
/// what we need to serialise.
class ScopedToolExecutor : public ToolExecutor {
public:
    struct Scope {
        std::string taskId;            // agent_tasks.id — lease owner
        int depth = 0;                 // this task's depth; children = depth+1
        std::vector<std::string> allowedPaths;  // canonical abs prefixes (empty = no restriction)
        std::vector<std::string> allowedTools;  // tool names (empty = no restriction)
        int writeLeaseTtlSeconds = 300;         // how long to hold a write lease
    };

    ScopedToolExecutor(const std::string& workspace,
                       Mode mode,
                       Scope scope,
                       AskUserFn askUser = nullptr);

    std::string execute(const std::string& name, const std::string& arguments) override;

    const Scope& scope() const { return scope_; }

    /// Which tool names are treated as writes (lease + path-gate).
    /// Exposed for SubAgentManager diagnostics / tests.
    static const std::unordered_set<std::string>& writeToolNames();

private:
    Scope scope_;
};

} // namespace avacli
