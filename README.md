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
| **Background services** | Scheduled prompts, RSS feeds, and custom scripts that run on intervals with auto-stop on repeated failures. |
| **Knowledge Base** | Persistent article storage for research, plans, and notes that survive across chat sessions. |
| **SQLite database** | Built-in persistence for apps, services, articles, event logs — no external database required. |

Runtime system dependencies are **libcurl**, **OpenSSL**, and **SQLite3**. Other libraries (**CLI11**, **nlohmann/json**, **spdlog**, **cpp-httplib**) are pulled at configure time via CMake `FetchContent`.

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
| Apps | `create_app`, `edit_app_file`, `list_apps` |
| Services | `create_service`, `manage_service`, `delete_service`, `list_services` |

Which tools are exposed depends on **mode** (`getToolsForMode`).

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
./packaging/build-all.sh 2.0.0
./packaging/build-deb.sh 2.0.0 amd64 build/avacli ./dist
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
