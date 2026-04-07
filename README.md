# avacli — open source

**avacli** (this repository) is the **open-source** autonomous AI coding agent: a single C++20 binary that talks to [xAI Grok](https://x.ai/) and can run as a **headless CLI** or as an **embedded HTTP server** with a mobile-friendly web UI.

**Naming:** This repository is **avacli (open source)** — a local-first agent and tooling stack, not a cloud “operating system” product or separate hosted service.

---

## What you get

| Capability | Description |
|------------|-------------|
| **Grok agent** | Multi-turn loop: model proposes tool calls → your workspace executes them → results return to the model (bounded turn count). |
| **`serve`** | cpp-httplib server: static SPA + `/api/*` (chat with SSE, files, settings, auth, tools, vault, sessions, optional Vultr helpers). |
| **Headless CLI** | Non-interactive runs: `-t` / `chat <msg>`, optional stdin pipe; output as plain text, JSON summary, or **stream-json** (line-delimited events for scripts). |
| **Sessions** | Named sessions persist conversation, todos, memory snippets, and edit history under your config directory. |
| **Vault** | AES-256-GCM encrypted storage (OpenSSL) for secrets the agent can use with vault tools. |
| **Custom tools** | JSON tool definitions in `~/.avacli/custom_tools/` (names must start with `custom_`). |

Runtime system dependencies are only **libcurl** and **OpenSSL**. Other libraries (**CLI11**, **nlohmann/json**, **spdlog**, **cpp-httplib**) are pulled at configure time via CMake `FetchContent`.

---

## How it works (architecture)

### Process and entry point

1. **`main`** parses CLI flags and subcommands (`serve`, `chat`, `login`, `logout`), resolves workspace (default `.`), loads or generates model metadata, and constructs **`Application`**.

2. **`Application::run`** branches:
   - **`serve` mode** (explicit `serve` subcommand **or** default when you run `avacli` with no task and no other exit-only flags): starts **`HttpServer`**; no xAI key is required at process start — keys can be supplied through the UI/settings.
   - **Headless mode**: requires `XAI_API_KEY` or a key saved with `--set-api-key`; runs **`Application::runHeadless`** → **`AgentEngine`** with **`XAIClient`** calling `https://api.x.ai/v1/chat/completions`.

3. **`AgentEngine`** maintains the message list, calls Grok with the current tool schema for the active **mode** (`question`, `plan`, `agent`), and executes tool calls through **`ToolExecutor`** using definitions from **`ToolRegistry`**. Tool output is fed back until the model finishes or limits hit (e.g. max tool turns, context warnings).

4. **`HttpServer`** wires **route modules** (`RoutesAuth`, `RoutesChat`, `RoutesFiles`, `RoutesSettings`, `RoutesTools`, `RoutesData`, `RoutesInfra`, …), **session cookies / master-key auth**, and serves **embedded frontend assets** (`EmbeddedAssets`) for `/`. Chat streaming uses **Server-Sent Events** where applicable.

### Context the agent sees

Per run, context can include:

- **`GROK_NOTES.md`** in the workspace (project notes snapshot).
- **Session todos** (active items), **pinned + recent memory**, and **recent file edits** (when a session id is set).

### Data on disk

Typical layout (see `platform/Paths` and config code for exact rules):

- **`~/.avacli/config.json`** — xAI API key storage (`--set-api-key`).
- **`~/.avacli/`** — accounts/master key material, vault, sessions, logs, and **`custom_tools/`** for JSON-defined tools.

Treat this directory like secrets: backup and permissions matter, especially if you expose `serve` beyond localhost.

### Built-in tools (summary)

Registered in **`ToolRegistry::registerAll`** — filesystem (read/search/list/glob), URLs, project notes, memory, web/X search, ask-user, edit/write/undo, shell, tests, todos, image/video generation hooks, **tool forge** (create/modify/delete/list custom tools), **API research/registry/call**, and **vault** store/list/get/remove. Which tools are exposed depends on **mode** (`getToolsForMode`).

---

## Requirements

- **CMake** ≥ 3.22  
- **C++20** compiler (GCC ≥ 10, Clang ≥ 12, or recent MSVC)  
- **libcurl** (dev package)  
- **OpenSSL** (dev package — e.g. `libssl-dev` on Debian/Ubuntu for headers)

Optional: **Ninja** (used by CMake presets), **vcpkg** on Windows.

---

## Building

### Linux — Debian / Ubuntu (`build.sh`)

```bash
git clone <your-repo-url>.git
cd <repo>
chmod +x build.sh
./build.sh
# Binary: ./build/avacli
```

The script installs, when `apt-get` is available: `build-essential`, `cmake`, `libcurl4-openssl-dev`, and **`libssl-dev`** (OpenSSL headers for CMake).

### Linux — manual (any distro)

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(nproc)"
```

**Package examples**

- **Debian / Ubuntu:** `sudo apt-get install build-essential cmake libssl-dev libcurl4-openssl-dev`  
- **Fedora / RHEL (dnf):** `sudo dnf install gcc-c++ cmake openssl-devel libcurl-devel`  
- **Arch:** `sudo pacman -S base-devel cmake openssl curl`

### macOS (Homebrew)

```bash
xcode-select --install   # if needed
brew install cmake openssl curl
export CMAKE_PREFIX_PATH="$(brew --prefix openssl);$(brew --prefix curl)"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j"$(sysctl -n hw.ncpu)"
```

If CMake still misses Curl/OpenSSL, pass `-DCURL_ROOT=...` and `-DOPENSSL_ROOT_DIR=...` pointing at the Homebrew prefixes.

### macOS / Linux — CMake preset (Ninja)

```bash
cmake --preset macos
cmake --build --preset macos
# Binary: ./build/macos/avacli
```

### Windows (Visual Studio + vcpkg)

1. Install [vcpkg](https://vcpkg.io/) and set **`VCPKG_ROOT`**.  
2. Triplet **`x64-windows`** should provide **`curl`** and **`openssl`**.  
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
./packaging/build-all.sh 1.1.0
./packaging/build-deb.sh 1.1.0 amd64 build/avacli ./dist
```

See **`packaging/`** for distribution notes. If downstream `.deb` metadata still mentions legacy names, align descriptions with **avacli (open source)** when you cut releases.

---

## Security

- Protect **`~/.avacli/`** (API keys, vault, sessions).  
- Master-key auth is aimed at **local or trusted networks**. For internet exposure, use **TLS termination**, firewalls, and strong passwords.  
- Prompts and tool use send data to **xAI** (and any third-party APIs you configure); review their terms and key scope.

---

## Contributing / GitHub

Maintainer: **iBerry420**

- License: **MIT** — [LICENSE](LICENSE)  
- Changelog: [CHANGELOG.md](CHANGELOG.md)  
