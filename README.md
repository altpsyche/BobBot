# BobBot

BobBot is an editor plugin for Unreal Engine 5.4 that puts Claude inside the editor. You tell it what you want and it runs Python against the live session to make it happen. It connects to the running engine through the Model Context Protocol, so Claude can see your project and make changes directly.

It uses your Claude Code subscription for authentication. No API keys involved.

## How it works

When you send a message, the plugin spawns Claude Code CLI as a subprocess. Claude figures out what to do and calls tools over MCP to execute Python inside the editor. The Python runs on the game thread through a TCP server on localhost. Claude can call `execute_unreal_python` to run code with access to the full `unreal` module, or `ping_unreal` to check the connection.

The chat UI talks to `bob_chat.py`, which manages the Claude subprocess. Claude connects to `bob_mcp_bridge.py`, a FastMCP server that launches automatically. The bridge forwards tool calls over TCP to `bob_mcp_server.py`, which runs inside the editor's Python environment and executes code on the game thread.

## Getting started

You need three things installed before BobBot will work.

**Claude Code** is the CLI that powers the chat. Install it with `npm install -g @anthropic-ai/claude-code`, then run `claude login` to authenticate.

**uv** is a Python package runner that the MCP bridge uses to pull in its dependencies at runtime. Install it from [astral.sh](https://docs.astral.sh/uv/).

**PythonScriptPlugin** ships with UE 5.4 but is disabled by default. Enable it in Edit > Plugins.

Once those are in place, drop the `BobBot` folder into your project's `Plugins/` directory and restart the editor. Open Window > BobBot. The Connect tab will detect your setup and tell you when you're ready.

## The two tabs

The Connect tab is a setup screen. At the top it shows whether Claude Code is installed, whether you're authenticated, and whether everything is ready. Below that you pick a model: Sonnet for speed, Opus for quality, Haiku for cost. Session stats show your current model, accumulated cost, and message count. Server port, permissions, system prompt, CLAUDE.md editor, and multi-editor setup are tucked under a collapsible Advanced section.

The Chat tab is where you talk to Claude. Type a message and press Enter. While Claude is thinking you see an animated indicator. Responses render code blocks in monospace on a dark background. Each bot message shows its cost, duration, and turn count. There is a copy button on every message, a stop button to kill a long request, and the history persists across editor restarts. Slash commands give quick access to common actions: `/clear` resets the session, `/cost` shows spending, `/model` switches models, `/help` lists everything.

## Permission modes

BobBot has three modes for controlling what Claude can do.

In Allow Always mode, Claude runs tools without asking. You say "make a Fresnel material" and it does it.

In Ask Me mode, every tool call pauses and shows you the code Claude wants to run. You get Approve and Deny buttons inline in the chat. Nothing executes until you say so.

In Chat Only mode, Claude can answer questions but cannot touch the editor. Good for asking about your project without risking changes.

## Blueprint API

UE 5.4's Python API has gaps around Blueprint editing. BobBot fills them with `UBobBotLib`, a C++ function library exposed to Python. Claude uses these when it needs to manipulate Blueprints:

```python
import unreal

bp = unreal.load_asset("/Game/MyBlueprint")

unreal.BobBotLib.add_blueprint_variable(bp, "Health", "float")
unreal.BobBotLib.set_blueprint_variable_default(bp, "Health", "100.0")
unreal.BobBotLib.set_blueprint_variable_category(bp, "Health", "Stats")
unreal.BobBotLib.set_blueprint_variable_slider_range(bp, "Health", 0.0, 200.0)

unreal.BobBotLib.add_component_to_blueprint(bp, unreal.StaticMeshComponent, "MyMesh")

unreal.BobBotLib.add_function_call_node(bp, "SetActorLocation", unreal.Actor)
unreal.BobBotLib.add_set_mpc_scalar_node(bp, "/Game/MPC_Global", "Brightness")

unreal.BobBotLib.compile_blueprint(bp)
```

Supported variable types: float, double, int32, int64, bool, FString, FName, FText, byte, FVector, FRotator, FTransform, FLinearColor, FVector2D, and object references like UTexture2D* or UStaticMesh*.

## Adding tools

The MCP bridge auto-discovers tools from the `Scripts/tools/` directory. To add one, create a Python file with a `register` function:

```python
# Scripts/tools/my_tool.py

def register(mcp, send_fn):
    @mcp.tool()
    def my_custom_tool(param: str) -> str:
        """Description that Claude sees when deciding which tool to use."""
        result = send_fn({"type": "execute", "code": f"print('{param}')"})
        return result.get("output", result.get("error", "No output"))
```

The bridge picks it up on the next connection. No C++ changes needed.

## Configuration

Settings live in `Saved/Config/BobBot.ini` and are exposed through the Connect tab's Advanced section. The defaults work out of the box.

The TCP server runs on port 13579 at 127.0.0.1. It starts automatically when the editor launches and accepts up to 2 concurrent MCP connections with a rate limit of 30 messages per second. The chat subprocess times out after 300 seconds.

The `.mcp.json` file at your project root is auto-generated so Claude Code and other MCP-compatible editors can find the bridge script. You can set up Cursor, VS Code, and Windsurf from the Advanced section.

## Architecture

The plugin separates business logic from UI. `FBobBotChatController` owns all chat state: sending messages, polling for responses, slash command dispatch, approval flow, and history persistence. It has no Slate dependencies. Widgets subscribe to its multicast delegates (`OnMessageAdded`, `OnHistoryCleared`, `OnApprovalStateChanged`, `OnThinkingStateChanged`) to react to state changes.

`SBobBotConnectTab` and `SBobBotChatTab` are the two tab widgets. They receive a pointer to the controller and bind to its delegates. `SBobBotPanel` is a thin orchestrator that creates the controller, wires up both tabs, and forwards tick.

`FBobBotPythonBridge` centralizes all C++ to Python communication. Instead of each widget constructing Python strings and managing temp files, everything goes through `ExecPythonWithJsonResult` or `ExecPythonCommand`. The bridge handles the temp directory, file cleanup, and JSON parsing.

`BobBotConstants` collects every color, temp file name, poll interval, and Python script fragment in one header. `FBobBotConfig` handles persistent settings. `FBobBotRuntimeStatus` holds transient detection state. These are separate so config and runtime concerns don't mix.

`UBobBotLib` is the Blueprint manipulation API with 17 static methods covering variables, components, graph nodes, compilation, and CDO access.

On the Python side, `bob_chat.py` manages the Claude subprocess, `bob_mcp_server.py` runs the TCP server on the game thread, and `bob_mcp_bridge.py` is the MCP bridge. The tool auto-discovery system means new tools are just Python files in a directory.

## Extending the plugin

Common extensions each have one place to go. To add a slash command, add one line to the `SlashCommands` map in the `FBobBotChatController` constructor. To add an MCP tool, drop a Python file in `Scripts/tools/`. To add a config setting, add a field to `FBobBotConfig` with its Load/Save lines, then add a UI control in `SBobBotConnectTab`. To add a Connect tab section, add Slate blocks in `SBobBotConnectTab::Construct`. To add a message type, add an `ESender` value in `FBobBotChatMessage` and a rendering case in `SBobBotChatTab`.

## Security

The TCP server only binds to localhost. There is no network exposure. Authentication is handled by Claude Code's OAuth flow, so no API keys touch the project. Each MCP client gets an isolated Python namespace. Config values reach Python through OS environment variables read via ctypes, not interpolated into code strings. Rate limiting is configurable per client. The three permission modes give you control over what Claude is allowed to do, from fully autonomous to fully locked down.

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
      BobBotLib.h               Blueprint API
      BobBotPythonBridge.h      Python IPC abstraction
      BobBotRuntimeStatus.h     Detection state
      Widgets/
        SBobBotPanel.h          Tab orchestrator
        SBobBotConnectTab.h     Connect tab
        SBobBotChatTab.h        Chat tab
    Private/
      (matching .cpp files)
  Scripts/
    bob_mcp_bridge.py           MCP bridge
    tools/
      core.py                   execute_unreal_python, ping_unreal
  Content/Python/
    bob_chat.py                 Claude subprocess manager
    bob_mcp_server.py           TCP server inside UE
  Docs/
    bobbot_audit.md             UX audit and sprint plan
    bobbot_code_audit.md        Code audit and bug tracker
```

22 C++ source files, 5 Python files, about 4,700 lines total. Version 1.0 beta.

Created by Siva Vadlamani, [altpsyche.dev](https://altpsyche.dev)
