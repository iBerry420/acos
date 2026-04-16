#include "services/ServiceManager.hpp"
#include "config/PlatformSettings.hpp"
#include "config/VaultStorage.hpp"
#include "db/Database.hpp"
#include "platform/ProcessSupervisor.hpp"
#include "platform/Runtime.hpp"
#include "server/LogBuffer.hpp"
#include "services/DepInstaller.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <system_error>

namespace avacli {

namespace fs = std::filesystem;

namespace {

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Resolve a `vault:<name>` reference via VaultStorage. Accepts both the
/// default "avacli_default" password and the actual vault password stored
/// elsewhere (we try default first; if the vault has a custom password, the
/// user needs to set the env var literally). Returns empty optional on any
/// failure; the caller logs and skips.
std::optional<std::string> resolveVaultRef(const std::string& name) {
    if (!VaultStorage::exists()) return std::nullopt;
    auto v = VaultStorage::retrieve("avacli_default", name);
    if (v) return v;
    return std::nullopt;
}

/// Canonicalise an env map: values prefixed "vault:" are resolved against the
/// vault; unresolved vault refs are dropped with a warning so the child never
/// sees a literal "vault:xyz" string.
std::map<std::string, std::string>
resolveEnv(const std::string& serviceId,
           const std::map<std::string, std::string>& raw,
           std::vector<std::string>& warnings) {
    std::map<std::string, std::string> out;
    for (const auto& [k, v] : raw) {
        if (v.rfind("vault:", 0) == 0) {
            std::string vname = v.substr(6);
            auto resolved = resolveVaultRef(vname);
            if (resolved) {
                out[k] = *resolved;
            } else {
                warnings.push_back("env[" + k + "] = vault:" + vname +
                                    " — not found in vault; skipping");
            }
        } else {
            out[k] = v;
        }
    }
    (void)serviceId;
    return out;
}

/// Parse the env block from config JSON into a string→string map.
std::map<std::string, std::string> parseEnv(const nlohmann::json& envJson) {
    std::map<std::string, std::string> out;
    if (!envJson.is_object()) return out;
    for (auto it = envJson.begin(); it != envJson.end(); ++it) {
        if (it.value().is_string()) out[it.key()] = it.value().get<std::string>();
        else if (!it.value().is_null()) out[it.key()] = it.value().dump();
    }
    return out;
}

/// Ensure the per-service workdir exists with tightened permissions.
/// Returns the absolute path, or empty string on failure.
std::string ensureServiceWorkdir(const std::string& serviceId) {
    std::string root = platformSettings::servicesWorkdirRoot();
    fs::path dir = fs::path(root) / serviceId;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        spdlog::error("Failed to create service workdir {}: {}", dir.string(), ec.message());
        return "";
    }
#if !defined(_WIN32)
    fs::permissions(dir, fs::perms::owner_all, fs::perm_options::replace, ec);
    // Non-fatal if perms can't be set.
#endif
    return dir.string();
}

struct ProcessConfig {
    std::string runtime;        // "python" | "node" | "bin"
    std::string entrypoint;     // e.g. "bot.py"
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::string workingDir;     // relative to workdir root, or absolute

    // Dependency installation (phase 2). `auto_install` is on by default for
    // python/node services: if a manifest is present in the workdir and its
    // content hash has changed, we rebuild the venv / node_modules once at
    // start time.
    bool depsAutoInstall = true;
    std::string requirementsFile = "requirements.txt";
    // Present-or-absent lockfile for node is detected automatically; no knob.

    // Restart policy
    std::string restartPolicy = "always"; // always | on_failure | never
    int backoffInitialMs = 1000;
    int backoffMaxMs     = 60'000;
    int maxRestartsPerHour = 20;

    int logTailBytes = 65'536;
};

std::vector<std::string> parseStringArray(const nlohmann::json& j) {
    std::vector<std::string> out;
    if (!j.is_array()) return out;
    for (const auto& e : j) {
        if (e.is_string()) out.push_back(e.get<std::string>());
    }
    return out;
}

ProcessConfig parseProcessConfig(const nlohmann::json& config) {
    ProcessConfig c;
    c.runtime    = config.value("runtime", std::string());
    c.entrypoint = config.value("entrypoint", std::string());
    if (config.contains("args"))
        c.args = parseStringArray(config["args"]);
    if (config.contains("env"))
        c.env = parseEnv(config["env"]);
    c.workingDir = config.value("working_dir", std::string());
    if (config.contains("restart") && config["restart"].is_object()) {
        const auto& r = config["restart"];
        c.restartPolicy        = r.value("policy", c.restartPolicy);
        c.backoffInitialMs     = r.value("backoff_initial_ms", c.backoffInitialMs);
        c.backoffMaxMs         = r.value("backoff_max_ms", c.backoffMaxMs);
        c.maxRestartsPerHour   = r.value("max_restarts_per_hour", c.maxRestartsPerHour);
    }
    if (config.contains("deps") && config["deps"].is_object()) {
        const auto& d = config["deps"];
        c.depsAutoInstall   = d.value("auto_install", c.depsAutoInstall);
        c.requirementsFile  = d.value("requirements_file", c.requirementsFile);
    }
    c.logTailBytes = config.value("log_tail_bytes", c.logTailBytes);
    return c;
}

/// Resolve the runtime invocation for the given process config.
/// Returns a fully-populated SpawnSpec on success, std::nullopt on failure
/// (with a message written to `error`).
///
/// If a per-service venv has been bootstrapped at `<serviceRoot>/venv`, the
/// python runtime is swapped to the venv's python so installed packages are
/// actually on sys.path. `serviceRoot` may be empty for the very first spawn
/// before auto-install runs -- in that case we fall back to the global python.
std::optional<SpawnSpec> buildSpawnSpec(const ProcessConfig& pc,
                                        const std::string& workdir,
                                        const std::string& serviceRoot,
                                        const std::map<std::string, std::string>& resolvedEnv,
                                        std::string& error) {
    SpawnSpec spec;
    spec.workingDir = workdir;
    spec.env        = resolvedEnv;
    spec.inheritEnv = true;
    spec.mergeStderr = true;
    spec.newProcessGroup = true;

    RuntimeInvocation inv;
    if (pc.runtime == "python") {
        std::string venvPy = DepInstaller::venvPython(serviceRoot);
        if (!venvPy.empty()) {
            // Use the service's venv interpreter so pip-installed packages
            // (e.g. discord.py) are importable without touching system python.
            inv.executable = venvPy;
            inv.args.push_back("-u");
            inv.args.push_back(pc.entrypoint);
            for (const auto& a : pc.args) inv.args.push_back(a);
        } else {
            inv = pythonInvocation(pc.entrypoint, pc.args);
            if (inv.executable.empty()) {
                error = "python runtime not found on PATH — install python3 or set AVALYNNAI_PYTHON";
                return std::nullopt;
            }
        }
    } else if (pc.runtime == "node") {
        inv = nodeInvocation(pc.entrypoint, pc.args);
        if (inv.executable.empty()) {
            error = "node runtime not found on PATH — install nodejs or set AVALYNNAI_NODE";
            return std::nullopt;
        }
    } else if (pc.runtime == "bin") {
        if (pc.entrypoint.empty()) {
            error = "runtime=bin requires 'entrypoint' to be an executable path";
            return std::nullopt;
        }
        inv.executable = pc.entrypoint;
        inv.args = pc.args;
    } else {
        error = "unknown runtime '" + pc.runtime + "' (expected python | node | bin)";
        return std::nullopt;
    }

    spec.executable = inv.executable;
    spec.args = inv.args;
    return spec;
}

/// Sliding-window restart counter: at most N restarts per hour.
class RestartWindow {
public:
    explicit RestartWindow(int maxPerHour) : maxPerHour_(maxPerHour) {}

    bool allow(long long tsMs) {
        // Drop entries older than 1 hour.
        const long long cutoff = tsMs - 3'600'000LL;
        while (!events_.empty() && events_.front() < cutoff) events_.pop_front();
        if (static_cast<int>(events_.size()) >= maxPerHour_) return false;
        events_.push_back(tsMs);
        return true;
    }

private:
    int maxPerHour_;
    std::deque<long long> events_;
};

} // namespace

// ── ServiceManager ──────────────────────────────────────────────────────

ServiceManager& ServiceManager::instance() {
    static ServiceManager mgr;
    return mgr;
}

ServiceManager::~ServiceManager() {
    shutdown();
}

ServiceManager::Kind ServiceManager::kindFromType(const std::string& type) const {
    if (type == "process") return Kind::Process;
    return Kind::Tick;
}

void ServiceManager::init() {
    try {
        auto& db = Database::instance();
        auto rows = db.query("SELECT id FROM services WHERE status = 'running'");
        for (const auto& r : rows) {
            std::string id = r["id"].get<std::string>();
            try {
                startService(id);
            } catch (...) {
                spdlog::warn("Failed to auto-start service {}", id);
            }
        }
    } catch (...) {}
}

void ServiceManager::shutdown() {
    shuttingDown_ = true;
    std::map<std::string, std::unique_ptr<RunningService>> toStop;
    {
        std::lock_guard<std::mutex> lock(mu_);
        toStop.swap(running_);
    }
    // Signal all threads to stop, then wait for them. Process threads will
    // SIGTERM their child in response to shouldStop.
    for (auto& [id, svc] : toStop) {
        svc->shouldStop = true;
    }
    for (auto& [id, svc] : toStop) {
        if (svc->thread.joinable())
            svc->thread.join();
    }
    toStop.clear();
}

void ServiceManager::startService(const std::string& id) {
    std::string type;
    nlohmann::json config;
    try {
        auto& db = Database::instance();
        auto row = db.queryOne("SELECT type, config FROM services WHERE id = ?1", {id});
        if (row.is_null()) {
            spdlog::warn("Cannot start service {}: not found in database", id);
            return;
        }
        type = row["type"].get<std::string>();
        if (row["config"].is_string()) {
            try { config = nlohmann::json::parse(row["config"].get<std::string>()); } catch (...) {}
        } else {
            config = row["config"];
        }

        // Per-type config sanity.
        if (type == "scheduled_prompt" && config.value("prompt", "").empty()) {
            logToService(id, "error", "Refused to start: scheduled_prompt requires 'prompt' in config");
            return;
        }
        if (type == "rss_feed" && config.value("url", "").empty()) {
            logToService(id, "error", "Refused to start: rss_feed requires 'url' in config");
            return;
        }
        if (type == "custom_script" && config.value("script", "").empty()) {
            logToService(id, "error", "Refused to start: custom_script requires 'script' in config");
            return;
        }
        if (type == "process") {
            auto pc = parseProcessConfig(config);
            if (pc.runtime.empty() || pc.entrypoint.empty()) {
                logToService(id, "error",
                    "Refused to start: process service requires 'runtime' and 'entrypoint' in config");
                return;
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Cannot validate service {} config: {}", id, e.what());
        return;
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (running_.count(id)) return;

    auto svc = std::make_unique<RunningService>();
    svc->kind = kindFromType(type);
    auto* svcPtr = svc.get();

    if (svc->kind == Kind::Process) {
        svc->thread = std::thread([this, id, svcPtr]() { runProcessLoop(id, *svcPtr); });
    } else {
        svc->thread = std::thread([this, id, svcPtr]() { runTickLoop(id, *svcPtr); });
    }

    running_[id] = std::move(svc);
    logToService(id, "info", "Service started");
}

void ServiceManager::stopService(const std::string& id) {
    std::unique_ptr<RunningService> svc;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = running_.find(id);
        if (it == running_.end()) return;
        svc = std::move(it->second);
        running_.erase(it);
    }
    svc->shouldStop = true;
    if (svc->thread.joinable())
        svc->thread.join();

    // Best-effort DB state clear for process services.
    if (svc->kind == Kind::Process) {
        try {
            Database::instance().execute(
                "UPDATE services SET pid = 0, started_at = 0 WHERE id = ?1", {id});
        } catch (...) {}
    }

    logToService(id, "info", "Service stopped");
}

bool ServiceManager::isRunning(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return running_.count(id) > 0;
}

void ServiceManager::logToService(const std::string& serviceId, const std::string& level,
                                  const std::string& message) {
    try {
        Database::instance().execute(
            "INSERT INTO service_logs (service_id, timestamp, level, message) VALUES (?1, ?2, ?3, ?4)",
            {serviceId, std::to_string(nowMs()), level, message});
    } catch (...) {}

    LogBuffer::instance().info("services", "[" + serviceId.substr(0, 8) + "] " + message);
}

nlohmann::json ServiceManager::processStatus(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = running_.find(id);
    if (it == running_.end() || it->second->kind != Kind::Process) return nullptr;

    auto& svc = *it->second;
    std::lock_guard<std::mutex> plock(svc.procMu);
    nlohmann::json j;
    j["alive"]           = svc.child ? svc.child->alive() : false;
    j["pid"]             = svc.pid;
    j["started_at_ms"]   = svc.startedAtMs;
    j["uptime_ms"]       = svc.startedAtMs > 0 ? (nowMs() - svc.startedAtMs) : 0;
    j["restart_count"]   = svc.restartCount;
    j["last_exit_code"]  = svc.lastExitCode;
    return j;
}

bool ServiceManager::signalProcess(const std::string& id, const std::string& signal) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = running_.find(id);
    if (it == running_.end() || it->second->kind != Kind::Process) return false;

    auto& svc = *it->second;
    std::lock_guard<std::mutex> plock(svc.procMu);
    if (signal == "term") {
        if (svc.child) svc.child->sendTerm();
        return true;
    }
    if (signal == "kill") {
        if (svc.child) svc.child->kill();
        return true;
    }
    if (signal == "restart") {
        // Ask the loop to respawn at the next tick by terminating the child;
        // the loop treats that as a crash and applies its restart policy.
        svc.restartRequested = true;
        if (svc.child) svc.child->sendTerm();
        return true;
    }
    return false;
}

void ServiceManager::persistSupervisorState(const std::string& id, RunningService& svc) {
    try {
        Database::instance().execute(
            "UPDATE services SET pid = ?1, started_at = ?2, restart_count = ?3, last_exit_code = ?4 "
            "WHERE id = ?5",
            {std::to_string(svc.pid),
             std::to_string(svc.startedAtMs),
             std::to_string(svc.restartCount),
             std::to_string(svc.lastExitCode),
             id});
    } catch (...) {}
}

// ── Tick loop (rss_feed / scheduled_prompt / custom_script / legacy) ────

void ServiceManager::runTickLoop(const std::string& id, RunningService& svc) {
    int consecutiveFailures = 0;
    auto& shouldStop = svc.shouldStop;

    while (!shouldStop && !shuttingDown_) {
        try {
            auto& db = Database::instance();
            auto row = db.queryOne("SELECT type, config, next_run FROM services WHERE id = ?1", {id});
            if (row.is_null()) break;

            std::string type = row["type"].get<std::string>();
            nlohmann::json config;
            if (row["config"].is_string()) {
                try { config = nlohmann::json::parse(row["config"].get<std::string>()); } catch (...) {}
            } else {
                config = row["config"];
            }

            int intervalSec = config.value("interval_seconds", 60);
            long long nextRun = row.value("next_run", (long long)0);
            auto now = nowMs();

            if (now >= nextRun) {
                bool tickOk = executeServiceTick(id, config, type);
                long long next = now + (intervalSec * 1000LL);
                db.execute("UPDATE services SET last_run = ?1, next_run = ?2 WHERE id = ?3",
                    {std::to_string(now), std::to_string(next), id});

                if (tickOk) {
                    consecutiveFailures = 0;
                } else {
                    consecutiveFailures++;
                    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                        logToService(id, "error",
                            "Auto-stopped after " + std::to_string(MAX_CONSECUTIVE_FAILURES) +
                            " consecutive failures. Fix the service config and restart.");
                        db.execute("UPDATE services SET status = 'error' WHERE id = ?1", {id});
                        break;
                    }
                }
            }
        } catch (const std::exception& e) {
            logToService(id, "error", std::string("Service error: ") + e.what());
            consecutiveFailures++;
            if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                logToService(id, "error", "Auto-stopped after " +
                    std::to_string(MAX_CONSECUTIVE_FAILURES) + " consecutive exceptions.");
                try {
                    Database::instance().execute("UPDATE services SET status = 'error' WHERE id = ?1", {id});
                } catch (...) {}
                break;
            }
        }

        for (int i = 0; i < 50 && !shouldStop && !shuttingDown_; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool ServiceManager::executeServiceTick(const std::string& id, const nlohmann::json& config,
                                         const std::string& type) {
    if (type == "rss_feed") {
        std::string url = config.value("url", "");
        if (url.empty()) {
            logToService(id, "error", "RSS feed URL not configured -- service cannot run without 'url' in config");
            return false;
        }
        logToService(id, "info", "RSS feed tick: " + url);
        return true;
    } else if (type == "scheduled_prompt") {
        std::string prompt = config.value("prompt", "");
        if (prompt.empty()) {
            logToService(id, "error", "No prompt configured -- service cannot run without 'prompt' in config");
            return false;
        }
        logToService(id, "info", "Scheduled prompt tick");
        return true;
    } else if (type == "custom_script") {
        std::string script = config.value("script", "");
        if (script.empty()) {
            logToService(id, "error", "No script configured -- service cannot run without 'script' in config");
            return false;
        }
        logToService(id, "info", "Custom script tick");
        return true;
    } else {
        logToService(id, "debug", "Service tick (type: " + type + ")");
        return true;
    }
}

// ── Process loop (type:"process" — long-running daemons) ────────────────

void ServiceManager::runProcessLoop(const std::string& id, RunningService& svc) {
    auto& shouldStop = svc.shouldStop;

    // Reload config from DB so hot edits (ALTER of services.config via the UI)
    // take effect on next restart without touching the loop's book-keeping.
    auto loadConfig = [&id]() -> std::pair<bool, ProcessConfig> {
        try {
            auto row = Database::instance().queryOne(
                "SELECT config FROM services WHERE id = ?1", {id});
            if (row.is_null()) return {false, {}};
            nlohmann::json cfg;
            if (row["config"].is_string()) {
                try { cfg = nlohmann::json::parse(row["config"].get<std::string>()); } catch (...) {}
            } else {
                cfg = row["config"];
            }
            return {true, parseProcessConfig(cfg)};
        } catch (...) { return {false, {}}; }
    };

    int currentBackoffMs = 1000;
    RestartWindow restartWindow{20};

    while (!shouldStop && !shuttingDown_) {
        auto [ok, pc] = loadConfig();
        if (!ok) {
            logToService(id, "error", "Could not load config; stopping supervisor");
            break;
        }

        // Apply policy knobs that may have changed between restarts.
        currentBackoffMs = std::max(100, pc.backoffInitialMs);
        // Rebuild window with the latest limit. (Cheap; few events.)
        restartWindow = RestartWindow{pc.maxRestartsPerHour};

        // Prepare workdir. `working_dir` from config is relative to the
        // per-service root by default; absolute paths are honoured.
        std::string serviceRoot = ensureServiceWorkdir(id);
        if (serviceRoot.empty()) {
            logToService(id, "error", "Could not create service workdir");
            break;
        }
        std::string workdir = serviceRoot;
        if (!pc.workingDir.empty()) {
            fs::path p(pc.workingDir);
            if (p.is_absolute()) workdir = p.string();
            else workdir = (fs::path(serviceRoot) / pc.workingDir).string();
        }

        // Resolve vault env refs.
        std::vector<std::string> vaultWarnings;
        auto resolvedEnv = resolveEnv(id, pc.env, vaultWarnings);
        for (const auto& w : vaultWarnings) logToService(id, "warn", w);

        // Auto-install dependencies (idempotent; skipped when hash matches).
        // A failure here is treated like any other startup failure -- we fall
        // through to the restart policy rather than permanently stopping, so
        // transient network hiccups during pip install self-heal.
        auto depLogCb = [this, &id](const std::string& lvl, const std::string& msg) {
            logToService(id, lvl, msg);
        };
        std::string priorDepsHash;
        try {
            auto cfgRow = Database::instance().queryOne(
                "SELECT config FROM services WHERE id = ?1", {id});
            if (!cfgRow.is_null()) {
                nlohmann::json cfg;
                if (cfgRow["config"].is_string()) {
                    try { cfg = nlohmann::json::parse(cfgRow["config"].get<std::string>()); } catch (...) {}
                } else {
                    cfg = cfgRow["config"];
                }
                priorDepsHash = cfg.value("_deps_hash", std::string());
            }
        } catch (...) {}

        DepInstaller::Result depRes;
        if (pc.runtime == "python") {
            depRes = DepInstaller::ensurePython(serviceRoot, workdir, pc.requirementsFile,
                                                 pc.depsAutoInstall, priorDepsHash, depLogCb);
        } else if (pc.runtime == "node") {
            depRes = DepInstaller::ensureNode(serviceRoot, workdir,
                                                pc.depsAutoInstall, priorDepsHash, depLogCb);
        } else {
            depRes.ok = true;
        }
        if (!depRes.ok) {
            logToService(id, "warn",
                "Dependency install failed; backing off and retrying: " + depRes.message);
            if (!restartWindow.allow(nowMs())) {
                logToService(id, "error",
                    "Restart budget exhausted during dep install; stopping");
                try {
                    Database::instance().execute(
                        "UPDATE services SET status = 'error' WHERE id = ?1", {id});
                } catch (...) {}
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(currentBackoffMs));
            currentBackoffMs = std::min(pc.backoffMaxMs, currentBackoffMs * 2);
            continue;
        }
        if (depRes.ran && !depRes.newHash.empty()) {
            // Persist the hash back into services.config._deps_hash so the
            // next spawn skips the install. We do a read-modify-write because
            // config is a free-form JSON blob stored as TEXT.
            try {
                auto& dbU = Database::instance();
                auto row2 = dbU.queryOne("SELECT config FROM services WHERE id = ?1", {id});
                if (!row2.is_null()) {
                    nlohmann::json cfg;
                    if (row2["config"].is_string()) {
                        try { cfg = nlohmann::json::parse(row2["config"].get<std::string>()); } catch (...) {}
                    } else {
                        cfg = row2["config"];
                    }
                    cfg["_deps_hash"] = depRes.newHash;
                    dbU.execute("UPDATE services SET config = ?1 WHERE id = ?2",
                                {cfg.dump(), id});
                }
            } catch (...) {}
        }

        // Build spawn spec.
        std::string err;
        auto maybeSpec = buildSpawnSpec(pc, workdir, serviceRoot, resolvedEnv, err);
        if (!maybeSpec) {
            logToService(id, "error", err);
            // Missing runtime is a user-fixable config error, not a flapping
            // process. Don't spin on it — give up and let the user retry.
            break;
        }

        // Spawn.
        std::unique_ptr<ChildProcess> child;
        try {
            child = spawnChild(*maybeSpec);
        } catch (const std::exception& e) {
            logToService(id, "error", std::string("spawn failed: ") + e.what());
            if (!restartWindow.allow(nowMs())) {
                logToService(id, "error", "Restart budget exhausted; stopping");
                try {
                    Database::instance().execute(
                        "UPDATE services SET status = 'error' WHERE id = ?1", {id});
                } catch (...) {}
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(currentBackoffMs));
            currentBackoffMs = std::min(pc.backoffMaxMs, currentBackoffMs * 2);
            continue;
        }

        // Commit supervisor state under the proc lock.
        {
            std::lock_guard<std::mutex> plock(svc.procMu);
            svc.child       = std::move(child);
            svc.pid         = svc.child->pid();
            svc.startedAtMs = nowMs();
        }
        persistSupervisorState(id, svc);
        logToService(id, "info",
            "Spawned " + pc.runtime + " " + pc.entrypoint +
            " (pid=" + std::to_string(svc.pid) + ", workdir=" + workdir + ")");

        // Watch loop. Drain output; detect exit; respect shouldStop.
        std::string outBuf;
        outBuf.reserve(4096);

        while (!shouldStop && !shuttingDown_) {
            // Drain available output.
            {
                std::lock_guard<std::mutex> plock(svc.procMu);
                if (svc.child) {
                    // Drain in small chunks to keep the lock hold short.
                    size_t totalThisTick = 0;
                    while (true) {
                        size_t n = svc.child->readAvailable(outBuf, 4096);
                        if (n == 0) break;
                        totalThisTick += n;
                        if (totalThisTick > static_cast<size_t>(pc.logTailBytes)) break;
                    }
                }
            }
            // Emit log lines (split on '\n'), truncated to log_tail_bytes per line.
            if (!outBuf.empty()) {
                size_t start = 0;
                while (start < outBuf.size()) {
                    size_t nl = outBuf.find('\n', start);
                    if (nl == std::string::npos) break;
                    std::string line = outBuf.substr(start, nl - start);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (!line.empty()) {
                        if (line.size() > static_cast<size_t>(pc.logTailBytes))
                            line.resize(static_cast<size_t>(pc.logTailBytes));
                        logToService(id, "info", line);
                    }
                    start = nl + 1;
                }
                outBuf.erase(0, start);
            }

            // Check liveness.
            bool alive = true;
            int exitCode = -1;
            {
                std::lock_guard<std::mutex> plock(svc.procMu);
                if (!svc.child) { alive = false; break; }
                alive = svc.child->alive();
                if (!alive) exitCode = svc.child->exitCode();
            }
            if (!alive) {
                // Final drain after EOF.
                std::string tail;
                {
                    std::lock_guard<std::mutex> plock(svc.procMu);
                    if (svc.child) {
                        while (svc.child->readAvailable(tail, 4096) > 0) {}
                    }
                }
                if (!tail.empty()) {
                    size_t start = 0;
                    while (start < tail.size()) {
                        size_t nl = tail.find('\n', start);
                        std::string line = (nl == std::string::npos)
                            ? tail.substr(start)
                            : tail.substr(start, nl - start);
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (!line.empty()) logToService(id, "info", line);
                        if (nl == std::string::npos) break;
                        start = nl + 1;
                    }
                }

                {
                    std::lock_guard<std::mutex> plock(svc.procMu);
                    svc.lastExitCode = exitCode;
                    svc.child.reset();
                    svc.pid = 0;
                }
                logToService(id, "info",
                    "Process exited with code " + std::to_string(exitCode));
                persistSupervisorState(id, svc);
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        // If we're stopping, cleanly shut the child down.
        if (shouldStop || shuttingDown_) {
            {
                std::lock_guard<std::mutex> plock(svc.procMu);
                if (svc.child) {
                    svc.child->sendTerm();
                }
            }
            // Give the child up to 5 s to exit gracefully.
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline) {
                bool alive;
                {
                    std::lock_guard<std::mutex> plock(svc.procMu);
                    alive = svc.child && svc.child->alive();
                }
                if (!alive) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            {
                std::lock_guard<std::mutex> plock(svc.procMu);
                if (svc.child && svc.child->alive()) {
                    svc.child->kill();
                }
                svc.child.reset();
                svc.pid = 0;
            }
            persistSupervisorState(id, svc);
            break;
        }

        // Apply restart policy.
        bool shouldRestart = false;
        if (svc.restartRequested.exchange(false)) {
            shouldRestart = true; // explicit /restart
        } else if (pc.restartPolicy == "always") {
            shouldRestart = true;
        } else if (pc.restartPolicy == "on_failure") {
            shouldRestart = (svc.lastExitCode != 0);
        } else {
            shouldRestart = false;
        }

        if (!shouldRestart) {
            logToService(id, "info",
                "Not restarting (policy=" + pc.restartPolicy +
                ", exit=" + std::to_string(svc.lastExitCode) + ")");
            try {
                Database::instance().execute(
                    "UPDATE services SET status = 'stopped' WHERE id = ?1", {id});
            } catch (...) {}
            break;
        }

        if (!restartWindow.allow(nowMs())) {
            logToService(id, "error",
                "Restart budget exhausted (>" + std::to_string(pc.maxRestartsPerHour) +
                "/hr); stopping");
            try {
                Database::instance().execute(
                    "UPDATE services SET status = 'error' WHERE id = ?1", {id});
            } catch (...) {}
            break;
        }

        svc.restartCount++;
        logToService(id, "warn",
            "Restarting in " + std::to_string(currentBackoffMs) +
            "ms (attempt #" + std::to_string(svc.restartCount) + ")");
        persistSupervisorState(id, svc);

        // Backoff between restarts. Check shouldStop every 100ms so stops
        // during the backoff window are snappy.
        int slept = 0;
        while (slept < currentBackoffMs && !shouldStop && !shuttingDown_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            slept += 100;
        }
        currentBackoffMs = std::min(pc.backoffMaxMs, currentBackoffMs * 2);
    }
}

} // namespace avacli
