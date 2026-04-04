# Configuration

All settings are accessible from the Connect tab. You don't need to edit files. If you prefer, the config file is at `Saved/Config/BobBot.ini`.

## Permission modes

Set from the Chat tab toolbar (bottom-left dropdown).

| Mode | Behavior | Best for |
|------|----------|----------|
| **Plan** | Read-only. BobBot suggests but doesn't execute. | Exploring what BobBot can do. |
| **Ask before edits** | Reads freely. Pauses before writes/creates with approve/deny buttons. | Daily use. Default. |
| **Auto** | Everything runs without asking. | Power users who trust the AI. |

### Auto-approve categories

When in **Ask before edits** mode, you can whitelist categories so they don't require approval. Set these in Connect > Advanced.

| Category | Tools covered | Default |
|----------|--------------|---------|
| Read-only | `get_*`, `search_*`, `is_*`, `list_*` | On |
| Viewport | `capture_*`, viewport camera tools | On |
| Create | `spawn_*`, `create_*`, `add_*` | Off |
| Modify | `set_*`, `delete_*`, `remove_*` | Off |
| Code execution | `execute_unreal_python`, `run_console_command` | Off |

### How approval works

When BobBot calls a tool that requires approval, the chat shows an inline panel with:
- The tool name and category
- The Python code it wants to run
- **Approve** and **Deny** buttons
- An "Always allow [Category]" button to auto-approve that category going forward

If you approve, the tool runs and BobBot continues. If you deny, BobBot is told the action was denied and will try a different approach or ask you how to proceed.

Tools that are auto-approved (based on the category settings above) run silently and show as a collapsed summary in the chat.

## Models

Switch from the Chat tab toolbar or with `/model`.

| Model | Strengths |
|-------|-----------|
| **Sonnet** | Fast, balanced. Good default. |
| **Opus** | Most capable. Best for complex tasks (full level setup, Blueprint systems). |
| **Haiku** | Fastest and cheapest. Simple tasks only. |

## Thinking and effort

Accessible from the gear icon in the Chat tab toolbar, or via slash commands.

**Extended thinking** gives the AI time to reason before responding:
- **Off**: default, fastest responses
- **On**: enables a thinking budget (default 10,000 tokens)
- **Adaptive**: the AI decides when thinking is useful

**Effort level** controls how thorough the AI is:
- **Low**: quick, surface-level answers
- **Medium**: balanced
- **High**: detailed, considers edge cases (default)
- **Max**: exhaustive, uses full context

## Cost budget

Default: **$5.00 USD** per message. Set to 0 for unlimited. Configurable in Connect > Advanced.

The chat header shows running cost with color thresholds (green < yellow < red as you approach the budget).

## Slash commands

Type these in the chat input.

| Command | What it does |
|---------|-------------|
| `/clear` | Clear current conversation |
| `/cost` | Show session cost, message count, model |
| `/model sonnet\|opus\|haiku` | Switch model |
| `/effort low\|medium\|high\|max` | Set effort level |
| `/thinking on\|off\|adaptive` | Toggle extended thinking |
| `/new` | Start a new conversation |
| `/chats` | List all saved conversations |
| `/rename <title>` | Rename the current conversation |
| `/tag <tag>` | Tag the current conversation |
| `/test [category]` | Run smoke tests (default: all) |
| `/help` | List all commands |

Arguments are case-insensitive. If you pass an invalid argument (e.g., `/model invalid`), BobBot shows the current value and valid options. `/effort` and `/thinking` take effect on the next message, not the current one. `/clear`, `/cost`, and `/help` are instant.

## Authentication

**Subscription (default)**: uses your Claude Code login via OAuth. No API key needed.

**API key**: bring your own from Anthropic, Amazon Bedrock, or Google Vertex. Enter in Connect > Authentication. Keys are stored in your OS keychain (not in config files).

For Bedrock, you'll also need an AWS region. For Vertex, you'll need a GCP project ID and region.

## Server settings

Available in Connect > Advanced. Defaults work out of the box.

| Setting | Default | Notes |
|---------|---------|-------|
| TCP server port | 13579 | MCP tool server inside UE |
| HTTP bridge port | 13580 | Persistent bridge for Claude |
| Auto-start server | On | Starts when editor launches |
| Auto-start bridge | On | Starts when editor launches |
| Auto-generate .mcp.json | On | Writes config at project root |
| Max clients | 4 | Concurrent MCP connections |
| Rate limit | 30/sec | Per-client message rate |
| Chat timeout | 300s | Max time per AI response |

## Multi-editor support

BobBot generates a `.mcp.json` at your project root so other editors can connect to the same tool server. One-click setup is available in Connect > Advanced for:

- Claude Code (auto-generated)
- Cursor
- VS Code
- Windsurf

All editors share the same tools. Sessions are isolated.
