# BobBot

BobBot is an editor plugin for Unreal Engine 5.4 that puts Claude inside the editor. You tell it what you want and it runs Python against the live session to make it happen. It connects to the running engine through the Model Context Protocol, so Claude can see your project and make changes directly.

It supports Claude Code subscription (OAuth) and direct API key authentication (Anthropic, Amazon Bedrock, Google Vertex).

## How it works

By default, BobBot uses the Agent SDK backend (`bob_chat_sdk.py`), which keeps a persistent Claude process alive across messages for near-zero latency. A subprocess fallback (`bob_chat.py`) spawns a new Claude CLI process per message if needed.

Claude has 159 built-in tools covering actors, assets, materials, levels, lighting, physics, animation, sequencer, AI, widgets, diagnostics, and much more. It also has `execute_unreal_python` for running arbitrary code when the built-in tools don't cover a case.

An HTTP MCP bridge (`bob_mcp_bridge_http.py`) runs persistently on port 13580, eliminating the 3-5 second cold start per message that the stdio bridge had. The bridge forwards tool calls over TCP to `bob_mcp_server.py`, which runs inside the editor's Python environment and executes code on the game thread. The original stdio bridge (`bob_mcp_bridge.py`) is still available as a fallback.

## Getting started

You need two things before BobBot will work.

**Claude Code** is the CLI that powers the chat. Install it with `npm install -g @anthropic-ai/claude-code`, then run `claude login` to authenticate.

**PythonScriptPlugin** ships with UE 5.4 but is disabled by default. Enable it in Edit > Plugins.

Drop the `BobBot` folder into your project's `Plugins/` directory and restart the editor. Open Window > BobBot. On first launch, the Welcome tab automatically sets up a Python environment and installs all dependencies (~30 seconds). No manual pip or uv commands needed.

## Built-in tools

BobBot ships with 158 MCP tools organized by category. Claude picks the right tool based on what you ask.

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

On top of these, Claude also has access to `UBobBotLib`, a C++ library exposed to Python that fills gaps in UE 5.4's Blueprint editing API (variables, components, function/event graph creation, graph nodes, compilation, CDO access). Tools automatically use modern UE APIs when available and fall back to BobBotLib on older versions.

## The four tabs

The **Connect** tab is a setup screen. The Setup section shows Claude Code status, backend mode (Agent SDK or Subprocess), and readiness. Below that, the Authentication section lets you choose between Claude Code subscription (OAuth) or your own API key with provider selection (Anthropic, Bedrock, Vertex). The HTTP Bridge section shows the bridge status and health with a restart button. Then you pick a model: Sonnet for speed, Opus for quality, Haiku for cost. Session stats show your current model, accumulated cost, and message count. A collapsible Advanced section holds tool permissions, backend mode toggle with an info dialog, cost budget, bridge and server settings, multi-editor setup, paths, and prerequisites.

The **Chat** tab is where you talk to Claude. Type a message and press Enter. Tool calls render inline with collapsible code blocks and Running/Complete status. Bot responses render inline markdown (bold, italic, code) and fenced code blocks in monospace. There is a copy button on every message, a stop button to kill a long request, and the history persists across editor restarts.

A dropdown in the chat header lets you switch between conversations. Click + to start a new chat. Each conversation saves independently with its own session, so multi-turn context is preserved when you switch back.

Slash commands: `/clear` resets the session, `/cost` shows spending, `/model` switches models, `/new` starts a new conversation, `/chats` lists all conversations, `/help` lists everything.

The **Context** tab controls what Claude knows. Three sections:

The system prompt editor lets you customize Claude's base instructions for BobBot conversations. Leave it empty to use the default.

The project context editor saves to PROJECT.md at your project root. Describe your project, conventions, and architecture here. The system prompt instructs Claude to read it at the start of each conversation.

The tool reference section shows the documentation that ships with BobBot. A concise CLAUDE.md in the plugin directory covers all 159 tools, the BobBotLib API, and UE essentials. 24 path-scoped rules files load on demand when Claude works with matching domains. This keeps Claude's context lean while giving it detailed reference material when it needs it.

The **Info** tab is a read-only reference. It lists all slash commands, all tools organized by category with collapsible cards, the BobBotLib C++ API, and an About section with dynamically computed stats (tool count, category count, C++ method count).

## Permission modes

BobBot has three modes for controlling what Claude can do.

In Allow Always mode, Claude runs tools without asking. You say "make a Fresnel material" and it does it.

In Ask Me mode, every tool call pauses and shows you the code Claude wants to run. You get Approve and Deny buttons inline in the chat. Nothing executes until you say so.

In Chat Only mode, Claude can answer questions but cannot touch the editor. Good for asking about your project without risking changes.

## Adding your own tools

BobBot scans two directories for tools:

1. `Plugins/BobBot/Scripts/tools/` for built-in tools that ship with the plugin
2. `<ProjectRoot>/BobBot/tools/` for project-specific tools you create

The project tools directory is created automatically on first launch with an example template. To add a tool, create a Python file with a `register` function:

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

The bridge picks it up on the next connection. No C++ changes, no recompilation. Project tools live outside the plugin directory so they survive plugin updates.

## Configuration

Settings live in `Saved/Config/BobBot.ini` and are exposed through the Connect tab's Advanced section. The defaults work out of the box.

The TCP server runs on port 13579 at 127.0.0.1. It starts automatically when the editor launches and accepts up to 4 concurrent MCP connections with a rate limit of 30 messages per second. The HTTP bridge runs on port 13580 and also auto-starts. The chat backend times out after 300 seconds. A per-message cost budget (default $5.00 USD) is configurable in Advanced.

The `.mcp.json` file at your project root is auto-generated so Claude Code and other MCP-compatible editors can find the bridge script. BobBot also generates an isolated copy at `Saved/BobBot/_bobbot_mcp.json` with `--strict-mcp-config` to prevent session conflicts with VS Code or other editors using Claude Code in the same project. You can set up Cursor, VS Code, and Windsurf from the Advanced section.

## Architecture

The plugin separates business logic from UI. `FBobBotChatController` owns all chat state: sending messages, polling for responses, slash command dispatch, multi-chat management, approval flow, and history persistence. It has no Slate dependencies. Widgets subscribe to its multicast delegates to react to state changes.

`SBobBotConnectTab`, `SBobBotChatTab`, `SBobBotContextTab`, and `SBobBotInfoTab` are the four tab widgets. They receive a pointer to the controller and bind to its delegates. `SBobBotPanel` is a thin orchestrator that creates the controller, wires up all tabs, and forwards tick.

`FBobBotPythonBridge` centralizes all C++ to Python communication. The bridge handles the temp directory, file cleanup, and JSON parsing.

`BobBotConstants` collects every color, temp file name, poll interval, and Python script fragment in one header. `FBobBotConfig` handles persistent settings. `FBobBotRuntimeStatus` holds transient detection state.

`UBobBotLib` is the Blueprint manipulation API with 19 static methods covering variables, components, function/event graph creation, graph nodes, compilation, and CDO access.

On the Python side, `bob_chat_sdk.py` is the default Agent SDK backend (persistent Claude process), with `bob_chat.py` as the subprocess fallback. `bob_mcp_bridge_http.py` is the persistent HTTP MCP bridge, with `bob_mcp_bridge.py` as the stdio fallback. `bob_bridge_launcher.py` manages bridge lifecycle. `bob_mcp_server.py` runs the TCP server on the game thread. All 40 tool modules share `tools/_common.py` for thread-safe UE command execution.

## Extending the plugin

Common extensions each have one place to go. To add an MCP tool, drop a Python file in `BobBot/tools/` at your project root. To add a slash command, add one line to the `SlashCommands` map in the `FBobBotChatController` constructor. To add a config setting, add a field to `FBobBotConfig` with its Load/Save lines, then add a UI control in `SBobBotConnectTab`. To add a Connect tab section, add Slate blocks in `SBobBotConnectTab::Construct`. To add a message type, add an `ESender` value in `FBobBotChatMessage` and a rendering case in `SBobBotChatTab`.

## Security

The TCP and HTTP servers only bind to localhost. There is no network exposure. Authentication is handled by Claude Code's OAuth flow by default. Users who supply their own API key have the key stored in the local INI file (OS keychain planned). Each MCP client gets an isolated Python namespace. Config values reach Python through OS environment variables, not interpolated into code strings. Rate limiting is configurable per client. The three permission modes give you control over what Claude is allowed to do, from fully autonomous to fully locked down.

## Project layout

```
BobBot/
  BobBot.uplugin
  CLAUDE.md                     Tool reference (~56 lines, imports PROJECT.md)
  Config/Rules/
    actors.md                   Spawning, properties, coordinates (on-demand)
    ai.md                       Behavior Trees, Blackboards, EQS (on-demand)
    animation.md                AnimBP, Montage, BlendSpace patterns (on-demand)
    audio.md                    Sound Cues, SoundWaves, spatial audio (on-demand)
    blueprints.md               BobBotLib, variables, compilation (on-demand)
    camera.md                   CameraActor, viewport, cinematic settings (on-demand)
    data_assets.md              Data Assets vs Data Tables (on-demand)
    data_tables.md              DataTable rows, struct schemas (on-demand)
    foliage.md                  Foliage types, instancing, stats (on-demand)
    input.md                    Enhanced Input actions and mappings (on-demand)
    landscape.md                Landscape API, layers, materials (on-demand)
    lighting.md                 Light types, outdoor setup, lightmaps (on-demand)
    materials.md                Material expressions and properties (on-demand)
    movie_render.md             Movie Render Queue, image sequences (on-demand)
    niagara.md                  Niagara VFX, known limitations (on-demand)
    pcg.md                      Procedural Content Generation graphs (on-demand)
    physics-collision.md        Collision presets, physics properties (on-demand)
    post_process.md             Post-process volumes, color grading (on-demand)
    python-patterns.md          UE Python API, assets, Content Browser (on-demand)
    sequencer.md                LevelSequence API, tracks, playback (on-demand)
    skeletal.md                 Skeletal meshes, sockets, bone hierarchy (on-demand)
    source_control.md           Checkout, check-in, revert workflows (on-demand)
    splines.md                  Spline actors, points, mesh extrusion (on-demand)
    umg_widgets.md              Widget Blueprints, UMG components (on-demand)
  Source/BobBot/
    Public/
      BobBot.h                  Module interface
      BobBotChatController.h    Chat logic, message struct, delegates
      BobBotCommands.h          Editor commands and menus
      BobBotConfig.h            Persistent settings (auth, SDK, budget, bridge)
      BobBotConstants.h         Colors, paths, intervals, scripts
      BobBotLib.h               Blueprint API (19 methods)
      BobBotPythonBridge.h      Python IPC abstraction
      BobBotRuntimeStatus.h     Transient detection and bridge state
      BobBotStyle.h             Slate styling
      Widgets/
        SBobBotPanel.h          Tab orchestrator (Connect, Chat, Context, Info)
        SBobBotConnectTab.h     Connect tab (setup, auth, bridge, model, advanced)
        SBobBotChatTab.h        Chat tab
        SBobBotContextTab.h     Context tab (prompt, project, rules)
        SBobBotInfoTab.h        Info tab (tools, API, about)
    Private/
      (matching .cpp files)
  Scripts/
    bob_mcp_bridge.py           MCP bridge (stdio, fallback)
    bob_mcp_bridge_http.py      MCP bridge (HTTP, default, persistent on port 13580)
    tools/
      _common.py                Thread-safe shared helpers for all tool modules
      core.py                   execute_unreal_python, ping_unreal
      system.py                 get_bobbot_status diagnostic tool
      actors.py                 Actor inspection, spawn, delete, properties
      assets.py                 Asset search, info, create Blueprint/Material
      asset_ops.py              Rename, duplicate, delete, move, references
      materials.py              Material expression graph editing
      levels.py                 Level open, save, info
      level_streaming.py        Streaming sub-levels
      viewport.py               Screenshot capture, console commands, output log
      context.py                Project info, editor state
      editor_ops.py             Selection, undo/redo, focus, rename, folders
      tags_layers.py            Actor tags and editor layers
      components.py             Component add/remove/inspect/modify
      world.py                  World settings, gravity, game mode
      collision.py              Collision presets, enabled state, channels
      physics.py                Physics simulation, mass, damping
      lighting.py               Create lights, sky atmosphere setup
      camera.py                 Camera actors, viewport camera control
      texture_mesh.py           Mesh/texture inspection, import, parameters
      import_export.py          Asset import/export, FBX options
      post_process.py           Post-process volumes, color grading, stats
      splines.py                Spline actors, points, mesh along spline
      data_assets.py            DataTables create/add rows/inspect
      skeletal.py               Skeleton/mesh info, sockets, attach
      sequencer.py              LevelSequence create/inspect/play
      animation.py              AnimBP, Montage, BlendSpace, skeleton anims
      blueprint_advanced.py     Functions, events, components, interfaces
      input_system.py           Enhanced Input actions and mappings
      audio.py                  SoundCues, audio assets, actor audio
      landscape.py              Landscape info, materials, paint layers
      foliage.py                Foliage types, stats, mesh registration
      niagara.py                Niagara particle systems
      pcg.py                    PCG graph create/inspect/execute
      ai_tools.py               Behavior Trees, Blackboards, EQS
      umg_widgets.py            Widget Blueprints, hierarchy, components
      pie.py                    Play-In-Editor control and inspection
      source_control.py         Checkout, checkin, revert, status
      build.py                  Lighting builds, BP compilation, validation
      movie_render.py           Movie Render Queue jobs and rendering
      debug_profiling.py        Frame/memory/GPU stats, benchmarking
      _test_tools.py            Test harness (not auto-loaded)
  Content/Python/
    bob_chat.py                 Claude subprocess backend (fallback)
    bob_chat_sdk.py             Claude Agent SDK backend (default, persistent)
    bob_bridge_launcher.py      HTTP bridge lifecycle manager
    bob_mcp_server.py           TCP server inside UE
  Docs/
    NewTools.md                 New tools plan and status
    bobbot_audit.md             UX audit and sprint plan
    bobbot_code_audit.md        Code audit and bug tracker
    improvement_scope.md        Competitive analysis and roadmap

<ProjectRoot>/
  PROJECT.md                    User's project context (created via Context tab)
  BobBot/tools/                 User-created project-specific tools (auto-discovered)
```

26 C++ source files, 49 Python files, 159 MCP tools. Version 3.0.

Created by Siva Vadlamani, [altpsyche.dev](https://altpsyche.dev)
