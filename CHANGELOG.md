## [2.3.6] — 2026-04-17

**Fix Windows CI so `v*` tag pushes can actually publish a GitHub Release.**

After tagging `v2.3.4` and `v2.3.5` I noticed both prior `main`-branch CI
runs had failed identically on the Windows job (Linux and both macOS
targets were green). Root cause turned out to be entirely cosmetic but
release-blocking:

- `scripts/bundle-default-prompt.py` and `scripts/bundle-ui.py` both
  print a success line ending in `... → src/server/<file>.hpp ...`
  after writing out the generated header. The bundling step *itself*
  succeeded on Windows (the `.hpp` files were written correctly — CI
  logs confirm `Processed ...: 187 original lines` / `6387 original
  lines, 31 splits inserted`). But Python 3.14 on the hosted Windows
  runner defaults `sys.stdout` to `cp1252`, which can't encode `\u2192`
  (`→`). The final `print()` raised `UnicodeEncodeError`, returning
  exit 1, which MSBuild surfaces as a custom-build failure — so the
  whole `avacli` target fails to link even though every input was
  already on disk.

Fix is a one-liner per script: reconfigure stdout to UTF-8 with a
fallback replace-error strategy, guarded by `try / except
(AttributeError, ValueError)` so it's a no-op on older Pythons or if
stdout isn't a `TextIOWrapper`. Keeps the nicer `→` in Linux/macOS
output, stops crashing on Windows.

```python
try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except (AttributeError, ValueError):
    pass
```

Because all four platform build jobs have to pass before the workflow's
`release` job fires (`needs: [build-linux, build-macos-arm64,
build-macos-x86_64, build-windows]`), the v2.3.4 and v2.3.5 tag pushes
both ended up with no GitHub Release auto-created. 2.3.6 is the first
tag in the 2.3.x series where the release workflow should finish end to
end and produce the multi-platform tarballs + `SHA256SUMS.txt` on the
GitHub Releases page. No runtime difference vs. 2.3.5 — same source,
same embedded prompt, same embedded UI.

Bumped `RelayClient.cpp` and `RoutesInfra.cpp` version strings plus
`.github/workflows/build.yml VERSION` to 2.3.6.

## [2.3.5] — 2026-04-17

**Fix the port-fallback footgun that briefly 502'd devacos during the
2.3.4 redeploy.**

Symptom: restarting `avacli.service` while Apache had live upstream
keep-alives open left port 9100 tied up in `TIME_WAIT` for ~60s. On
the new process's first bind attempt the kernel rejected the port, at
which point `findAvailablePort()` happily fell through to 9101 and
the server started there — but Apache kept proxying to 9100, so
`devacos.avalynn.ai` returned 502 Bad Gateway until the TIME_WAIT
entries drained and a manual re-restart landed the listener back on
9100. Two bugs in one place:

1. **Wrong socket option on the listen socket.** httplib's
   `default_socket_options` callback picks `SO_REUSEPORT` on Linux (or
   `SO_REUSEADDR` as a fallback), but those two flags do very different
   things here:
   - `SO_REUSEPORT` is the multi-process load-balancing flag. If both
     the old and new avacli set it, the kernel is *happy* to let them
     both bind to 9100 at the same time and randomly splits requests
     between them — actually *worse* than the original bug, because a
     stale old binary would silently answer half the traffic. (Verified
     this empirically with a local test — a second avacli instance
     with `SO_REUSEPORT` set bound successfully next to the running
     service.)
   - `SO_REUSEADDR` is the "please rebind over a lingering TIME_WAIT
     4-tuple" flag, which is what we actually need, and it enforces
     exclusive ownership between live listeners.
   We now install a custom `set_socket_options()` callback in
   `HttpServer::start()` that sets `SO_REUSEADDR` only on Linux
   (and `SO_REUSEADDR` + `SO_EXCLUSIVEADDRUSE` on Windows, since
   Windows' `SO_REUSEADDR` has inverted semantics and needs the
   `EXCLUSIVEADDRUSE` pin to keep single-listener behavior).
2. **Silent fallback to port+1 when `--port` was explicit.** The
   old `findAvailablePort()` + `svr.listen()` combo was fine for
   ad-hoc `avacli serve` on a developer laptop (conflict-free
   side-by-side dev instances), but wrong for a systemd-managed or
   reverse-proxied deployment where the configured port is part of
   the contract. `ServeConfig` now carries a `portExplicit` flag
   (plumbed from a new `Config::servePortExplicit`, which `main.cpp`
   populates from `CLI11`'s `->count()` on the `--port` option).
   When set, `HttpServer::start()` calls `bind_to_port()` once and,
   on failure, logs an error, sets `running_ = false`, and returns
   `false`. `Application::runServe()` propagates that to exit code 1
   so `Restart=always RestartSec=5` in the unit file does the right
   thing — the service manager retries on a 5-second cadence and
   Apache recovers on its own as soon as the port clears.

Implementation notes:

- The `HttpServer::start()` signature changed from `void` to `bool`.
  Only in-process caller is `Application::runServe()`, which now does
  `return server.start() ? 0 : 1;`.
- Switched from `svr.listen(host, port)` to the split
  `svr.bind_to_port(host, port)` → `svr.listen_after_bind()` pair so
  we can check the bind return value and surface the failure *before*
  blocking on `accept()`.
- Kept the probe-for-next-free-port behavior intact for the
  `portExplicit == false` path. `avacli serve` with no args still
  hops to 8081/8082/… when 8080 is busy; only explicit `--port N`
  now enforces a strict bind.

No config or API changes — the fix is purely in how the server
negotiates its own listen socket, so 2.3.5 is a drop-in for anything
that was running 2.3.4.

Bumped `RelayClient.cpp` and `RoutesInfra.cpp` version strings plus
`.github/workflows/build.yml VERSION` to 2.3.5.

## [2.3.4] — 2026-04-17

**Remote-UI iframe: make the proxy actually transparent.**

2.3.3 shipped with the Remote-UI overlay and the node-mesh chat hookup,
but a user who installed 2.3.3 from apt and opened a remote node saw:

1. The iframe rendered the remote's HTML shell, but the sidebar chip
   and "Local" node entry inside the iframe showed the **client's**
   workspace/path, not the remote's. User phrased this as *"the server
   shows the servers host instead of the client host"*.
2. Remote viewing showed the exact same workspace/chats/blueprints as
   the local client — nothing felt "remote".
3. Sending a chat message inside the iframe did nothing.

Root cause was entirely in the proxy layer (`RoutesInfra.cpp` GET + POST
handlers for `/api/nodes/:id/proxy/*`):

- **URL rewriting only ran on `text/html` responses.** The remote's
  `app.js` comes back as `application/javascript`, so the rewriter was
  completely skipped. Every `fetch()`, `api()`, `XMLHttpRequest`,
  `EventSource` call inside the iframe's JS resolved against the
  iframe's own origin (the client), not the remote. The iframe was
  effectively just the remote's HTML rendered over the client's data.
- **Even on HTML, the rewriter was pattern-based** — it only caught
  literal `fetch('/...')` / `fetch("/...")` / `src="/..."` /
  `href="/..."`. The UI's helper is `api(path)` which does
  `fetch(path, opts)` with a variable, so every single API call the
  UI makes was invisible to the rewriter.
- **The POST proxy buffered + JSON-parsed the upstream response.**
  For `text/event-stream` (chat), this replaced the entire stream
  with `{"error":"invalid JSON"}` after the libcurl timeout. Chat
  never streamed a single token through the iframe.

Fix has two parts, both server-side so the client just upgrades:

**(a) Inject a tiny JS shim at the top of `<head>` in any proxied
HTML.** It monkey-patches `window.fetch`, `XMLHttpRequest.prototype.open`,
`window.EventSource`, and `window.WebSocket` so any absolute-path URL
starting with `/` is transparently rewritten to `/api/nodes/:id/proxy<path>`.
This runs before any other script on the page, so by the time the
remote's `app.js` calls `api('/api/status')`, the wrapped `fetch`
redirects it through the proxy automatically. The shim also shadows
`window.localStorage` and `window.sessionStorage` with per-iframe
in-memory stores, so the iframe's session/token state is isolated
from the parent page (no accidental cross-contamination of
`ava_last_session`, etc.), and seeds a dummy `avacli_token` so the
remote UI starts "logged in" — the proxy route doesn't validate that
forwarded token, it authenticates to the remote using the node's
stored admin token on the server side.

The old text-based `src=`/`href=` rewriter stays as a belt-and-suspenders
fallback for no-JS paths (initial paint `<img>`, `<link rel>`, etc.).

**(b) Make the POST proxy a true streaming passthrough.** New
`NodeClient::proxyRawPost(path, body, contentType, onHeaders, onChunk)`
runs curl in a worker thread, fires `onHeaders` on first byte with the
upstream status + content-type, then fires `onChunk` for every chunk
as it arrives. The route handler parks the caller in a
`set_chunked_content_provider` loop fed by a lock+cv+queue, so SSE
tokens reach the browser live instead of being buffered for 10+
minutes and JSON-reparsed. The request `Content-Type` is also forwarded
now (so multipart uploads through the proxy would work, not just JSON).

Two small extras that fell out of the same diff:

- Added `/api/health` sibling endpoint `/api/hostname` returning
  `{"hostname": "<gethostname>"}` so a remote UI can show "you are on
  host X" without needing to cross-reference the nodes table.
- The GET + POST proxy routes now forward the querystring (`?foo=bar`)
  to the remote — previously stripped, which broke pagination on
  `/api/sessions`, `/api/logs`, etc.

Deploy: out-of-tree via `BUILD_DIR=build-deploy` so the running devacos
service (currently on 2.3.1) isn't replaced mid-session.

---

## [2.3.3] — 2026-04-17

**UI: Remote UI button, `@` mention popup, and persistent chat-target indicator.**

Three fixes to the node-mesh UX that only surfaced once people actually
tried to use the feature end-to-end on a fresh apt install:

1. **`Servers → Remote UI` did nothing.** The `<div id="remoteUiOverlay">`
   host-element was only emitted inside `renderChat()`, so
   `window.openRemoteUI()` would early-return on the Servers page
   (`getElementById('remoteUiOverlay') === null`). Promoted the overlay
   into the global `render()` app-shell so it's present on every route.
   Clicking `Remote UI` from `/servers` now mounts the iframe as
   expected; clicking again from the Nodes panel on `/chat` still works
   unchanged (the CSS `position:fixed; inset:0` makes DOM parent
   irrelevant).
2. **`@` in chat never opened the picker.** `setupMentionPicker` only
   rendered the popup if there were `status==='connected'` **remote**
   matches. A fresh install has zero nodes, so typing `@` silently did
   nothing and looked broken. Rewrote to always include a synthetic
   `Local (this node)` entry at the top, plus a dashed hint item
   *"No remote nodes connected — click to add one in Servers"* when the
   user has no connected remotes. ArrowUp/Down skip the hint item;
   selecting Local clears `S.activeNodeId` and strips the `@` fragment
   from the textarea; selecting a remote inserts `@<label>` as before
   (parsed per-message in `sendMsg`).
3. **No indication of current chat target.** The sidebar foot had a
   hardcoded `Local` badge that never reflected `S.activeNodeId`.
   Replaced with a live `chat-target-chip` rendered by
   `renderChatTargetChip()`:
   - **Local state** (green dot, workspace path): chat goes to this
     node.
   - **Remote state** (purple dot, arrow, node label + ip/latency, ✕
     button): chat is being proxied via `/api/nodes/<id>/chat` to the
     selected node. The ✕ calls `clearChatTarget()` which drops
     `S.activeNodeId` back to `null` and toasts confirmation.
   The chip also renders on non-chat routes, so you always see where
   chat will go. `updateChatTargetIndicator()` is invoked from
   `onNodeClick`, `insertMention` (when Local is picked), the
   `/api/status` poll, and `loadChatNodes` (which also scrubs a stale
   `activeNodeId` if the target got removed). The chat-input gets a
   subtle top-border accent (`chat-input-remote` class) while a remote
   target is active, so it's obvious while typing.

Deploy: bundled via `scripts/bundle-ui.py` → `EmbeddedAssets.hpp`, built
**out-of-tree** (`BUILD_DIR=build-deploy`) so the running devacos binary
at `/home/avalynnai_opensource/build/avacli` is not touched and systemd
doesn't trigger `Restart=always`.

---

## [2.3.2] — 2026-04-17

**P2P node mesh: fix "not avacli" when adding Apache/nginx-fronted nodes.**

Symptoms: adding a node pointing at an avacli server that lives behind
Apache (like `devacos.avalynn.ai`) with port 80 would mark the node as
**"not avacli"** regardless of token. Root cause: `NodeClient` probes
`/api/health` with libcurl, and the vhost 301-redirects `:80 → https://`.
libcurl was never told to follow redirects, so `NodeClient::checkHealth`
parsed the HTML redirect page as JSON, failed, and marked `isAvacli=false`.
The token was never the issue — `/api/health` is public; the token only
starts mattering once the node is marked `connected` and the client begins
proxying `/api/chats`, `/api/system/blueprints`, `/api/apps`, etc. via
`Authorization: Bearer <token>` (all of which already 401 on devacos
without it).

- **`src/client/NodeClient.cpp`** —
  - `get()`, `post()`, `proxyChatStream()` now set `CURLOPT_FOLLOWLOCATION=1`
    and `CURLOPT_MAXREDIRS=5`. POST variants additionally set
    `CURLOPT_POSTREDIR = CURL_REDIR_POST_{301,302,303}` so the chat-stream
    JSON body survives the http→https bounce (libcurl's default silently
    downgrades POST→GET on 301/302). The `Authorization` header is attached
    via `CURLOPT_HTTPHEADER` and libcurl retransmits it on same-host
    redirects, so tokens still reach the upstream.
  - `baseUrl()` rewritten to pick a scheme intelligently: honour an
    explicit `http://`/`https://` prefix on the host field, default
    `port=443 → https`, strip the default port from the canonical URL for
    cleanliness and to keep the `Host:` header the upstream vhost expects.
- **`ui/js/app.js`** — the Add Node modal now makes the port/scheme picker
  self-explanatory: "8080 for direct avacli · 80/443 for Apache/nginx-fronted
  (redirects are followed automatically)" and the password field is relabeled
  "Admin token" with a hint that it's sent as `Authorization: Bearer`.
  The `ui/` directory and the compiled-in fallback are re-bundled together
  via the existing CMake `bundle-ui` step, so both on-disk and embedded UIs
  match.

The existing Remote UI overlay (chat → nodes panel → globe icon on a
connected node → iframe at `/api/nodes/:id/proxy/`) already rewrites
`src`/`href`/`fetch('/` back through the proxy, so once a node flips from
`not_avacli → connected` its chat history, blueprints, tools and apps all
become browseable from the local UI. Verified end-to-end against devacos:
fresh 2.3.2 binary on port 9199 + host `devacos.avalynn.ai:80` →
`status:"connected", latency_ms:67, version:"2.3.1"`, and
`/api/nodes/:id/proxy/` returns devacos's full HTML with 4 rewritten proxy
paths.

---

## [2.3.2-default-prompt] — 2026-04-17 (rolled into 2.3.2)

**Ship a default system prompt + default blueprint out of the box.**

Fresh apt installs had an empty System page — no system prompt, no
blueprints, no reset target. Devacos works because its workspace ships
`GROK_SYSTEM_PROMPT.md` on disk; clean installs had nothing.

- **New `scripts/bundle-default-prompt.py`** bakes `GROK_SYSTEM_PROMPT.md` into a generated `src/server/DefaultSystemPrompt.hpp` header at build time (same pattern as `bundle-ui.py`). Exposes `getDefaultSystemPrompt()` and `getDefaultBlueprintJson()`.
- **CMake custom command** regenerates the header whenever `GROK_SYSTEM_PROMPT.md` changes; `build.sh` and `packaging/deploy-to-packages.sh` also invoke it explicitly so the published `.deb` can never drift from the canonical markdown.
- **`AgentEngine::buildUnifiedPrompt`** — the fallback used when no workspace `GROK_SYSTEM_PROMPT.md` exists is now the full baked-in default instead of a four-line skeleton, so the agent and the UI agree on what "default" means.
- **`RoutesSystem.cpp`** —
  - `GET /api/system/prompt` returns the embedded default (with `is_default: true`) when the workspace file is missing so the UI never shows an empty textarea.
  - New `GET /api/system/prompt/default` for previewing the shipped default.
  - New `POST /api/system/prompt/reset` overwrites `<workspace>/GROK_SYSTEM_PROMPT.md` with the embedded default.
  - New `POST /api/system/blueprints/reset` restores `~/.avacli/blueprints/default.json` to the canonical "Default Coding Agent" blueprint.
  - Server startup seeds `~/.avacli/blueprints/default.json` if missing; the blueprint list endpoint re-seeds on demand if the user wiped it, and always pins `default` to the top of the list.
- **UI (`ui/js/app.js`)** — new "Reset to Default" button on the System page next to Save (confirm dialog, writes the embedded default and re-renders). The prompt editor now shows a "Default (unsaved)" badge when the UI is displaying the fallback.

No behavior change for users who already have a workspace `GROK_SYSTEM_PROMPT.md` or a customized blueprints dir — existing files are untouched. Prompt and UI bundlers run independently; editing `GROK_SYSTEM_PROMPT.md` triggers a rebuild just like editing `ui/js/app.js` does.

---

## [2.3.1] — 2026-04-17

**Packaging fix — prerm self-kill during apt upgrade.**

- **`packaging/build-deb.sh`** — the `prerm` maintainer script used `pkill -f "avacli"` which, because `-f` matches against the full command line of every process, also matched `apt-get install avacli`, `dpkg --unpack .../avacli_*.deb`, and the prerm script's own invocation line. Running it SIGTERMed dpkg mid-unpack, leaving the package in "a very bad inconsistent state" (`Preparing to unpack … / Terminated`). Replaced with `pkill -x avacli` (exact argv[0] match, never matches dpkg/apt) and scoped to `remove|deconfigure` only — upgrades now rely on atomic file replacement so running instances keep executing the old inode until users restart. A new `postrm` stub is shipped for future purge-time cleanup.

Embedded UI unchanged from 2.3.0.

---

## [2.3.0] — 2026-04-16

**xAI prompt-cache fixes, cached-token observability, sticky conversation routing, Responses-API chaining + reasoning preservation, and Batch API support (all five phases of `.planning/XAI_CACHE_AND_BATCH_PLAN.md`).**

Cached input tokens bill at ~10× less on `grok-4.20-*` (4× on `grok-4-1-fast`). We were running at effectively 0% hit rate because the agent was re-writing `messages[0]` on every tool turn, requests had no routing hint, and nothing surfaced `cached_tokens` to us. Phase 5 layers xAI's Batch API on top — a flat 50% discount that stacks with caching for a ~20× effective discount on offline/bulk workloads.

### Stop cache-busting the system prefix

- **Split `AgentEngine::buildSystemPrompt`** into `buildStaticSystemPrompt` (cacheable: `GROK_SYSTEM_PROMPT.md` + workspace env) and `buildDynamicContext` (volatile: session TODOs, memory, recent edits, project notes).
- **`runChatCompletions` / `runResponses` no longer mutate `messages[0]` inside the tool loop.** The static prefix is set once per `run()` entry and the volatile context is injected as a dedicated user-role message immediately before the latest user turn, so new turns append to a byte-stable prefix.
- **Injected scaffolding is stripped on write-back** — the persisted session no longer accumulates system messages or stale context refreshes.

### Sticky routing via conversation ID

- **`ChatOptions::convId`** field plumbed end-to-end. `XAIClient::chatStream` emits `x-grok-conv-id: <id>` as an HTTP header; `XAIClient::responsesStream` emits `prompt_cache_key: <id>` as a body field. xAI uses these to pin subsequent requests to the same server pool so the KV cache is actually warm on turn N+1.
- **`SessionData` persists `conv_id`.** Minted once via `SessionManager::generateConvId()` (`"conv-<16hex>"`) and never regenerated — renaming a session doesn't evict the xAI cache. Back-compat: old sessions missing the field get a fresh id on next load.
- **Application, RoutesChat, and the media-gen path** all mint-if-empty then call `AgentEngine::setConvId()` (and `ToolExecutor::setConvId()` so sub-agent spawns inherit). `AgentEngine` now logs the conv id on each `run()` entry for grep-able correlation with the existing `[cache] prompt=X cached=Y` lines.
- **Sub-agents inherit the parent routing prefix.** `SubAgentManager::SpawnOptions` gains `parentConvId`; each spawned child computes `childConvId = parent_conv_id + ":" + task_id`. Siblings share the prefix so xAI's router lands them on the same pool, where they reuse each other's cached sub-agent scaffolding. `POST /api/subagents` accepts `parent_conv_id` for programmatic spawns.

### Surface cache hits

- **`Usage` struct** gains `cachedPromptTokens`, `reasoningTokens`, `billablePromptTokens()`, `cacheHitRatio()`.
- **`XAIClient`** now parses `prompt_tokens_details.cached_tokens` / `input_tokens_details.cached_tokens` from both the chat-completions and Responses APIs, plus `completion_tokens_details.reasoning_tokens`. An `spdlog::info("[cache] prompt=X cached=Y ratio=Z%")` line fires on every response so hit rate is greppable from logs.
- **`TokenTracker`** gains `addCached`, `addReasoning`, `cachedPromptTokens()`, `reasoningTokens()`, `billablePromptTokens()`, `cacheHitPercent()`, plus a 4-arg `addUsage(prompt, completion, cached, reasoning)` overload. `formatForDisplay` / `formatForDisplayDetailed` render `"(Xk cached, YY%)"` when cached > 0.
- **Surfaces** — `/api/chat` SSE `usage` + `done` events include `cached_tokens` / `reasoning_tokens` / `billable_prompt_tokens` / `cache_hit_rate`. Headless `--format json` adds the same. Sub-agent `result.usage` carries the full breakdown; `spawn_task`'s top-level `usage` prefers the richer block when present. Persisted usage records (`appendUsageRecord`) now include cached + reasoning so the `/usage` dashboard can attribute savings over time.

### Responses API chaining (`previous_response_id`)

xAI stores the server-side conversation for 30 days when you stream from `/v1/responses`. Passing the stored id back on the next turn lets us send only the new input items (tool results, user follow-ups) — the server replays everything before it, including the encrypted reasoning trace, and the whole chain hits the same cache.

- **`ChatOptions::previousResponseId`** field — emitted as `"previous_response_id"` in the Responses API body when non-empty. Unused by the chat-completions path.
- **`StreamCallbacks::onResponseId`** — new callback; `XAIClient::responsesStream` parses `response.id` out of `response.completed` / `response.done` SSE frames and fires it exactly once per successful stream.
- **`AgentEngine` chain bookkeeping** — new `lastResponseId_` + `lastSubmittedCount_` state, persisted across `run()` calls. `runResponses` now:
  - Slices `apiMessages` to the tail past `lastSubmittedCount_` when chaining, so the server only sees new turns.
  - Skips `instructions` on chained calls (server already has the cached prefix).
  - Injects the dynamic-context refresh **inside** the slice so xAI actually sees the latest TODO / memory / edits.
  - Updates `lastResponseId_` and the slice base after every successful stream.
  - Detects stale-id errors (`previous_response_id` rejected / expired / not found) and transparently falls back to a full-history resend for the rest of the run, then resumes chaining from the next response.
  - Clears chain state on `MAX_TOOL_TURNS` so the next `run()` re-syncs from scratch rather than continuing from a torn chain.
- **`SessionData` persists `last_response_id` + `last_submitted_count`.** Wired through `Application::saveSession` / `loadSession` (headless) and `RoutesChat` (web). Clearing history (`use_history:false`) resets both so we don't chain into a mismatched server state.
- **Panic switch** — `AgentEngine::setUsePreviousResponseId(false)` disables chaining entirely. Default on.

### Reasoning content preservation (chat completions)

Reasoning models on the chat-completions path (`grok-4-reasoning`, `grok-4-fast-reasoning`) stream `reasoning_content` deltas. We previously inlined them into assistant `content` as `<thought>…</thought>`, which broke xAI's reasoning cache on replay because the round-tripped messages no longer matched the originally-cached ones.

- **`Message::reasoning_content`** — new field, persisted per assistant turn.
- **`StreamCallbacks::onReasoning`** — new callback. When set, `XAIClient::chatStream` / `responsesStream` route raw `reasoning_content` deltas there instead of inlining them via `onContent`. Callers without `onReasoning` still get the legacy `<thought>` wrapping for UI back-compat.
- **`AgentEngine::runChatCompletions`** now captures raw reasoning into the assistant `Message`, then re-emits `<thought>…</thought>` to `onStreamDelta_` for the web/TUI transcript splitters — the on-wire message the next turn sends back to xAI is byte-identical to what the model produced, so the reasoning cache stays warm.
- **`XAIClient::buildRequestBody`** echoes `reasoning_content` back on assistant turns (both plain and with tool_calls). Non-reasoning models ignore the field.
- **`SessionManager`** persists `reasoning_content` on each message, round-tripping it across save/load.

### Batch API (`/v1/messages/batches`)

xAI's Batch API returns a flat 50% discount on every token (input, cached, output, reasoning) and gives the service 24 hours to complete the work. Stacks multiplicatively with prompt caching — cached input on a batched request bills at 50% × ~10% = ~5% of the sticker price.

- **`BatchClient` (`src/client/BatchClient.{hpp,cpp}`)** — thin C++ wrapper over `/v1/messages/batches`. Methods: `create(name)`, `addRequests(batchId, envelopes)`, `get(batchId)`, `listResults(batchId, limit, pageToken)`, `cancel(batchId)`, `list(limit, pageToken)`. Static builders `makeChatRequest(id, body)` and `makeResponsesRequest(id, body)` produce the `{custom_id, params: {endpoint, body}}` envelopes xAI expects. Token/cost accounting is extracted into `BatchResult::{promptTokens, completionTokens, cachedTokens, reasoningTokens, costUsdTicks}` so Batch usage can be rolled into the existing `TokenTracker` / `/usage` dashboard.
- **SQLite migration v7** — `batches` table with `(id PRIMARY KEY, name, owner, kind, state, num_requests, num_pending, num_success, num_error, num_cancelled, cost_usd_ticks, expires_at, created_at, updated_at, last_polled, request_ids_json, results_json, metadata)` plus indexes on `state`, `owner`, `created_at`. `cost_usd_ticks` stores 10⁻¹⁰ USD to keep arithmetic in `int64_t` without float drift.
- **`BatchPoller` background thread (`src/services/BatchPoller.{hpp,cpp}`)** — singleton service started by `HttpServer::start()`. Sleeps on a condition variable, wakes every `xai.batch.poll_interval_sec` (default 30s) or when any code path calls `BatchPoller::pollNow()`. On each tick it selects `state = 'processing'` rows, calls `BatchClient::get` per batch, updates the local row, and — once terminal — fetches the full results page and snapshots it to `results_json` so the UI/tools can serve results without a round-trip to xAI. Exponential backoff on transient xAI errors, bounded by `max_consecutive_failures` before the poller pauses the offending batch.
- **HTTP routes (`src/server/RoutesBatches.cpp`)** — `POST /api/batches` creates a batch (accepts either `{requests:[…]}` to pre-populate or returns the id for a later `POST /api/batches/:id/requests`), `GET /api/batches` lists with `owner` / `kind` / `state` filters, `GET /api/batches/:id` returns the merged local+live snapshot, `GET /api/batches/:id/results?limit=&page_token=` serves paginated results (cache-first, falls through to xAI when the cache is empty), `POST /api/batches/:id/cancel`, `POST /api/batches/:id/refresh` forces a single-batch poll, `POST /api/batches/refresh` kicks the poller, `DELETE /api/batches/:id` removes the local record (does not call xAI). All JWT-guarded and owner-scoped.
- **Agent-facing tools (`ToolRegistry` + `ToolExecutor`)** — five new tools the agent can invoke directly: `batch_submit(name, kind, model, prompts[])`, `get_batch(batch_id)`, `list_batches(owner?, kind?, state?, limit?)`, `get_batch_results(batch_id, limit?, pagination_token?)`, `cancel_batch(batch_id)`. `batch_submit` accepts the friendly `{system, user, model, max_tokens}` shape per prompt (or raw `messages[]`) and translates into xAI envelopes on the way out, persists a local row, and kicks the poller so state flips from `processing` → `completed` without the agent having to poll. `batch_submit` is gated to tool-mode ≥ 2 (writes) to keep casual read-only sessions from accidentally queuing paid jobs.
- **Sub-agent plumbing** — `ToolExecutor::convId_` is threaded into sub-agent spawns so a batch created by a sub-agent inherits the parent's routing prefix (`conv-<hex>:<task_id>`); cached batch prefixes land on the same xAI pool as the parent's interactive turns.

---

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
