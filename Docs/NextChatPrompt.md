# BobBot v4.0 — Full SDK Integration

Read these docs first:
- `Plugins/BobBot/Docs/SDKClientRewrite.md` — the core rewrite plan (query() → ClaudeSDKClient)
- `Plugins/BobBot/Docs/SDKCapabilities.md` — full SDK feature reference (BobBot uses 5% today)
- Memory at `~/.claude/projects/c--UGW-game/memory/project_bobbot_v3.md`

## Context

BobBot is an AI chat plugin for Unreal Engine 5.4. It has 159 MCP tools, a built-in chat UI with 5 tabs (Welcome, Connect, Chat, Context, Info), and connects to Claude via the Agent SDK. The plugin is at `c:\UGW\game\Plugins\BobBot\`.

The current SDK backend (`bob_chat_sdk.py`) uses `query()` which spawns a new `claude.exe` per message. This is broken — no conversation continuity, context fills in 2-3 messages, slow. The rewrite to `ClaudeSDKClient` (persistent subprocess) is planned but not implemented.

## What to implement

### Phase 1: Core Rewrite (Critical)
Rewrite `bob_chat_sdk.py` from `query()` to `ClaudeSDKClient`:
- Persistent connection via `connect()` / `disconnect()`
- `client.query()` + `client.receive_response()` per message
- Inline CLAUDE.md + PROJECT.md in system prompt (stop reading via tools)
- `get_context_usage()` for accurate context bar data
- `interrupt()` for graceful cancel
- Proper `cleanup()` / `clear_session()` that disconnects
- OAuth subscription works (same claude.exe, same credentials)

### Phase 2: Enhanced Chat Features
- `set_model()` — live model switching without reconnect
- `set_permission_mode()` — switch between allow/plan/review mid-chat
- Effort control slider (`effort: "low"|"medium"|"high"|"max"`)
- Extended thinking toggle (`ThinkingConfig`)
- `stop_task(task_id)` — cancel running subagent

### Phase 3: Hooks Integration
Replace the file-based approval handshake with SDK hooks:
- `PermissionRequest` hook → custom Slate approval UI (no more _approval_pending.json)
- `PreToolUse` / `PostToolUse` hooks → tool call logging in chat
- `Notification` hook → display Claude's alerts in BobBot UI
- `SubagentStart` / `SubagentStop` hooks → better subagent tracking

### Phase 4: Session Management
Use SDK session functions instead of BobBot's custom JSON persistence:
- `list_sessions()` → browse history sidebar
- `get_session_info()` → show metadata without loading transcript
- `get_session_messages()` → load conversation with pagination
- `rename_session()` → custom titles
- `tag_session()` → filter by workflow
- `delete_session()` → clean removal

### Phase 5: Advanced Features
- `enable_file_checkpointing` + `rewind_files()` → undo to any message
- Custom agents via `AgentDefinition` → specialized sub-agents
- `get_mcp_status()` / `toggle_mcp_server()` → MCP status panel
- `create_sdk_mcp_server()` → in-process tools (no TCP round-trip)
- `task_budget` → token-paced execution

## Key files
- `Content/Python/bob_chat_sdk.py` — SDK backend (full rewrite)
- `Content/Python/bob_chat.py` — subprocess fallback + dispatch
- `Content/Python/bob_mcp_server.py` — TCP server with approval logic (hooks will replace file-based flow)
- `Source/BobBot/Private/BobBotChatController.cpp` — chat state, polling, approval handling
- `Source/BobBot/Private/Widgets/SBobBotChatTab.cpp` — chat UI, header, approval widget
- `Source/BobBot/Private/Widgets/SBobBotConnectTab.cpp` — settings, troubleshooting
- `Source/BobBot/Public/BobBotConfig.h` — config fields

## Constraints
- OAuth/subscription auth must work (claude.exe handles it, SDK doesn't touch auth)
- C++ side polls `bob_chat.poll()` every 100ms — this pattern stays
- `_stream_events` list + `_lock` is the async→game thread bridge — this stays
- UE's Python is single-threaded game thread — async runs in background thread
- Subagent display, fork, auto-approve, cost/context bar all exist and should keep working

Start with Phase 1. Plan incrementally, implement, test, then move to the next phase.
