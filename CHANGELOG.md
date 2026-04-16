## [2.2.0] — 2026-04-16

**Platform capabilities: long-running services, per-app SQLite, and scoped sub-agents. Four phases, one release.**

This release lands every deliverable from `.planning/PLATFORM_CAPABILITIES_PLAN.md` — avacli can now host 24/7 daemons (Discord bots, Python workers, Node apps), generated web apps get their own authenticated SQLite DB with an auto-injected client SDK, and the agent can delegate scoped work to background sub-agents with lease-based write coordination. xAI-only throughout, no new runtime deps, everything per-user under `~/.avacli/`.

### Phase 1 — Long-running services (process supervisor)

- **`type:"process"` services** — a new service kind for Python / Node / native daemons that must stay up. Replaces the scattered `custom_script` stubs. Config shape: `{runtime, entrypoint, args, env, working_dir, restart:{policy, backoff_initial_ms, backoff_max_ms, max_restarts_per_hour}, log_tail_bytes}`.
- **`ProcessSupervisor`** — new cross-platform primitive (`src/platform/ProcessSupervisor.{hpp,cpp}`) that wraps long-lived children. POSIX uses `fork + setpgid + execve`; Windows uses `CreateProcessW` + `Job Object` with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`. Non-blocking reads, `sendTerm` / `kill`, whole-process-group signalling.
- **Runtime detection** — `src/platform/Runtime.{hpp,cpp}` caches resolved paths for `python`, `node`, and `npm` (handles the Windows `npm.cmd` shim). Overridable via `AVALYNNAI_NODE` etc.
- **Per-service workdir** — every process service gets an isolated directory under `~/.avacli/services/<id>/` (or wherever `services_workdir_root` points). Contains `venv/`, `node_modules/`, `logs/`, `state/`. Rooted lazily at first spawn with `0700`.
- **Vault-backed env** — env values like `"vault:discord_bot_token"` are resolved from the encrypted vault at spawn time; plaintext secrets never touch `services.config`.
- **Schema migration v4** — adds `services.pid`, `services.started_at`, `services.restart_count`, `services.last_exit_code`, plus `idx_svc_logs_svc_ts_desc` for fast tail queries.
- **API** — `GET /api/services/:id/status` returns `{alive, pid, uptime_s, restart_count, last_exit_code}`. `POST /api/services/:id/signal` accepts `{signal: "term"|"kill"|"restart"}`.

### Phase 2 — Auto dep install, restart policies, SSE logs

- **Per-service venv / npm** — first start of a `process` service with `deps.auto_install: true` runs `python -m venv` + `pip install -r requirements.txt` (or `npm ci`) into the service's workdir. Reinstalls only when the `sha256` of `requirements.txt` / `package.json` changes (stored in `services.config._deps_hash`).
- **Restart policies** — `always` / `on_failure` / `never` with exponential backoff (`backoff_initial_ms` → `backoff_max_ms`) and a per-hour ceiling (`max_restarts_per_hour`). Exceeding the ceiling transitions to `status='error'` with a log entry; tick-loop auto-stop (5 consecutive failures) continues to apply to `rss_feed` / `scheduled_prompt`.
- **`DepInstaller`** — new helper (`src/services/DepInstaller.{hpp,cpp}`) wraps the install shell-outs with long timeouts and cache hashing.
- **`tail_service_logs` tool** — available in Question mode (0). Returns the most-recent N log lines (default 50, max 500) with optional level filter. Lets the agent diagnose a running service without escalating to Agent mode.
- **SSE log stream** — `GET /api/services/:id/logs/stream?tail=100` does an initial backfill then streams new rows as they land in `service_logs`. 20-second heartbeat comments keep proxies from idling the connection.
- **Packaging** — `packaging/build-deb.sh` now declares `Recommends: python3 (>= 3.8), python3-venv, nodejs (>= 18), npm`. Core avacli still has no runtime deps beyond libcurl + libssl + sqlite3; the runtimes are only pulled in for users hosting process services. Homebrew formula gets the same runtimes as `:optional`.

### Phase 3 — Internal SQLite for agent-generated apps

- **Per-app SQLite file** — each app gets its own database at `~/.avacli/app_data/<slug>.db`, opened with WAL + FK and an `sqlite3_set_authorizer` hook that allows SELECT / INSERT / UPDATE / DELETE / CREATE TABLE|INDEX / DROP TABLE (owned only), denies ATTACH / DETACH, and whitelists only `PRAGMA user_version` / `PRAGMA table_info`. An app can't escape its sandbox.
- **`AppDatabase`** — new class (`src/db/AppDatabase.{hpp,cpp}`) with connection caching per slug, `query(sql, params)` / `execute(sql, params)` / `schema()` / quota checks.
- **Agent tokens** — first write to an app mints a 32-byte hex `agent_token` (OpenSSL `RAND_bytes`). `src/server/AppTokens.{hpp,cpp}` handles mint, verify, and constant-time compare.
- **Routes** (new `src/server/RoutesAppDB.cpp`):
  - `POST /api/apps/:slug/db/query`, `POST /api/apps/:slug/db/execute`, `GET /api/apps/:slug/db/schema`, `GET /api/apps/:slug/db/export`
  - `POST /api/apps/:slug/main/query` — whitelisted view access to the main DB (`articles_public`, `apps_directory`, `my_app`). Raw SQL never reaches `avacli.db`.
  - All SDK endpoints bypass the master-key session and require `Authorization: Bearer <agent_token>` with a same-origin referer.
- **Auto-injected SDK** — `GET /apps/:slug/_sdk.js` serves templated JS with the slug + token baked in. The default `RoutesApps` HTML includes `<script src="_sdk.js">`, exposing `window.avacli.db.{query,execute,schema}` and `window.avacli.main.query(viewName, params)` to every generated app without manual plumbing.
- **Size cap enforcement** — global default `apps.db_size_cap_mb = 256`, range 16–16384, per-app override in `apps.db_size_cap_mb` column (0 = inherit global). Writes over quota return HTTP 507 `{"error":"db_quota_exceeded","limit_mb":N}`; reads are unaffected so an app can still drain its data.
- **Usage accounting** — new `app_usage` table tracks `db_read` / `db_write` / `ai_call` events per app with token + byte counters.
- **Schema migration v5** — adds `apps.agent_token`, `apps.db_enabled`, `apps.ai_enabled`, `apps.db_size_cap_mb`, `app_usage` table, and a partial unique index on non-empty `agent_token` values.
- **New tools** (Agent mode): `app_token(app_id)` returns the token for manual fetch calls, `app_db_execute(app_id, sql, params)` seeds data into an app's DB, `app_db_set_cap(app_id, mb)` sets a per-app override.

### Phase 4 — Sub-agents with scoped writes and lease coordination

- **`spawn_subagent` tool** — the agent can now delegate a bounded task to a child `AgentEngine` running on its own thread. Input: `{goal, allowed_paths, allowed_tools, model?}`. Returns a `task_id`. Always xAI-backed — `XAIClient` is hard-coded; no `LLMClient` interface, no OpenAI / Anthropic / Ollama indirection.
- **Default-deny depth gate** — `settings.subagents.max_depth` defaults to `0`, which causes every `spawn_subagent` call to return `{"error":"subagents_disabled","hint":"Raise subagents.max_depth in Settings"}`. Raising the slider to 1–16 enables progressively deeper nesting; exceeding the cap returns `{"error":"max_depth_exceeded","limit":N}`. No server restart required.
- **`SubAgentManager`** — new class (`src/agents/SubAgentManager.{hpp,cpp}`) owns task lifecycle (pending → running → succeeded | failed | cancelled), spawns a background thread per task, and serves `wait(task_id, timeout_ms)` blocking via `condition_variable` so polling SQLite is avoided.
- **`ScopedToolExecutor`** — new subclass (`src/agents/ScopedToolExecutor.{hpp,cpp}`) of `ToolExecutor` that enforces `allowed_paths` (glob match, canonicalised) and `allowed_tools` (whitelist intersected with Agent-mode tools) on every call. Shell and `run_tests` are denied unless explicitly whitelisted. `spawn_subagent` from a sub-agent is gated by both depth budget and the parent's `deniedTools`.
- **`LeaseManager`** — new class (`src/agents/LeaseManager.{hpp,cpp}`) implementing write leases. Before any mutating tool call on a path, the executor acquires an exclusive lease via `INSERT OR FAIL INTO agent_leases`. On conflict, the tool returns `{"error":"resource_locked","holder":"<task_id>","expires_in_s":N}` — the agent is expected to `wait_subagent(holder)` or re-plan. Leases are released on task terminal-state transition; a periodic sweeper reaps stale entries.
- **Schema migration v6** — adds `agent_tasks` (id, parent_task_id, session_id, goal, allowed_paths/tools JSON, depth, status, result, error, token counters, timestamps, cancel_requested) and `agent_leases` (task_id, resource_key UNIQUE, acquired_at, expires_at). Indexes on parent / status / session for cheap listing.
- **API** (new `src/server/RoutesSubAgents.cpp`): `POST /api/tasks` spawns from outside the agent loop, `GET /api/tasks` lists with `parent` / `status` filters, `GET /api/tasks/:id` returns status + result, `POST /api/tasks/:id/cancel`.
- **Tools**: `spawn_subagent` (Agent), `wait_subagent` (Agent, blocks with timeout), `cancel_subagent` (Agent), `list_subagents` (Question).
- **System prompt addendum** — `GROK_SYSTEM_PROMPT.md` now includes guidance on when to delegate, how to partition write paths across siblings, and the xAI-only constraint.

### Cross-cutting — Settings, validators, UI

- **New settings keys** (`src/config/PlatformSettings.{hpp,cpp}`) — `services_workdir_root` (string path, default `~/.avacli/services`), `apps_db_size_cap_mb` (int, default 256, range 16–16384), `subagents_max_depth` (int, default 0, range 0–16). All read fresh on every call — no in-process cache, no restart needed after a save.
- **Settings page** — new "Platform capabilities" section on the Settings page (`ui/js/app.js`) with three controls: workdir text input + reset, DB cap slider + numeric input, sub-agent depth slider (with `0` labelled "Disabled"). Help text and tooltips explain each.
- **Server-side validators** — `POST /api/settings` rejects invalid values with HTTP 400 before any partial save: workdir must be an absolute path with an existing parent; DB cap and depth are range-checked.

### Files

**New:**
- `src/platform/ProcessSupervisor.{hpp,cpp}`, `src/platform/Runtime.{hpp,cpp}`, `src/platform/Hash.{hpp,cpp}`
- `src/config/PlatformSettings.{hpp,cpp}`
- `src/db/AppDatabase.{hpp,cpp}`
- `src/server/RoutesAppDB.cpp`, `src/server/RoutesSubAgents.cpp`, `src/server/AppTokens.{hpp,cpp}`
- `src/agents/SubAgentManager.{hpp,cpp}`, `src/agents/ScopedToolExecutor.{hpp,cpp}`, `src/agents/LeaseManager.{hpp,cpp}`
- `src/services/DepInstaller.{hpp,cpp}`

**Modified:** `src/db/Database.cpp` (migrations v4/v5/v6), `src/services/ServiceManager.{hpp,cpp}` (Tick vs Process split, supervisor wiring), `src/server/HttpServer.cpp` (app-SDK auth bypass, sub-agent config wiring), `src/server/Routes.hpp`, `src/server/RoutesServices.cpp` (status + signal + SSE logs), `src/server/RoutesApps.cpp` (agent_token issuance + `_sdk.js` injection), `src/server/RoutesSettings.cpp` (new settings keys + validators), `src/tools/ToolRegistry.cpp` (9 new tool schemas, TOOL_MIN_MODE updates), `src/tools/ToolExecutor.{hpp,cpp}` (new dispatchers; `execute` virtual so `ScopedToolExecutor` can wrap), `ui/js/app.js` (Platform capabilities settings section), `packaging/build-deb.sh`, `packaging/homebrew/avacli.rb`, `CMakeLists.txt`.

### Notes

- **xAI only, by design** — no `LLMClient` interface, no OpenAI / Anthropic / Ollama indirection. Sub-agents use the same `XAIClient` as the root chat, with only the `model` string varying. Reintroducing multi-provider later is a one-file refactor.
- **Per-user filesystem** — service workdirs and app DBs live under `$HOME/.avacli/`. No `/var/lib/avacli` is created. Multi-user installs are expected to use dedicated service accounts.
- **Non-goals**: Windows SCM / launchd handoff (optional phase 5), containerised service isolation, cross-node sub-agent delegation, per-call cost metering. Token counters exist on `agent_tasks`; dollar cost is a follow-up.

---

## [2.1.0] — 2026-04-09

**Detached UI: the web frontend is now served from disk, enabling instant edits, theming, and customization without recompiling.**

### New features

- **Disk-based UI serving** — the embedded monolithic HTML/CSS/JS frontend has been extracted into a standalone `ui/` directory. The server reads files from disk at request time, so any edit to HTML, CSS, or JS is reflected on the next browser refresh — zero recompile, zero restart.
- **`--ui-dir` flag** — point `avacli serve` at any directory to serve a completely custom frontend. Defaults to `~/.avacli/ui/` if it exists, otherwise falls back to the compiled-in embedded assets.
- **`--ui-embedded` flag** — force the compiled-in UI even when a disk `ui/` directory exists, for debugging or rollback.
- **`--ui-theme` flag** — load a theme CSS overlay from the `ui/themes/` directory (e.g. `--ui-theme cyberpunk`).
- **`--ui-init` flag** — extract the built-in embedded UI to `~/.avacli/ui/` (or the path given by `--ui-dir`) as a starting point for customization.
- **Theme system** — `ui/css/variables.css` defines all design tokens (colors, fonts, radii). Drop a CSS file in `ui/themes/` to override them. Ships with `default`, `light`, and `cyberpunk` themes.
- **Theme persistence** — active theme is saved in `settings.json` (`ui_theme` key) and loaded on startup.

### Architecture

- **`UIFileServer`** — new C++ class (`src/server/UIFileServer.hpp/cpp`) handles disk-based file serving with MIME type detection, path traversal protection, and automatic SPA fallback (serves `index.html` for unmatched routes).
- **Embedded fallback preserved** — `EmbeddedAssets.hpp` remains in the binary. If no `ui/` directory is found and `--ui-dir` is not set, the server serves the compiled-in UI exactly as before. Single-binary deployment still works.
- **Extracted frontend** — `ui/index.html` loads `css/variables.css`, `css/style.css`, and `js/app.js` as separate files instead of one inlined monolith.

### CLI changes

```
avacli serve                             # disk UI if available, else embedded
avacli serve --ui-dir ./my-custom-ui     # serve from any directory
avacli serve --ui-embedded               # force compiled-in UI
avacli serve --ui-theme light            # apply a theme
avacli serve --ui-init                   # extract embedded UI to disk
avacli serve --ui-init --ui-dir ./my-ui  # extract to a custom path
```

### New files

- `src/server/UIFileServer.hpp` / `UIFileServer.cpp`
- `ui/index.html`, `ui/css/variables.css`, `ui/css/style.css`, `ui/js/app.js`
- `ui/themes/default.css`, `ui/themes/light.css`, `ui/themes/cyberpunk.css`
- `scripts/extract-ui.py` — re-extract UI from `EmbeddedAssets.hpp`

---

## [2.0.0] — 2026-04-08

**Major release: SQLite persistence, Apps platform, Services engine, Knowledge Base, and agent reliability overhaul.**

### New features

- **SQLite database** — persistent storage for apps, services, articles, event logs, and service logs. Auto-initializes on first run with schema versioning and migrations.
- **Apps platform** — create, edit, and serve user-built web apps directly from the database (`/apps/<slug>/`). Agent can build full HTML/CSS/JS apps via `create_app` + `edit_app_file`. Phone-style app drawer UI with context menus (right-click on desktop, long-press on mobile).
- **Background services** — `create_service`, `manage_service`, `delete_service` tools for scheduled prompts, RSS feeds, and custom scripts. Auto-stops after 5 consecutive failures to prevent runaway loops.
- **Knowledge Base** — `save_article` and `search_articles` tools let the agent persist research, plans, and notes across sessions. Dedicated UI page with search.
- **Database query tool** — `db_query` gives the agent direct SQL access to the application database for reading app data, service data, articles, and logs.
- **Dashboard landing page** — new home screen with quick stats (apps, services, sessions, articles), feature cards, and recent activity feed pulled from the event log.
- **Vultr account info** — new `/api/vultr/account` endpoint; deploy now supports `user_data` (cloud-init) via `VultrDeployUserdata` templates.
- **Event log persistence** — `LogBuffer` now dual-writes to SQLite `event_log` table with category/level filtering and full-text search from the Logs page.
- **Asset analysis modal** — click the brain icon on any analyzed asset to view full AI analysis, tags, and re-analyze.

### Agent reliability fixes

- **`edit_app_file` validates app_id** — returns a clear error with available apps list if the ID doesn't exist, instead of a raw SQL foreign key error. Tool description explicitly says "use the ID from create_app, NOT the name."
- **`create_service` requires config** — rejects empty config objects with type-specific required fields and examples. `manage_service` (start) also validates config before starting.
- **New `delete_service` tool** — the agent can now properly delete services instead of trying `delete_tool` (wrong tool).
- **ServiceManager auto-stop** — after 5 consecutive tick failures, the service is stopped and set to `error` status with a clear log message.
- **ServiceManager thread safety** — fixed `stopFlag` reference-after-move bug by using raw pointer to the pre-moved `RunningService`.
- **ServiceManager config validation** — refuses to start services with missing required fields (e.g. no `prompt` for `scheduled_prompt`).
- **`research_api` chains to `setup_api`** — return value now includes `_next_steps` instructing the agent to call `setup_api` then `vault_store`. Tool description also says this explicitly.
- **HTML entity decode safeguard** — `create_app`, `edit_app_file`, and the app file PUT endpoint all decode `&lt;`/`&gt;`/`&amp;`/`&quot;` to prevent models that HTML-encode their tool arguments from producing broken output.

### Security hardening

- **Search result trust metadata** — `web_search` and `x_search` now tag results as `untrusted_public` and flag possible context poisoning (prompt injection phrases in summaries).
- **Agent system prompt updated** — search tools described as returning untrusted content; agent instructed to verify facts and ignore embedded directives.
- **Interactive shell warnings** — `run_shell` detects commands that likely need a TTY (vim, nano, ssh, mysql, etc.) and returns an `interactive_warning` advising non-interactive alternatives.
- **Shell stdin disconnected** — `ProcessRunner` now redirects stdin from `/dev/null` on Unix to prevent commands from hanging on input.
- **Shell timeout notes** — timed-out commands include a `note` field explaining partial output and suggesting alternatives.

### UI improvements

- **App drawer** — grid layout with gradient letter avatars (or custom icons); context menu with Open, Edit in Files, Rename, Generate Icon, Delete.
- **Files page integration** — "Edit in Files" exports app files to the workspace, navigates to the Files page, and auto-expands the app directory.
- **Logs page overhaul** — new category filters (auth, deploy, assets, knowledge, apps, services, db), search bar, stats counters, expandable detail rows, date display.
- **Sidebar additions** — Knowledge Base, Apps, and Services now appear in navigation.
- **New icons** — book, grid, chevLeft, pause added to the icon set.

### Build & packaging

- **SQLite3 dependency** — `find_package(SQLite3 REQUIRED)` added to CMakeLists.txt; linked as `SQLite::SQLite3`.
- **New source files** — `Database.cpp`, `ServiceManager.cpp`, `RoutesApps.cpp`, `RoutesDB.cpp`, `RoutesKnowledge.cpp`, `RoutesServices.cpp`, `VultrDeployUserdata.cpp`.
- **Version bumped to 2.0.0** in CI workflow, Makefile, and build-deb.sh.
- **Homepage URL corrected** to `https://github.com/iBerry420/acos` in all packaging metadata.
- **Debian depends** updated to include `libssl3 | libssl1.1`.

### Logging improvements

- Auth events (login success, failed attempts, logout) now logged.
- Settings changes (model, workspace) logged.
- Asset operations (upload, delete, folder create, analyze) logged.
- Chat session operations (load, delete) logged.
- Vultr deploy/destroy operations logged with context.
- API registry operations logged.

---

## [1.0.0] — 2025-04-05

**Initial Open Source Release**

The repository has been restructured, rebranded as **avacli (open source)**, made fully MIT licensed, and open-sourced. It is a single-binary C++20 autonomous AI coding agent with embedded WebIDE, tool forge, vault, media tools, Vultr integration, and self-improvement capabilities.

See README.md for full build/run instructions, features, and agent tools overview.

**Key changes from prior versions:**
- Single binary with minimal deps (libcurl + OpenSSL).
- Agent-first architecture using Grok API + extensible Python tool forge.
- Web UI with WebIDE for editing the codebase itself.
- Packaging pipeline for easy distribution.
- Consistent **avacli (open source)** naming while keeping binary name `avacli`.

For detailed history of UI/features (mobile SPA, node syncing, etc.), see prior internal notes.

## Older Changes
*(Pre-open-source entries abbreviated for this release.)*
