#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace avacli {

/// Specification for a long-lived child process.
///
/// Complements the short-lived synchronous `runShellCommand` in ProcessRunner —
/// this is for processes that stay up (bots, daemons, language runtimes under
/// a supervisor).
struct SpawnSpec {
    std::string executable;                         // absolute path, or resolvable via PATH
    std::vector<std::string> args;                  // argv starting after argv[0]
    std::string workingDir;                         // must exist; empty = current dir
    std::map<std::string, std::string> env;         // overlays parent env; values UTF-8
    bool inheritEnv = true;                         // if true, merge parent env first
    bool mergeStderr = true;                        // if true, stderr is read alongside stdout
    bool newProcessGroup = true;                    // POSIX: setpgid(0,0); Win32: CREATE_NEW_PROCESS_GROUP + Job Object
};

/// Handle to a running child.
///
/// All methods are thread-safe. Drains of `readAvailable` can happen from a
/// dedicated reader thread while another thread calls `alive()`, `sendTerm()`,
/// or `kill()`.
class ChildProcess {
public:
    virtual ~ChildProcess() = default;

    /// True while the child has not been reaped.
    virtual bool alive() = 0;

    /// Exit code. Only valid once `alive()` has returned false.
    /// Returns -1 while the child is still running, or if reaping failed.
    virtual int exitCode() const = 0;

    /// Polite termination: SIGTERM on POSIX, CTRL_BREAK_EVENT on Windows.
    /// Targets the whole process group when `newProcessGroup = true`.
    virtual void sendTerm() = 0;

    /// Force kill: SIGKILL on POSIX, TerminateJobObject / TerminateProcess on Windows.
    virtual void kill() = 0;

    /// OS pid (POSIX) or PID (Windows).
    virtual long pid() const = 0;

    /// Non-blocking: append up to `maxBytes` of merged stdout/stderr into `out`.
    /// Returns the number of bytes appended. 0 means "nothing available right now"
    /// (transient) OR "pipe closed" — check `alive()` to disambiguate.
    virtual size_t readAvailable(std::string& out, size_t maxBytes = 4096) = 0;
};

/// Spawn a child per `spec`. Throws on failure (exec error, bad workdir, etc.).
///
/// The child does not inherit stdin from the parent — stdin is redirected to
/// /dev/null (POSIX) or NUL (Windows). If this is ever a problem (a bot wanting
/// interactive input), we'll add `SpawnSpec::stdinMode` later.
std::unique_ptr<ChildProcess> spawnChild(const SpawnSpec& spec);

} // namespace avacli
