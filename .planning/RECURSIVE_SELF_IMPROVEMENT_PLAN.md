# Recursive Self-Improvement Architecture

## Status: Phase 1 Ready for Implementation

## Overview

Enable the avacli agent to audit, measure, and improve its own behavior over time.
The system prompt is now externalized to `GROK_SYSTEM_PROMPT.md` (the agent can already
edit it with `edit_file`). This plan covers the remaining infrastructure.

## Architecture

```
┌────────────────────┐
│   Agent Session     │
│  (AgentEngine.run)  │
│                     │
│  ┌──────────────┐   │     ┌───────────────────┐
│  │ Tool Calls   │───┼────▶│ MetricsCollector   │
│  │ Completions  │   │     │ (per-session stats)│
│  │ Errors       │   │     └───────┬───────────┘
│  └──────────────┘   │             │
└────────────────────┘             │
                                    ▼
                        ┌───────────────────┐
                        │ metrics.jsonl      │
                        │ (append-only log)  │
                        └───────┬───────────┘
                                │
         ┌──────────────────────┼──────────────────────┐
         ▼                      ▼                      ▼
┌─────────────┐    ┌──────────────────┐    ┌──────────────────┐
│ self_audit   │    │ agent_metrics    │    │ improve_prompt   │
│ (tool)       │    │ (tool)           │    │ (tool)           │
│              │    │                  │    │                  │
│ Reviews last │    │ Returns success  │    │ Proposes and     │
│ N sessions,  │    │ rates, error     │    │ applies prompt   │
│ identifies   │    │ patterns, avg    │    │ edits with       │
│ patterns     │    │ response time    │    │ version control  │
└─────────────┘    └──────────────────┘    └──────────────────┘
```

## Phase 1: Metrics Collection (minimal C++ changes)

### 1.1 MetricsCollector class

**File:** `src/core/MetricsCollector.hpp / .cpp`

Tracks per-session:
- Tool calls attempted / succeeded / failed
- Total tokens (prompt + completion)
- Wall-clock time
- Disconnection events (sink write failures)
- Error types encountered

Writes one JSON line per session to `~/.avacli/metrics.jsonl` on session end.

```cpp
struct SessionMetrics {
    std::string sessionId;
    std::string model;
    int64_t startTime;
    int64_t endTime;
    int toolCallsTotal;
    int toolCallsSucceeded;
    int toolCallsFailed;
    size_t promptTokens;
    size_t completionTokens;
    bool disconnected;       // true if sink.write() returned false
    bool cancelled;
    std::vector<std::string> errors;
};
```

### 1.2 Integration points

- `RoutesChat.cpp`: Create `MetricsCollector` per chat, pass to engine.
  Track `disconnected = true` when `sink.write()` returns false.
- `AgentEngine.cpp`: Call `collector.recordToolCall(name, success, duration)` in tool callbacks.
- On agent thread exit: `collector.flush()` writes to metrics.jsonl.

## Phase 2: Self-Audit Tool

### 2.1 `self_audit` tool

**Register in:** `ToolRegistry.cpp`

Reads the last N sessions from `~/.avacli/sessions/` and `~/.avacli/metrics.jsonl`.
Returns a structured report:

```json
{
  "sessions_reviewed": 5,
  "tool_success_rate": 0.94,
  "avg_tokens_per_session": 45000,
  "common_errors": ["file_not_found (3x)", "json_parse_error (1x)"],
  "disconnections": 2,
  "recommendations": [
    "High disconnection rate — consider shorter tool chains",
    "Repeated file_not_found on large files — check maxBytes limit"
  ]
}
```

### 2.2 `agent_metrics` tool

Simple accessor: reads `metrics.jsonl`, returns aggregated stats for
the last N sessions (configurable, default 10).

## Phase 3: Prompt Self-Improvement

### 3.1 `improve_prompt` tool

Now that `GROK_SYSTEM_PROMPT.md` is loaded from disk:

1. Agent calls `self_audit` to identify patterns
2. Agent proposes prompt edits based on findings
3. `improve_prompt` tool:
   - Backs up current prompt to `GROK_SYSTEM_PROMPT.md.bak.<timestamp>`
   - Applies the edit via the existing `edit_file` mechanism
   - Records the change in `~/.avacli/prompt_changelog.jsonl`
4. Next session loads the updated prompt automatically

Safety rails:
- Maximum 3 prompt edits per day
- Edits must not remove safety section
- Backup always created before modification
- User can revert with `undo_edit` on the prompt file

### 3.2 Prompt versioning

Store in `~/.avacli/prompt_changelog.jsonl`:
```json
{"timestamp": 1712800000, "session": "chat_xxx", "diff": "...", "reason": "..."}
```

## Phase 4: Auto-Summarization on Disconnect

### 4.1 Context-aware summarization

In `AgentEngine.cpp`, after the main loop:
- If token usage exceeds `CONTEXT_WARN_THRESHOLD` (already defined at 110K)
- OR if the session was marked as disconnected
- Auto-call `summarize_and_new_chat` tool (already exists)

### 4.2 Heartbeat detection

Add to `RoutesChat.cpp` chunked content provider:
- Track last successful `sink.write()` timestamp
- If the write interval exceeds 30s, set a `stalled` flag
- Agent thread can check this to trigger early summarization

## Implementation Priority

| Item | Effort | Impact | Priority |
|------|--------|--------|----------|
| MetricsCollector class | ~2h | Foundation | **P0** |
| Integration in RoutesChat | ~1h | Foundation | **P0** |
| self_audit tool | ~2h | Visibility | **P1** |
| agent_metrics tool | ~1h | Visibility | **P1** |
| improve_prompt tool | ~3h | Self-improvement | **P2** |
| Auto-summarize on disconnect | ~2h | Reliability | **P1** |
| Heartbeat detection | ~1h | Reliability | **P2** |

## Files to Create/Modify

**New files:**
- `src/core/MetricsCollector.hpp`
- `src/core/MetricsCollector.cpp`

**Modified files:**
- `src/tools/ToolRegistry.cpp` — Register self_audit, agent_metrics, improve_prompt
- `src/tools/ToolExecutor.cpp` — Implement the three new tools
- `src/server/RoutesChat.cpp` — Wire up MetricsCollector
- `src/core/AgentEngine.cpp` — Auto-summarization trigger
- `CMakeLists.txt` — Add MetricsCollector to build
