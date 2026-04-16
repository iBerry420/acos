#include "platform/Runtime.hpp"
#include "platform/ProcessRunner.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdlib>
#include <mutex>
#include <optional>

namespace avacli {

namespace {

std::once_flag g_detectOnce;
std::mutex     g_mu;
RuntimePaths   g_cached;

std::string envOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    if (v && *v) return std::string(v);
    return fallback;
}

/// Run `<cmd> --version` and return true if it exits 0.
/// `cmd` may contain spaces (e.g. "py -3"); we pass it through the shell so
/// those cases work on Windows without needing CreateProcess-level argv parsing.
bool versionProbe(const std::string& cmd, const std::string& expectContains) {
    auto r = runShellCommand("", cmd + " --version", 5);
    if (r.exitCode != 0) return false;
    if (expectContains.empty()) return true;
    return r.capturedOutput.find(expectContains) != std::string::npos;
}

std::string detectPython() {
    std::string override_ = envOr("AVALYNNAI_PYTHON", "");
    if (!override_.empty()) {
        if (versionProbe(override_, "Python")) return override_;
        spdlog::warn("AVALYNNAI_PYTHON='{}' did not respond to --version; ignoring", override_);
    }
#if defined(_WIN32)
    const std::array<const char*, 3> candidates = {"py -3", "python3", "python"};
#elif defined(__APPLE__)
    const std::array<const char*, 3> candidates = {"python3", "/usr/local/bin/python3", "python"};
#else
    const std::array<const char*, 2> candidates = {"python3", "python"};
#endif
    for (const auto& c : candidates) {
        if (versionProbe(c, "Python")) return std::string(c);
    }
    return "";
}

std::string detectNode() {
    std::string override_ = envOr("AVALYNNAI_NODE", "");
    if (!override_.empty()) {
        if (versionProbe(override_, "")) return override_;
        spdlog::warn("AVALYNNAI_NODE='{}' did not respond to --version; ignoring", override_);
    }
#if defined(_WIN32)
    const std::array<const char*, 2> candidates = {"node", "node.exe"};
#else
    const std::array<const char*, 2> candidates = {"node", "nodejs"};
#endif
    for (const auto& c : candidates) {
        if (versionProbe(c, "")) return std::string(c);
    }
    return "";
}

std::string detectNpm() {
    std::string override_ = envOr("AVALYNNAI_NPM", "");
    if (!override_.empty()) {
        if (versionProbe(override_, "")) return override_;
        spdlog::warn("AVALYNNAI_NPM='{}' did not respond to --version; ignoring", override_);
    }
#if defined(_WIN32)
    // On Windows, npm ships as both `npm.cmd` (shim) and `npm.ps1`. We probe
    // via cmd.exe so either works, but we record the base name for the
    // invocation helper to wrap appropriately.
    if (versionProbe("npm", "")) return "npm";
    return "";
#else
    if (versionProbe("npm", "")) return "npm";
    return "";
#endif
}

void doDetect() {
    RuntimePaths p{};
    p.python   = detectPython();
    p.node     = detectNode();
    p.npm      = detectNpm();
    p.pythonOk = !p.python.empty();
    p.nodeOk   = !p.node.empty();
    p.npmOk    = !p.npm.empty();

    spdlog::info("Runtime detection: python={} ({}), node={} ({}), npm={} ({})",
                 p.pythonOk ? p.python : "none", p.pythonOk ? "ok" : "missing",
                 p.nodeOk   ? p.node   : "none", p.nodeOk   ? "ok" : "missing",
                 p.npmOk    ? p.npm    : "none", p.npmOk    ? "ok" : "missing");

    std::lock_guard<std::mutex> lock(g_mu);
    g_cached = std::move(p);
}

} // namespace

const RuntimePaths& detectRuntimes() {
    std::call_once(g_detectOnce, doDetect);
    std::lock_guard<std::mutex> lock(g_mu);
    return g_cached;
}

void refreshRuntimes() {
    doDetect();
}

RuntimeInvocation pythonInvocation(const std::string& entrypoint,
                                   const std::vector<std::string>& extraArgs) {
    const auto& r = detectRuntimes();
    RuntimeInvocation out;
    if (!r.pythonOk) return out;

    // `py -3` on Windows has a space. Split on the first space so argv[0] is
    // `py` and argv[1] is `-3` — CreateProcess needs individual args, not a
    // single shell string, when we spawn under ProcessSupervisor.
    std::string exe = r.python;
    std::vector<std::string> preArgs;
    auto sp = exe.find(' ');
    if (sp != std::string::npos) {
        preArgs.push_back(exe.substr(sp + 1));
        exe = exe.substr(0, sp);
    }

    out.executable = exe;
    out.args = preArgs;
    // `-u` for unbuffered stdout — essential for live log tailing.
    out.args.push_back("-u");
    out.args.push_back(entrypoint);
    for (const auto& a : extraArgs) out.args.push_back(a);
    return out;
}

RuntimeInvocation nodeInvocation(const std::string& entrypoint,
                                 const std::vector<std::string>& extraArgs) {
    const auto& r = detectRuntimes();
    RuntimeInvocation out;
    if (!r.nodeOk) return out;

    out.executable = r.node;
    out.args.push_back(entrypoint);
    for (const auto& a : extraArgs) out.args.push_back(a);
    return out;
}

RuntimeInvocation npmInvocation(const std::vector<std::string>& npmArgs) {
    const auto& r = detectRuntimes();
    RuntimeInvocation out;
    if (!r.npmOk) return out;

#if defined(_WIN32)
    // npm on Windows is npm.cmd (batch) — CreateProcess cannot exec .cmd
    // directly, so route through cmd.exe.
    out.executable = "cmd.exe";
    out.args.push_back("/c");
    out.args.push_back("npm");
    for (const auto& a : npmArgs) out.args.push_back(a);
#else
    out.executable = r.npm;
    for (const auto& a : npmArgs) out.args.push_back(a);
#endif
    return out;
}

} // namespace avacli
