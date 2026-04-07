# Project Notes — avacli (open source)

## Architecture
- Single-binary C++20 application (`avacli` target via CMake).
- Uses FetchContent for CLI11, nlohmann/json, spdlog, cpp-httplib.
- Links against libcurl, OpenSSL (for AES-256-GCM vault).
- Core: AgentEngine, HttpServer (httplib), ToolRegistry/Executor, VaultStorage, XAIClient, VultrClient.
- Embedded SPA served from memory or files; WebIDE for self-editing.
- Tools implemented as extensible registry with Python forging capability for custom tools.
- Memory: Session + persistent GROK_NOTES.md + TODOs + vault.

## Key Decisions
- Single binary for easy deployment/edge use (no runtime deps beyond libcurl).
- MIT license for true open source (updated packaging from proprietary).
- Agent-first development: Use the tools in this very system to improve itself (README rewrite, packaging fixes).
- Device flow + gh CLI for auth in remote/headless environments.
- Consistent **avacli (open source)** naming across docs/packaging; binary name remains `avacli`.

## Gotchas
- Root ownership on files in containerized env — use sudo where needed for install/build.
- Binary name inconsistency (root `avalynnai` vs build/`avacli`) — ignored in .gitignore; build from source recommended.
- Packaging scripts were outdated vs rebrand — now aligned with MIT and current quickstart.
- Large media generation tools are async; video tools have custom helpers.
- Git was uninitialized/broken (.git empty); now fixed with `main` branch.

## Setup Instructions
- `./build.sh` (installs deps, runs CMake + make).
- `./build/avacli serve` for web UI.
- `./packaging/build-all.sh <version>` for release artifacts.
- Set xAI key in UI or via vault tools.
- `git pull`, edit via WebIDE or `edit_file` tool.

## Integration Notes
- xAI/Grok API for agent reasoning.
- Vultr for cloud nodes.
- AES-256-GCM for master keys and vault.
- GitHub for packaging/distribution (APT repo, Homebrew, Releases).
- Custom tools auto-registered via Python scripts in tool forge.

## Future Work
- Complete remaining packaging scripts (homebrew, APT publish).
- Add GitHub Actions for CI/CD and auto-releases.
- Improve video/media pipeline (custom_trim_last_frame etc. are already available).
- Expand node relay features.
- Polish WebIDE UI further.
- Create official releases on GitHub under iBerry420/avalynn-ai.

Last updated: April 2025 by Grok agent.
