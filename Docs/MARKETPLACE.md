# BobBot Fab Marketplace Listing

Copy-paste reference for filling out the Fab listing form. Each section maps to a form field.

---

## Product Title

BobBot: AI Assistant for Unreal Engine

## Short Description (one line)

Talk to your editor in plain English. 200 MCP tools let AI create assets, edit Blueprints, adjust lighting, and build levels directly in UE 5.4+.

## Category

Tool and Plugin > Editor Tool

## Tags

AI, MCP, Claude, Editor Automation, Blueprint, Level Design, Python, Productivity, Assistant, Tool

## Price

$19.99 USD (one-time, includes support and updates)

## License Type

Per Seat

---

## Long Description

BobBot is an AI assistant that lives inside the Unreal Engine editor. Type what you want in plain English and it happens. Spawn actors, create materials, wire Blueprint nodes, set up lighting, run builds. No menus, no searching through panels. Just describe what you need.

**How it works**

BobBot connects to Claude (Anthropic's AI) through the Model Context Protocol. It has 200 built-in tools that cover nearly every editor operation. When you send a message, the AI picks the right tools and executes them against your live editor session. You see the changes immediately.

The plugin runs a persistent AI process with near-zero latency between messages. Tool calls show inline in the chat so you always know what's happening. You can interrupt, undo, branch conversations, and switch between past chats.

**What you can do with it**

- "Set up a basic level with a floor, some lights, and a player start"
- "Create a glowing blue material and apply it to the selected cube"
- "Add a health variable to my character Blueprint and wire it to the HUD"
- "Build the lighting and tell me if there are any errors"
- "Take a screenshot of the viewport"

**200 tools across 38 categories**

Actors, Assets, Materials, Material Instances, Blueprints, Lighting, Animation, Sequencer, Physics, Collision, VFX (Niagara), AI/Behavior Trees, UI Widgets, Landscape, Foliage, Audio, Enhanced Input, Data Tables, Splines, Skeletal Meshes, PCG, Source Control, Post-Processing, Camera, Import/Export, Level Streaming, Movie Render, Build/Validate/Package, Debug/Profiling, PIE Runtime, Console Variables, View Modes, Content Browser, LOD Management, Navigation Mesh, Project Settings, Notifications, and more.

When the built-in tools don't cover something, BobBot can run arbitrary Python against the live editor.

**Blueprint editing**

BobBot includes BobBotLib, a C++ library that fills the gaps in UE 5.4's Python API for Blueprint editing. Create variables, add function and event graphs, place nodes (Branch, Get/Set Variable, Cast, function calls), wire pins together, and compile. All from natural language.

**Three permission modes**

- Auto: everything runs without asking. Fastest workflow.
- Ask before edits: reads freely, pauses for your approval before creating or modifying anything. You choose which categories to auto-approve.
- Plan: read-only. The AI describes what it would do but doesn't touch the editor. Good for learning.

**Works with other editors too**

BobBot auto-generates a .mcp.json at your project root. Claude Code, Cursor, VS Code, and Windsurf can all connect to the same tool server. One-click setup for each editor in the Connect tab.

**First-time setup is automatic**

Drop the plugin in, open Window > BobBot. The Welcome tab creates a Python environment from UE's bundled Python, installs dependencies, starts the bridge, and verifies the SDK. About 30 seconds, fully automatic. Per-step retry buttons if anything fails.

**Open source**

The full source code is available on GitHub at github.com/altpsyche/bobbot under the MIT license. The Fab listing includes direct support and priority issue handling.

---

## Technical Details

**Supported Engine Versions:** 5.4, 5.5

**Supported Platforms:** Windows (primary), macOS, Linux (experimental)

**Module Type:** Editor only (not included in packaged builds)

**Network:** Localhost only. TCP server on port 13579, HTTP bridge on port 13580. Nothing exposed to the network.

**Dependencies:**
- PythonScriptPlugin (ships with UE 5.4+, must be enabled)
- Claude Code CLI (free install via npm) with Claude subscription or Anthropic API key
- Python packages installed automatically into isolated venv: mcp, claude-agent-sdk

**Plugin Size:** ~2 MB source, ~300 MB with venv after first-launch setup

**Generated Files:**
- Saved/Config/BobBot.ini (settings)
- Saved/BobBot/.venv/ (Python environment, created on first launch)
- Saved/BobBot/ (session data, bridge logs, temp files)
- .mcp.json at project root (MCP server config for external editors, can be disabled)

**Authentication:** Claude Code subscription via OAuth (default) or direct API key (Anthropic, Amazon Bedrock, Google Vertex). API keys stored in OS keychain, not in config files.

**Source Code:** Full C++ and Python source included.

---

## Important Note (for listing description)

BobBot requires a Claude Code subscription ($20/month from Anthropic) or an Anthropic API key to function. The plugin connects to Claude's AI service to process your messages. Without authentication, the chat will not work. The plugin itself is a one-time purchase with no additional fees from us.

---

## Screenshots (what to capture)

1. Chat tab with a conversation showing tool calls inline (the main selling point)
2. Permission mode dropdown showing Auto/Ask/Plan
3. A before/after of "set up a basic level" showing the empty viewport then the populated one
4. Blueprint editing conversation showing node creation
5. Connect tab showing multi-editor setup
6. Welcome tab mid-setup (shows the automatic FTUE)

---

## Support Description (for the $20 value prop)

Includes priority support via GitHub Issues, direct responses from the developer, and all future updates. The full source code is MIT licensed and available on GitHub for those who prefer to self-support.

---

## SEO Keywords (for Fab search)

AI assistant, MCP, Model Context Protocol, Claude, natural language, editor automation, Blueprint scripting, level design assistant, Python scripting, productivity tool, code generation, Anthropic, Claude Code
