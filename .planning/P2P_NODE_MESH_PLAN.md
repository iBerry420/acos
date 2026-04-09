# P2P Node Mesh Network — Implementation Plan

## Overview

Transform the existing basic node list into a fully interconnected private mesh network where every node is aware of every other node, users can chat with specific nodes via `@tag` mentions, and any node's full UI is accessible remotely from any other node.

## Current State

- Nodes stored in `~/.avacli/nodes.json` (flat file, IP/port/label/credentials)
- `POST /api/nodes` adds a node, `GET /api/nodes` lists, `DELETE /api/nodes/:id` removes
- Servers page shows node cards with "Open" (new tab) and "Remove" buttons
- No connection testing, no health checks, no mesh awareness, no chat integration

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    User's Browser                        │
│                                                          │
│  ┌──────────┐   ┌─────────┐   ┌────────────────────┐   │
│  │ Chat UI  │   │ Node    │   │ Remote UI Panel    │   │
│  │ @mentions│   │ Sidebar │   │ (iframe / proxy)   │   │
│  └──────────┘   └─────────┘   └────────────────────┘   │
└────────────────────────┬─────────────────────────────────┘
                         │
           ┌─────────────┼─────────────────┐
           │             │                 │
    ┌──────▼──────┐ ┌────▼──────┐  ┌──────▼──────┐
    │  Node A     │ │  Node B   │  │  Node C     │
    │  (local)    │ │  (vultr)  │  │  (vultr)    │
    │  :8080      │ │  :8080    │  │  :8080      │
    └──────┬──────┘ └─────┬─────┘  └──────┬──────┘
           │              │               │
           └──────────────┼───────────────┘
                    P2P Mesh Layer
              (each node knows all others)
```

## Phases

### Phase 1: Connection Testing & Health Monitoring

**Goal:** When a node is added, verify it's reachable and running avacli.

**Backend changes (`RoutesInfra.cpp`):**
- `POST /api/nodes` → after saving, attempt HTTP `GET /api/health` on the target node
- Store connection status: `connected`, `unreachable`, `auth_failed`, `not_avacli`
- New `POST /api/nodes/:id/test` → re-test a specific node on demand
- New `GET /api/nodes/health` → batch health check all nodes (called periodically by UI)
- Response includes: `status`, `latency_ms`, `version`, `workspace`, `uptime`

**Node health endpoint** (`GET /api/health`):
- Already exists implicitly via `GET /api/auth/status`
- Add a lightweight `/api/health` that returns `{status: "ok", version: "2.x.x", uptime_s: N}`

**Frontend changes:**
- Add Node modal: show "Testing connection..." spinner after submit
- Node cards: real-time status dot (green=connected, yellow=latency>500ms, red=unreachable)
- "Test" button on each card to re-check

**Files to modify:**
- `src/server/RoutesInfra.cpp` — new test/health routes
- `src/server/EmbeddedAssets.hpp` — UI updates for node cards + add modal
- `src/client/` — new `NodeClient.hpp/cpp` for HTTP calls to peer nodes

### Phase 2: Chat Page Node Sidebar + @tag Mentions

**Goal:** Show connected nodes in a right-side panel on the chat page. Users type `@` to get a picker, and the message is forwarded to that node.

**Chat UI layout change:**
```
Desktop:
┌──────────┬─────────────────────────┬──────────┐
│ Sidebar  │     Chat Area           │  Nodes   │
│ (nav)    │                         │  Panel   │
│          │                         │  240px   │
└──────────┴─────────────────────────┴──────────┘

Mobile (swipe):
┌─────────────────────────┐     ┌──────────┐
│     Chat Area           │ ←→  │  Nodes   │
│                         │     │  Panel   │
└─────────────────────────┘     └──────────┘
```

**@mention system:**
- When user types `@` in the chat input, show a popup/dropdown of connected nodes
- Selecting a node inserts `@node-label` at cursor
- On submit, detect `@node-label` prefixes and route the message:
  1. Local processing: message goes to local agent as normal
  2. Node forwarding: `POST /api/chat` on the target node with the message (minus the @tag)
  3. Response streams back and is displayed with a node badge

**Backend for message forwarding:**
- New `POST /api/nodes/:id/chat` — proxy chat to a remote node
- Uses the stored credentials to authenticate with the target
- Streams the response back to the client via SSE (same as normal chat)

**Mobile swipe gesture:**
- CSS: nodes panel positioned off-screen right with `transform: translateX(100%)`
- JS: touch event handlers for swipe-left to open, swipe-right to close
- Reuse the existing swipe infrastructure from `assets/swipe.js` (if applicable) or implement similar

**Files to modify:**
- `src/server/EmbeddedAssets.hpp` — chat layout, node sidebar, @mention picker, mobile swipe
- `src/server/RoutesInfra.cpp` — proxy chat endpoint
- `src/client/NodeClient.hpp/cpp` — chat forwarding with SSE relay

### Phase 3: Mesh Awareness (All Nodes Know All Nodes)

**Goal:** When a node is added/removed on any node in the network, all other nodes learn about it.

**Gossip protocol:**
- `POST /api/mesh/sync` — accepts a node list, merges with local, returns merged result
- On node add/remove, broadcast to all known nodes: `POST /api/mesh/sync` with full node list
- Each node calls `/api/mesh/sync` on all peers every 60 seconds as heartbeat
- Conflict resolution: union of all nodes, most-recent-wins for metadata updates

**Mesh sync payload:**
```json
{
  "source_id": "node-abc123",
  "nodes": [
    {"id": "...", "ip": "...", "port": 8080, "label": "...", "added_at": 1712345678}
  ],
  "timestamp": 1712345678
}
```

**Security:**
- Mesh sync requires the same auth token (Bearer) used between nodes
- Nodes only accept sync from nodes they already know (or from their direct parent)
- First connection is always manually initiated by the user

**Files to modify:**
- `src/server/RoutesInfra.cpp` — `/api/mesh/sync` endpoint
- `src/services/MeshManager.hpp/cpp` — new service: periodic sync, node list management
- `src/server/HttpServer.cpp` — start mesh manager on boot

### Phase 4: Remote UI Access (Click a Node → See Its UI)

**Goal:** Click on a node card to open its full UI within the current browser window.

**Approach: Reverse proxy via the local node**

Rather than opening a new tab (which requires the user to know the IP/port and handle auth), proxy the remote node's UI through the local avacli instance:

- `GET /api/nodes/:id/proxy/*` → fetches the corresponding path from the remote node and returns it
- The UI renders an `<iframe>` pointed at `/api/nodes/:id/proxy/` which loads the remote node's SPA
- All API calls from the iframe automatically route through the proxy
- Auth is handled server-side (local node authenticates with remote node using stored credentials)

**UI integration:**
- Clicking a node in the sidebar/servers page opens a full-width panel with the iframe
- Back button returns to local view
- Node badge in the corner shows which node you're connected to
- Keyboard shortcut: `Ctrl+Shift+N` to toggle node switcher

**Alternative: Direct connection**
- If the remote node is publicly accessible (has a domain), open it directly in an iframe
- Fall back to proxy mode if the node is on a private network

**Files to modify:**
- `src/server/RoutesInfra.cpp` — proxy endpoint with streaming body relay
- `src/server/EmbeddedAssets.hpp` — iframe container, node switching UI
- `src/client/NodeClient.hpp/cpp` — HTTP proxy relay

### Phase 5: Node Auto-Registration After Deploy

**Goal:** When a VPS is deployed via Vultr, automatically register it as a node once provisioning completes.

**Flow:**
1. User deploys instance via deploy modal
2. Backend stores deploy record with instance ID
3. Background poller checks instance status every 15s
4. Once instance is `active` with a valid IP:
   - SSH or HTTP to the instance to verify avacli is running
   - Auto-register as a node with the instance IP/port
   - Trigger mesh sync so all nodes learn about the new one
5. Toast notification: "New node `my-server` (149.28.x.x) is online and connected"

**Files to modify:**
- `src/server/RoutesInfra.cpp` — deploy tracking, post-provision registration
- `src/infra/ProvisioningPoller.hpp/cpp` — new background service for polling instance status
- `src/server/EmbeddedAssets.hpp` — provisioning status UI in deploy modal

## Data Model Changes

### `~/.avacli/nodes.json` → SQLite `nodes` table

Migrate from flat JSON file to SQLite for reliability and querying:

```sql
CREATE TABLE IF NOT EXISTS nodes (
    id TEXT PRIMARY KEY,
    ip TEXT NOT NULL,
    port INTEGER DEFAULT 8080,
    domain TEXT,
    label TEXT,
    username TEXT,
    password_hash TEXT,
    status TEXT DEFAULT 'unknown',  -- connected, unreachable, auth_failed
    latency_ms INTEGER,
    version TEXT,
    workspace TEXT,
    vultr_instance_id TEXT,         -- link to Vultr instance if auto-deployed
    added_at INTEGER NOT NULL,
    last_seen INTEGER,
    last_sync INTEGER
);
```

### New `mesh_events` table (for gossip protocol)

```sql
CREATE TABLE IF NOT EXISTS mesh_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,       -- node_added, node_removed, sync
    node_id TEXT,
    source_node_id TEXT,
    payload TEXT,
    created_at INTEGER NOT NULL
);
```

## New Source Files

| File | Purpose |
|------|---------|
| `src/client/NodeClient.hpp/cpp` | HTTP client for communicating with peer nodes |
| `src/services/MeshManager.hpp/cpp` | Background service: health checks, gossip sync |
| `src/infra/ProvisioningPoller.hpp/cpp` | Background service: poll Vultr for new instance readiness |

## Security Considerations

- **Credential storage:** Node passwords stored as bcrypt hashes in SQLite (never in plaintext JSON)
- **Auth relay:** When proxying to a remote node, the local node authenticates using stored credentials; the user's browser never sees remote credentials
- **Mesh sync auth:** Only nodes that share a mesh key can sync. Generated on first mesh setup, distributed during `POST /api/nodes` to the target
- **TLS:** Nodes should use HTTPS in production. The proxy endpoint validates TLS certs by default (can be disabled for self-signed via config)

## Estimated Effort

| Phase | Effort | Dependencies |
|-------|--------|-------------|
| 1. Connection Testing | 1-2 days | None |
| 2. Chat @mentions + Sidebar | 2-3 days | Phase 1 |
| 3. Mesh Gossip | 2-3 days | Phase 1 |
| 4. Remote UI Proxy | 2-3 days | Phase 1 |
| 5. Auto-Registration | 1-2 days | Phase 1, 3 |

Phases 2-4 can run in parallel after Phase 1 is complete.

## Priority Order

1. **Phase 1** — prerequisite for everything
2. **Phase 2** — highest user-visible value (chat + @mentions)
3. **Phase 4** — remote UI access is a killer feature
4. **Phase 3** — mesh awareness completes the network
5. **Phase 5** — nice-to-have automation
