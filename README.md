# BobBot

BobBot is an editor plugin for Unreal Engine 5.4 that puts Claude inside the editor. You tell it what you want and it runs Python against the live session to make it happen. It connects to the running engine through the Model Context Protocol, so Claude can see your project and make changes directly.

It supports Claude Code subscription (OAuth) and direct API key authentication (Anthropic, Amazon Bedrock, Google Vertex).

## How it works

BobBot uses the Agent SDK backend, which keeps a persistent Claude process alive across messages for near-zero latency. A subprocess fallback spawns a new Claude CLI process per message if the SDK is unavailable.

Claude has 159 built-in tools covering actors, assets, materials, levels, lighting, physics, animation, sequencer, AI, widgets, diagnostics, and much more. It also has `execute_unreal_python` for running arbitrary code when the built-in tools don't cover a case.

An HTTP MCP bridge runs persistently on port 13580. The bridge forwards tool calls over TCP to a server running inside the editor's Python environment, which executes code on the game thread.

## Getting started

You need two things before BobBot will work.

**Claude Code** is the CLI that powers the chat. Install it with `npm install -g @anthropic-ai/claude-code`, then run `claude login` to authenticate.

**PythonScriptPlugin** ships with UE 5.4 but is disabled by default. Enable it in Edit > Plugins.

Drop the `BobBot` folder into your project's `Plugins/` directory and restart the editor. Open Window > BobBot. On first launch, the Welcome tab automatically creates a Python environment from UE's bundled Python and installs all dependencies (~30 seconds). No manual commands needed.

## Use with other editors

BobBot's tools are not limited to the built-in chat. Any editor with MCP support can connect to the same tool server. The `.mcp.json` at your project root is auto-generated with HTTP transport so Claude Code picks it up immediately.

From the Connect tab's Advanced section, click "Setup" next to any editor to write its MCP config:

| Editor | Config location |
|--------|----------------|
| Claude Code | `.mcp.json` (auto-generated) |
| Cursor | `.cursor/mcp.json` |
| VS Code | `.vscode/mcp.json` |
| Windsurf | `~/.codeium/windsurf/mcp_config.json` |

All editors share the same HTTP bridge and tool server. Each gets an isolated Python namespace. Sessions are independent — conversations in BobBot chat don't appear in VS Code and vice versa.

## Built-in tools

BobBot ships with 159 MCP tools organized by category. Claude picks the right tool based on what you ask.

**Actors** (6): get selected actors, list all actors with optional class filter, spawn by class path, delete selected, inspect properties and components, set properties.

**Assets** (4): search by path and type, get detailed info, create Blueprints, create Materials.

**Asset Operations** (6): rename, duplicate, delete, move assets, get references, get dependencies.

**Materials** (3): add expression nodes, connect to properties, list expressions.

**Levels** (3): get current level info, open level, save level.

**Level Streaming** (3): add streaming sub-level, remove streaming level, list all streaming levels.

**Viewport** (3): capture screenshot, run console commands, read output log.

**Context** (2): get project info, get editor state.

**Core** (2): execute arbitrary Python, ping editor.

**Editor Operations** (8): select/deselect actors, undo/redo, focus viewport on actor, rename actors, organize into folders, full selection info.

**Tags & Layers** (4): set/get actor tags, find actors by tag, set editor layer.

**Components** (4): add/remove components on actors, get/set component properties.

**World** (4): get/set world settings (gravity, kill Z), get/set game mode.

**Collision** (3): set collision preset, enable/disable collision, get collision info.

**Physics** (4): enable/disable physics, set collision channel, get physics info, set mass/damping.

**Lighting** (5): create lights (Point/Spot/Directional/Rect/Sky), set properties, list all lights, set lightmap resolution, create full outdoor lighting setup.

**Camera** (4): create camera actors, set lens properties, get/set viewport camera position.

**Texture & Mesh** (5): inspect static meshes (LODs, bounds, materials), set mesh on actors, inspect textures, import images as textures, set material texture parameters.

**Import/Export** (3): import files (FBX, OBJ, PNG, WAV), export assets, FBX import with options.

**Post-Process** (5): create volumes, set post-process settings, get overridden settings, color grading, rendering stats.

**Splines** (4): create spline actors, inspect splines, add points, set mesh along spline.

**Data Tables** (3): create DataTables, add/update rows, list rows.

**Skeletal Mesh** (4): inspect skeletons and skeletal meshes, create sockets, attach actors to sockets.

**Sequencer** (5): create LevelSequences, inspect tracks, add actor bindings, set length, play/preview.

**Animation** (5): create AnimBPs/Montages/BlendSpaces, list skeleton animations, inspect AnimBP info.

**Blueprint Advanced** (6): create functions/events, list functions/components, reparent, create interfaces.

**Enhanced Input** (4): create Input Actions/Mapping Contexts, add key mappings, list input assets.

**Audio** (3): create SoundCues, list audio assets, set audio on actors.

**Landscape** (3): get landscape info, set material, list paint layers.

**Foliage** (3): list foliage types, register mesh as foliage, get instance stats.

**Niagara/VFX** (3): create particle systems, inspect emitters, set parameters.

**AI/Behavior** (6): create Behavior Trees/Blackboards/EQS, add blackboard keys, list AI assets.

**PCG** (4): create PCG graphs, inspect, execute on actors, list PCG volumes.

**UMG/Widgets** (4): create Widget Blueprints, inspect hierarchy, add widget components, list widgets.

**PIE Runtime** (5): start/stop PIE, check status, inspect game actors, run console commands in game.

**Source Control** (4): check status, checkout, checkin, revert assets.

**Build** (4): build lighting, compile all Blueprints, validate assets, run map check.

**Movie Render** (3): create render jobs, check queue status, render sequences to images.

**Debug/Profiling** (4): frame stats, memory stats, GPU stats, scene benchmark.

**System** (1): get_bobbot_status for diagnostics.

Claude also has access to `UBobBotLib`, a C++ library exposed to Python that fills gaps in UE 5.4's Blueprint editing API (variables, components, function/event graph creation, graph nodes, compilation, CDO access).

## The five tabs

The **Welcome** tab appears on first launch. It walks through prerequisites, creates a Python environment from UE's bundled Python, installs dependencies, starts the bridge, and verifies the SDK. All automatic. A Skip button is available for advanced users. The tab never appears again after setup completes.

The **Connect** tab is the main setup screen. The Setup section shows Claude Code status, backend mode, and readiness. The Authentication section supports OAuth or API key with provider selection (Anthropic, Bedrock, Vertex). The Bridge section shows health and has a restart button. Model selection (Sonnet, Opus, Haiku) and session stats follow. A collapsible Advanced section holds tool permissions with auto-approve categories, backend mode toggle, cost budget, bridge and server settings, multi-editor setup, paths, prerequisites, and troubleshooting tools (health check, bridge log viewer, rebuild venv, reinstall packages, regen MCP config, clear temp files, kill port conflicts, factory reset).

The **Chat** tab is where you talk to Claude. The header shows model, cost (with color thresholds against budget), message count, and context usage percentage with a progress bar. Tool calls render inline with collapsible code blocks. Subagent tasks appear as expandable colored areas with nested tool calls. Session forking lets you branch conversations. A dropdown shows all chats in a tree structure (parent/fork relationships). Slash commands: `/clear`, `/cost`, `/model`, `/new`, `/chats`, `/help`.

The **Context** tab controls what Claude knows: system prompt editor, project context (PROJECT.md), and tool reference display with 24 domain-specific rules files.

The **Info** tab is a read-only reference: slash commands, all tools by category, BobBotLib API, and about section with dynamically computed stats.

## Permission modes

BobBot has three modes for controlling what Claude can do.

In **Allow Always** mode, Claude runs tools without asking.

In **Ask Me** mode, tool calls pause for approval. Auto-approve categories let you whitelist read-only and viewport tools while requiring approval for create, modify, and code execution. Each approval shows the tool name, category, code to run, and an "Always allow [Category]" button. Auto-approved tools are grouped into collapsed summaries in the chat.

In **Chat Only** mode, Claude can answer questions but cannot touch the editor.

## Adding your own tools

BobBot scans two directories for tools:

1. `Plugins/BobBot/Scripts/tools/` for built-in tools
2. `<ProjectRoot>/BobBot/tools/` for project-specific tools

To add a tool, create a Python file with a `register` function:

```python
# BobBot/tools/my_tool.py

def register(mcp, send_fn):
    @mcp.tool()
    def my_custom_tool(param: str) -> str:
        """Description that Claude sees when choosing tools."""
        result = send_fn({"type": "execute", "code": f"print('Hello from {param}')"})
        if result.get("success"):
            return result.get("output", "(no output)")
        return "Error: " + result.get("error", "unknown")
```

The bridge picks it up on the next connection. No C++ changes needed. Project tools live outside the plugin directory so they survive plugin updates.

## Configuration

Settings live in `Saved/Config/BobBot.ini` and are exposed through the Connect tab's Advanced section. The defaults work out of the box.

The TCP server runs on port 13579 at 127.0.0.1 with up to 4 concurrent MCP connections and a rate limit of 30 messages per second. The HTTP bridge runs on port 13580 and auto-starts. A per-message cost budget (default $5.00 USD) is configurable in Advanced.

## Architecture

The plugin separates business logic from UI. `FBobBotChatController` owns all chat state: sending messages, polling, slash commands, multi-chat management, approval flow, subagent tracking, session forking, and history persistence. Widgets subscribe to its multicast delegates.

`SBobBotPanel` orchestrates five tabs: Welcome, Connect, Chat, Context, Info. The Welcome tab gates all others during first-time setup.

On the Python side, `bob_chat_sdk.py` is the default Agent SDK backend (persistent Claude process), with `bob_chat.py` as the subprocess fallback. `bob_mcp_bridge_http.py` is the persistent HTTP MCP bridge. `bob_bridge_launcher.py` manages bridge lifecycle including venv creation and dependency installation. `bob_mcp_server.py` runs the TCP server on the game thread with tool classification and auto-approve logic. All tool modules share `tools/_common.py` for thread-safe UE command execution with automatic tool name extraction.

## Security

The TCP and HTTP servers only bind to localhost. Authentication uses Claude Code's OAuth flow by default. Users who supply their own API key have it stored in the local INI file. Each MCP client gets an isolated Python namespace. Config values reach Python through OS environment variables. Rate limiting is configurable per client. Three permission modes control what Claude can do. Tool calls are classified into five categories with independent auto-approve settings.

## Project layout

```
BobBot/
  BobBot.uplugin
  CLAUDE.md                     Tool reference (159 tools, BobBotLib API, UE essentials)
  Config/Rules/                 24 domain-specific reference files (on-demand)
  Source/BobBot/
    Public/
      BobBot.h                  Module interface
      BobBotChatController.h    Chat logic, message struct, delegates, subagent tracking, fork
      BobBotCommands.h          Editor commands and menus
      BobBotConfig.h            Persistent settings (auth, SDK, budget, bridge, auto-approve)
      BobBotConstants.h         Colors, paths, intervals, scripts
      BobBotLib.h               Blueprint API (19 methods)
      BobBotPythonBridge.h      Python IPC abstraction
      BobBotRuntimeStatus.h     Transient detection and bridge state
      BobBotStyle.h             Slate styling
      Widgets/
        SBobBotPanel.h          Tab orchestrator (Welcome, Connect, Chat, Context, Info)
        SBobBotWelcomeTab.h     Welcome/FTUE tab
        SBobBotConnectTab.h     Connect tab (setup, auth, bridge, model, advanced, troubleshooting)
        SBobBotChatTab.h        Chat tab (cost/context header, subagents, fork, approval)
        SBobBotContextTab.h     Context tab (prompt, project, rules)
        SBobBotInfoTab.h        Info tab (tools, API, about)
    Private/
      (matching .cpp files)
  Scripts/
    bob_mcp_bridge.py           MCP bridge (stdio, fallback)
    bob_mcp_bridge_http.py      MCP bridge (HTTP, default, persistent)
    tools/
      _common.py                Thread-safe shared helpers with tool name extraction
      core.py                   execute_unreal_python, ping_unreal
      system.py                 get_bobbot_status diagnostic tool
      (38 more tool modules)
  Content/Python/
    bob_chat.py                 Claude subprocess backend (fallback)
    bob_chat_sdk.py             Claude Agent SDK backend (default, persistent)
    bob_bridge_launcher.py      Venv creation, dependency install, bridge lifecycle
    bob_mcp_server.py           TCP server with tool classification and auto-approve

<ProjectRoot>/
  .mcp.json                    Auto-generated MCP config (HTTP transport)
  PROJECT.md                   User's project context (created via Context tab)
  BobBot/tools/                User-created project-specific tools (auto-discovered)
```

Created by Siva Vadlamani, [altpsyche.dev](https://altpsyche.dev)
