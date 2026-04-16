#pragma once

#include <string>

namespace avacli {

/// Typed accessors for the "Platform capabilities" settings defined in
/// .planning/PLATFORM_CAPABILITIES_PLAN.md §P0.3.
///
/// All three settings persist to the same `~/.avacli/settings.json` file as
/// the rest of the UI settings — they're surfaced here with typed getters +
/// validators so feature code (ServiceManager, RoutesAppDB, SubAgentManager)
/// has a single canonical API.
///
/// Values are read fresh on every call (no in-process cache) so raising a
/// slider in the UI takes effect without a server restart. The underlying
/// settings file is tiny; the cost is negligible.
namespace platformSettings {

// ── services.workdir_root ────────────────────────────────────────────────
// Root directory under which per-service workdirs (venv, node_modules, logs,
// state) live. Defaults to `~/.avacli/services`. The literal `~` is expanded
// to the user's home on read.

constexpr const char* KEY_SERVICES_WORKDIR_ROOT = "services_workdir_root";

/// Default: "~/.avacli/services" (expanded).
std::string servicesWorkdirRoot();

/// Returns the expanded default (no settings file read).
std::string defaultServicesWorkdirRoot();

// ── apps.db_size_cap_mb ──────────────────────────────────────────────────
// Global ceiling for per-app SQLite file size. Range 16–16384. Default 256.
// Per-app override lives in the apps table (phase 3); this key is the
// fallback.

constexpr const char* KEY_APPS_DB_SIZE_CAP_MB = "apps_db_size_cap_mb";
constexpr int         APPS_DB_SIZE_CAP_MIN_MB = 16;
constexpr int         APPS_DB_SIZE_CAP_MAX_MB = 16384;
constexpr int         APPS_DB_SIZE_CAP_DEFAULT_MB = 256;

int appsDbSizeCapMb();

// ── subagents.max_depth ──────────────────────────────────────────────────
// Ceiling on sub-agent recursion depth. 0 disables sub-agents entirely
// (spawn_subagent returns `subagents_disabled`). Range 0–16. Default 0.

constexpr const char* KEY_SUBAGENTS_MAX_DEPTH = "subagents_max_depth";
constexpr int         SUBAGENTS_MAX_DEPTH_MIN = 0;
constexpr int         SUBAGENTS_MAX_DEPTH_MAX = 16;
constexpr int         SUBAGENTS_MAX_DEPTH_DEFAULT = 0;

int subagentsMaxDepth();

// ── Validators ───────────────────────────────────────────────────────────
// Called from `/api/settings` POST handler. Throws std::invalid_argument with
// a user-readable message on rejection.

/// Validates and normalises a workdir_root value. Expands `~`, rejects empty
/// strings and non-absolute paths, and verifies the parent directory exists
/// (so typos like `/nonexistent/foo` are caught at save time rather than at
/// service-spawn time). Returns the normalised absolute path.
std::string validateWorkdirRoot(const std::string& raw);

int validateAppsDbSizeCapMb(int raw);
int validateSubagentsMaxDepth(int raw);

} // namespace platformSettings

} // namespace avacli
