# BobBot

An AI assistant that lives inside the Unreal Engine editor. Tell it what you want in plain English and it makes it happen. spawning actors, building materials, editing Blueprints, adjusting lighting, anything you'd normally do by hand.

## What can it do?

Just type what you need:

- "Set up a basic level with a floor, some lights, and a player start"
- "Create a glowing blue material and apply it to the cube"
- "Add a health variable to my character Blueprint"
- "What actors are in this level?"
- "Build the lighting and tell me if there are any errors"
- "Take a screenshot of the viewport"

<!-- AUTOGEN:TOOLS START -->

BobBot has 222 MCP tools across 52 categories: AI/Behavior, Actors, Animation, Asset Operations, Assets, Audio, Blueprint Advanced, Blueprint Graph, Build, Camera, Collision, Components, Console Variables, Content Browser, Context, Core, Data Tables, Debug/Profiling, Editor Operations, Enhanced Input, Foliage, Import/Export, LOD, Landscape, Level Streaming, Levels, Lighting, Material Instances, Materials, Meta, Movie Render, Navigation, Niagara/VFX, Notifications, PCG, PIE Runtime, Perf Audit, Physics, Post-Process, Project Settings, Sequencer, Skeletal, Source Control, Splines, System, Tags & Layers, Texture & Mesh, Trace, UMG/Widgets, View Modes, Viewport, World.

<!-- AUTOGEN:TOOLS END -->

## Install

1. Install Claude Code: `npm install -g @anthropic-ai/claude-code`, then `claude login`
2. Enable **PythonScriptPlugin** in Edit > Plugins (ships with UE 5.4+, disabled by default)
3. Drop the `BobBot` folder into your project's `Plugins/` directory
4. Restart the editor and open **Window > BobBot**

On first launch, BobBot automatically sets up a Python environment and installs dependencies. Takes about 30 seconds. After that, you're ready to chat.

## Authentication

BobBot supports two ways to authenticate:

- **Claude Code subscription** (default). uses your existing Claude login via OAuth. No API key needed.
- **API key**. bring your own key from Anthropic, Amazon Bedrock, or Google Vertex. Enter it in the Connect tab. Keys are stored securely in your OS keychain, not in config files.

## The interface

BobBot has five tabs:

**Chat** is where you talk to it. The toolbar at the bottom lets you switch permission modes, pick a model (Sonnet, Opus, Haiku), and adjust thinking/effort settings. The header shows cost and context usage. Tool calls show inline so you can see exactly what BobBot is doing. You can branch conversations, switch between past chats, and use slash commands: `/clear`, `/cost`, `/model`, `/effort`, `/thinking`, `/new`, `/chats`, `/rename`, `/tag`, `/test`, `/help`.

**Connect** is the settings screen. Authentication, bridge status, and an Advanced section with cost budgets, port configuration, troubleshooting tools (health check, log viewer, rebuild venv, factory reset), and multi-editor setup.

**Context** lets you edit BobBot's system prompt and project context. You can write a PROJECT.md describing your project's conventions, and BobBot will follow them.

**Welcome** appears once on first launch to run setup. You won't see it again after that.

**Info** is a reference card listing all tools and slash commands.

## Permission modes

You control how much freedom BobBot has. Switch modes any time from the dropdown in the chat toolbar.

**Auto**. BobBot runs tools without asking. Fastest workflow.

**Ask before edits**. BobBot can read freely but pauses for your approval before creating, modifying, or running code. You can auto-approve safe categories (read-only, viewport) while keeping approval required for everything else.

**Plan**. Read-only. BobBot suggests what to do but doesn't execute anything. Good for learning or reviewing what it would change.

## Use with other editors

BobBot's tools aren't limited to the built-in chat. Any AI editor with MCP support can connect to the same tool server. BobBot auto-generates a `.mcp.json` at your project root, so Claude Code picks it up immediately.

One-click setup is available in the Connect tab for Cursor, VS Code, and Windsurf. All editors share the same tools and run against the same live UE session.

## What it can work with

| Category | Examples |
|----------|----------|
| Actors & Levels | Spawn, move, delete, inspect actors. Open/save levels. Streaming sub-levels. |
| Assets | Search, create, rename, duplicate, move, delete. Full dependency tracking. |
| Materials | Create materials, add expression nodes, wire them to properties. |
| Blueprints | Create variables, functions, events. Add nodes, wire pins, compile. |
| Lighting | Create any light type, adjust properties, build lighting. |
| Animation | Create AnimBPs, Montages, BlendSpaces. Inspect skeletons. |
| Sequencer | Create sequences, add actors, set length, play/preview. |
| Physics & Collision | Enable physics, set mass/damping, configure collision presets. |
| VFX | Create Niagara systems, set parameters. |
| AI | Create Behavior Trees, Blackboards, EQS queries. |
| UI | Create Widget Blueprints, inspect hierarchy, add widget components. |
| Landscape & Foliage | Inspect landscape, set materials, manage foliage types. |
| Audio | Create SoundCues, assign audio to actors. |
| Build & Debug | Compile Blueprints, validate assets, run map check, frame/memory/GPU stats. |
| Source Control | Check status, checkout, checkin, revert. |
| Viewport | Capture screenshots, run console commands, read output log. |
| PIE | Start/stop Play-In-Editor, inspect game actors, run commands in-game. |

## Adding your own tools

Create a Python file in `<YourProject>/BobBot/tools/`:

```python
# BobBot/tools/my_tool.py

def register(mcp, send_fn):
    @mcp.tool()
    def my_custom_tool(param: str) -> str:
        """Description that BobBot sees when choosing tools."""
        result = send_fn({"type": "execute", "code": f"print('Hello from {param}')"})
        if result.get("success"):
            return result.get("output", "(no output)")
        return "Error: " + result.get("error", "unknown")
```

BobBot picks up new tools automatically on the next connection. Project tools live outside the plugin directory, so they survive plugin updates.

## Working with traces

BobBot can record and inspect Unreal Insights `.utrace` files. Six tools in the **Trace** category cover navigation, recording, and a header-only summary; deeper frame-stat analysis lands in v1.7-trace-2.

**Where traces live**

- Default: `<YourProject>/Saved/Traces/`. BobBot creates the folder on first use.
- Override: set the `BOB_TRACE_DIR` environment variable to any other directory (e.g. a mounted device-pull location).
- Drop `.utrace` files pulled from a device into this folder; BobBot scans it whenever you ask "list my traces" or "summarize my latest trace".

**Recording locally**

- `start_trace(channels="cpu,gpu,frame,bookmark")` runs `Trace.Enable` + `Trace.Start File` and writes a session file at `Saved/BobBot/.trace_session.json`. About 5 MB/min/channel — stop promptly.
- `stop_trace()` reads the session file for accurate wall-clock duration and cleans it up. Survives a bridge restart.
- Only one recording at a time; `start_trace` refuses if a session file already exists.

**PC vs device perf**

Traces recorded on your PC are not a substitute for device captures on a mobile target. BobBot prepends `"PC trace; not a substitute for device capture."` to every summary unless the trace's recording host differs from the current machine. Treat PC numbers as triangulation, not ground truth.

**Inspecting traces**

- `list_traces()` — newest first, with sizes and mtimes.
- `summarize_trace(path)` — file size, magic check, size-based duration estimate, Insights-binary presence note. Real frame-time histograms, p50/p95/p99, GPU pass breakdown require v1.7-trace-2.
- `open_trace_in_insights(path)` — launches `UnrealInsights` with the trace loaded. Hard-fails clean if the binary is missing; lists every path checked so you can drop one into `Saved/BobBot/.config.json::insights_binary` (see below).
- `delete_trace(paths=[...])` — list-only, requires approval, validates each path is under the trace dir before unlinking. A single approval covers a batch.

**Runtime config (`Saved/BobBot/.config.json`)**

Optional. Tool-side runtime settings, distinct from the chat-SDK config. Schema:

```jsonc
{
  "insights_binary": "C:/UGW/engine/Engine/Binaries/Win64/UnrealInsights.exe"
}
```

If absent, BobBot searches platform-standard engine paths in order: `UnrealInsights-Cmd` (preferred for trace-2's headless query path) then `UnrealInsights`.

**Thresholds (`Saved/BobBot/trace_thresholds.json`, agent-side)**

Optional knobs the agent applies when synthesizing action items in v1.7-trace-2. Trace-1 ships no loader. Schema sketch:

```jsonc
{
  "frame_budget_ms": 33.3,
  "hitch_threshold_ms": 33,
  "gpu_pass_warn_ms": 5,
  "cpu_scope_inclusive_pct_warn": 15,
  "actor_tick_p99_warn_ms": 2,
  "draw_call_warn": 2000
}
```

## Configuration

Everything is configurable from the Connect tab. you don't need to edit files. Defaults work out of the box.

If you want to tweak things manually, settings live in `Saved/Config/BobBot.ini`. Ports, rate limits, cost budgets, and auto-approve categories are all there.

## Security

- All network traffic stays on localhost. Nothing is exposed to the network.
- API keys are stored in your OS keychain, not in plaintext config files.
- Three permission modes let you control exactly what BobBot can do.
- Each connected editor gets an isolated session.

## Documentation

### Getting started
- [Quickstart](Docs/Quickstart.md): install, first launch, first conversation
- [Configuration](Docs/Configuration.md): permission modes, models, slash commands, auth, all settings

### Using BobBot
- [ToolReference](Docs/ToolReference.md): all MCP tools by category (auto-generated from the registry)
- [CustomTools](Docs/CustomTools.md): write your own tools, helper functions, discovery
- [Troubleshooting](Docs/Troubleshooting.md): common problems, health check, factory reset

### Developing BobBot
- [Architecture](Docs/Architecture.md): system overview, message flow, tool call flow, module structure
- [API](Docs/API.md): C++ classes, Python modules, config-to-Python flow, extending the plugin
- [BobBotLib](Docs/BobBotLib.md): Blueprint editing API (variables, nodes, wiring, compilation)


## Requirements

- Unreal Engine 5.4 or later
- Claude Code installed and authenticated
- Windows, macOS, or Linux (primary testing is on Windows)

BobBot auto-enables four engine-bundled plugins on first load: `PythonScriptPlugin` (Python runtime — required), `EditorScriptingUtilities` (editor APIs — required), `Niagara` (used by VFX inspection tools), and `EnhancedInput` (used by input-mapping tools). All four ship with the engine; no marketplace downloads needed.

---

Created by Siva Vadlamani. [altpsyche.dev](https://altpsyche.dev)
