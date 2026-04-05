# BobBot v1.6 Roadmap

v1.5 shipped 200 tools, the full SDK integration, a redesigned UI, and comprehensive docs. v1.6 is about making it solid and making it smarter.

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

### 1.5 Fresh project test

Install BobBot on a blank UE 5.4 and 5.5 project. Run the FTUE wizard. Chat. Use tools. Verify nothing assumes the UGW project structure.

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

### 2.4 Screenshot after action

When BobBot spawns, moves, or deletes an actor, automatically capture a viewport screenshot and include it in the tool result. BobBot sees the image (via the SDK's vision capability) and can self-correct if something looks wrong.

Implementation: in the actor tools (spawn_actor, set_actor_property, delete_selected_actors), append a capture call after the action completes. Return the image path in the tool output. The image preview feature from v1.5 displays it inline.

This is the "live preview loop" from the original roadmap, now with the infrastructure to support it.

## Phase 3: Efficiency

### 3.1 Batch operations

New tools:
- `batch_set_actor_property(class_filter, property_name, value)` applies a property change to all matching actors
- `batch_rename_actors(pattern, replacement)` regex rename across all actors
- `batch_move_assets(source_path, dest_path)` move all assets in a folder

Right now "rename all temp actors" requires N tool calls. Batch tools do it in one.

### 3.2 Tool result caching

Some tools return data that doesn't change between calls (project info, engine version, plugin list). Cache these results for the duration of a session and return the cached version if the underlying data hasn't changed.

Implementation: add a simple dict cache in _common.py keyed by tool name + args. Clear on session reset. Add a `@cacheable` decorator for tools that opt in.

### 3.3 Cost tracking improvements

Show per-tool cost breakdown (which tools are expensive in terms of tokens). Some tools return verbose output that wastes context. Identify and trim the chattiest tools.

Add a `/cost breakdown` slash command that shows token usage by tool category.

## Phase 4: Quality of life

### 4.1 Conversation templates

Predefined prompts for common workflows:
- "Set up a first-person shooter level" (floor, walls, player start, weapon pickup, enemy spawn points)
- "Create a material library" (PBR base, glass, metal, emissive)
- "Build a health system" (Blueprint with health variable, damage event, death logic, HUD binding)

Show these as chips in the chat, more specific than the current quickstart suggestions.

### 4.2 Undo timeline

BobBot makes many changes across multiple tool calls. The current undo (rewind_to_message) reverts file changes via SDK checkpoints, but doesn't undo editor actions (spawned actors, modified properties).

Add a `/history` command that shows what BobBot changed in the current session (tool calls with their effects). Let users selectively revert specific changes.

### 4.3 Progress indicators for long operations

build_lighting, compile_blueprints, package_project, and build_navigation are long-running. Currently BobBot just says "started, check the Output Log." Instead, poll for completion and report progress in chat.

### 4.4 Multi-select awareness

When the user selects multiple actors and says "make these red", BobBot should apply the material to all selected actors, not just one. The get_selected_actors tool exists but most modification tools work on a single actor_label. Add multi-actor variants or teach the system prompt to loop.

## Phase 5: Platform and distribution

### 5.1 macOS testing

The platform abstraction (bob_platform.py) and the C++ guards are in place. Actually test on a Mac with UE 5.4+. Fix whatever breaks. Key risk areas: venv creation (Mac Python paths differ), pywin32 is Windows-only (should be skipped cleanly on Mac), bridge process management.

### 5.2 UE 5.5 / 5.6 compatibility

Test against newer engine versions as they release. The main risk is Python API changes (property names, function signatures) in the newer UE builds. The tool implementations that use try/except for version differences should handle this, but needs verification.

### 5.3 Fab marketplace listing

Submit to Fab with the MARKETPLACE.md content. Price at $19.99 for support tier, open source on GitHub. Monitor reviews and iterate.

### 5.4 Usage analytics (opt-in)

Anonymous, opt-in telemetry: which tools are used most, which fail most, average session length, most common first messages. This data drives v1.7 priorities.

## Priority summary

| Phase | When | What | Why |
|-------|------|------|-----|
| 1 | Before anything else | Test and fix v1.5 | Nothing else matters if the tools don't work |
| 2 | Core of v1.6 | Context and intelligence | Makes BobBot actually useful, not just capable |
| 3 | After phase 2 | Speed and cost | Users will hit friction on repetitive tasks |
| 4 | Polish | Quality of life | Keeps users coming back |
| 5 | Ongoing | Platform and distribution | Grows the user base |

## Definition of done for v1.6

- All 200 tools verified against live editor (5.4 and 5.5)
- Streaming text confirmed working end-to-end
- Image preview confirmed rendering
- Blueprint graph reading tools added and tested
- Project memory working across sessions
- Batch operations for common bulk tasks
- Tested on at least one fresh project (not UGW)
- Updated CHANGELOG with v1.6 entry
