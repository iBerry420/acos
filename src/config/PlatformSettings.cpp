#include "config/PlatformSettings.hpp"
#include "config/ServeSettings.hpp"
#include "platform/Paths.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace avacli {

namespace fs = std::filesystem;

namespace {

/// Expand a leading `~` or `~/` to the user's home directory.
/// Anything else is returned untouched.
std::string expandHome(const std::string& p) {
    if (p.empty() || p[0] != '~') return p;
    std::string home = userHomeDirectoryOrFallback();
    if (p.size() == 1) return home;
    if (p[1] == '/' || p[1] == '\\') return home + p.substr(1);
    return p; // ~foo/bar — don't try to resolve other users; leave as-is
}

/// Return settings.json as JSON, or empty object on any error.
nlohmann::json loadSettings() {
    try {
        return loadServeSettingsJson();
    } catch (...) {
        return nlohmann::json::object();
    }
}

} // namespace

namespace platformSettings {

std::string defaultServicesWorkdirRoot() {
    std::string home = userHomeDirectoryOrFallback();
    return (fs::path(home) / ".avacli" / "services").string();
}

std::string servicesWorkdirRoot() {
    auto j = loadSettings();
    if (j.contains(KEY_SERVICES_WORKDIR_ROOT) && j[KEY_SERVICES_WORKDIR_ROOT].is_string()) {
        std::string v = j[KEY_SERVICES_WORKDIR_ROOT].get<std::string>();
        std::string expanded = expandHome(v);
        if (!expanded.empty()) return expanded;
    }
    return defaultServicesWorkdirRoot();
}

int appsDbSizeCapMb() {
    auto j = loadSettings();
    if (j.contains(KEY_APPS_DB_SIZE_CAP_MB) && j[KEY_APPS_DB_SIZE_CAP_MB].is_number_integer()) {
        int v = j[KEY_APPS_DB_SIZE_CAP_MB].get<int>();
        if (v >= APPS_DB_SIZE_CAP_MIN_MB && v <= APPS_DB_SIZE_CAP_MAX_MB) return v;
    }
    return APPS_DB_SIZE_CAP_DEFAULT_MB;
}

int subagentsMaxDepth() {
    auto j = loadSettings();
    if (j.contains(KEY_SUBAGENTS_MAX_DEPTH) && j[KEY_SUBAGENTS_MAX_DEPTH].is_number_integer()) {
        int v = j[KEY_SUBAGENTS_MAX_DEPTH].get<int>();
        if (v >= SUBAGENTS_MAX_DEPTH_MIN && v <= SUBAGENTS_MAX_DEPTH_MAX) return v;
    }
    return SUBAGENTS_MAX_DEPTH_DEFAULT;
}

std::string validateWorkdirRoot(const std::string& raw) {
    if (raw.empty())
        throw std::invalid_argument("services_workdir_root cannot be empty");

    std::string expanded = expandHome(raw);

    // Must be absolute: anything relative would resolve against the server's
    // CWD which is fragile (differs between systemd, shell invocation, IDE).
    fs::path p(expanded);
    if (!p.is_absolute())
        throw std::invalid_argument("services_workdir_root must be an absolute path");

    // Parent must exist so mkdir at spawn-time has a fair chance of succeeding.
    // We allow the leaf itself to be missing — it's created lazily.
    fs::path parent = p.parent_path();
    if (!parent.empty() && !fs::exists(parent))
        throw std::invalid_argument("parent of services_workdir_root does not exist: " + parent.string());

    // Canonicalise only if it exists; otherwise trust the user.
    try {
        if (fs::exists(p)) return fs::canonical(p).string();
    } catch (...) {}
    return p.lexically_normal().string();
}

int validateAppsDbSizeCapMb(int raw) {
    if (raw < APPS_DB_SIZE_CAP_MIN_MB || raw > APPS_DB_SIZE_CAP_MAX_MB) {
        throw std::invalid_argument(
            "apps_db_size_cap_mb must be between " +
            std::to_string(APPS_DB_SIZE_CAP_MIN_MB) + " and " +
            std::to_string(APPS_DB_SIZE_CAP_MAX_MB));
    }
    return raw;
}

int validateSubagentsMaxDepth(int raw) {
    if (raw < SUBAGENTS_MAX_DEPTH_MIN || raw > SUBAGENTS_MAX_DEPTH_MAX) {
        throw std::invalid_argument(
            "subagents_max_depth must be between " +
            std::to_string(SUBAGENTS_MAX_DEPTH_MIN) + " and " +
            std::to_string(SUBAGENTS_MAX_DEPTH_MAX));
    }
    return raw;
}

} // namespace platformSettings

} // namespace avacli
