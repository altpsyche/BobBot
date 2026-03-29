# BobBot Improvement Scope

## Where BobBot sits today

BobBot is an in-editor MCP chat plugin for UE 5.4+ that lets you talk to Claude and have it run Python against the live editor session. It uses OAuth (no API key), has multi-chat with persistent history, a streaming event system, session isolation from VS Code, inline markdown rendering, collapsible tool call display, and a Connect tab with setup checklist, model selector, system prompt editor, CLAUDE.md editor, and multi-editor MCP config.

The codebase was recently decomposed from a monolithic panel into focused components (controller, bridge, tabs, constants, runtime status). Adding new features has a predictable pattern.

## The competition

### UnrealClaude (free, UE 5.7 only)
https://github.com/Natfii/UnrealClaude

Chat panel docked in the editor with live streaming responses, grouped tool call display, and code block rendering. 20+ MCP tools across actors, levels, blueprints, animation blueprints, materials, enhanced input, assets, and characters. Dynamic UE 5.7 documentation context system that Claude can query on demand via `unreal_get_ue_context`. Automatic project context gathering (source modules, plugins, settings, recent assets). Session persistence across editor restarts.

Key advantage over BobBot: purpose-built tools mean Claude doesn't have to write boilerplate Python for common operations. The documentation context system means Claude knows the right API to call instead of hallucinating.

### CLAUDIUS ($60, UE 5.4-5.7)
https://claudiuscode.com/

Not a chat tool. An automation framework with 130+ JSON commands across 19 categories: Level (28), Editor (23), Console (16), Sequencer (13), AI/Behavior Trees (13), Asset (12), Viewport (12), Blueprint (8), Animation (8), Build (8), plus Skills, PCG, Recording, Landscape, Niagara, Physics, Audio, Source Control, UI. HTTP API at 10-50ms latency. Ships a CLAUDE.md that documents all commands so Claude Code learns them automatically.

Key advantage: breadth. It can control sequencer, AI blackboards, Niagara, landscape, and PIE runtime. Generic Python exec can't do most of these without extensive boilerplate.

### Cursor
https://cursor.com/features

Inline diffs with accept/reject per hunk. Checkpoint/rewind to any previous state. Agent mode with autonomous tool use and self-healing. Composer for instant multi-file refactors. @-mentions for files with line ranges. Real-time context awareness of open files and recent changes. Review changes button at end of conversation showing all diffs.

### Claude Code VS Code Extension
https://code.claude.com/docs/en/vs-code

Native graphical chat panel in VS Code. Checkpoints that track file edits with rewind/fork. @-mention files with line ranges from selection. Inline diffs in dedicated sidebar. MCP server management via /mcp. Conversation history with resume. Auto-accept mode for edits.

### Windsurf Cascade
https://windsurf.com/cascade

Tracks all user actions (edits, commands, clipboard, terminal) to infer intent. Reads entire codebase for context. Coordinated multi-file edits from single instruction. Autonomous memory generation across conversations. Diff toolbar to accept/reject changes. Context window usage meter. @-mention previous conversations for cross-session context.

### GitHub Copilot
https://docs.github.com/en/copilot/get-started/features

Next edit suggestions predicting location and content of next edit. Agent mode with autonomous code search, error recognition, and self-healing. Inline terminal command suggestions. Expandable context showing which searches were done. Snapshot-based undo for chat interactions.

## What BobBot has that others don't

- OAuth auth with no API key required
- Multi-editor MCP config generation (Claude Code, Cursor, VS Code, Windsurf) from one UI
- Ask Me permission mode with inline approve/deny in chat
- UBobBotLib filling UE 5.4 Python API gaps (Blueprint variables, components, graph nodes, CDO)
- Multi-chat with persistent history, switcher dropdown, auto-titles
- In-editor system prompt and CLAUDE.md editors with live sync
- UE 5.4+ support (most competitors require 5.7)
- Session isolation from VS Code via --strict-mcp-config

## What's missing

### Tier 1: Would make Claude dramatically more useful

**More MCP tools.** BobBot has 2 tools (execute_unreal_python, ping_unreal). UnrealClaude has 20+. CLAUDIUS has 130+. The generic Python exec tool works but Claude has to guess the UE Python API, often hallucinating function names. Purpose-built tools would be more reliable and faster because Claude doesn't need to write boilerplate.

Minimum viable set (10-15 tools):
- `get_selected_actors` — what the user is looking at right now
- `spawn_actor` — by class path, at location/rotation
- `delete_actors` — by label or selection
- `get_actor_properties` — inspect any actor's details
- `set_actor_property` — modify by property name
- `open_level` — by asset path
- `get_level_actors` — list all actors in current level with types
- `create_material` — from template, at content path
- `create_blueprint` — by parent class, at content path
- `search_assets` — by name pattern and type
- `get_asset_references` — what references/is referenced by an asset
- `run_console_command` — execute a UE console command
- `capture_viewport` — screenshot of the viewport (see below)
- `get_project_info` — plugins, modules, target platform, engine version

Each of these is a thin wrapper over existing UE Python API calls. The value is that Claude doesn't have to know or remember the API.

**Project context awareness.** Claude currently knows nothing about the project unless the user tells it or it runs Python to discover things. UnrealClaude auto-gathers source modules, plugins, settings, and recent assets. Windsurf tracks which files you view and edit.

BobBot should automatically include in the system prompt:
- Project name and engine version
- List of enabled plugins
- List of source modules
- Current level name and actor count
- Recently modified assets (last 10-20)
- Selected actors in the viewport

This context should update when the user sends a message, not just on startup. It could be gathered by a Python function called before each chat message.

**Viewport screenshot capture.** CLAUDIUS and UnrealClaude can capture what the viewport looks like. This is critical for visual tasks. Claude can't help with "make this look better" if it can't see anything.

Implementation: a `capture_viewport` MCP tool that takes a screenshot via `unreal.AutomationLibrary.take_high_res_screenshot` or `FScreenshotRequest`, saves to a temp file, and returns the path. The Claude CLI can read images when given file paths.

**Pre-built CLAUDE.md with UE API documentation.** CLAUDIUS ships a CLAUDE.md documenting all 130+ commands. BobBot should ship a CLAUDE.md (or append to the system prompt) that documents:
- All UBobBotLib methods with examples
- Common UE Python API patterns (spawning actors, creating assets, modifying materials, blueprint editing)
- Project-specific conventions (content paths, naming)

This would dramatically reduce hallucination. Claude would stop inventing function names because the real ones are in context.

### Tier 2: Would improve the editing workflow

**Checkpoints and undo.** When Claude modifies files or creates assets, there's no way to roll back. The gold standard (Cursor, Claude Code VS Code) has automatic checkpoints before each change with rewind/fork.

For BobBot, a pragmatic version:
- Before each `execute_unreal_python` call that modifies state, run `unreal.EditorTransaction` to create an undo checkpoint
- Show a "Revert" button next to completed tool calls in the chat
- Clicking Revert calls `unreal.EditorLevelLibrary.editor_undo()` N times
- This doesn't cover file writes but covers most editor operations

**File diff preview.** When Claude writes or modifies files through Python, show the changes as a diff in the chat. This requires intercepting file writes and comparing before/after, which is complex. A simpler version: after a tool call, show which files were modified and offer to open them in the editor.

**@-mentions for assets and files.** Let users type `@/Game/Materials/M_Base` or `@BP_PlayerCharacter` in the chat input and have BobBot inject the asset's properties or file contents into the message context. Requires autocomplete which is non-trivial in Slate.

A simpler version: a `/context` slash command that takes a path and reads the asset or file.

### Tier 3: Quality of life

**Output log integration.** Show the last N output log lines in the chat when a tool call fails. Currently errors go to the output log and the user has to switch panels to see them. Capture `unreal.log` output from within the tool execution namespace and return it with the result.

**Cost budget.** Add a `MaxBudgetUSD` config field. When `TotalSessionCost` exceeds the budget, show a warning and optionally block further messages. The Claude CLI supports `--max-budget-usd` already; just pass it through.

**Conversation search.** With multi-chat accumulating many conversations, searching across them would be useful. A `/search <query>` slash command that greps through all chat history JSON files.

**Asset drag-and-drop.** Let users drag an asset from the Content Browser into the BobBot chat input. Slate supports drag-and-drop. The dropped asset path would be inserted as text into the message.

**Keyboard shortcuts.** Ctrl+N for new chat, Ctrl+L for clear, Ctrl+K to focus the input. Register editor commands with keybindings.

**Export conversation.** Export a chat as markdown or plain text for documentation or sharing. A `/export` slash command.

### Tier 4: Longer term

**Blueprint visual diff.** When Claude modifies a Blueprint, show a before/after visual comparison. This is hard because Blueprint diffs aren't text-based. UE has some internal Blueprint diff tools that could potentially be invoked.

**Voice input.** UE has audio capture APIs. A push-to-talk button that transcribes speech and sends it as a chat message. Useful for hands-on-keyboard-and-mouse workflows where typing in the chat panel interrupts flow.

**Multi-agent support.** The Claude Agent SDK supports subagents. BobBot could spawn specialized agents for different tasks (one for Blueprint editing, one for material creation, one for level design) and route based on the user's request. This is only practical with the Agent SDK which requires API keys, so it's blocked by the OAuth constraint.

**PIE runtime integration.** CLAUDIUS can control actors during Play-In-Editor. BobBot could expose tools that work during PIE (query game state, modify AI blackboards, teleport the player). This requires MCP server awareness of whether PIE is active.

## Implementation priorities

If I were picking what to build next, in order:

1. **More MCP tools** (Tier 1). Highest leverage. 10-15 tools covering actors, levels, materials, assets, and console commands. Each is a ~20-40 line Python function in `Scripts/tools/`. No C++ changes. Could be done in a day.

2. **Project context in system prompt** (Tier 1). Second highest leverage. A Python function that gathers project info and injects it into the system prompt before each message. Moderate effort, mostly Python.

3. **Pre-built CLAUDE.md** (Tier 1). Write a comprehensive CLAUDE.md documenting UBobBotLib and common UE Python patterns. Ship it with the plugin. Zero code changes, pure documentation, but dramatically reduces hallucination.

4. **Viewport screenshot** (Tier 1). One new MCP tool. Moderate effort (need to handle the async screenshot capture in UE).

5. **Cost budget** (Tier 3). Small effort. Add config field, pass `--max-budget-usd` to CLI, show warning in chat.

6. **Undo checkpoints** (Tier 2). Medium effort. Wrap tool execution in UE transactions. Add Revert button to tool call widgets.

7. **Output log capture** (Tier 3). Small effort. Redirect unreal.log during tool execution and include in result.

8. Everything else follows based on user feedback.
