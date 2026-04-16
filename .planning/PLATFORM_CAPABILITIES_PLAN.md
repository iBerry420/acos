# Platform Capabilities — Long-Running Services, App DB Access, Sub-Agents

## Status: Draft — ready for phased implementation

## Scope

Three related capabilities, all cross-platform (Linux, macOS, Windows) from day one:

1. **Long-running processes** — host Python / Node / native daemons (e.g. a 24/7 Discord bot) as supervised child processes, not as tick-based stubs.
2. **Internal SQLite for apps** — let agent-generated apps persist and query their own data via a scoped, authenticated API.
3. **Sub-agents** — let the agent delegate a bounded task to a child agent (same or different LLM) with explicit scope, no-clobber coordination, and depth limits.

Guiding constraints:

- No new platform-specific deps beyond what's already in `vcpkg.json` and the build recipes.
- Cross-platform: use `std::filesystem`, reuse `ProcessRunner` pattern (Win32 branch + POSIX branch).
- Packaging: apt package gets `Recommends: python3, python3-venv, nodejs, npm`; macOS/Windows docs note the same.
- Everything observable via existing `service_logs`, `event_log`, and `LogBuffer` infra.
- **xAI-only**: all agent/sub-agent LLM calls go through `XAIClient`. No OpenAI, Anthropic, or Ollama support — explicit non-goal.
- **Per-user filesystem**: service workdirs and app DBs live under `$HOME/.avacli/`. No `/var/lib/avacli/` or system-wide mutable state. Multi-user installs use per-user service accounts.

---

## Cross-cutting prereqs

Before any feature lands, two small primitives are added once and reused everywhere.

### P0.1 `ProcessSupervisor` (new)

**File:** `src/platform/ProcessSupervisor.hpp` / `.cpp`

Wraps platform-specific **long-lived** child process management. Complements the existing synchronous `runShellCommand` (which stays as-is for short, one-shot jobs).

Public API:

```cpp
namespace avacli {

struct SpawnSpec {
    std::string executable;              // resolved absolute path
    std::vector<std::string> args;
    std::string workingDir;
    std::map<std::string, std::string> env;  // overlays parent env
    bool mergeStderr = true;
    bool newProcessGroup = true;         // allow killing whole tree
};

class ChildProcess {
public:
    bool alive() const;
    int  exitCode() const;               // -1 while alive
    void sendTerm();                     // SIGTERM / CTRL_BREAK_EVENT
    void kill();                         // SIGKILL / TerminateJobObject
    // Non-blocking: reads available stdout/stderr bytes into out.
    size_t readAvailable(std::string& out, size_t maxBytes = 4096);
    long pid() const;
};

std::unique_ptr<ChildProcess> spawn(const SpawnSpec& spec);

} // namespace avacli
```

Platform notes:

- **POSIX**: `pipe2(O_CLOEXEC)` for stdout/stderr, `fork()` + `setpgid(0,0)` + `execve`. `sendTerm` = `kill(-pgid, SIGTERM)`. `kill` = `kill(-pgid, SIGKILL)`. `waitpid(pid, &st, WNOHANG)` for status.
- **Windows**: `CreateProcessW` with `CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED`, assigned to a `Job Object` with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`. `sendTerm` = `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)`. `kill` = `TerminateJobObject`. Status via `GetExitCodeProcess`.
- Reuse `utf8ToWide` helper that already lives in `ProcessRunner.cpp` (lift into a shared header).

### P0.2 Runtime detection

**File:** extend `src/platform/Runtime.hpp` (new) / reuse the `findPythonCommand()` pattern.

```cpp
struct RuntimePaths {
    std::string python;   // resolved via findPythonCommand()
    std::string node;     // "node" / "node.exe" (AVALYNNAI_NODE override)
    std::string npm;      // "npm" / "npm.cmd" (Windows)
};
RuntimePaths detectRuntimes();
```

Cache like `cachedPythonCmd`. `npm` on Windows is a `.cmd` shim — must be invoked via `cmd.exe /c npm ...` OR we point at `npm.cmd` directly. Add a helper `runNpm(args, cwd)` that handles the `.cmd` wrapping.

### P0.3 Settings keys (central registry)

All user-tunable behaviour lands in the existing `settings` key/value store and is surfaced in the Settings page. Three new keys are introduced by this plan; they are the **single source of truth** for their respective knobs — feature code must read from `SettingsManager::get(...)`, never hard-code.

| Key | Type | Default | Range / Values | Feature | UI Control |
|---|---|---|---|---|---|
| `services.workdir_root` | string (path) | `~/.avacli/services` | any writable absolute path; `~` expanded | F1 | Text input + "Reset" |
| `apps.db_size_cap_mb` | integer | `256` | `16`–`16384` | F2 | Slider + numeric |
| `subagents.max_depth` | integer | `0` | `0`–`16` | F3 | Slider; `0` label = "Disabled" |

Behavioural contracts:

- **`services.workdir_root`** — resolved once at service-spawn; existing running services keep their original paths. Changing it mid-flight only affects newly created services.
- **`apps.db_size_cap_mb`** — global ceiling. Per-app override via new `apps.db_size_cap_mb` column (0 = inherit global). Enforced on every write (see 2.3).
- **`subagents.max_depth`** — `0` disables sub-agents entirely: `spawn_subagent` returns `{"error":"subagents_disabled","hint":"Raise subagents.max_depth in Settings"}` without a server restart. Any value `N>0` allows tasks with `depth < N` to spawn; tasks at `depth == N` cannot. Changes take effect on the next `spawn_subagent` call.

UI: a new **"Platform capabilities"** section in the Settings page groups these three controls with short help text and `?` tooltips. Persistence is handled by the existing settings infra — no new DB table.

---

## Feature 1 — Long-running services (Discord bot, Python daemons, Node apps)

### 1.1 New service `type: "process"`

Replaces the scattered `custom_script`/`bot`/`custom` stubs with a single explicit process spec.

`services.config` JSON shape (stored in existing `services` table):

```json
{
  "runtime": "python" | "node" | "bin",
  "entrypoint": "bot.py",
  "args": ["--env", "production"],
  "env": {
    "DISCORD_TOKEN": "vault:discord_bot_token",
    "LOG_LEVEL": "info"
  },
  "deps": {
    "requirements_file": "requirements.txt",
    "auto_install": true
  },
  "working_dir": "apps/my-discord-bot",
  "restart": {
    "policy": "always" | "on_failure" | "never",
    "backoff_initial_ms": 1000,
    "backoff_max_ms": 60000,
    "max_restarts_per_hour": 20
  },
  "log_tail_bytes": 65536
}
```

`env` values prefixed `vault:<name>` are resolved at spawn time from the encrypted vault — secrets **never** get stored in `services.config` in plaintext.

### 1.2 Schema migration (v4)

```sql
-- services.pid + services.started_at for supervisor state
ALTER TABLE services ADD COLUMN pid INTEGER DEFAULT 0;
ALTER TABLE services ADD COLUMN started_at INTEGER DEFAULT 0;
ALTER TABLE services ADD COLUMN restart_count INTEGER DEFAULT 0;
ALTER TABLE services ADD COLUMN last_exit_code INTEGER DEFAULT 0;

-- service_logs already works; add an index for tail queries
CREATE INDEX IF NOT EXISTS idx_svc_logs_svc_ts_desc
  ON service_logs(service_id, timestamp DESC);
```

### 1.3 `ServiceManager` refactor

**File:** `src/services/ServiceManager.cpp`

- Split `RunningService` into two subclasses:
  - `TickService` — current behaviour for `rss_feed`, `scheduled_prompt`. The stubs get real implementations here (curl for RSS, `XAIClient` call for scheduled prompt — both already available).
  - `ProcessService` — owns a `ChildProcess` from `ProcessSupervisor::spawn`. Runs a single reader thread that drains `readAvailable` → `service_logs` (batched, rate-limited to e.g. 1000 lines/min per service, truncated at `log_tail_bytes` per line).
- Supervisor loop:
  1. Resolve runtime via `detectRuntimes()`.
  2. If `deps.auto_install` and deps changed since last run (hash of `requirements.txt` / `package.json` stored in `services.config._deps_hash`), run install once via `runShellCommand` with a long timeout.
  3. Build `SpawnSpec` (resolve vault env, compute absolute paths).
  4. `spawn()`. Record pid, started_at.
  5. Watch every 1 s: if child exits, log exit code, apply `restart` policy with exponential backoff. Cap at `max_restarts_per_hour`; after that, set `status='error'` and stop.
- `shutdown()`: `sendTerm` to all children, wait 5 s, then `kill`. Same for single `stopService`.

### 1.4 Dep installation (cross-platform)

Per-service venv / node_modules, not system-wide. All service filesystem state lives under `settings.services.workdir_root` (default `~/.avacli/services/<id>/`):

- Python: `<workdir_root>/<id>/venv`. Bootstrap: `python -m venv venv`, then `venv/bin/pip install -r requirements.txt` (`venv\Scripts\pip.exe` on Windows).
- Node: `cd <workdir_root>/<id> && npm ci` (via `runNpm`).
- Record `_deps_hash = sha256(requirements.txt content)` after success; skip reinstall if unchanged.

The supervisor creates `<workdir_root>/<id>/` lazily on first start with mode `0700` (POSIX) / owner-only ACL (Windows). Files inside (`venv/`, `node_modules/`, `logs/`, `state/`) are rooted here so `rm -rf <workdir_root>/<id>` fully deprovisions a service.

### 1.5 Agent tool updates

`ToolRegistry.cpp` — extend `create_service` / `manage_service`:

- `create_service` gains `type: "process"` with the config shape above. Keep the old types working for compatibility.
- New tool `tail_service_logs(id, lines)` in Question mode (0) so the agent can diagnose a running service without Agent-level privileges.
- `list_services` returns `pid`, `started_at`, `restart_count`, `last_exit_code`.

### 1.6 API surface

`RoutesServices.cpp` — add:

- `GET /api/services/:id/status` → `{alive, pid, uptime_s, restart_count, last_exit_code}`
- `POST /api/services/:id/signal` → `{signal: "term"|"kill"|"restart"}`
- `GET /api/services/:id/logs/stream` → SSE tail (same pattern as `/api/chat`)

### 1.7 Packaging

- `packaging/debian/control`: `Recommends: python3, python3-venv, nodejs (>= 18), npm`
- `packaging/debian/postinst`: **no** service-dir creation — service dirs are per-user, created lazily at spawn-time under `$HOME/.avacli/services/`. For multi-user / system-wide installs, ops are directed (via docs + `NEWS`) to create a dedicated `avacli` user whose `$HOME` serves as the root; no `/var/lib/avacli` is required or created.
- macOS Homebrew formula: same runtimes as recommended deps; `~/.avacli/services/` created by the app itself.
- Windows MSI: docs link users to python.org / nodejs.org; no auto-install. `%USERPROFILE%\.avacli\services\` used as workdir root.

### 1.8 Acceptance test (Discord bot dog-food)

1. `create_app` + `edit_app_file` puts a `bot.py` + `requirements.txt` into `services/discord-bot/`.
2. `vault_store` the bot token.
3. `create_service` with `runtime: "python", entrypoint: "bot.py", env.DISCORD_TOKEN: "vault:discord_bot_token", restart.policy: "always"`.
4. Kill the binary with `kill <pid>` externally → bot survives because `waitpid` sees `SIGTERM`, but the avacli parent has the supervisor which respawns. (For host-level avacli restart survival, see 1.9.)
5. Stop the service via `manage_service` → SIGTERM delivered to whole process group, bot exits cleanly.

### 1.9 Optional: handoff to host supervisor

For "survives avacli restart" use cases, add `restart.handoff: "systemd"|"launchd"|"scm"`:

- Linux: write a `systemd --user` unit under `~/.config/systemd/user/avacli-svc-<id>.service`, `systemctl --user enable/start`.
- macOS: plist under `~/Library/LaunchAgents/ai.avalynn.svc.<id>.plist` + `launchctl bootstrap gui/<uid>`.
- Windows: `sc.exe create` + `nssm` if available (otherwise docs).

This is **optional** and off by default — the in-process supervisor covers the common case.

---

## Feature 2 — Internal SQLite for agent-generated apps

### 2.1 Isolation model

Each app gets **its own SQLite file** at `~/.avacli/app_data/<slug>.db`. Pros: rock-solid isolation, no SQL-rewriting layer, cheap to back up / delete per app. Cons addressed below.

For cross-app / main-DB access, a **read-only whitelist proxy** exposes specific views from `avacli.db`.

### 2.2 Schema / migration (v5)

Main DB — per-app auth + feature flags:

```sql
ALTER TABLE apps ADD COLUMN agent_token     TEXT    DEFAULT '';  -- secret, 32 random bytes hex
ALTER TABLE apps ADD COLUMN db_enabled      INTEGER DEFAULT 1;   -- off-switch
ALTER TABLE apps ADD COLUMN ai_enabled      INTEGER DEFAULT 1;   -- off-switch for agent access
ALTER TABLE apps ADD COLUMN db_size_cap_mb  INTEGER DEFAULT 0;   -- 0 = inherit settings.apps.db_size_cap_mb
CREATE UNIQUE INDEX IF NOT EXISTS idx_apps_agent_token ON apps(agent_token);

-- Quotas (per 24h rolling window, enforced in proxy)
CREATE TABLE IF NOT EXISTS app_usage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_id TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    kind TEXT NOT NULL,         -- 'db_read' | 'db_write' | 'ai_call'
    tokens INTEGER DEFAULT 0,
    bytes INTEGER DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_app_usage_app_ts ON app_usage(app_id, timestamp);
```

Token generated on first `POST /api/apps` using `openssl_rand_bytes(16)` via OpenSSL (already a dep).

### 2.3 Per-app SQLite opener

**File:** `src/db/AppDatabase.hpp` / `.cpp`

```cpp
class AppDatabase {
public:
    // Opens (or creates) ~/.avacli/app_data/<slug>.db with WAL + FK.
    // Caches one sqlite3* per slug behind a mutex.
    static AppDatabase& forSlug(const std::string& slug);

    nlohmann::json query(const std::string& sql, const std::vector<nlohmann::json>& params);
    int64_t execute(const std::string& sql, const std::vector<nlohmann::json>& params);
    nlohmann::json schema();   // lists tables/columns
};
```

Enforced at open time via `sqlite3_set_authorizer`:

- Allow: `SQLITE_SELECT`, `SQLITE_INSERT`, `SQLITE_UPDATE`, `SQLITE_DELETE`, `SQLITE_CREATE_TABLE`, `SQLITE_CREATE_INDEX`, `SQLITE_DROP_TABLE` (owned only), `SQLITE_PRAGMA` (whitelist: `user_version`, `table_info`).
- Deny: `SQLITE_ATTACH`, `SQLITE_DETACH`, any pragma outside whitelist → returns `SQLITE_DENY`. This prevents an app from attaching `avacli.db` and escaping its sandbox.

**Size cap**: resolved per-request as `apps.db_size_cap_mb` if non-zero, else `settings.apps.db_size_cap_mb` (default 256). Checked in middleware before every write via `PRAGMA page_count * PRAGMA page_size`; exceeding returns HTTP 507 `{"error":"db_quota_exceeded","limit_mb":N}`. Reads are unaffected so the app can still drain data when over quota. The Settings page exposes the global default; per-app override is set via an admin UI or the `app_db_set_cap(app_id, mb)` tool in Agent mode.

### 2.4 HTTP endpoints (new `RoutesAppDB.cpp`)

All require `Authorization: Bearer <agent_token>` **and** a same-origin referer matching `/apps/<slug>/...`. Both checks because the token lives in the HTML the browser serves, so XSRF + token gives defence-in-depth.

- `POST /api/apps/:slug/db/query`   body `{sql, params}` → `{rows, count}` (any statement allowed per authorizer).
- `POST /api/apps/:slug/db/execute` body `{sql, params}` → `{changes, last_insert_rowid}`.
- `GET  /api/apps/:slug/db/schema`   → tables + columns.
- `GET  /api/apps/:slug/db/export`   → SQLite file download (for "download your data").
- `POST /api/apps/:slug/main/query`  → whitelisted **read-only** access to main DB views (see 2.6).

All increment `app_usage` for quota tracking.

### 2.5 Auto-injected client SDK

In `RoutesApps.cpp` at the `defaultHtml` site (currently around line 186), inject a `<script src="/apps/:slug/_sdk.js">` tag. The SDK script is served from a new route:

- `GET /apps/:slug/_sdk.js` → templated JS with the slug + agent_token baked in (server-side); `Cache-Control: no-store`.

SDK surface:

```js
window.avacli = {
  db: {
    query(sql, params) { /* POST .../db/query */ },
    execute(sql, params) { /* POST .../db/execute */ },
    schema() { /* GET .../db/schema */ },
  },
  main: {
    query(viewName, params) { /* POST .../main/query (whitelisted) */ },
  },
  ai: {
    chat(message, opts) { /* POST .../agent (see Feature 3 scaffolding) */ },
    stream(message, onDelta, opts) { /* SSE */ },
  },
};
```

This removes the 401 problem that hits apps like `ai-playlist-builder` today — `window.avacli.ai.chat(...)` Just Works inside any generated app.

### 2.6 Main DB whitelisted views

Define **read-only materialised views** the apps are allowed to query by name (not raw SQL):

- `articles_public` → `SELECT id, title, summary, tags, created_at FROM articles`
- `apps_directory` → `SELECT slug, name, description, icon_url FROM apps WHERE status='active'`
- `my_app` → scoped to the calling app (`SELECT * FROM apps WHERE slug = :self`)

The proxy maps the `viewName` to a prepared SQL. No raw SQL from apps ever touches the main DB.

### 2.7 Agent tool updates

- New tool `app_db_execute(app_id, sql, params)` in Agent mode (2) — lets the agent seed data into an app's DB while building it.
- New tool `app_token(app_id)` in Plan mode (1) — returns the agent_token so it can be embedded in app HTML for manual `fetch` calls if the SDK isn't enough.

---

## Feature 3 — Sub-agents (agent delegates to agents)

### 3.1 Core idea

`spawn_subagent` is a tool the agent can call that starts a **second `AgentEngine`** in a background thread, with a **constrained scope**. The parent continues immediately (fire-and-forget) or awaits via `wait_subagent`.

All sub-agents use `XAIClient`; the `model` parameter picks from xAI's catalogue (grok-4, grok-4-fast, grok-code-fast, etc.). No other LLM providers are supported — see 3.4.

**Default-deny**: sub-agents are disabled entirely out-of-the-box. The Settings-page knob `subagents.max_depth` (default `0`) must be raised to `1` or higher before any `spawn_subagent` call succeeds (see P0.3).

### 3.2 Schema (v5 continued)

```sql
CREATE TABLE IF NOT EXISTS agent_tasks (
    id TEXT PRIMARY KEY,
    parent_task_id TEXT DEFAULT '',
    root_task_id  TEXT NOT NULL,
    depth INTEGER NOT NULL DEFAULT 0,
    session_id TEXT NOT NULL,
    model TEXT NOT NULL,                     -- xAI model id (grok-4, grok-4-fast, ...)
    mode TEXT NOT NULL,                      -- 'question' | 'plan' | 'agent'
    goal TEXT NOT NULL,
    scope_json TEXT NOT NULL DEFAULT '{}',   -- see 3.5
    status TEXT NOT NULL DEFAULT 'pending',  -- pending|running|done|failed|cancelled
    result_json TEXT DEFAULT '{}',
    prompt_tokens INTEGER DEFAULT 0,
    completion_tokens INTEGER DEFAULT 0,
    cost_usd REAL DEFAULT 0,
    created_at INTEGER NOT NULL,
    started_at INTEGER DEFAULT 0,
    finished_at INTEGER DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_agent_tasks_parent ON agent_tasks(parent_task_id);
CREATE INDEX IF NOT EXISTS idx_agent_tasks_status ON agent_tasks(status);

CREATE TABLE IF NOT EXISTS agent_task_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    kind TEXT NOT NULL,          -- 'content'|'tool_start'|'tool_done'|'thinking'|'done'|'error'
    payload TEXT NOT NULL DEFAULT '{}'
);
CREATE INDEX IF NOT EXISTS idx_agent_task_events_task ON agent_task_events(task_id, id);

-- Write leases: prevents two agents editing the same file concurrently.
CREATE TABLE IF NOT EXISTS agent_leases (
    resource TEXT PRIMARY KEY,   -- canonical absolute path, or 'service:<id>', 'app:<slug>', 'table:<name>'
    task_id TEXT NOT NULL,
    acquired_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL  -- hard expiry (e.g. 5 min); auto-released on task done
);
```

### 3.3 `SubAgentManager`

**File:** `src/core/SubAgentManager.hpp` / `.cpp`

```cpp
struct SubAgentSpec {
    std::string goal;                          // the child's prompt
    std::string model;                         // xAI model id; defaults to parent's model
    std::string apiKeyVault;                   // vault name; empty = inherit XAI_API_KEY
    Mode mode = Mode::Agent;
    int maxTurns = 20;
    int maxTokens = 100000;
    // Note: no maxDepth field in the spec — depth ceiling is a global setting
    // (settings.subagents.max_depth, P0.3). The parent cannot raise it per-call.
    struct Scope {
        std::vector<std::string> allowedPaths;  // globs, workspace-relative
        std::vector<std::string> deniedPaths;
        std::vector<std::string> allowedTools;  // if empty, inherits parent mode tools
        std::vector<std::string> deniedTools;   // `spawn_subagent` is always denied in
                                                // children unless the global depth budget
                                                // still permits and parent opts in
        bool canNetwork = true;
        bool canShell = false;
    } scope;
};

class SubAgentManager {
public:
    std::string spawn(const std::string& parentTaskId,
                      const SubAgentSpec& spec);        // returns task_id, runs in background
    nlohmann::json status(const std::string& taskId);
    nlohmann::json wait(const std::string& taskId, int timeoutMs);
    void cancel(const std::string& taskId);
};
```

Implementation:

- Each task runs on its own `std::thread`, with its own `AgentEngine`, its own `ToolExecutor` (always an `XAIClient` under the hood), and a bounded `ToolRegistry` filtered by `scope`.
- Streaming events go into `agent_task_events` instead of SSE; parents (or HTTP callers) poll or SSE-tail.
- Depth gate: on every `spawn_subagent` call, read `settings.subagents.max_depth` (live, not cached), compute the new child's depth as `parent.depth + 1`, and reject if `child_depth > max_depth`. If `max_depth == 0`, every call is rejected with `{"error":"subagents_disabled"}`. If `max_depth == 16` (the cap), an honest 16-deep chain is legal but anything deeper is rejected with `{"error":"max_depth_exceeded","limit":16}`.

### 3.4 LLM provider — xAI only

By user decision, **`XAIClient` is the only supported LLM backend**. No OpenAI, no Anthropic, no Ollama, no local models. The schema reflects reality — there is no `provider` column; re-adding one is a trivial future migration if multi-provider ever materialises.

Concretely:

- No `LLMClient` interface, no `OpenAIClient` / `AnthropicClient` / `OllamaClient` classes get created.
- `SubAgentManager` hard-codes `XAIClient` as the client type; only the `model` string and API-key selection vary per sub-agent.
- Sub-agents can use any xAI model available in the catalogue (`grok-4`, `grok-4-fast`, `grok-code-fast`, etc.); validation reuses the existing xAI model list.
- API key: defaults to the global `XAI_API_KEY` from the vault; `apiKeyVault` lets a sub-agent use a different vault-stored xAI key (e.g. a separate billing account) — still xAI.

Rationale: reduces the surface area of phase 4 by ~60%, keeps billing/rate-limit/observability in one place, and avoids maintenance burden of multiple SDK shapes.

Reversibility: if a future change ever needs multi-provider, introducing an `LLMClient` interface above `XAIClient` is a one-file refactor — none of the sub-agent plumbing, scope-enforcement, or lease logic is provider-specific.

### 3.5 Scope enforcement (the "don't clobber each other" guarantee)

The `ToolExecutor` for a sub-agent is wrapped with a `ScopedToolExecutor`:

1. **Path gating** — every `read_file` / `write_file` / `edit_file` / `list_directory` / `run_shell` argument is canonicalised and checked against `scope.allowedPaths` (glob match) and `scope.deniedPaths`. `run_shell` is additionally rejected if `scope.canShell == false`, regardless of path.
2. **Lease acquisition** — before any write op on a path, `ScopedToolExecutor` tries to `INSERT OR FAIL INTO agent_leases(resource, task_id, acquired_at, expires_at)`. On conflict (another task holds it), the tool call fails with a structured error: `{"error":"resource_locked","holder":"<task_id>","expires_in_s":123}`. The agent is expected to `wait_subagent(holder)` or re-plan.
3. **Lease release** — on task `done`/`failed`/`cancelled`, `DELETE FROM agent_leases WHERE task_id = ?`. Also a periodic sweeper deletes expired leases.
4. **Tool gating** — `scope.allowedTools` intersected with the mode's natural tools. `spawn_subagent` is gated by the global `settings.subagents.max_depth` check (3.3) **and** by the parent's `scope.deniedTools`: a parent can explicitly forbid its child from spawning grandchildren even when the global depth budget still permits.

### 3.6 New agent-facing tools

| Tool | Min mode | Purpose |
|---|---|---|
| `spawn_subagent` | Agent (2) | Start a child with `{goal, model, scope, max_turns}`. Returns `task_id`. Rejects with `{"error":"subagents_disabled"}` when `settings.subagents.max_depth == 0`, or `{"error":"max_depth_exceeded","limit":N}` when the depth ceiling is reached. |
| `wait_subagent`  | Question (0) | Block (server-side, non-blocking HTTP) until the task reaches a terminal state or timeout. Returns `{status, result, tokens}`. |
| `list_subagents` | Question (0) | List tasks spawned by the caller + descendants with status/tokens. |
| `cancel_subagent`| Agent (2) | Send cancel to a running task. |
| `read_subagent_events` | Question (0) | Tail the event stream for a task (agent diagnostics). |

`spawn_subagent` is always advertised in the Agent-mode tool schema (not hidden when disabled) so that raising `subagents.max_depth` from `0` to `1` in Settings takes effect without a server restart and without re-issuing tool schemas to in-flight sessions. The runtime gate lives in `ScopedToolExecutor`.

Update `TOOL_MIN_MODE` in `ToolRegistry.cpp` accordingly.

### 3.7 System prompt addendum

Append to `GROK_SYSTEM_PROMPT.md` (existing file):

> ## Delegating to sub-agents
>
> Sub-agents are **off by default**. If `spawn_subagent` returns `subagents_disabled`, tell the user to raise `subagents.max_depth` in Settings — do not pester retry.
>
> When enabled, prefer `spawn_subagent` for sub-tasks that are:
> - **Embarrassingly parallel** — e.g. "summarise each of these 10 files".
> - **Domain-scoped** — e.g. "fix the frontend CSS while I refactor the backend".
>
> Before spawning, **define a scope**: allowed paths (globs) and tools. Two sub-agents MUST NOT share writable paths — partition the work first. If they need shared reads, that's fine; only writes are locked via `agent_leases`.
>
> Never spawn a sub-agent with the *same* goal as another running sibling. Always `list_subagents` first.
>
> Depth is capped by the user's setting (0–16). The tool will tell you the current limit if you exceed it. Sub-agents cannot spawn sub-sub-agents unless your own `scope.deniedTools` doesn't forbid it AND there's still depth budget remaining.
>
> All sub-agents run on xAI models — pick `model` from the xAI catalogue.

### 3.8 API surface

New `RoutesSubAgents.cpp`:

- `POST /api/tasks` — spawn from outside the agent loop (web UI, CLI).
- `GET  /api/tasks`  — list with filters `parent`, `status`.
- `GET  /api/tasks/:id` — status + result.
- `GET  /api/tasks/:id/events/stream` — SSE tail.
- `POST /api/tasks/:id/cancel`.

### 3.9 UI: Task tree panel

In the chat page, when the active session has any child tasks, render a collapsible tree (unicode, no emoji unless the user theme asks for them):

```
[root] session (you)
  |-- [done]   #a1b2 "Summarise repo"             grok-4           12s   2.3k tok
  |-- [run]    #c3d4 "Write tests for RoutesApps" grok-code-fast   45s  18.0k tok
  `-- [fail]   #e5f6 "Benchmark DB"               grok-4-fast       8s   (timeout)
```

Clicking a node opens its event transcript. When `settings.subagents.max_depth == 0`, the panel shows a small "Sub-agents are disabled — enable in Settings" hint instead of the tree.

---

## File-by-file change inventory

New files:

```
src/platform/ProcessSupervisor.hpp
src/platform/ProcessSupervisor.cpp
src/platform/Runtime.hpp
src/platform/Runtime.cpp
src/db/AppDatabase.hpp
src/db/AppDatabase.cpp
src/server/RoutesAppDB.cpp
src/server/RoutesSubAgents.cpp
src/core/SubAgentManager.hpp
src/core/SubAgentManager.cpp
src/tools/ScopedToolExecutor.hpp
src/tools/ScopedToolExecutor.cpp
```

Explicitly **not** created (xAI-only decision): `LLMClient.hpp`, `OpenAIClient.{hpp,cpp}`, `AnthropicClient.{hpp,cpp}`, `OllamaClient.{hpp,cpp}`.

Modified:

```
src/db/Database.cpp                  (+ migrations v4, v5; no LLMClient refactor)
src/services/ServiceManager.{hpp,cpp}(process service type + supervisor wiring;
                                      workdir rooted at settings.services.workdir_root)
src/server/RoutesServices.cpp        (status / signal / SSE logs)
src/server/RoutesApps.cpp            (agent_token issuance, _sdk.js route,
                                      defaultHtml injects SDK, db_size_cap_mb column)
src/server/HttpServer.cpp            (register new route bundles)
src/tools/ToolRegistry.cpp           (new tool schemas, TOOL_MIN_MODE updates,
                                      app_db_set_cap tool)
src/tools/ToolExecutor.cpp           (wire tail_service_logs, app_db_execute,
                                      spawn_subagent — XAIClient only)
src/core/SettingsManager.{hpp,cpp}   (register 3 new keys from P0.3; validators)
src/client/XAIClient.{hpp,cpp}       (no interface extraction; used directly)
web/settings/*                       (new "Platform capabilities" section: 3 controls)
CMakeLists.txt                       (new source files — smaller list than before)
vcpkg.json                           (no change expected)
packaging/debian/control             (Recommends python3-venv, nodejs, npm)
packaging/debian/postinst            (no /var/lib/avacli/services creation)
GROK_SYSTEM_PROMPT.md                (sub-agent guidance block; xAI-only note)
README.md / docs/                    (new capabilities section + Settings docs)
```

---

## Phasing (suggested PR boundaries)

| Phase | Scope | Risk | Gate |
|---|---|---|---|
| **1** | P0.1 ProcessSupervisor, P0.2 Runtime, P0.3 Settings keys (all three registered + Settings-page section), migration v4, `type:"process"` service, basic start/stop/logs, `settings.services.workdir_root` honoured. No venv/npm install yet — assume deps pre-installed. | Low (new code path, old types unchanged) | Discord bot written by user runs 24/7 across avacli restarts *within one session*, workdir is `~/.avacli/services/<id>/` |
| **2** | Auto dep install (venv, npm ci), restart policies, SSE log streaming, `tail_service_logs` tool, packaging `Recommends`. | Medium (shell-out + caching) | Bot spec references only `requirements.txt`, everything else auto-bootstraps |
| **3** | Migration v5 apps columns (incl. `db_size_cap_mb`), `AppDatabase`, `RoutesAppDB`, `/apps/:slug/_sdk.js` injection, main-DB whitelist views, `apps.db_size_cap_mb` setting wired to middleware. | Medium (auth + authorizer) | ai-playlist-builder stores real playlists in its own DB; over-quota writes return 507 |
| **4** | Sub-agent schema (v5 continued), `SubAgentManager`, `ScopedToolExecutor`, `spawn_subagent`/`wait_subagent`/`list_subagents`/`cancel_subagent` tools — **all using `XAIClient` only**. Depth ceiling enforced from `settings.subagents.max_depth`. | Medium (correctness of scope/leases; no multi-provider complexity) | With setting `=0`, `spawn_subagent` returns `subagents_disabled`. With `=2`, parent spawns two children on disjoint paths; concurrent edits don't collide; one child fails, other finishes; grandchild spawn rejected at depth 3. |
| **5** | UI: services status tab, app DB explorer, task-tree panel. Optional systemd/launchd handoff. | Low | Polished UX |

Phases 1–4 are independent enough to ship as separate PRs. Phase 5 is UI polish. Phase 4 is materially smaller than the original multi-provider draft — roughly the same scope as phase 3.

---

## Non-goals (for now)

- **Non-xAI LLM providers** — OpenAI, Anthropic, Ollama, local models are explicitly out of scope. `XAIClient` only.
- **System-wide service directories** — no `/var/lib/avacli/services/`. Everything is per-user under `$HOME/.avacli/`. Multi-user installs use dedicated service accounts.
- **Windows service (SCM) integration** — phase 5 optional.
- **Containerised isolation** (Docker/Podman) for services — out of scope; a supervised child process is the MVP. Add later as another `runtime: "container"`.
- **Cross-node sub-agents** — sub-agents run on the same node as the parent. Mesh delegation can layer on top via the existing P2P plan.
- **Fine-grained cost metering / billing** for sub-agent LLM calls — tokens are tracked in `agent_tasks`; monetary cost is a follow-up.

---

## Decisions (answered — frozen for phase 1)

| # | Question | Decision | Where enforced |
|---|---|---|---|
| 1 | Service filesystem root | **Per-user** — `$HOME/.avacli/services/<id>/`, configurable via `settings.services.workdir_root`. No `/var/lib/avacli`. | 1.4, 1.7, P0.3 |
| 2 | Sub-agent depth | **Default `0` (disabled)**, max `16`, single slider in Settings page. When `0`, `spawn_subagent` returns `subagents_disabled` without a restart. | 3.3, 3.6, P0.3 |
| 3 | App DB size cap | **Default `256` MB global setting**, per-app override via `apps.db_size_cap_mb` column (0 = inherit). Settings-page control exposes the global. | 2.2, 2.3, P0.3 |
| 4 | LLM providers | **xAI only**. No OpenAI, Anthropic, Ollama, or local models. Phase 4 scope materially reduced. | 3.4, Non-goals |

---

## Ready to start phase 1

No remaining blockers. Phase 1 deliverables:

1. `src/platform/ProcessSupervisor.{hpp,cpp}` — Win32 + POSIX, with tests on both.
2. `src/platform/Runtime.{hpp,cpp}` — cached `python` / `node` / `npm` resolution.
3. `src/core/SettingsManager` registration of the three P0.3 keys with validators and defaults; Settings-page "Platform capabilities" section with the three controls.
4. Migration v4 (`pid`, `started_at`, `restart_count`, `last_exit_code`, log index).
5. `ServiceManager` split into `TickService` + `ProcessService`; `type:"process"` config shape; `spawn` / `sendTerm` / `kill` / log draining.
6. `GET /api/services/:id/status` and `POST /api/services/:id/signal` (SSE log stream + auto-install deferred to phase 2).

Acceptance: a user writes a minimal `bot.py` with `discord.py` (installed manually on the host for phase 1), creates a `process` service pointing at it with `runtime:"python"`, and the bot stays connected across a deliberate `kill -9` of the bot process (restart policy fires) — all while the workdir is `~/.avacli/services/<id>/` and the service is visible in `list_services` with a live `pid`.
