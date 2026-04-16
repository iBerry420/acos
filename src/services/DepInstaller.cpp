#include "services/DepInstaller.hpp"
#include "platform/Hash.hpp"
#include "platform/ProcessRunner.hpp"
#include "platform/Runtime.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <system_error>

namespace avacli {

namespace fs = std::filesystem;

namespace {

// Long timeouts: pip/npm can genuinely take a while on fresh installs. These
// are caps, not expectations. The user's alternative is to pre-install deps
// and set auto_install: false.
constexpr int kVenvCreateTimeoutSec  = 120;
constexpr int kPipInstallTimeoutSec  = 600;
constexpr int kNpmInstallTimeoutSec  = 900;

/// Quote a path for shell use (POSIX: single quotes with escape; Windows:
/// double quotes). `runShellCommand` routes through `/bin/sh -c` on POSIX
/// and `cmd.exe /c` on Windows, so quoting conventions must match.
std::string shellQuote(const std::string& p) {
#if defined(_WIN32)
    // cmd.exe: wrap in double quotes; embedded quotes are rare in our paths.
    return "\"" + p + "\"";
#else
    // sh -c: wrap in single quotes; embedded single quotes need ''\''' dance.
    std::string out = "'";
    for (char c : p) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}

#if defined(_WIN32)
constexpr const char* kVenvPython = "Scripts\\python.exe";
constexpr const char* kVenvPip    = "Scripts\\pip.exe";
#else
constexpr const char* kVenvPython = "bin/python";
constexpr const char* kVenvPip    = "bin/pip";
#endif

} // namespace

std::string DepInstaller::venvPython(const std::string& serviceRoot) {
    if (serviceRoot.empty()) return {};
    fs::path p = fs::path(serviceRoot) / "venv" / kVenvPython;
    std::error_code ec;
    if (fs::exists(p, ec)) return p.string();
    return {};
}

DepInstaller::Result DepInstaller::ensurePython(const std::string& serviceRoot,
                                                 const std::string& workdir,
                                                 const std::string& requirementsFile,
                                                 bool autoInstall,
                                                 const std::string& priorHash,
                                                 const LogFn& log) {
    Result r;
    if (!autoInstall) { r.ok = true; return r; }
    if (serviceRoot.empty() || workdir.empty()) {
        r.message = "dep install skipped: no workdir";
        r.ok = true;
        return r;
    }

    // Locate the manifest. If it doesn't exist, treat as "no deps" -- the
    // service spawns directly from the global python.
    std::string req = requirementsFile.empty() ? std::string("requirements.txt") : requirementsFile;
    fs::path reqPath(req);
    if (reqPath.is_relative()) reqPath = fs::path(workdir) / req;

    std::error_code ec;
    if (!fs::exists(reqPath, ec)) {
        r.ok = true;
        r.message = "no requirements.txt -- skipping install";
        return r;
    }

    // Hash the manifest; skip install if unchanged AND a venv already exists.
    std::string hash = sha256File(reqPath.string());
    std::string venvPy = venvPython(serviceRoot);
    if (!hash.empty() && hash == priorHash && !venvPy.empty()) {
        r.ok = true;
        r.message = "deps unchanged -- using cached venv";
        return r;
    }

    const auto& rt = detectRuntimes();
    if (!rt.pythonOk) {
        r.message = "python runtime not available -- cannot create venv";
        if (log) log("error", r.message);
        return r;
    }

    // Create the venv if missing. We shell out rather than using
    // ProcessSupervisor because this is one-shot synchronous work with a
    // long timeout, which is exactly what runShellCommand is designed for.
    fs::path venvDir = fs::path(serviceRoot) / "venv";
    if (!fs::exists(venvDir, ec)) {
        if (log) log("info", "creating python venv at " + venvDir.string());
        std::string cmd = rt.python + " -m venv " + shellQuote(venvDir.string());
        auto res = runShellCommand(serviceRoot, cmd, kVenvCreateTimeoutSec);
        if (res.exitCode != 0) {
            r.message = "venv creation failed (exit=" + std::to_string(res.exitCode) + ")";
            if (log) {
                log("error", r.message);
                if (!res.capturedOutput.empty())
                    log("error", res.capturedOutput.substr(0, 2048));
            }
            return r;
        }
        venvPy = venvPython(serviceRoot);
    }

    // Install. We use the venv's pip binary directly; this is more robust
    // than `venvPython -m pip` on Windows where the shim can confuse PATH
    // resolution inside nested sh/cmd.exe layers.
    fs::path pipBin = fs::path(serviceRoot) / "venv" / kVenvPip;
    if (!fs::exists(pipBin, ec)) {
        r.message = "venv pip not found at " + pipBin.string();
        if (log) log("error", r.message);
        return r;
    }
    if (log) log("info", "pip install -r " + reqPath.string());
    std::string installCmd = shellQuote(pipBin.string()) +
                              " install --disable-pip-version-check --no-input -r " +
                              shellQuote(reqPath.string());
    auto res = runShellCommand(workdir, installCmd, kPipInstallTimeoutSec);

    // Pipe captured output to service_logs in chunks -- users need to see
    // which package failed.
    if (!res.capturedOutput.empty() && log) {
        const std::string& buf = res.capturedOutput;
        size_t start = 0;
        while (start < buf.size()) {
            size_t nl = buf.find('\n', start);
            std::string line = (nl == std::string::npos)
                ? buf.substr(start)
                : buf.substr(start, nl - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) log(res.exitCode == 0 ? "info" : "error", "pip: " + line);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }

    if (res.exitCode != 0) {
        r.message = "pip install failed (exit=" + std::to_string(res.exitCode) + ")";
        if (log) log("error", r.message);
        return r;
    }

    r.ok      = true;
    r.ran     = true;
    r.newHash = hash;
    r.message = "deps installed";
    if (log) log("info", r.message);
    return r;
}

DepInstaller::Result DepInstaller::ensureNode(const std::string& serviceRoot,
                                               const std::string& workdir,
                                               bool autoInstall,
                                               const std::string& priorHash,
                                               const LogFn& log) {
    Result r;
    if (!autoInstall) { r.ok = true; return r; }
    if (serviceRoot.empty() || workdir.empty()) {
        r.message = "dep install skipped: no workdir";
        r.ok = true;
        return r;
    }

    fs::path pkgJson  = fs::path(workdir) / "package.json";
    fs::path lockJson = fs::path(workdir) / "package-lock.json";

    std::error_code ec;
    if (!fs::exists(pkgJson, ec)) {
        r.ok = true;
        r.message = "no package.json -- skipping npm install";
        return r;
    }

    // Hash package.json + lockfile together so either change triggers a
    // reinstall.
    std::string hash = sha256Files({pkgJson.string(), lockJson.string()});
    fs::path nodeModules = fs::path(workdir) / "node_modules";
    if (!hash.empty() && hash == priorHash && fs::exists(nodeModules, ec)) {
        r.ok = true;
        r.message = "deps unchanged -- using cached node_modules";
        return r;
    }

    const auto& rt = detectRuntimes();
    if (!rt.npmOk) {
        r.message = "npm runtime not available -- cannot install node deps";
        if (log) log("error", r.message);
        return r;
    }

    const bool useCi = fs::exists(lockJson, ec);
    std::string cmd;
#if defined(_WIN32)
    cmd = std::string("cmd.exe /c npm ") + (useCi ? "ci" : "install");
#else
    cmd = std::string("npm ") + (useCi ? "ci" : "install");
#endif
    if (log) log("info", cmd + " (cwd=" + workdir + ")");

    auto res = runShellCommand(workdir, cmd, kNpmInstallTimeoutSec);

    if (!res.capturedOutput.empty() && log) {
        const std::string& buf = res.capturedOutput;
        size_t start = 0;
        while (start < buf.size()) {
            size_t nl = buf.find('\n', start);
            std::string line = (nl == std::string::npos)
                ? buf.substr(start)
                : buf.substr(start, nl - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) log(res.exitCode == 0 ? "info" : "error", "npm: " + line);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }

    if (res.exitCode != 0) {
        r.message = "npm install failed (exit=" + std::to_string(res.exitCode) + ")";
        if (log) log("error", r.message);
        return r;
    }

    r.ok      = true;
    r.ran     = true;
    r.newHash = hash;
    r.message = "deps installed";
    if (log) log("info", r.message);
    return r;
}

} // namespace avacli
