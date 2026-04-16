# avacli — open source

**avacli** (this repository) is the **open-source** autonomous AI coding agent: a single C++20 binary that talks to [xAI Grok](https://x.ai/) and can run as a **headless CLI** or as an **embedded HTTP server** with a mobile-friendly web UI.

**Naming:** This repository is **avacli (open source)** — a local-first agent and tooling stack, not a cloud "operating system" product or separate hosted service.

---

## What you get

| Capability | Description |
|------------|-------------|
| **Grok agent** | Multi-turn loop: model proposes tool calls → your workspace executes them → results return to the model (bounded turn count). |
| **`serve`** | cpp-httplib server: detached UI (served from disk, themeable, hot-reloadable) + `/api/*` (chat with SSE, files, settings, auth, tools, vault, sessions, optional Vultr helpers). Falls back to compiled-in assets if no `ui/` directory exists. |
| **Headless CLI** | Non-interactive runs: `-t` / `chat <msg>`, optional stdin pipe; output as plain text, JSON summary, or **stream-json** (line-delimited events for scripts). |
| **Sessions** | Named sessions persist conversation, todos, memory snippets, and edit history under your config directory. |
| **Vault** | AES-256-GCM encrypted storage (OpenSSL) for secrets the agent can use with vault tools. |
| **Custom tools** | JSON tool definitions in `~/.avacli/custom_tools/` (names must start with `custom_`). |
| **Apps platform** | Build and serve full web apps (HTML/CSS/JS) from the embedded database. Agent can scaffold apps via `create_app` + `edit_app_file`. |
| **Per-app SQLite** | Every generated app gets its own sandboxed `~/.avacli/app_data/<slug>.db` with an auto-injected `window.avacli` SDK (`db`, `main`, `ai`). Agent tokens + authorizer hooks keep apps inside their own sandbox. |
| **Background services** | Scheduled prompts, RSS feeds, custom scripts, and **long-running `process` services** (Python / Node / native binaries) supervised cross-platform with restart policies, exponential backoff, and per-service venv / npm install. |
| **Sub-agents (scoped)** | The agent can delegate bounded work to child agents with `spawn_subagent {goal, allowed_paths, allowed_tools}`. Lease-based write coordination prevents siblings from clobbering each other. xAI-only, depth-capped in Settings, disabled by default. |
| **Knowledge Base** | Persistent article storage for research, plans, and notes that survive across chat sessions. |
| **SQLite database** | Built-in persistence for apps, services, articles, event logs, agent tasks — no external database required. |

Runtime system dependencies are **libcurl**, **OpenSSL**, and **SQLite3**. Other libraries (**CLI11**, **nlohmann/json**, **spdlog**, **cpp-httplib**) are pulled at configure time via CMake `FetchContent`.

**Optional runtimes** (only needed if you plan to host long-running Python or Node services through the process supervisor — e.g. a Discord bot): `python3` + `python3-venv`, or `nodejs` + `npm`. The Debian package declares these as `Recommends`; they are **not** required for core avacli functionality.

---

## How it works (architecture)

### Process and entry point

1. **`main`** parses CLI flags and subcommands (`serve`, `chat`, `login`, `logout`), resolves workspace (default `.`), loads or generates model metadata, and constructs **`Application`**.

2. **`Application::run`** branches:
   - **`serve` mode** (explicit `serve` subcommand **or** default when you run `avacli` with no task and no other exit-only flags): starts **`HttpServer`**; no xAI key is required at process start — keys can be supplied through the UI/settings.
   - **Headless mode**: requires `XAI_API_KEY` or a key saved with `--set-api-key`; runs **`Application::runHeadless`** → **`AgentEngine`** with **`XAIClient`** calling `https://api.x.ai/v1/chat/completions`.

3. **`AgentEngine`** maintains the message list, calls Grok with the current tool schema for the active **mode** (`question`, `plan`, `agent`), and executes tool calls through **`ToolExecutor`** using definitions from **`ToolRegistry`**. Tool output is fed back until the model finishes or limits hit (e.g. max tool turns, context warnings).

4. **`HttpServer`** wires **route modules** (`RoutesAuth`, `RoutesChat`, `RoutesFiles`, `RoutesSettings`, `RoutesTools`, `RoutesData`, `RoutesInfra`, `RoutesApps`, `RoutesServices`, `RoutesKnowledge`, `RoutesDB`, …), **session cookies / master-key auth**, and serves the **frontend UI**. By default, the UI is served from a `ui/` directory on disk via **`UIFileServer`**, with automatic fallback to compiled-in **`EmbeddedAssets`** if no disk UI is found. Chat streaming uses **Server-Sent Events** where applicable.

### Context the agent sees

Per run, context can include:

- **`GROK_NOTES.md`** in the workspace (project notes snapshot).
- **Session todos** (active items), **pinned + recent memory**, and **recent file edits** (when a session id is set).

### Data on disk

Typical layout (see `platform/Paths` and config code for exact rules):

- **`~/.avacli/config.json`** — xAI API key storage (`--set-api-key`).
- **`~/.avacli/avacli.db`** — SQLite database (apps, services, articles, event logs, service logs).
- **`~/.avacli/`** — accounts/master key material, vault, sessions, logs, and **`custom_tools/`** for JSON-defined tools.

Treat this directory like secrets: backup and permissions matter, especially if you expose `serve` beyond localhost.

### Built-in tools (summary)

Registered in **`ToolRegistry::registerAll`**:

| Category | Tools |
|----------|-------|
| Filesystem | `read_file`, `search_files`, `list_directory`, `glob_files`, `edit_file`, `write_file`, `undo_edit` |
| Web | `read_url`, `web_search`, `x_search` |
| Execution | `run_shell`, `run_tests` |
| Context | `project_notes_read`, `project_notes_update`, `memory_store`, `memory_list`, `ask_user`, `summarize_and_new_chat` |
| Media | `generate_image`, `generate_video`, `search_assets` |
| Tool forge | `create_tool`, `modify_tool`, `delete_tool`, `list_tools` |
| API integration | `research_api`, `setup_api`, `call_api` |
| Vault | `vault_store`, `vault_retrieve`, `vault_remove` |
| Database | `db_query` |
| Knowledge | `save_article`, `search_articles` |
| Apps | `create_app`, `edit_app_file`, `list_apps`, `app_token`, `app_db_execute`, `app_db_set_cap` |
| Services | `create_service`, `manage_service`, `delete_service`, `list_services`, `tail_service_logs` |
| Sub-agents | `spawn_subagent`, `wait_subagent`, `cancel_subagent`, `list_subagents` |
| Batch API (v2.3) | `batch_submit`, `get_batch`, `list_batches`, `get_batch_results`, `cancel_batch` |

Which tools are exposed depends on **mode** (`getToolsForMode`).

### Platform capabilities (v2.2 — services, app DB, sub-agents)

Three knobs in the Settings page control how much the agent is allowed to do:

| Setting | Default | Range | Effect |
|---|---|---|---|
| `services_workdir_root` | `~/.avacli/services` | absolute path | Where per-service workdirs (`venv/`, `node_modules/`, logs) live. Per-user, no `/var/lib/avacli/`. |
| `apps_db_size_cap_mb` | `256` | `16`–`16384` | Global ceiling for per-app SQLite size. Per-app override via `apps.db_size_cap_mb` column; writes over quota return HTTP 507. |
| `subagents_max_depth` | `0` (disabled) | `0`–`16` | Nesting cap for `spawn_subagent`. `0` disables the feature entirely. Raising the slider takes effect without a server restart. |

**Long-running process services** — `create_service` now accepts `type: "process"` with a runtime (`python` / `node` / `bin`), entrypoint, args, env (secrets via `vault:<name>` are resolved at spawn), working dir, and restart policy (`always` / `on_failure` / `never` with exponential backoff). The supervisor auto-bootstraps a per-service venv or `node_modules` on first start and rehashes `requirements.txt` / `package.json` to skip reinstalls. SSE log stream at `GET /api/services/:id/logs/stream`; status at `GET /api/services/:id/status`; signal at `POST /api/services/:id/signal` with `{"signal": "term"|"kill"|"restart"}`.

**Per-app SQLite** — every app gets `~/.avacli/app_data/<slug>.db`, opened with WAL + FK + an `sqlite3_set_authorizer` hook that denies `ATTACH` / `DETACH` and restricts pragmas. Generated HTML is served with `<script src="_sdk.js">` injected, exposing:

```js
window.avacli.db.query(sql, params)     // POST /api/apps/:slug/db/query
window.avacli.db.execute(sql, params)   // POST /api/apps/:slug/db/execute
window.avacli.db.schema()               // GET  /api/apps/:slug/db/schema
window.avacli.main.query(viewName, p)   // whitelisted main DB views only
```

Endpoints auth with the per-app `agent_token` (bearer + same-origin referer). A whitelist of views (`articles_public`, `apps_directory`, `my_app`) is the only path into the main DB — apps never run raw SQL there.

**Sub-agents** — `spawn_subagent` starts a child `AgentEngine` on its own thread with a `ScopedToolExecutor` that enforces `allowed_paths` (glob match) and `allowed_tools` (whitelist). Write leases in `agent_leases` serialize sibling writes to the same path; the second writer gets `{"error":"resource_locked","holder":"<task_id>"}` and is expected to `wait_subagent(holder)`. All sub-agents use `XAIClient` — no OpenAI / Anthropic / Ollama. Tasks are visible at `GET /api/tasks`, with SSE event streams at `GET /api/tasks/:id/events/stream`.

### Cost optimization (v2.3 — prompt cache + Batch API)

xAI's prompt cache rewards **byte-stable prefixes**; repeated system messages that differ by a timestamp or a per-run UUID get 0% cache hits. v2.3 re-engineers the request builder around that constraint and adds a Batch API surface for workloads that don't need a live SSE stream.

| Feature | Effect | Where |
|---|---|---|
| **Byte-stable system prefix** | Dynamic context (`GROK_NOTES.md`, todos, memory, edit history) is split out as a separate `system` message *after* the stable prompt, so the prefix is identical across turns and users. | `AgentEngine::runChatCompletions`, `runResponses` |
| **Sticky `conv_id` routing** | `X-Xai-Conv-Id` header on chat-completions + `conv_id` body field on the Responses API pin cache routing to the same server shard for the life of a conversation. Inherited by sub-agents so tool-heavy sessions stay warm. | `XAIClient::buildRequestBody`, `SessionManager` |
| **Cache-hit telemetry** | Every response logs `[cache] prompt=X cached=Y ratio=Z%`. `cached_tokens`, `reasoning_tokens`, `billable_prompt_tokens`, and `cache_hit_rate` are surfaced on SSE `token_usage` frames, the headless `--format json` summary, the sub-agent `usage` event, and the `/usage` dashboard. | end-to-end |
| **`previous_response_id` chaining** | Responses API turns send only new items (tool results, next user message) + `previous_response_id` — the server reconstructs history from its 30-day retention. Transparent full-resend fallback on stale-ID errors. | `XAIClient::responsesStream`, `AgentEngine::runResponses` |
| **Reasoning preservation** | Chat-completions reasoning models stream `reasoning_content` through a dedicated callback; it's stored per-message and echoed byte-identically on replay so reasoning stays inside the cached prefix. | `Message::reasoning_content`, `SessionManager` |
| **Batch API** | Flat **50% discount** on `/v1/messages/batches`, stacks with prompt caching for **~20× effective discount** on offline workloads. New `batches` table (migration v7), background `BatchPoller` reconciles state every 30s, full REST surface `/api/batches/*` + five agent tools. | `BatchClient`, `BatchPoller`, `RoutesBatches` |

Typical session goes from **~0% cache hits** (pre-v2.3, broken by per-run UUIDs in the system prompt) to **70–90% from turn 2 onward** — a 5–10× cost drop on interactive chat, plus another 2× on top for anything batched.

### Agent reliability features

- **Tool argument validation** — tools like `edit_app_file` verify foreign keys exist before INSERT and return helpful hints.
- **Config validation** — `create_service` rejects empty config with type-specific examples; `manage_service` validates before start.
- **Search trust metadata** — web/X search results are tagged `untrusted_public` with context-poisoning detection for prompt injection attempts.
- **Shell safety** — `run_shell` detects interactive commands (vim, nano, ssh) and warns; stdin is disconnected (`/dev/null`); timeouts produce partial-output notes.
- **HTML entity decode** — safeguard against models that HTML-encode their tool arguments.

---

## Requirements

- **CMake** ≥ 3.22
- **C++20** compiler (GCC ≥ 10, Clang ≥ 12, or recent MSVC)
- **libcurl** (dev package)
- **OpenSSL** (dev package — e.g. `libssl-dev` on Debian/Ubuntu for headers)
- **SQLite3** (dev package — e.g. `libsqlite3-dev` on Debian/Ubuntu)

Optional: **Ninja** (used by CMake presets), **vcpkg** on Windows.

---

## Building

### Linux — Debian / Ubuntu (`build.sh`)

```bash
git clone https://github.com/iBerry420/acos.git
cd acos
chmod +x build.sh
./build.sh
# Binary: ./build/avacli
```

The script installs, when `apt-get` is available: `build-essential`, `cmake`, `libcurl4-openssl-dev`, **`libssl-dev`**, and **`libsqlite3-dev`**.

### Linux — manual (any distro)

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
```

**Package examples**

- **Debian / Ubuntu:** `sudo apt-get install build-essential cmake libssl-dev libcurl4-openssl-dev libsqlite3-dev`
- **Fedora / RHEL (dnf):** `sudo dnf install gcc-c++ cmake openssl-devel libcurl-devel sqlite-devel`
- **Arch:** `sudo pacman -S base-devel cmake openssl curl sqlite`

### macOS (Homebrew)

```bash
xcode-select --install   # if needed
brew install cmake openssl curl sqlite
export CMAKE_PREFIX_PATH="$(brew --prefix openssl);$(brew --prefix curl);$(brew --prefix sqlite)"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(sysctl -n hw.ncpu)"
```

If CMake still misses Curl/OpenSSL/SQLite, pass `-DCURL_ROOT=...`, `-DOPENSSL_ROOT_DIR=...`, and `-DSQLite3_DIR=...` pointing at the Homebrew prefixes.

### macOS / Linux — CMake preset (Ninja)

```bash
cmake --preset macos
cmake --build --preset macos
# Binary: ./build/macos/avacli
```

### Windows (Visual Studio + vcpkg)

1. Install [vcpkg](https://vcpkg.io/) and set **`VCPKG_ROOT`**.
2. Triplet **`x64-windows`** should provide **`curl`**, **`openssl`**, and **`sqlite3`**.
3. From a **Developer Command Prompt**:

```powershell
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg --config Release
```

Release binary is under **`build/windows-vcpkg/`** (e.g. `Release\avacli.exe` with MSVC — exact path follows your generator).

---

## Running

### Web UI + API (`serve`)

```bash
./build/avacli serve
./build/avacli serve --port 9090 --host 127.0.0.1
```

Default bind: **`0.0.0.0:8080`** (if the port is busy, the server may pick the next free port and log it).

Create the admin user for the UI:

```bash
./build/avacli --generate-master-key
# or
./build/avacli --set-master-key 'your-secure-password'
```

Then open the printed URL, sign in (default username **`admin`**), and configure xAI (and optional Vultr, etc.) in **Settings** if you did not set a key on the host.

### Detached UI (theming & customization)

The web frontend lives in a `ui/` directory on disk, separate from the binary. Edit any file and refresh your browser — no recompile needed.

```bash
# Serve with disk-based UI (default behavior if ui/ exists)
./build/avacli serve --ui-dir ./ui

# Extract the built-in UI as a starting point for customization
./build/avacli serve --ui-init
./build/avacli serve --ui-init --ui-dir ./my-custom-ui

# Apply a theme
./build/avacli serve --ui-theme light
./build/avacli serve --ui-theme cyberpunk

# Force the compiled-in UI (ignore disk)
./build/avacli serve --ui-embedded
```

**UI directory layout:**

```
ui/
├── index.html              # App shell
├── css/
│   ├── variables.css       # Design tokens (colors, fonts, radii)
│   └── style.css           # All styles
├── js/
│   └── app.js              # Application logic
└── themes/
    ├── default.css          # Empty (uses variables.css as-is)
    ├── light.css            # Light theme
    └── cyberpunk.css        # Cyberpunk theme
```

To create a custom theme, add a CSS file to `ui/themes/` that overrides the variables in `variables.css`, then pass `--ui-theme <name>` or set `"ui_theme"` in `~/.avacli/settings.json`.

If no `ui/` directory is found and `--ui-dir` is not specified, the server falls back to the compiled-in embedded assets — the binary still works as a single-file deployment.

### Headless / automation

Requires **`XAI_API_KEY`** or **`avacli --set-api-key`**.

```bash
export XAI_API_KEY=xai-...
./build/avacli chat "Summarize this project"
./build/avacli --non-interactive -t "Run the test suite" --workspace /path/to/repo
./build/avacli --format stream-json -t "List files"   # machine-readable events
./build/avacli --list-models
```

### Other CLI helpers

```bash
./build/avacli --set-api-key "xai-..."
./build/avacli login    # verify master key (stdin password)
./build/avacli logout
```

### Prebuilt Linux helper

If you ship a binary next to **`install.sh`**, that script can install to `/usr/local/bin` and check **libcurl** (oriented toward Linux x86_64). Other architectures: build from source.

---

## Packaging (maintainers)

```bash
./packaging/build-all.sh 2.3.0
./packaging/build-deb.sh 2.3.0 amd64 build/avacli ./dist
```

See **`packaging/`** for distribution notes.

---

## Security

- Protect **`~/.avacli/`** (API keys, vault, sessions, database).
- Master-key auth is aimed at **local or trusted networks**. For internet exposure, use **TLS termination**, firewalls, and strong passwords.
- Prompts and tool use send data to **xAI** (and any third-party APIs you configure); review their terms and key scope.
- Web and X search results are tagged as untrusted — the agent is instructed to verify facts and ignore adversarial instructions embedded in search results.

---

## Contributing / GitHub

Maintainer: **iBerry420**

- License: **MIT** — [LICENSE](LICENSE)
- Changelog: [CHANGELOG.md](CHANGELOG.md)
