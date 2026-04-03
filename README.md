# BobBot

An AI assistant that lives inside the Unreal Engine editor. Tell it what you want in plain English and it makes it happen — spawning actors, building materials, editing Blueprints, adjusting lighting, anything you'd normally do by hand.

## What can it do?

Just type what you need:

- "Set up a basic level with a floor, some lights, and a player start"
- "Create a glowing blue material and apply it to the cube"
- "Add a health variable to my character Blueprint"
- "What actors are in this level?"
- "Build the lighting and tell me if there are any errors"
- "Take a screenshot of the viewport"

BobBot has 159 tools covering actors, assets, materials, levels, lighting, physics, animation, sequencer, Blueprints, UI widgets, VFX, AI, landscape, audio, and more. When the built-in tools don't cover something, it can run arbitrary Python against the live editor.

## Install

1. Install Claude Code: `npm install -g @anthropic-ai/claude-code`, then `claude login`
2. Enable **PythonScriptPlugin** in Edit > Plugins (ships with UE 5.4+, disabled by default)
3. Drop the `BobBot` folder into your project's `Plugins/` directory
4. Restart the editor and open **Window > BobBot**

On first launch, BobBot automatically sets up a Python environment and installs dependencies. Takes about 30 seconds. After that, you're ready to chat.

## Authentication

BobBot supports two ways to authenticate:

- **Claude Code subscription** (default) — uses your existing Claude login via OAuth. No API key needed.
- **API key** — bring your own key from Anthropic, Amazon Bedrock, or Google Vertex. Enter it in the Connect tab. Keys are stored securely in your OS keychain, not in config files.

## The interface

BobBot has five tabs:

**Chat** is where you talk to it. The toolbar at the bottom lets you switch permission modes, pick a model (Sonnet, Opus, Haiku), and adjust thinking/effort settings. The header shows cost and context usage. Tool calls show inline so you can see exactly what BobBot is doing. You can branch conversations, switch between past chats, and use slash commands: `/clear`, `/cost`, `/model`, `/effort`, `/thinking`, `/new`, `/chats`, `/rename`, `/tag`, `/test`, `/help`.

**Connect** is the settings screen. Authentication, bridge status, and an Advanced section with cost budgets, port configuration, troubleshooting tools (health check, log viewer, rebuild venv, factory reset), and multi-editor setup.

**Context** lets you edit BobBot's system prompt and project context. You can write a PROJECT.md describing your project's conventions, and BobBot will follow them.

**Welcome** appears once on first launch to run setup. You won't see it again after that.

**Info** is a reference card listing all tools and slash commands.

## Permission modes

You control how much freedom BobBot has. Switch modes any time from the dropdown in the chat toolbar.

**Auto** — BobBot runs tools without asking. Fastest workflow.

**Ask before edits** — BobBot can read freely but pauses for your approval before creating, modifying, or running code. You can auto-approve safe categories (read-only, viewport) while keeping approval required for everything else.

**Plan** — Read-only. BobBot suggests what to do but doesn't execute anything. Good for learning or reviewing what it would change.

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

## Configuration

Everything is configurable from the Connect tab — you don't need to edit files. Defaults work out of the box.

If you want to tweak things manually, settings live in `Saved/Config/BobBot.ini`. Ports, rate limits, cost budgets, and auto-approve categories are all there.

## Security

- All network traffic stays on localhost. Nothing is exposed to the network.
- API keys are stored in your OS keychain, not in plaintext config files.
- Three permission modes let you control exactly what BobBot can do.
- Each connected editor gets an isolated session.

## Requirements

- Unreal Engine 5.4 or later
- PythonScriptPlugin enabled
- Claude Code installed and authenticated
- Windows, macOS, or Linux (primary testing is on Windows)

---

Created by Siva Vadlamani — [altpsyche.dev](https://altpsyche.dev)
