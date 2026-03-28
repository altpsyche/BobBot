# BobBot Code Audit — 2026-03-28

## Overview

| Metric | Value |
|--------|-------|
| **C++ files** | 12 (6 headers, 6 implementations) |
| **C++ lines** | ~2,890 |
| **Python files** | 5 |
| **Python lines** | ~980 |
| **Total source** | ~3,870 lines |
| **Git commits** | 7 |
| **Plugin version** | 1.0 (IsBetaVersion=true) |

**Architecture:** Editor chat spawns `claude -p --output-format json` subprocess via user's Claude Code OAuth. Claude Code calls MCP tools (`execute_unreal_python`, `ping_unreal`) via a TCP bridge. No API key required.

```
SBobBotPanel (C++ Slate UI)
    |
    | ExecPythonCommand
    v
bob_chat.py (subprocess manager)
    |
    | subprocess.Popen("claude -p ...")
    v
Claude CLI
    |
    | MCP protocol
    v
bob_mcp_bridge.py (FastMCP server, launched by Claude)
    |
    | TCP (newline-delimited JSON)
    v
bob_mcp_server.py (game-thread tick callback)
    |
    | exec(code, namespace)
    v
UE Python + unreal.BobBotLib (C++)
```

---

## File Inventory

### C++ Source

| File | Lines | Role |
|------|-------|------|
| `Source/BobBot/Public/BobBot.h` | 46 | Module interface |
| `Source/BobBot/Private/BobBot.cpp` | 355 | Startup, prereqs, JSON gen, server auto-start |
| `Source/BobBot/Public/BobBotConfig.h` | 64 | Config singleton + EBobBotPermissionMode enum |
| `Source/BobBot/Private/BobBotConfig.cpp` | 95 | Load/Save from BobBot.ini, ApplyEnvironmentVars |
| `Source/BobBot/Public/BobBotLib.h` | 105 | Blueprint manipulation API (17 methods) |
| `Source/BobBot/Private/BobBotLib.cpp` | 489 | Type resolution, variables, components, graph nodes |
| `Source/BobBot/Public/Widgets/SBobBotPanel.h` | 163 | UI panel header (50+ methods, 40+ members) |
| `Source/BobBot/Private/Widgets/SBobBotPanel.cpp` | 1,456 | Full UI: Connect tab, Chat tab, polling, approval |
| `Source/BobBot/Public/BobBotCommands.h` | 23 | Single UI command |
| `Source/BobBot/Private/BobBotCommands.cpp` | 12 | Command registration |
| `Source/BobBot/Public/BobBotStyle.h` | 32 | Slate style |
| `Source/BobBot/Private/BobBotStyle.cpp` | 61 | Style initialization |

### Python Source

| File | Lines | Role |
|------|-------|------|
| `Content/Python/bob_chat.py` | 378 | Claude CLI subprocess wrapper, session mgmt |
| `Content/Python/bob_mcp_server.py` | 422 | TCP server inside UE, game-thread dispatch, approval queue |
| `Scripts/bob_mcp_bridge.py` | 134 | Standalone MCP server (Claude launches this) |
| `Scripts/tools/core.py` | 48 | MCP tool definitions (execute_unreal_python, ping_unreal) |
| `Scripts/tools/__init__.py` | 1 | Package marker |

### Config & Data

| File | Purpose |
|------|---------|
| `BobBot.uplugin` | Plugin manifest |
| `Source/BobBot/BobBot.Build.cs` | 19 module dependencies |
| `Saved/Config/BobBot.ini` | Runtime config (port, model, permissions) |
| `.mcp.json` (project root) | MCP server config for Claude CLI |
| `Saved/BobBot/_system_prompt.txt` | System prompt for Claude subprocess |
| `Saved/BobBot/_chat_msg.txt` | IPC: user message to bob_chat.py |
| `Saved/BobBot/_approval_pending.json` | IPC: tool approval request |
| `Saved/BobBot/_approval_response.json` | IPC: tool approval decision |

---

## Sprint Status vs. Audit Plan

Reference: [bobbot_audit.md](bobbot_audit.md) (the UX audit)

| Sprint | Item | Status |
|--------|------|--------|
| **1** | Thinking indicator | DONE |
| **1** | Enter-to-send | DONE |
| **1** | Model button highlight | DONE |
| **1** | Send disabled while thinking | DONE |
| **2** | Setup checklist at top | DONE |
| **2** | "Chat Session" replaces "Connected Clients" | DONE |
| **2** | Server status simplified (no comma, no client count) | DONE |
| **2** | Advanced section collapsed by default | DONE |
| **2** | Permission mode radio buttons | DONE |
| **2** | Single Setup/Regenerate button per editor | DONE |
| **2** | Config existence check per editor | DONE |
| **3** | Thinking as animated chat message | DONE |
| **3** | Stop button | DONE |
| **3** | Chat header bar (model, cost, count) | DONE |
| **3** | Error messages in orange | DONE |
| **3** | Chat history persistence | NOT DONE |
| **4** | System prompt editor | DONE |
| **4** | CLAUDE.md project context editor | DONE |
| **4** | Server settings UI (port, auto-start, max clients, rate) | DONE |
| **4** | Chat timeout setting | NOT DONE (hardcoded 300s in env var) |
| **4** | Paths debug section | DONE |
| **5** | Code block rendering | NOT DONE |
| **5** | Message copy button | NOT DONE |
| **5** | Slash commands | DONE (/clear, /cost, /model, /help) |
| **5** | "Ask Me" approval dialog | DONE |

**Summary:** Sprints 1-2 complete. Sprint 3 missing persistence. Sprint 4 missing timeout UI. Sprint 5 missing rendering and copy.

---

## Bugs

### BUG-1: Unsafe pointer comparison in approval buttons — FIXED

Changed range-for to indexed loop, replaced `&Msg == &ChatHistory.Last()` with `MsgIdx == ChatHistory.Num() - 1`.

---

### BUG-2: Brittle JSON parsing in PollServerStatus — FIXED

Replaced string-based `Contains()`/`Find()` parsing with `FJsonSerializer::Deserialize` + `TryGetBoolField`/`TryGetNumberField`.

---

### BUG-3: Race condition on `_session_id` and `_total_session_cost` writes — FIXED

Moved `_session_id` assignment and `_total_session_cost += cost` inside the existing `with _lock:` block alongside `_pending_response` and `_is_thinking`.

---

### BUG-4: `_process` access without lock in `_chat_thread` — FIXED

Timeout handler now copies `_process` under lock before calling `.kill()`.

---

### BUG-5: `TryGetStringField` doesn't populate output on failure — FIXED

Added `PendingApprovalTool.Empty()` and `PendingApprovalCode.Empty()` before the TryGet calls.

---

### BUG-6: No port binding failure handling — FIXED

Wrapped `socket.bind()` and related calls in try/except OSError. On failure, logs via `unreal.log_error()`, cleans up the socket, and returns gracefully.

---

## Security

### SEC-1: Python code interpolation in AutoStartPythonServer — FIXED

Replaced `FString::Printf` interpolation of config values into Python source with a ctypes-based approach that reads env vars directly from the OS environment block via `GetEnvironmentVariableW`, avoiding any config-to-code injection vector.

---

### SEC-2: `exec()` without sandboxing

**File:** `bob_mcp_server.py:79`
```python
exec(code, namespace)
```
By design, MCP tools execute arbitrary Python in the editor. This is the core feature. The mitigations are:
- Localhost-only TCP (no network exposure)
- Rate limiting (30 msg/s)
- "Ask Me" mode with UI approval
- "Chat Only" mode disabling tools entirely

**Assessment:** Acceptable for an editor plugin. Document the trust model.

---

### SEC-3: System prompt file not validated after custom save

**File:** `bob_chat.py:183-184`
```python
if _SYSTEM_PROMPT_FILE and os.path.isfile(_SYSTEM_PROMPT_FILE):
    return _SYSTEM_PROMPT_FILE
```
Uses the file path without checking if it was tampered with between writes. An adversary with file system access could modify the prompt file to inject instructions.

**Risk in practice:** Very low (requires local file system access, at which point the attacker has full control anyway).

---

## Code Quality Issues

### CQ-1: SBobBotPanel.cpp is 1,456 lines — monolithic

This single file contains:
- Connect tab construction (335 lines of Slate macros)
- Chat tab construction
- Chat message management
- Send/Clear/Stop handlers
- Tool approval polling and UI
- Thinking animation
- Server status polling
- Chat update polling
- 50+ helper methods

**Recommendation:** Extract into separate files:
- `SBobBotConnectTab.cpp` — connect tab build + helpers
- `SBobBotChatTab.cpp` — chat tab build + message rendering
- `BobBotChatController.cpp` — polling, subprocess interaction, approval flow

---

### CQ-2: Permission mode string conversion duplicated 3 times — FIXED

Added `FBobBotConfig::PermissionModeToString()` static method. All three call sites now use it.

---

### CQ-3: Variable search logic duplicated in BobBotLib.cpp — FIXED

Extracted `FindBlueprintVariable(UBlueprint*, FName)` static helper. All three functions now use it.

---

### CQ-4: Missing null checks after FindPin in BobBotLib.cpp — N/A

On review, the existing code already uses `if (UEdGraphPin* Pin = CallNode->FindPin(...))` guards. No fix needed.

---

### CQ-5: No error handling on ExecPythonCommand calls

Throughout `SBobBotPanel.cpp`, `ExecPythonCommand()` is called ~10 times with no return value check. If the Python script throws, the error is silently swallowed and the subsequent file read returns stale data.

**Impact:** Silent failures. The UI shows stale status or no response.

**Fix:** At minimum, check the bool return and log a warning. Better: have the Python write a sentinel value so the C++ side can detect a failed execution.

---

### CQ-6: RebuildChatMessages() reconstructs entire UI on every message — FIXED

Extracted `BuildChatMessageWidget()` static method shared by both paths. `AddChatMessage()` now appends the new widget directly (append-only). Full rebuild only happens for approval messages and `OnClearChatClicked()`.

---

### CQ-7: File-based IPC for Python communication

The C++ side communicates with Python by:
1. Writing a Python script string
2. Executing it via `ExecPythonCommand`
3. Python writes JSON to a temp file
4. C++ reads the temp file
5. C++ deletes the temp file

This is done for: Claude detection, server status polling, chat polling, approval polling, sending messages.

**Issues:**
- File I/O on every tick (100ms while thinking)
- Race conditions if file is read while Python is still writing
- Temp files left behind on crashes

**Recommendation:** This is the standard pattern for UE Python interop and is acceptable for the polling frequency. Consider buffering writes on the Python side and reading less frequently during idle (already done — 1s idle, 100ms active).

---

## Missing Features (from audit plan)

### MF-1: Chat history persistence (Sprint 3, item 12)
No save/restore of chat history. Closing the BobBot tab or restarting the editor loses all messages.

### MF-2: Chat timeout UI (Sprint 4, item 16)
Timeout is set via `BOB_CHAT_TIMEOUT` env var (default 300s) but has no UI control.

### MF-3: Code block rendering (Sprint 5, item 18)
Bot responses containing code are rendered as plain text. No monospace background, no syntax highlighting.

### MF-4: Message copy button (Sprint 5, item 19)
No way to copy a single message to clipboard.

---

## Configuration Completeness

| Setting | In Config | In UI | In .ini | In Env Var | Notes |
|---------|-----------|-------|---------|------------|-------|
| Port | Yes | Yes (SpinBox) | Yes | BOB_MCP_PORT | |
| Host | Yes | No | Yes | BOB_MCP_HOST | Always 127.0.0.1, no UI needed |
| Auto-start server | Yes | Yes (Checkbox) | Yes | No | |
| Auto-generate .mcp.json | Yes | Yes (Checkbox) | Yes | No | |
| Max clients | Yes | Yes (SpinBox) | Yes | BOB_MCP_MAX_CLIENTS | |
| Rate limit | Yes | Yes (SpinBox) | Yes | BOB_MCP_RATE_LIMIT | |
| Chat model | Yes | Yes (Buttons) | Yes | No | Set via bob_chat.set_model() |
| Permission mode | Yes | Yes (Radio) | Yes | BOB_PERMISSION_MODE | |
| System prompt | Yes | Yes (TextBox) | Yes | No | Written to file |
| Chat timeout | Yes | **No** | Yes | BOB_CHAT_TIMEOUT | Missing UI |
| Approval timeout | **No** | No | No | No | Hardcoded 120s in Python |
| Retry count | **No** | No | No | No | Hardcoded 2 in bridge |
| Socket timeout | **No** | No | No | No | Hardcoded 60s/120s |

---

## Dependencies

### C++ Module Dependencies (19)
Core, CoreUObject, Engine, EditorFramework, UnrealEd, LevelEditor, Slate, SlateCore, Kismet, KismetCompiler, BlueprintGraph, EditorScriptingUtilities, PythonScriptPlugin, Json, JsonUtilities, ApplicationCore, Projects, InputCore, ToolMenus

### Python Dependencies
- `mcp[cli]` — installed on-demand via `uv run --with mcp[cli]`
- `unreal` — provided by PythonScriptPlugin
- No pip/requirements.txt needed

### External Tools
- `claude` CLI — npm package `@anthropic-ai/claude-code`
- `uv` — Python package runner (astral.sh)
- `python` — system Python (for detection only; UE uses embedded Python)

---

## Recommendations (Priority Order)

### DONE (fixed 2026-03-28)
1. ~~**Fix BUG-2**~~ — Replaced string JSON parsing with FJsonSerializer in PollServerStatus
2. ~~**Fix BUG-6**~~ — Wrapped socket.bind() in try/except in bob_mcp_server.py
3. ~~**Fix BUG-3**~~ — Moved _session_id and _total_session_cost writes under lock
4. ~~**Fix CQ-4**~~ — Already had null checks (no fix needed)
5. ~~**Fix CQ-2**~~ — Added FBobBotConfig::PermissionModeToString(), used in all 3 sites
6. ~~**Fix CQ-3**~~ — Extracted FindBlueprintVariable() helper in BobBotLib.cpp
7. ~~**Fix SEC-1**~~ — Replaced Python string interpolation with ctypes env var reads
8. ~~**Fix CQ-6**~~ — Append-only chat rendering; full rebuild only for approvals/clear
9. ~~**Fix BUG-1**~~ — Index-based comparison for approval buttons
10. ~~**Fix BUG-4**~~ — Lock-protected _process access in timeout handler
11. ~~**Fix BUG-5**~~ — Reset approval fields before TryGetStringField
12. ~~**bob_mcp_bridge.py**~~ — Added UnicodeDecodeError to exception handler

### Remaining
- **Implement MF-2** — Add chat timeout SpinBox to Advanced section
- **Implement MF-1** — Chat history persistence (serialize to JSON on close)
- **Fix CQ-1** — Split SBobBotPanel.cpp into 3 files
- **Fix CQ-5** — Add ExecPythonCommand return value checks
- **Implement MF-3** — Code block rendering
- **Implement MF-4** — Message copy button
