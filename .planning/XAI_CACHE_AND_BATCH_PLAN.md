# xAI Prompt Caching & Batch API — Cost Reduction Plan

## Status: Draft — scoped for v2.3

## Why this matters (the numbers)

From [xAI Models & Pricing](https://docs.x.ai/developers/models):

| Model                         | Input      | **Cached** | Output |
|-------------------------------|------------|------------|--------|
| `grok-4.20-*-reasoning`       | $2.00 / 1M | **$0.20**  | $6.00  |
| `grok-4.20-*-non-reasoning`   | $2.00 / 1M | **$0.20**  | $6.00  |
| `grok-4-1-fast-*`             | $0.20 / 1M | **$0.05**  | $0.50  |
| `grok-4.20-multi-agent`       | $2.00 / 1M | **$0.20**  | $6.00  |

- **Cached prompt tokens are ~10× cheaper** on the flagship models (4× on fast).
- **Batch API is 50% off** all token types (input, output, cached, reasoning).
- Stacking: a batched request that cache-hits pays **$0.10 / 1M** on the 4.20 flagship — a **20× discount** vs. cold real-time input.

Every turn of an agent loop re-sends the full conversation history. For a single 30-turn session on `grok-4.20-reasoning` we re-transmit the same system + prefix ~30 times. That's where the cache wins live.

---

## Problem 1 — The current agent is **actively cache-busting itself**

`src/core/AgentEngine.cpp` overwrites the system prompt on **every turn**:

```314:326:src/core/AgentEngine.cpp
bool AgentEngine::runChatCompletions(XAIClient& client,
                                     const std::string& model,
                                     std::vector<Message>& messagesInOut,
                                     const std::vector<nlohmann::json>& tools,
                                     UsageFn onUsage) {
    std::vector<Message> apiMessages = messagesInOut;

    if (apiMessages.empty() || apiMessages.front().role != "system") {
        apiMessages.insert(apiMessages.begin(),
            {"system", buildSystemPrompt(mode_, workspace_)});
    } else {
        apiMessages[0].content = buildSystemPrompt(mode_, workspace_);
    }
```

`buildSystemPrompt` calls `loadContextSnapshot`, which appends:

- Session TODOs (changes any time `list_todos` is updated)
- Session memory (pinned + last 15 — changes any time `memory_store` runs)
- Recent file edits (last 15 — changes any time `edit_file` / `write_file` runs)

So after **any** tool call that writes state, the first message flips → **full cache miss on the next turn**. Combined with the lack of `x-grok-conv-id`, hit rate is effectively zero. The xAI docs are explicit:

> Never modify earlier messages — only append new ones. Any edit, removal, or reorder breaks the cache.

**No new features needed to unlock big savings — we just need to stop evicting ourselves.**

---

## Problem 2 — No conv-id / prompt_cache_key on requests

`XAIClient::chatStream` and `responsesStream` only send `Content-Type` + `Authorization`. Without `x-grok-conv-id` (chat completions) or `prompt_cache_key` (Responses API), requests bounce across xAI's server pool and land on boxes with cold KV caches even when our prefix is stable.

---

## Problem 3 — No visibility into cached_tokens

`Usage` tracks `promptTokens` / `completionTokens`. It reads `prompt_tokens`, `input_tokens`, and a reasoning-token add-on, but **ignores `prompt_tokens_details.cached_tokens` / `input_tokens_details.cached_tokens`**. We can't observe the cache hit rate, so we can't tune for it.

---

## Problem 4 — Reasoning-model cache misses

For `grok-4.20-*-reasoning` and `grok-4.20-multi-agent`, xAI requires either:

1. `reasoning_content` to be sent back on subsequent requests, **or**
2. `previous_response_id` (Responses API only — stateful mode).

Today `XAIClient.cpp:136-145` pulls `reasoning_content` out of the stream and wraps it in `<thought>…</thought>` injected into the text content. That's great for UI but it **discards the encrypted reasoning blob needed for cache hits**. Every reasoning turn is a full cache miss.

---

## Problem 5 — No Batch API support

There's nothing in `src/client/` for `/v1/batches`. A lot of our workload is already async-safe:

- RSS feed digests (Services: `rss_feed`)
- Scheduled prompts (Services: `scheduled_prompt`)
- Knowledge base re-summarization
- Recursive-self-improvement eval runs (`.planning/RECURSIVE_SELF_IMPROVEMENT_PLAN.md`)
- Bulk tool-forge tests / migrations

All of that could pay 50% less token cost by going through `/v1/batches`.

---

# Proposed work

Four phases. Phase 1 alone should give most of the savings; the rest is polish & reach.

## Phase 1 — Stop cache-busting (highest ROI, smallest diff)

### 1a. Freeze the system prompt

Split `buildSystemPrompt` into two parts:

- **Static prefix** (cacheable): `GROK_SYSTEM_PROMPT.md` + `## Environment / Workspace: …` — this stays as `messages[0]`.
- **Dynamic context** (volatile): TODOs, memory, edits, notes — move into a separate message appended at the **end of the prefix** (before the latest user turn) as `{role: "user", content: "## Current session state\n…"}` **or** keep it injected but only once per session (stored in `SessionData`) and never rewritten mid-loop.

Preferred route: emit dynamic context as a **tool-like "context refresh" user message** appended on each `run()` entry, but not re-written between tool turns within the same run. Within a single `run()` only new turns (assistant + tool + user) get appended — the prefix stays byte-identical.

### 1b. Preserve user messages once sent

Drop the `apiMessages[0].content = buildSystemPrompt(…)` overwrite inside the `runChatCompletions` / `runResponses` loops. The system prompt is set once at the start of `run()` and never touched again.

Between top-level `run()` invocations (e.g. new user message in the web UI) we can refresh the context refresh block, but that still only evicts a tail segment — the static prefix stays cached.

### 1c. Acceptance test

Instrument a log line after each API call: `[cache] prompt=X cached=Y ratio=Z%`. Run the demo chat `chat "Explain async/await"` → follow up with `"Now show me a code example"`. Turn 2 should show `cached > 0`. Turn 3 `cached >> turn2`. Target: **≥ 70% cache hits by turn 3** on `grok-4.20-reasoning`.

---

## Phase 2 — Route requests stickily

### 2a. Plumb a conversation ID

- Add `std::string convId` to `ChatOptions`.
- `XAIClient::chatStream`: if non-empty, append header `x-grok-conv-id: <id>`.
- `XAIClient::responsesStream`: if non-empty, add `"prompt_cache_key": "<id>"` to the JSON body.
- `Application::runHeadless` → derive conv id from `config_.session` (existing `--session` flag). If empty, generate a stable UUID and persist it in `SessionData` (new field `conv_id`).
- `RoutesChat.cpp` → already has `sessionName` per chat; reuse it.
- `SubAgentManager` → new child conv ids per spawned task (`parent_conv_id + ":" + task_id`).

### 2b. Persist the ID per session

Extend `SessionData`:

```cpp
struct SessionData {
    std::string conv_id;          // NEW — stable UUID, never regenerated
    std::vector<Message> messages;
    // … existing fields
};
```

Serialize in `SessionManager::save` / `::load`. Back-compat: if missing on load, generate once and save.

---

## Phase 3 — Observe & report cache hits

### 3a. Extend `Usage`

```cpp
struct Usage {
    size_t promptTokens = 0;
    size_t completionTokens = 0;
    size_t cachedPromptTokens = 0;  // NEW
    size_t reasoningTokens = 0;     // NEW (separate from completion for accounting)
    size_t totalTokens() const { return promptTokens + completionTokens; }
    size_t billablePromptTokens() const { return promptTokens - cachedPromptTokens; }
};
```

Parse both shapes:

```cpp
if (usage.contains("prompt_tokens_details"))
    u.cachedPromptTokens = usage["prompt_tokens_details"].value("cached_tokens", 0);
if (usage.contains("input_tokens_details"))
    u.cachedPromptTokens = usage["input_tokens_details"].value("cached_tokens", 0);
```

### 3b. Extend `TokenTracker`

- Add `addCached(size_t)` and `cachedTokens()`.
- `formatForDisplay()` → `"I: 12k (8k cached) | O: 3k | Total: 15k"`.
- Emit a `cache_hit_rate` field on stream-json `onUsage` events.

### 3c. UI + CLI surface

- TUI/headless: show cache hit % alongside token totals.
- `ui/js/app.js` status bar: new badge for cached-token savings.
- `--format json` summary: add `cached_tokens` and `estimated_cost_usd` (use `ModelRegistry` to look up rates).

---

## Phase 4 — Reasoning-model correctness for cache reuse

Two options; we likely ship both.

### 4a. Responses API → use `previous_response_id`

- `responsesStream` already parses `response.completed`. Extend it to capture `response.id`.
- Emit in a new `IdCallback` on `StreamCallbacks`.
- `AgentEngine::runResponses`: after each round, stash `last_response_id`; next round sends `previous_response_id` **instead of** the full input (for stateful continuation) — or alongside for safety.
- Docs: <https://docs.x.ai/developers/model-capabilities/text/generate-text#chaining-the-conversation>

### 4b. Preserve `reasoning_content` for chat completions

- New field on `Message`: `std::string reasoning_content;`
- `XAIClient` streaming: capture `reasoning_content` into the assistant message instead of (or in addition to) inlining it as `<thought>`.
- `buildRequestBody`: for assistant messages with non-empty `reasoning_content`, include it in the payload.
- Docs: <https://docs.x.ai/developers/model-capabilities/text/reasoning#encrypted-reasoning-content>

Gate behind `ModelConfig.reasoning == true`; non-reasoning models ignore the field.

---

## Phase 5 — Batch API support

### 5a. New client surface

`src/client/BatchClient.{hpp,cpp}` with:

```cpp
class BatchClient {
public:
    struct BatchState { int pending, success, error, cancelled, total; std::string status; };
    struct BatchResult {
        std::string batch_request_id;
        bool succeeded;
        nlohmann::json response;   // chat_get_completion / image_response / video_response
        std::string error_message;
        size_t prompt_tokens = 0, completion_tokens = 0, cached_tokens = 0;
    };

    std::string create(const std::string& name);
    bool addRequests(const std::string& batch_id, const std::vector<nlohmann::json>& requests);
    BatchState get(const std::string& batch_id);
    std::vector<BatchResult> listResults(const std::string& batch_id);  // paginated
    bool cancel(const std::string& batch_id);
};
```

Synchronous curl — no SSE. Reuse the `Authorization: Bearer <XAI_API_KEY>` pattern from `XAIClient`.

### 5b. Schema

Migration v6 adds:

```sql
CREATE TABLE batches (
  id TEXT PRIMARY KEY,             -- xAI batch_id
  name TEXT NOT NULL,
  owner TEXT,                      -- session id / app slug / service id
  kind TEXT NOT NULL,              -- 'summarize' | 'eval' | 'service' | 'user'
  created_at INTEGER NOT NULL,
  state TEXT NOT NULL,             -- 'processing' | 'complete' | 'cancelled' | 'error'
  num_requests INTEGER DEFAULT 0,
  num_success INTEGER DEFAULT 0,
  num_error INTEGER DEFAULT 0,
  cost_usd_ticks INTEGER DEFAULT 0,
  results_json TEXT                -- cached results after completion
);
CREATE INDEX idx_batches_state ON batches(state, created_at);
```

### 5c. Server routes

`src/server/RoutesBatches.cpp`:
- `POST /api/batches` — create & add requests in one shot
- `GET /api/batches` — list, filter by owner/kind
- `GET /api/batches/:id` — status + cost
- `GET /api/batches/:id/results` — paginated
- `POST /api/batches/:id/cancel`

### 5d. Background poller

New service kind in `ServiceManager` (or a built-in always-on poller): every 60 s, list `state = 'processing'` batches, call `batch.get()`, update row, fetch results when `num_pending == 0`. Fire a callback / event so creators (e.g. an RSS service) can consume.

### 5e. First integrations (opt-in, feature-flagged)

| Caller                 | Why batch                                | Cost impact              |
|------------------------|------------------------------------------|--------------------------|
| `rss_feed` service     | Digest 20 feed items overnight           | 50% off content digests  |
| Knowledge base rebuild | Re-summarize all saved articles          | 50% off a manual op      |
| Eval runner (Phase 2 of self-improvement plan) | Score 100s of prompts | 50% off metrics runs     |
| New `batch_submit` tool (Agent mode) | Agent queues large workloads itself | Agent learns cost control |

Interactive chat **stays on real-time**; batch is only for things where ≤24 h latency is fine.

---

## Rollout / risk

- **Phase 1 + 2 are safe** — no schema change, no API surface change. Worst case: a missed cache hit (status quo).
- **Phase 3** touches `Usage` + `TokenTracker` — ABI-ish change inside the binary but no external surface. Backward-compatible JSON.
- **Phase 4a (`previous_response_id`)**: needs careful test — if the server loses the response we need a fallback to the full message resend. Add a toggle in Settings.
- **Phase 4b (reasoning_content)**: for chat completions API only; models without reasoning ignore the field.
- **Phase 5**: largest diff. Ship behind a Settings flag (`batch_api_enabled`). Existing features unaffected when disabled.

---

## Suggested merge order

1. **Phase 1** (freeze prefix) — 1 commit. Measurable immediately.
2. **Phase 3** (observability) — 1 commit. Validates Phase 1 worked.
3. **Phase 2** (conv-id) — 1 commit. Multiplies the gain.
4. **Phase 4a** (`previous_response_id` for Responses API). 1 commit.
5. **Phase 4b** (reasoning_content for chat completions). 1 commit.
6. **Phase 5** (Batch API) — 3–4 commits (client → schema → routes+poller → integrations).

Phases 1–3 are a single afternoon. Phase 4 is a day. Phase 5 is a week.

## Open questions

- Do we want to expose `batch_submit` as a **built-in tool**? It lets the agent self-decide what's batchable, but costs us guardrails (agent could queue runaway jobs). Default: tool gated by Settings, quota per app/service.
- Do we surface cached-token savings in the Platform plans.php / pricing page? Probably yes once we can prove the numbers — the marketing writes itself ("avacli cuts your xAI bill by 10× out of the box").
- Should the Phase 1 "dynamic context block" live at the end of the prefix or inside each user turn? Putting it in each user turn is cleanest for caching but balloons token count; end-of-prefix appended once per `run()` is a better tradeoff.
