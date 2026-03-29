# BobBot

BobBot is an editor plugin for Unreal Engine 5.4 that puts Claude inside the editor. You tell it what you want and it runs Python against the live session to make it happen. It connects to the running engine through the Model Context Protocol, so Claude can see your project and make changes directly.

It uses your Claude Code subscription for authentication. No API keys involved.

## How it works

When you send a message, the plugin spawns Claude Code CLI as a subprocess with streaming output. Claude figures out what to do and calls tools over MCP to execute Python inside the editor. The Python runs on the game thread through a TCP server on localhost.

Claude has 26 built-in tools covering actors, assets, materials, levels, viewport capture, console commands, and project context. It also has `execute_unreal_python` for running arbitrary code when the built-in tools don't cover a case.

The chat UI talks to `bob_chat.py`, which manages the Claude subprocess and streams events back as they happen. Claude connects to `bob_mcp_bridge.py`, a FastMCP server that launches automatically. The bridge forwards tool calls over TCP to `bob_mcp_server.py`, which runs inside the editor's Python environment and executes code on the game thread.

## Getting started

You need three things installed before BobBot will work.

**Claude Code** is the CLI that powers the chat. Install it with `npm install -g @anthropic-ai/claude-code`, then run `claude login` to authenticate.

**uv** is a Python package runner that the MCP bridge uses to pull in its dependencies at runtime. Install it from [astral.sh](https://docs.astral.sh/uv/).

**PythonScriptPlugin** ships with UE 5.4 but is disabled by default. Enable it in Edit > Plugins.

Once those are in place, drop the `BobBot` folder into your project's `Plugins/` directory and restart the editor. Open Window > BobBot. The Connect tab will detect your setup and tell you when you're ready.

## Built-in tools

BobBot ships with 26 MCP tools organized by category. Claude picks the right tool based on what you ask.

**Actors** (6 tools): get selected actors, list all actors in the level with optional class filter, spawn an actor by class path at a location, delete selected actors, inspect actor properties and components, set actor properties by name.

**Assets** (4 tools): search assets by path and type, get detailed asset info, create Blueprints with a parent class, create Materials.

**Materials** (3 tools): add expression nodes to a material graph, connect expressions to material properties (BaseColor, Metallic, Roughness, etc.), list all expressions in a material.

**Levels** (3 tools): get current level info with actor counts by type, open a level by path, save the current level.

**Viewport** (3 tools): capture a viewport screenshot, run UE console commands, read the output log.

**Project context** (2 tools): get project info (name, engine version, plugins, source modules, content folders), get current editor state (selected actors, selected assets, current level).

**Core** (2 tools): execute arbitrary Python code with access to the full `unreal` module, ping the editor connection.

On top of these, Claude also has access to `UBobBotLib`, a C++ library exposed to Python that fills gaps in UE 5.4's Blueprint editing API (variables, components, graph nodes, compilation, CDO access).

## The two tabs

The Connect tab is a setup screen. At the top it shows whether Claude Code is installed, whether you're authenticated, and whether everything is ready. Below that you pick a model: Sonnet for speed, Opus for quality, Haiku for cost. Session stats show your current model, accumulated cost, and message count. Server port, permissions, system prompt, CLAUDE.md editor, and multi-editor setup are tucked under a collapsible Advanced section.

The Chat tab is where you talk to Claude. Type a message and press Enter. Tool calls render inline with collapsible code blocks and Running/Complete status. Bot responses render inline markdown (bold, italic, code) and fenced code blocks in monospace. There is a copy button on every message, a stop button to kill a long request, and the history persists across editor restarts.

A dropdown in the chat header lets you switch between conversations. Click + to start a new chat. Each conversation saves independently with its own session, so multi-turn context is preserved when you switch back.

Slash commands: `/clear` resets the session, `/cost` shows spending, `/model` switches models, `/new` starts a new conversation, `/chats` lists all conversations, `/help` lists everything.

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

The TCP server runs on port 13579 at 127.0.0.1. It starts automatically when the editor launches and accepts up to 2 concurrent MCP connections with a rate limit of 30 messages per second. The chat subprocess times out after 300 seconds.

The `.mcp.json` file at your project root is auto-generated so Claude Code and other MCP-compatible editors can find the bridge script. BobBot also generates an isolated copy at `Saved/BobBot/_bobbot_mcp.json` with `--strict-mcp-config` to prevent session conflicts with VS Code or other editors using Claude Code in the same project. You can set up Cursor, VS Code, and Windsurf from the Advanced section.

## Architecture

The plugin separates business logic from UI. `FBobBotChatController` owns all chat state: sending messages, polling for responses, slash command dispatch, multi-chat management, approval flow, and history persistence. It has no Slate dependencies. Widgets subscribe to its multicast delegates to react to state changes.

`SBobBotConnectTab` and `SBobBotChatTab` are the two tab widgets. They receive a pointer to the controller and bind to its delegates. `SBobBotPanel` is a thin orchestrator that creates the controller, wires up both tabs, and forwards tick.

`FBobBotPythonBridge` centralizes all C++ to Python communication. The bridge handles the temp directory, file cleanup, and JSON parsing.

`BobBotConstants` collects every color, temp file name, poll interval, and Python script fragment in one header. `FBobBotConfig` handles persistent settings. `FBobBotRuntimeStatus` holds transient detection state.

`UBobBotLib` is the Blueprint manipulation API with 17 static methods covering variables, components, graph nodes, compilation, and CDO access.

On the Python side, `bob_chat.py` manages the Claude subprocess with streaming event parsing, `bob_mcp_server.py` runs the TCP server on the game thread, and `bob_mcp_bridge.py` is the MCP bridge with auto-discovery from both built-in and project tool directories.

## Extending the plugin

Common extensions each have one place to go. To add an MCP tool, drop a Python file in `BobBot/tools/` at your project root. To add a slash command, add one line to the `SlashCommands` map in the `FBobBotChatController` constructor. To add a config setting, add a field to `FBobBotConfig` with its Load/Save lines, then add a UI control in `SBobBotConnectTab`. To add a Connect tab section, add Slate blocks in `SBobBotConnectTab::Construct`. To add a message type, add an `ESender` value in `FBobBotChatMessage` and a rendering case in `SBobBotChatTab`.

## Security

The TCP server only binds to localhost. There is no network exposure. Authentication is handled by Claude Code's OAuth flow, so no API keys touch the project. Each MCP client gets an isolated Python namespace. Config values reach Python through OS environment variables, not interpolated into code strings. Rate limiting is configurable per client. The three permission modes give you control over what Claude is allowed to do, from fully autonomous to fully locked down.

## Project layout

```
BobBot/
  BobBot.uplugin
  Source/BobBot/
    Public/
      BobBot.h                  Module interface
      BobBotChatController.h    Chat logic, message struct, delegates
      BobBotConfig.h            Persistent settings
      BobBotConstants.h         Colors, paths, intervals, scripts
      BobBotLib.h               Blueprint API (17 methods)
      BobBotPythonBridge.h      Python IPC abstraction
      BobBotRuntimeStatus.h     Detection state
      Widgets/
        SBobBotPanel.h          Tab orchestrator
        SBobBotConnectTab.h     Connect tab
        SBobBotChatTab.h        Chat tab
    Private/
      (matching .cpp files)
  Scripts/
    bob_mcp_bridge.py           MCP bridge with multi-directory tool discovery
    tools/
      core.py                   execute_unreal_python, ping_unreal
      actors.py                 Actor inspection, spawn, delete, properties
      assets.py                 Asset search, info, create Blueprint/Material
      materials.py              Material expression graph editing
      levels.py                 Level open, save, info
      viewport.py               Screenshot capture, console commands, output log
      context.py                Project info, editor state
  Content/Python/
    bob_chat.py                 Claude subprocess manager with streaming
    bob_mcp_server.py           TCP server inside UE
  Docs/
    bobbot_audit.md             UX audit and sprint plan
    bobbot_code_audit.md        Code audit and bug tracker
    improvement_scope.md        Competitive analysis and roadmap

<ProjectRoot>/BobBot/tools/     User-created project-specific tools (auto-discovered)
```

22 C++ source files, 11 Python files, 26 MCP tools. Version 1.0 beta.

Created by Siva Vadlamani, [altpsyche.dev](https://altpsyche.dev)
