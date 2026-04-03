# BobBot SDK Rewrite: query() → ClaudeSDKClient

## Problem

BobBot uses `query()` which spawns a **new claude.exe subprocess per message** and destroys it after each response. This causes:

1. **No conversation continuity** — each message is independent, Claude doesn't remember what you said before
2. **Context fills in 2-3 messages** — `--resume` replays full history from disk every time + Claude reads CLAUDE.md/PROJECT.md via tool calls every message
3. **Slow** — subprocess spawn + MCP tool discovery + file reads every single message

## Solution

Switch to `ClaudeSDKClient` which keeps **one persistent subprocess** alive:
- `connect()` once → subprocess stays alive across messages
- `client.query()` per message → context maintained naturally in-process
- `client.get_context_usage()` → accurate context data (totalTokens, maxTokens, percentage)
- `disconnect()` on cleanup/clear/new chat
- Inline CLAUDE.md + PROJECT.md in system prompt → no tool calls to read them
- OAuth works via the same bundled `claude.exe` which reads `~/.claude/.credentials.json`

## Current Architecture (BROKEN)

```
send_message("hello")
  → spawn NEW claude.exe (3-5s)
  → claude reads session file from disk (--resume, replays everything)
  → claude reads CLAUDE.md via tool call (~2.5k tokens added to history)
  → claude reads PROJECT.md via tool call (more tokens in history)
  → response streams back
  → claude.exe killed
  
send_message("do something else")
  → spawn ANOTHER new claude.exe (3-5s again)
  → claude re-reads entire session from disk (now includes previous tool reads)
  → claude reads CLAUDE.md AGAIN via tool call (duplicated in history)
  → context is now 60%+ full from just 2 messages
  → response streams back
  → claude.exe killed
```

## Target Architecture

```
send_message("hello")
  → _ensure_client() → ClaudeSDKClient.connect() → spawns claude.exe ONCE
  → client.query("hello") → sends to living process
  → client.receive_response() → streams response
  → client.get_context_usage() → accurate token data
  → claude.exe stays alive

send_message("do something else")
  → _ensure_client() → already connected, reuse
  → client.query("do something else") → context naturally includes previous exchange
  → no file reads, no session replay, no redundant tool calls
  → response streams back
  → claude.exe stays alive

cleanup() / clear_session()
  → client.disconnect() → kills claude.exe
  → next send_message() creates fresh connection
```

## ClaudeSDKClient API

```python
from claude_agent_sdk import ClaudeSDKClient, ClaudeAgentOptions

# Options — same as current, works with OAuth and API key
options = ClaudeAgentOptions(
    model="sonnet",
    system_prompt="...",                    # Inline CLAUDE.md content here
    cwd="/path/to/Saved/BobBot",
    mcp_servers={"unreal": {...}},
    permission_mode="bypassPermissions",
    cli_path="/path/to/bundled/claude.exe", # Same bundled exe, OAuth works
)

# Persistent client
client = ClaudeSDKClient(options=options)
await client.connect()                      # Spawns claude.exe ONCE

# First message
await client.query("Hello")
async for msg in client.receive_response():
    # msg: AssistantMessage, UserMessage, ResultMessage, SystemMessage,
    #       TaskStartedMessage, TaskProgressMessage, TaskNotificationMessage
    pass

# Follow-up (context preserved automatically)
await client.query("Now create a material")
async for msg in client.receive_response():
    pass

# Accurate context data
usage = await client.get_context_usage()
# Returns: {totalTokens: 12345, maxTokens: 200000, percentage: 6.17, model: "claude-sonnet-4-6", ...}

# Model switching (live, no reconnect needed)
await client.set_model("opus")

# Done
await client.disconnect()                   # Kills claude.exe
```

## What Changes in bob_chat_sdk.py

### 1. New Module State
```python
_client: ClaudeSDKClient | None = None
_client_connected: bool = False
```

### 2. Inline System Prompt
Read CLAUDE.md (~9KB, ~3k tokens) and PROJECT.md at module load. Include directly in system prompt string. Add "Do NOT read CLAUDE.md or PROJECT.md via tools — content is below." Rules files stay as references (24 files, too many to inline).

### 3. `_ensure_client()` — Lazy Connect
```python
async def _ensure_client():
    if _client is not None and _client_connected:
        return _client
    # Disconnect stale client if any
    # Create ClaudeSDKClient(options)
    # await client.connect()
    # If _session_id set, pass options.resume for chat history restore
    return client
```

### 4. `_send_and_stream()` — Use Persistent Client
```python
client = await _ensure_client()
await client.query(user_message)
async for msg in client.receive_response():
    # Same isinstance checks as current code
    # Same event appending to _stream_events
    pass
# After ResultMessage:
ctx = await client.get_context_usage()
# Fold totalTokens/maxTokens into "complete" event
```

### 5. `cleanup()` / `clear_session()` — Disconnect
```python
def cleanup():
    # await _client.disconnect()  (via event loop)
    # _client = None
    # Stop event loop
    # Next send_message() reconnects fresh

def clear_session():
    # Same — disconnect, clear state
    # Fresh conversation on next message
```

### 6. `set_session_id(sid)` — Trigger Reconnect
When loading a saved chat, disconnect current client so next `_ensure_client()` reconnects with `options.resume = sid`.

### 7. `set_model(name)` — Live Switch
If connected, `await client.set_model(name)`. No reconnect needed.

### 8. Context Usage — Accurate Data
`client.get_context_usage()` returns `totalTokens`, `maxTokens`, `percentage` directly from the SDK. No more guessing from `ResultMessage.usage` fields.

## What Changes in bob_chat.py

Update `_SYSTEM_PROMPT` to inline CLAUDE.md content (subprocess fallback has the same file-reading problem).

## What Stays the Same

- **Public API**: `send_message()`, `poll()`, `cleanup()`, etc. — identical signatures
- **C++ side**: no changes needed (polls same events, same "complete" event format)
- **Threading model**: async loop in background thread, `_stream_events`, `_lock`
- **Auth**: OAuth via `~/.claude/.credentials.json`, API key via env vars — same `claude.exe`
- **Popen monkey-patch**: `CREATE_NO_WINDOW` stays for console exe
- **Subagent events**: TaskStartedMessage/Progress/Notification parsing unchanged
- **Fork support**: `fork_current_session()` uses standalone `fork_session()`, not the client
- **Tool classification**: Unchanged (happens in bob_mcp_server.py)
- **Auto-approve**: Unchanged

## Files to Modify

| File | Changes |
|------|---------|
| `Content/Python/bob_chat_sdk.py` | Full rewrite: ClaudeSDKClient, inline prompt, persistent lifecycle, get_context_usage() |
| `Content/Python/bob_chat.py` | Inline CLAUDE.md in system prompt for subprocess fallback |

## Implementation Order

1. Inline system prompt (both files)
2. Add `_ensure_client()` with ClaudeSDKClient
3. Rewrite `_send_and_stream()` → `client.query()` + `client.receive_response()`
4. Rewrite `cleanup()` / `clear_session()` → disconnect
5. Update `set_session_id()` → trigger reconnect
6. Update `set_model()` → live switching
7. Fold `get_context_usage()` into "complete" event

## Verification

1. Send message → response arrives
2. Send follow-up → Claude remembers previous message naturally
3. 10+ messages in one chat → context bar shows reasonable % (not 200%)
4. Clear session → next message starts completely fresh (no carry-over)
5. Switch chats → each chat has independent context
6. Context bar shows accurate % from `get_context_usage()`
7. Delete venv, restart → Welcome tab, setup, first message works
8. OAuth auth works (no API key configured, uses subscription)
9. Subagent tasks display correctly
10. Fork creates independent branch
