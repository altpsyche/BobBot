# Quickstart

Get BobBot running in under two minutes.

## Prerequisites

1. **Claude Code** -- install with `npm install -g @anthropic-ai/claude-code`, then run `claude login` to sign in.
2. **PythonScriptPlugin** -- ships with UE 5.4+ but is off by default. Enable it in Edit > Plugins, search "Python", check the box, restart the editor.

## Install

Drop the `BobBot` folder into your project's `Plugins/` directory. Restart the editor. Open **Window > BobBot**.

## First launch

The **Welcome** tab runs automatically. It does 6 things:

1. Checks prerequisites (Claude Code installed, authenticated, Python plugin enabled)
2. Creates a Python virtual environment from UE's bundled Python
3. Installs the MCP bridge (`mcp[cli]`)
4. Installs the Agent SDK (`claude-agent-sdk`)
5. Starts the HTTP bridge on port 13580
6. Verifies the SDK is importable from UE Python

This takes about 30 seconds. Each step shows a green check on success or a red X with a **Retry** button and a hint if something fails. Once done, BobBot switches to the Chat tab automatically.

If you want to skip setup and configure things manually later, click **Skip Setup**.

## Your first conversation

The Chat tab shows suggestion chips at the bottom:

- "Set up a basic level"
- "Show me what you can do"
- "Create a material"
- "Describe my project"

Click one or type your own message. BobBot will pick the right tools and make changes directly in the editor.

## Permission modes

The dropdown in the bottom-left of the chat input area controls what BobBot can do:

| Mode | What it means |
|------|--------------|
| **Plan** | Read-only. BobBot describes what it would do but doesn't touch the editor. |
| **Ask before edits** | BobBot reads freely but pauses before creating, modifying, or running code. You approve each action inline in the chat. |
| **Auto** | BobBot does everything without asking. Fastest workflow. |

The default is **Ask before edits**. Switch to Auto once you trust it. See [Configuration.md](Configuration.md) for auto-approve categories and details.

## Next steps

- [Configuration.md](Configuration.md) -- all settings, slash commands, model selection
- [CustomTools.md](CustomTools.md) -- add your own tools
- [Architecture.md](Architecture.md) -- how the system works under the hood
- [Troubleshooting.md](Troubleshooting.md) -- if something isn't working
