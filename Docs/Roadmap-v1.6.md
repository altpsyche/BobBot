# BobBot v1.6 Roadmap

v1.5 shipped 200 tools, the full SDK integration, a redesigned UI, and comprehensive docs. v1.6 is about making it solid and making it smarter.

## Phase 0: v1.5.1 hotfix notes

Bugs found and fixed during v1.5 testing. These are the baseline for v1.6 work.

- **pywin32 `.pth` processing**: UE's embedded Python skips `site.py`, so packages relying on `.pth` files (pywin32, setuptools) didn't load. Replaced with a generic `process_pth_files()` in `bob_platform.py` that reads every `.pth` file, adds path entries to `sys.path`, runs `import` lines, and registers Windows DLL directories.
- **Permission mode switching never wired**: UI buttons set the env var but never told the running SDK client. Fixed by calling `bob_chat.set_permission_mode()` from each dropdown button.
- **`can_use_tool` returning `None`**: SDK requires `PermissionResultAllow` or `PermissionResultDeny`. The fallback to `None` caused MCP server failures. Fixed by handling all permission modes inside `_can_use_tool` itself.
- **`actor_exec` / `asset_exec` indentation bug**: The `"    " + line` indentation strategy split f-strings at escaped newlines, generating invalid Python. Fixed by removing indentation entirely — user code runs at top level after the lookup preamble.
- **MCP tool name prefix stripping**: `_classify_tool` was matching short names like `ping_unreal`, but the SDK passes full prefixed names like `mcp__bobbot-internal__ping_unreal`. Fixed by stripping the `mcp__server__` prefix.
- **`acceptEdits` mode auto-approving everything**: `acceptEdits` is meant for file edits but Claude Code treats MCP tool calls as edits too, bypassing the approval UI. Changed Ask mode to use `default` SDK mode instead, which routes all permission requests through our hooks.
- **`PermissionRequest` hook missing**: Removed when consolidating into `can_use_tool`, but the hook is needed for external MCP tool calls (the 200 tools). Added back, registered for all modes, with runtime mode check.
- **Internal MCP server pywintypes failure**: `_get_internal_mcp_server` failed because pywin32 DLLs weren't registered yet. Fixed by calling `bob_platform.process_pth_files()` before the import.

Patch version: v1.5.1.

## Phase 1: Harden v1.5

Everything here is testing and fixing what already exists. No new features until this is done.

### 1.1 Test new tools against live editor

41 tools were added in the last push (cvars, view modes, material instances, content browser, LOD, navmesh, project settings, notifications, plus extensions to sequencer, build, editor ops, viewport, materials). Each one needs to be called from a real BobBot chat session and verified.

Expected issues:
- Console command output parsing (cvars, view modes) may not work on all UE versions because log format varies
- MaterialInstanceConstantFactoryNew may need different factory setup depending on parent material type
- LOD SourceModels property access may differ between 5.4 and 5.5
- Sequencer keyframe APIs (add_transform_key) are the most likely to need rework
- NavMesh property names on RecastNavMesh may vary

Fix: test each tool category, fix what breaks, add fallback paths where APIs differ between versions.

### 1.2 Verify streaming text

The C++ side updates bot messages in-place via StreamingBotMessageIndex. The Python side sends partial AssistantMessage events via include_partial_messages=True. Need to verify the full pipeline works: partial text appears progressively in the chat, not all at once after completion.

If partial messages aren't arriving (the SDK may batch them), consider polling more frequently during streaming or switching to a delta-based approach.

### 1.3 Verify image preview

ExtractImagePath scans bot messages for image file paths. BuildInlineImageWidget creates FSlateDynamicImageBrush from the path. This may fail silently if the brush can't load the image format or if the file isn't flushed to disk yet when the widget tries to load it.

Test: ask BobBot to capture a viewport screenshot, verify the image shows inline in chat.

### 1.4 Test Blueprint wiring

ConnectPins matches nodes by title or internal name. In a real Blueprint with multiple nodes of the same type (e.g., two "Print String" nodes), the match is ambiguous. Test with real Blueprints and decide whether to add index-based disambiguation.

### 1.5 Verify model live switching

Model switch buttons call `bob_chat.set_model()` which calls `client.set_model()` on the live SDK client. Confirmed in code review but never tested mid-conversation. Test: start a chat with Sonnet, switch to Opus mid-conversation, verify the next response uses Opus (check the `/cost` slash command for model name).

### 1.6 Fix or document thinking/effort live switching

Thinking and effort buttons currently only set env vars. The SDK client reads these once at connection time. Two options:

- **Fix**: add `bob_chat.set_thinking_mode()` and `bob_chat.set_effort()` Python functions that update the live client (similar to `set_model`). The SDK exposes `client.set_model()` — check if there's an equivalent for thinking/effort.
- **Document**: if the SDK doesn't support live thinking/effort changes, update the dropdown UI to say "applies on next message" so users aren't confused.

Pick one. Don't ship the current ambiguous behavior.

### 1.7 Stress-test the approval flow

Verify Ask mode handles edge cases:
- Single external tool approval (already tested, works)
- Single internal tool approval (already tested, works)
- Multiple sequential approvals in one conversation (untested)
- Approving while another approval is pending (race condition risk)
- Switching mode (Ask → Auto) while a tool is pending approval
- Denying a tool, then asking again — does Claude retry or move on
- Approval timeout (120s) — verify the deny actually reaches Claude
- Closing the chat tab during a pending approval

### 1.8 MCP server permission gating

Currently the 200 external tools auto-approve everything in `bob_mcp_server.py` regardless of mode. The Ask mode approval works through `can_use_tool` callback when BobBot's chat is the client, but if a non-BobBot MCP client (Cursor, VS Code) connects to the same bridge, they bypass all permissions and can run any tool.

Either:
- Gate the MCP server itself to require approval from BobBot's UI for non-read-only tools
- Or document clearly that other editors connecting via `.mcp.json` operate in a separate trust context

### 1.9 Large tool output handling

Some tools can return huge output (`get_output_log(10000)`, `find_unused_assets("/Game")` on a big project). What happens when a tool returns 100KB of text?

- Does the chat UI lag?
- Does it eat conversation context?
- Does the JSON serialization in `_exec` time out?

Add output truncation in `_common.py._exec()`: if output exceeds N KB, truncate and append a `(... 87 more KB truncated, use get_output_log to see full)` marker.

### 1.10 Fresh project + cross-version test

Install BobBot on a blank UE 5.4, 5.5, and 5.6 project. Run the FTUE wizard. Chat. Use tools. Verify nothing assumes the UGW project structure or a specific UE version. This consolidates the old "fresh project test" with the cross-version compatibility tests.

## Phase 2: Smarter context

BobBot currently treats every conversation as a blank slate. It doesn't remember project conventions, can't read existing logic, and doesn't look at what it changed.

### 2.1 Project memory

The SDK has auto-memory (writes to ~/.claude/projects/{hash}/memory/) but BobBot doesn't actively use it. Add a system prompt instruction telling BobBot to remember project conventions when the user states them ("always use metric", "materials go in /Game/Materials", "use BP_ prefix for Blueprints").

Test: tell BobBot a convention in one session, start a new session, verify it follows the convention without being reminded.

### 2.2 Blueprint graph reading

New tools:
- `get_graph_nodes(blueprint_path, graph_name)` returns all nodes with their types, positions, pin names, and connections
- `get_node_details(blueprint_path, node_name)` returns a single node's full pin list with connected targets

This lets BobBot understand existing Blueprint logic before modifying it. Right now it can only append nodes blindly.

### 2.3 Material graph reading

New tool:
- `get_material_graph(material_path)` returns all expression nodes with types, positions, connections, and parameter values

Same idea as Blueprint reading but for materials. BobBot currently can add nodes and wire them to properties, but can't see what's already there.

### 2.4 Screenshot after action (vision-model only, opt-in)

When BobBot spawns, moves, or deletes an actor, optionally capture a viewport screenshot and include it in the tool result. BobBot sees the image and can self-correct if something looks wrong.

**Constraints:**
- Only enabled when current model has vision capability (Sonnet, Opus — not Haiku)
- Opt-in via config setting `bAutoCaptureAfterEdits` (default off — capturing per actor mod is expensive)
- Capture is throttled: at most one per 2 seconds, debounced across rapid sequential actor changes
- Output path included in the tool result; the v1.5 image preview feature displays it inline

This is the "live preview loop" from the original roadmap, scoped down to be practical.

## Phase 3: Efficiency

### 3.1 Batch operations (with dry-run safety)

New tools:
- `batch_set_actor_property(class_filter, property_name, value, dry_run=True)` applies to all matching actors
- `batch_rename_actors(pattern, replacement, dry_run=True)` regex rename across all actors
- `batch_move_assets(source_path, dest_path, dry_run=True)` move all assets in a folder

**Safety requirements:**
- `dry_run=True` is the default. Returns a list of what would change without changing anything.
- Tools must print "WOULD modify N items" in dry-run mode and "Modified N items" in execute mode.
- An undo checkpoint is created before any non-dry-run batch operation, so users can revert.
- Before execute mode runs, the tool prints the count and waits for the next message to confirm — the user can say "do it" or "cancel".

Right now "rename all temp actors" requires N tool calls. Batch tools do it in one — but with safety rails so a misunderstood prompt doesn't nuke a level.

### 3.2 Tool result caching

Some tools return data that doesn't change between calls (project info, engine version, plugin list). Cache these results for the duration of a session and return the cached version if the underlying data hasn't changed.

Implementation: add a simple dict cache in `_common.py` keyed by tool name + args. Clear on session reset. Add a `@cacheable` decorator for tools that opt in.

### 3.3 Cost tracking improvements

Show per-tool cost breakdown (which tools are expensive in terms of tokens). Some tools return verbose output that wastes context. Identify and trim the chattiest tools.

Add a `/cost breakdown` slash command that shows token usage by tool category.

### 3.4 Verbose logging toggle

Add a `bVerboseLogging` setting in the Connect tab (and a config field in BobBotConfig). When enabled, every tool call, permission decision, and chat event logs to the Output Log with detailed context. When disabled (default), only errors and warnings are logged.

Currently the codebase has scattered `unreal.log()` calls used for debugging — they always log regardless of mode. The toggle should gate all of them behind a single `_log_verbose()` helper in `_common.py` that checks the env var:

```python
def _log_verbose(msg):
    if os.environ.get("BOB_VERBOSE_LOG") == "1":
        unreal.log("[BobBot] " + msg)
```

The C++ side sets `BOB_VERBOSE_LOG` from the config field via `ApplyEnvironmentVars()`. Python tools and SDK callbacks use `_log_verbose()` instead of raw `unreal.log()`.

Settings tied to this:
- Connect tab > Advanced > "Verbose Logging" checkbox (off by default for clean output)
- Logs include: every tool call with args, every permission decision, every stream event, every approval flow step
- When off: only `unreal.log_warning()` and `unreal.log_error()` calls fire

Files to update in v1.6:
- `BobBotConfig.h` — add `bVerboseLogging = false`
- `BobBotConfig.cpp` — load/save + ApplyEnvironmentVars sets `BOB_VERBOSE_LOG`
- `bob_sdk_events.py` — replace direct log calls with `_log_verbose()`
- `bob_sdk_permissions.py` — same
- `bob_sdk_client.py` — same
- `_common.py` — add `_log_verbose()` helper
- `SBobBotConnectTab.cpp` — add the checkbox via `UI::ConfigCheckbox(&FBobBotConfig::bVerboseLogging, ...)`

## Phase 4: Quality of life

### 4.1 Conversation templates

Predefined prompts for common workflows:
- "Set up a first-person shooter level" (floor, walls, player start, weapon pickup, enemy spawn points)
- "Create a material library" (PBR base, glass, metal, emissive)
- "Build a health system" (Blueprint with health variable, damage event, death logic, HUD binding)

Show these as chips in the chat, more specific than the current quickstart suggestions.

### 4.2 Action history (full revert only)

BobBot makes many changes across multiple tool calls. The current undo (rewind_to_message) reverts file changes via SDK checkpoints, but doesn't undo editor actions (spawned actors, modified properties).

Add a `/history` command that shows what BobBot changed in the current session: list of tool calls with their effects. **Full revert only** — user can roll back the entire session to a saved checkpoint, but selective revert (cherry-picking specific changes to undo) is out of scope for v1.6 because UE's transaction system tracks changes linearly. Selective undo is a v1.7+ research item.

### 4.3 Progress indicators for long operations

build_lighting, compile_blueprints, package_project, and build_navigation are long-running. Currently BobBot just says "started, check the Output Log." Instead, poll for completion and report progress in chat.

### 4.4 Multi-select awareness

When the user selects multiple actors and says "make these red", BobBot should apply the material to all selected actors, not just one. The get_selected_actors tool exists but most modification tools work on a single actor_label. Add multi-actor variants or teach the system prompt to loop.

## Phase 5: Distribution

### 5.1 macOS testing

The platform abstraction (bob_platform.py) and the C++ guards are in place. Actually test on a Mac with UE 5.4+. Fix whatever breaks. Key risk areas: venv creation (Mac Python paths differ), pywin32 is Windows-only (should be skipped cleanly on Mac), bridge process management.

### 5.2 Fab marketplace listing

Submit to Fab with the MARKETPLACE.md content. Price at $19.99 for support tier, open source on GitHub. Monitor reviews and iterate.

## Priority summary

| Phase | When | What | Why |
|-------|------|------|-----|
| 0 | Already done | v1.5.1 hotfixes | Baseline for v1.6 work |
| 1 | Before anything else | Test and fix v1.5 | Nothing else matters if the tools don't work |
| 2 | Core of v1.6 | Context and intelligence | Makes BobBot actually useful, not just capable |
| 3 | After phase 2 | Speed and cost | Users will hit friction on repetitive tasks |
| 4 | Polish | Quality of life | Keeps users coming back |
| 5 | Ongoing | Distribution | Grows the user base |

## Definition of done for v1.6

- All 200 tools verified against live editor (5.4, 5.5, 5.6)
- Streaming text confirmed working end-to-end
- Image preview confirmed rendering
- Approval flow stress-tested in Ask mode
- Model and permission mode live switching verified
- Thinking/effort live switching either fixed or UI-clarified
- MCP server permission gating decided and documented
- Large tool output handling (truncation) in place
- Blueprint graph reading tools added and tested
- Project memory working across sessions
- Batch operations with dry-run safety
- Verbose logging toggle wired
- Tested on at least one fresh project (not UGW)
- Tested on macOS
- Updated CHANGELOG with v1.6 entry
