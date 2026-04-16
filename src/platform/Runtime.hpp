#pragma once

#include <string>
#include <vector>

namespace avacli {

/// Cached cross-platform lookup of language runtimes used by `type:"process"` services.
///
/// All fields are command strings ready to pass to `runShellCommand` or to
/// `ProcessSupervisor::spawnChild`. On Windows, `.cmd` shims (e.g. npm.cmd)
/// cannot be exec'd directly — callers must invoke them via the `npmInvocation`
/// helper below which handles the wrapping.
struct RuntimePaths {
    std::string python;   // e.g. "python3", "py", "/usr/local/bin/python3"
    std::string node;     // e.g. "node", "node.exe"
    std::string npm;      // e.g. "npm", "npm.cmd" (Windows)
    bool        pythonOk = false;
    bool        nodeOk   = false;
    bool        npmOk    = false;
};

/// Returns the cached RuntimePaths, detecting once on first call.
/// Respects env overrides: AVALYNNAI_PYTHON, AVALYNNAI_NODE, AVALYNNAI_NPM.
const RuntimePaths& detectRuntimes();

/// Re-run detection, replacing the cached result. Call this after the user
/// installs a missing runtime so subsequent spawns pick it up without a
/// server restart.
void refreshRuntimes();

/// Build an argv list for a Python service entrypoint.
///
/// Python is straightforward on every platform — `python3 script.py arg1 arg2`.
/// Returned pair is (`executable`, `args`) suitable for `SpawnSpec`.
struct RuntimeInvocation {
    std::string executable;
    std::vector<std::string> args;
};

/// Build `python <entrypoint> <extra args...>` honouring the detected python.
/// Returns empty executable if python isn't available.
RuntimeInvocation pythonInvocation(const std::string& entrypoint,
                                   const std::vector<std::string>& extraArgs);

/// Build `node <entrypoint> <extra args...>` honouring the detected node.
RuntimeInvocation nodeInvocation(const std::string& entrypoint,
                                 const std::vector<std::string>& extraArgs);

/// Build the argv to invoke npm with the given sub-command.
/// On POSIX: `npm <args...>`. On Windows: `cmd.exe /c npm.cmd <args...>` —
/// because `npm.cmd` is a batch shim and CreateProcess cannot exec it directly.
RuntimeInvocation npmInvocation(const std::vector<std::string>& npmArgs);

} // namespace avacli
