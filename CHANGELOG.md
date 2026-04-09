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
