#pragma once

#include <functional>
#include <string>

namespace avacli {

/// Per-service dependency bootstrapping for `type:"process"` services.
///
/// - Python: creates `<serviceRoot>/venv` on first use and installs
///   `requirements.txt` (if present in the resolved workdir) via the venv's
///   `pip`. Subsequent starts are skipped when the manifest hash is unchanged.
/// - Node: runs `npm ci` when a `package-lock.json` is present, `npm install`
///   otherwise, against the resolved workdir.
///
/// All operations are cross-platform (Windows uses `python.exe`/`pip.exe`
/// under `venv\Scripts\` and the `cmd.exe /c npm` wrapper from Runtime.hpp).
class DepInstaller {
public:
    struct Result {
        bool ok = false;         // install succeeded or was a no-op
        bool ran = false;        // true if an install step actually executed
        std::string message;     // human-readable summary (for service_logs)
        std::string newHash;     // empty when unchanged; otherwise to be stored
    };

    /// Log callback: called with (level, message) for each step. `level` is
    /// one of "info", "warn", "error" -- matches the service_logs vocabulary.
    using LogFn = std::function<void(const std::string& level, const std::string& message)>;

    /// Bootstrap Python deps for a service.
    ///
    /// - `serviceRoot`: absolute path to `<workdir_root>/<id>`.
    /// - `workdir`: absolute resolved working directory (== serviceRoot unless
    ///   the service config overrides `working_dir`).
    /// - `requirementsFile`: path relative to `workdir` (default
    ///   "requirements.txt"). If it doesn't exist, the call is a no-op.
    /// - `autoInstall`: if false, the function immediately returns ok=true
    ///   and does nothing.
    /// - `priorHash`: the last-known `_deps_hash` stored in services.config.
    ///   If it matches the current manifest digest, install is skipped and
    ///   `newHash` in the result is empty.
    static Result ensurePython(const std::string& serviceRoot,
                                const std::string& workdir,
                                const std::string& requirementsFile,
                                bool autoInstall,
                                const std::string& priorHash,
                                const LogFn& log);

    /// Bootstrap Node deps for a service. Prefers `npm ci` when
    /// `package-lock.json` is present, falls back to `npm install`.
    static Result ensureNode(const std::string& serviceRoot,
                              const std::string& workdir,
                              bool autoInstall,
                              const std::string& priorHash,
                              const LogFn& log);

    /// Returns the absolute path to the venv python for this service, or
    /// empty if no venv exists yet. Safe to call regardless of platform.
    static std::string venvPython(const std::string& serviceRoot);
};

} // namespace avacli
