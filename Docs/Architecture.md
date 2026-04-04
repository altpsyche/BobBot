# Architecture

How BobBot is built, for contributors and power users.

## System overview

```
User types message
       |
       v
C++ (SBobBotChatTab)
       |
       v
FBobBotChatController  -->  FBobBotPythonBridge  -->  UE Python (game thread)
       |                                                      |
       v                                                      v
bob_chat_sdk.py (facade)                             bob_mcp_server.py (TCP)
       |                                                      ^
       v                                                      |
ClaudeSDKClient (persistent process)  -->  HTTP Bridge (port 13580)  -->  MCP tools
```

A user message goes from the Chat tab through the controller, which calls `bob_chat.send_message()` via UE's Python bridge. The SDK client sends it to Claude, which responds with text and tool calls. Tool calls go through the HTTP bridge to the TCP server running on the game thread, which executes Python code against the live editor.

## C++ side

| File | Responsibility |
|------|---------------|
| `BobBotChatController` | Chat state, message flow, slash commands, polling, approval |
| `BobBotPythonBridge` | C++ to Python IPC (`ExecPythonCommand`, `CallPythonJson`) |
| `BobBotConfig` | Settings persistence (`Saved/Config/BobBot.ini`) |
| `BobBotStatusMonitors` | Three monitors: server, bridge health + reconnection, SDK availability |
| `BobBotLib` | Blueprint editing API (variables, nodes, wiring, compilation) |
| `BobBotStyle` | SVG icon registration, rich text styles |
| `BobBotTheme` | Colors, fonts, spacing constants |
| `BobBotConstants` | UI helpers (`Card`, `Container`, `ConfigCheckbox`), model names, paths |

**Widget tabs** (all in `Source/BobBot/Private/Widgets/`):

| Tab | File | Notes |
|-----|------|-------|
| Welcome | `SBobBotWelcomeTab` | FTUE wizard, 6-step auto-setup |
| Connect | `SBobBotConnectTab` | Settings, auth, diagnostics. Construct split into 12 builder methods. |
| Chat | `SBobBotChatTab` | Message display, input toolbar. Construct split into 6 builder methods. |
| Context | `SBobBotContextTab` | System prompt, project context, memory viewer |
| Info | `SBobBotInfoTab` | Tool reference, slash commands, about |

## Python side

| Module | Responsibility |
|--------|---------------|
| `bob_platform.py` | Platform abstraction (Windows/macOS/Linux paths, subprocess flags, process management) |
| `bob_sdk_config.py` | System prompt, model state, MCP server config, agent definitions |
| `bob_sdk_permissions.py` | Tool classification (5 categories), auto-approve logic, permission hooks |
| `bob_sdk_events.py` | Stream event processing, hook callbacks, `_send_and_stream()` |
| `bob_sdk_client.py` | ClaudeSDKClient lifecycle (connect, disconnect, event loop, dead process detection) |
| `bob_chat_sdk.py` | Public API facade. Imports from above modules. This is what `bob_chat.py` re-exports. |
| `bob_chat.py` | Thin re-export wrapper so C++ can `import bob_chat` without knowing about the SDK split |
| `bob_bridge_launcher.py` | Venv creation, pip installs, bridge process lifecycle |
| `bob_mcp_server.py` | TCP server on the game thread, tool classification, auto-approve |

**Tool modules** (39 files in `Scripts/tools/`):

Each tool file has a `register(mcp, send_fn)` function that registers `@mcp.tool()` decorated functions. All tools use helpers from `_common.py`:

- `_exec_ue(code)` -- execute with `import unreal` pre-applied
- `actor_exec(label, code)` -- find actor by label, execute code with `target` bound
- `asset_exec(path, code)` -- load asset by path, execute code with `asset` bound

## Message flow (detailed)

1. User types in `SBobBotChatTab`, hits Enter
2. `FBobBotChatController::SendMessage()` writes message to a temp file, calls `bob_chat.send_message()`
3. `bob_chat_sdk.send_message()` ensures the SDK client is connected, then queues `_send_and_stream()` on the async event loop
4. `ClaudeSDKClient.query()` sends to the persistent Claude process
5. `_send_and_stream()` iterates `client.receive_response()`, appending events to `_stream_events` (text chunks, tool calls, completions)
6. C++ polls `bob_chat.poll()` every 50ms, gets events, dispatches to `HandleStreamEvent_*` methods
7. Each handler updates `ChatHistory` and broadcasts delegates
8. `SBobBotChatTab` receives delegate callbacks, rebuilds the affected message widget

## Tool call flow

1. Claude decides to call a tool (e.g., `spawn_actor`)
2. SDK sends tool call to the HTTP bridge (port 13580)
3. Bridge forwards via TCP to `bob_mcp_server.py` inside UE
4. Server executes the tool's Python code on the game thread
5. Output (stdout capture) is returned through the chain
6. Claude sees the result and continues reasoning

## Theme system

All visual styling is centralized:

- Colors: `BobBot::Theme::` and `BobBot::Colors::` (in `BobBotTheme.h`)
- Fonts: `Theme::FontBody()`, `FontHeading()`, `FontCode()`, etc.
- Containers: `UI::Card()`, `UI::Container()`, `UI::Toolbar()`, `UI::AccentCard()`
- Config checkboxes: `UI::ConfigCheckbox(&FBobBotConfig::bField, Label)`
- 14 SVG icons registered in `BobBotStyle.cpp`
