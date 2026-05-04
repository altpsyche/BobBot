# Changelog

All notable changes to BobBot are documented in this file.

## [1.6 — in progress]

Phase 1 hardening of v1.5 + Phase 2 smarter context. Phase 1 fixes regressions and locks down trust boundaries. Phase 2 adds three new graph-reading tools, makes BobBot remember conventions across sessions, and gives vision-capable models a viewport screenshot after they edit the level.

### v1.7 refactor: structured envelopes, registry-driven catalog, doc autogen

Single PR turning every tool into a predictable, traceable unit. Cutoff/silent-stop pain resolved as a side effect because every output now has a typed `summary` separate from its `data`, and oversized payloads always spill to disk via a uniform path the agent fetches with one tool.

- **Structured tool envelope.** Every tool result is `{ok, summary, data, spill_path, error, meta}`. `summary` is small, always present, human-readable. `data` is structured. `spill_path` is populated when the serialized envelope exceeds the inline budget for the tool's `output_kind`. The chat-UI/log/system prompt all read `summary`. Agents read `data`. Helpers in [Scripts/tools/_common.py](Scripts/tools/_common.py): `envelope()`, `serialize_envelope()`, `_spill_envelope()`. The old `_MAX_OUTPUT_BYTES = 32KB` magic threshold is gone.
- **Tool registry as data.** New `bob_tool(category=, output_kind=, default_timeout=, sensitive_args=)` decorator in [Scripts/tools/_registry.py](Scripts/tools/_registry.py). Records `_TOOL_REGISTRY`, applies `with_timeout` automatically, captures stdout into `summary` for legacy print-style tools, redacts sensitive args before activity log, normalizes any return into an envelope, then JSON-serializes for the wire. Every tool file (~50) migrated to `@bob_tool(...)` under its existing `@mcp.tool()`. Mechanical migration via `Scripts/build_docs.py` companion.
- **Single tool manifest kills 4-way doc drift.** Bridge writes `<ProjectRoot>/Saved/BobBot/.tool_manifest.json` from `_TOOL_REGISTRY` at startup. Three consumers all read this manifest: `list_tools` MCP tool, `SBobBotInfoTab.cpp` (replaced the 264-line hand-maintained `GToolCategories[]`), and the new `Scripts/build_docs.py` doc generator. Hand edits to tool lists in `CLAUDE.md` / `README.md` / `Docs/ToolReference.md` no longer needed — `<!-- AUTOGEN:TOOLS START/END -->` markers wrap the regenerated sections; `python Scripts/build_docs.py --check` is a CI-ready drift gate.
- **Output spill + `read_overflow`.** Oversized envelopes write full JSON to `Saved/BobBot/output_overflow/<ts>_<tool>.json` and return inline with `data=null`, `meta.truncated=true`, and `spill_path` populated. New `read_overflow(path, offset, max_bytes)` paginates through the spilled file. Single tool, single protocol — agents follow the spill_path until `data.eof=true`. Directory pruned at 200MB.
- **`hook_tool_result [:2000]` silent slice gone.** [Content/Python/bob_sdk_events.py](Content/Python/bob_sdk_events.py) now parses the envelope and surfaces `summary`. Legacy strings get an explicit truncation marker instead of a silent slice.
- **System prompt update.** [Content/Python/bob_sdk_config.py](Content/Python/bob_sdk_config.py) explains the envelope shape to the model so it knows to read `summary`, parse `data` when relevant, and follow `spill_path` via `read_overflow` when `meta.truncated=true`.
- **Per-tool timeout default.** Replaces the scattered manual `@tool_timeout(180)` from Sprint 2; each tool declares `default_timeout=...` once in `bob_tool(...)` and the decorator applies it on every invocation.
- **`audit_textures` MCP tool.** Walks materials referenced by in-use meshes, follows expression graph for texture nodes, rolls up dimensions + estimated mip-pyramid bytes + reference counts. Flags `OVERSIZED_DIM(WxH)` and `HEAVY_BYTES(N)`. Returns envelope with structured `data.textures` and `data.totals`.
- **Tool count: 216** across 51 categories (was 214 hand-counted, 215 once `read_overflow` registered, 216 with `audit_textures`).

Files touched: [Scripts/tools/_common.py](Scripts/tools/_common.py), new [Scripts/tools/_registry.py](Scripts/tools/_registry.py), all 53 [Scripts/tools/*.py](Scripts/tools/), [Scripts/bob_mcp_bridge.py](Scripts/bob_mcp_bridge.py), [Scripts/bob_mcp_bridge_http.py](Scripts/bob_mcp_bridge_http.py), [Scripts/build_docs.py](Scripts/build_docs.py) (new), [Content/Python/bob_sdk_events.py](Content/Python/bob_sdk_events.py), [Content/Python/bob_sdk_config.py](Content/Python/bob_sdk_config.py), [Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp](Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp), [CLAUDE.md](CLAUDE.md), [README.md](README.md), [Docs/ToolReference.md](Docs/ToolReference.md).

Skipped: `BobBotChatController.cpp` envelope rendering — investigation showed `hook_tool_result` events aren't displayed by the C++ chat panel; the chat UI shows model-generated text from streaming, which already renders correctly with the system prompt update.

Verification: 5/5 bridge tests green; 216/216 tools registered with `category` populated; envelope smoke test (small + huge) confirms inline ≤ budget and oversized payloads spill + reload cleanly; `Scripts/build_docs.py` round-trips clean against committed docs.

### Atomic perf signals to widen the audit search

Concern: a single `audit_map_perf` orchestrator narrows what BobBot looks at to its hardcoded checks. Mitigation = pair it with **5 atomic signal tools** the agent composes freely. Each is a single-call probe over already-resident editor data; no asset loads.

- `get_actor_perf_signal(actor_label)` — per-actor: total component count, tick group / interval, per-SMC mobility + cast-shadow + lightmap-override + material count + instance count, light components by mobility.
- `get_lightmap_density_summary` — map-wide lightmap pixel² roll-up split by mobility (Static / Stationary / Movable), plus top 20 contributors with override-flag marker. Reveals single high-budget overrides drowning the lightmap pool.
- `get_texture_pool_status` — read-only cvar dump (`r.Streaming.PoolSize`, `MaxTempMemoryAllowed`, `MipMapLODBias`, etc.) and triggers `Memreport -full` so live pool stats land in the output log; pair with `get_output_log`.
- `get_light_summary` — counts of LightComponents by class + mobility, with the movable shadow-caster set listed by intensity (mobile's most expensive case).
- `get_foliage_density_report` — InstancedFoliageActor + ISM/HISM instance counts grouped by mesh, top 30 by instance count. Distinguishes bloat (one mesh, 50k instances) from diversity (1k unique types).

Tool count: 214.

### Mobile-aware perf audit

- **`audit_map_perf` MCP tool** ([Scripts/tools/perf_audit.py](Scripts/tools/perf_audit.py)) — mobile-aware orchestrator. Walks the open level once and reports prioritized outliers per mesh, Niagara system, and material — without loading any new assets, so it does not trigger a shader compile storm. Splits StaticMeshComponent references into SMC / ISM / HISM so over-replicated plain SMC usage gets flagged for HISM conversion. Tunable thresholds: HEAVY_TRIS / HIGH_TRIS_NO_LOD / HIGH_LIGHTMAP / MANY_MATERIALS / SHOULD_INSTANCE.
- **Mobile-friendly `get_lod_summary`** retune — drops the Nanite-required-or-bad assumption that was wrong for mobile projects. New flags: HEAVY_TRIS (>20k LOD0 tris), HIGH_TRIS_NO_LOD (>5k tris with <2 runtime LODs), HIGH_LIGHTMAP (>256), MANY_MATERIALS (>4 slots).
- **New BobBotLib getters**: `GetStaticMeshRuntimeLODCount` (uses `UStaticMesh::GetNumLODs()` from RenderData — the chain actually used at runtime, not the editor-side source models), `GetStaticMeshNumTriangles(lod)`, `GetStaticMeshLightmapResolution`, `GetStaticMeshMaterialCount`. Tool count: 209.

### Sprint 2: Phase 3+4 typed getters, per-tool timeout, activity log, bridge tests

- **Phase 3+4 typed getters** for the remaining broken-introspection subsystems: Sequencer (`GetMovieSceneTrackCount/TrackClass/BindingCount/BindingName` — wraps `UMovieScene::GetTracks()/GetBindings()`), UMG (`GetWidgetBlueprintRoot` — walks `WidgetBlueprint::WidgetTree->RootWidget`), Blueprint internals (`GetBlueprintFunctionGraphsArr/UbergraphPagesArr/SCSNodes`), Blackboard (`GetBlackboardKeyCount/Name/Type` reading `UBlackboardData::Keys` direct), Input mapping (`GetInputContextMappingCount/Action/Key` via `UInputMappingContext::GetMappings()`), Landscape (`GetLandscapeEditorLayerCount/Name` reading `ALandscapeProxy::EditorLayerSettings`). Each routes through the public engine getter where one exists; per-field accessors avoid the `TArray<USTRUCT>` Python-binding risk flagged in the Sprint 1 plan.
- **Python tool rewrites** for `sequencer.py::get_sequence_info/get_sequence_tracks`, `umg_widgets.py::get_widget_tree`, `blueprint_advanced.py::get_blueprint_functions/get_blueprint_components`, `ai_tools.py::get_blackboard_keys`, `landscape.py::get_landscape_info/get_landscape_layers` — all now route through the typed getters instead of the silently-failing `get_editor_property`.
- **New `get_input_context_mappings` MCP tool** — first-class read for `UInputMappingContext::Mappings`, previously only writable via the broken `add_input_mapping` path. Total tool count: 208.
- **Per-tool timeout override** via `with_timeout(seconds)` context manager and `@tool_timeout(seconds)` decorator in `_common.py`. Bridge `_send_and_receive` reads optional `timeout` from the payload and applies to the socket; values are clamped to 1–300s. Solves the long-running `auto_generate_lods` false-alarm.
- **Activity log JSONL** at `<ProjectRoot>/Saved/BobBot/activity.jsonl`. Every `_exec` call appends a record `{ts, tool, ok, dur_s, code, out}` with both code and output truncated to 500 chars. Two-pattern regex redaction strips api/key/token/password assignments and ≥40-char hex blobs from the `code` field before write (Risk 2.4 mitigation — best-effort, not a security boundary). Rotates at 50MB. Disable via `BOB_ACTIVITY_LOG=0`.
- **Bridge test harness** at `Scripts/tests/test_bridge.py`. Five scenarios — connect / send-receive roundtrip / oversized-response / per-call timeout / disconnect-recover — running against a fake UE TCP server bound to an ephemeral port. All five pass; `python Scripts/tests/test_bridge.py` is the sprint gate.
- **Build.cs** picks up `MovieScene`, `MovieSceneTracks`, `UMG`, `UMGEditor`, `AIModule`, `EnhancedInput`, `Landscape` for the new typed getters.

Deliberately out of Sprint 2: `FScopedTransaction` per chat turn (Risk 2.1 manual QA matrix not yet run) and the History tab in the BobBot panel (depends on transactions). Activity log is the data layer for the eventual History tab — landing it now keeps the audit trail accruing.

### Map inspection unblocked + runtime tool discovery

- **Reflection helpers in BobBotLib** that bypass UE Python's `BlueprintReadable` gate on protected UPROPERTYs. `read_property_as_string(obj, name)` returns ExportText serialization of any UPROPERTY by name; `read_object_array_property(obj, name)` returns the contents of a `TArray<UObject*>`-typed UPROPERTY (covers `TArray<TObjectPtr<T>>` too). These work because `FProperty` reflection does not check access modifiers — only Python's binding does.
- **Phase 1 streaming-level fix.** `level_streaming.py::get_streaming_levels` and `remove_streaming_level` previously called `world.get_editor_property("StreamingLevels")` which silently fails because `UWorld::StreamingLevels` is a protected UPROPERTY. New typed helper `BobBotLib::GetWorldStreamingLevels(UWorld*)` wraps the public `UWorld::GetStreamingLevels()` getter; both Python tools now call it directly. Same pattern applies to the wider audit (~120 broken `get_editor_property` calls across 13 files).
- **Phase 2 perf-inspection blind spots.** Typed C++ getters added for the perf-critical subsystems: `GetStaticMeshLODCount/LODScreenSize/NaniteEnabled/NaniteFallbackPercent`, `GetMaterialExpressions` (routed through `ReadObjectArrayProperty` to sidestep UE 5.x API drift on `UMaterial::GetExpressions` vs `GetExpressionCollection`), `GetNiagaraEmitterCount/Name/Enabled`, `GetSkeletonSockets`, `GetSkeletalMeshMaterialCount`. Niagara module added to `BobBot.Build.cs::PrivateDependencyModuleNames`.
- **Three new perf-summary MCP tools** that flag outliers directly: `get_lod_summary` (NO_LODS / HIGH_POLY_NO_NANITE flags), `get_material_complexity` (COMPLEX >50 / OVERCOMPLEX >150 expressions), `get_niagara_summary` (HEAVY >5 enabled emitters / MASSIVE >15). The point isn't more inspection — it's actionable signal when BobBot walks a map.
- **`list_tools` runtime discovery**. New `Scripts/tools/meta.py` registers a `list_tools(category="")` MCP tool that walks the FastMCP registry and returns the live catalog. Eliminates the four-way doc drift (`CLAUDE.md` / `Docs/ToolReference.md` / `README.md` / `SBobBotInfoTab.cpp::GToolCategories[]` previously claimed 203 / 200 / 200 / 202 tools respectively). CLAUDE.md now banners the live catalog as authoritative; the static lists are best-effort docs for humans. Also added `## Adding a tool` checklist enumerating the four sync points so new entries don't drift again.
- **Tool count: 207** (up from ~203). New: `list_tools`, `get_lod_summary`, `get_material_complexity`, `get_niagara_summary`, plus existing tools that were silently broken now work.

### Smarter context (Phase 2)

- **Project memory via native CLAUDE.local.md.** BobBot now seeds `<project>/Saved/BobBot/CLAUDE.local.md` at startup and instructs Claude to record user-stated conventions there in the same turn. The file lives at BobBot's cwd, so Claude Code's native upward filesystem walk picks it up as a memory file automatically — no custom reader code, no parallel format. The directory hierarchy gives natural isolation: BobBot's walk passes through both `Saved/BobBot/` (its own conventions) and `<project>/` (any project-wide CLAUDE.local.md), but terminal Claude Code running from `<project>/` never descends into `Saved/BobBot/` so it never sees BobBot's session-scoped notes. One-way visibility, by file system, no plumbing.
- **Blueprint graph reading.** Two new tools, `get_graph_nodes(blueprint_path, graph_name)` and `get_node_details(blueprint_path, node_name)`, let Claude introspect Blueprint logic before mutating it. Both delegate to new C++ helpers `UBobBotLib::DescribeBlueprintGraph` and `DescribeBlueprintNode` that walk `Blueprint->GetAllGraphs() → Graph->Nodes → Pin->LinkedTo` and emit one line per node with name, title, position, and pin connections. Implemented in C++ rather than Python because UE 5.4's `unreal.EdGraphPin` reflection is unreliable for pin types and `linked_to`. The detail tool returns the same `is ambiguous (...)` error format as the v1.6 ConnectPins fix, listing every candidate's unique synthesized name when multiple nodes match by title.
- **Material graph reading.** New `get_material_graph(material_path)` tool extends the existing materials.py reflection to walk every expression with class, editor position, parameter name and default value (for Scalar/Vector/TextureSample parameters), input connections, and which expressions feed which root material properties (BaseColor, Metallic, Roughness, Normal, EmissiveColor, Opacity, etc). Pure Python — material expressions are UObjects and `FExpressionInput` USTRUCTs are reflected, so no C++ helper needed.
- **Auto-capture viewport after edits (opt-in).** New `bAutoCaptureAfterEdits` config flag (off by default) and a new `@autocaptured` decorator in `_common.py`. When enabled, decorated write tools return a FastMCP multi-content response: `[text_result, Image(path=screenshot), "Auto-captured: <path>"]`. Vision-capable Claude models read the `ImageContent` block directly and self-correct if the result looks wrong. The trailing text path triggers the in-chat image preview pipeline (the v1.6 1.3 fix). Decorator-based rather than baked into `_exec()` because FastMCP's Pydantic validation enforces a function's annotated return type, so the affected tools' return annotations were dropped to allow `Any`. Throttled at one capture per 2 seconds via a `threading.Lock`-guarded module global. Initial whitelist: `spawn_actor`, `delete_selected_actors`, `set_actor_property`, `set_actor_label`, `set_simulate_physics`, `set_physics_properties`, `create_light`, `set_light_properties`, `set_static_mesh_on_actor`, `focus_on_actor`. Vision-capability gating was deliberately cut for v1.6 — the bridge process is launched once at editor startup and inherits env vars frozen at that moment, so pushing runtime model changes to it would require a TCP control channel that doesn't exist yet. Tradeoff: a Haiku user who flips the opt-in gets a useless image content block; they opted in.
- **`_get_tool_name` frame-walking fix.** The existing tool-name detector in `_common.py` skipped `_exec` and `actor_exec` but not `asset_exec` or `_exec_ue`, so the dozen+ tools that route through `asset_exec` (most material and blueprint tools) were getting misclassified for the 32KB output truncation marker. Walked into the `@autocaptured` work; fixed at the same time. The internal-frame skip list is now `(_exec, _exec_ue, actor_exec, asset_exec)` and the walker loops past every internal frame instead of doing a single hard-coded one-step lookup.
- **4 new test categories** in `_test_runner.py`: `memory` (asserts the seed file exists and the system prompt references its absolute path), `material_graph` (creates a transient material with a known scalar parameter, walks the expression list, deletes), `blueprint_graph` (creates a transient Blueprint with a custom event, calls the new C++ helper, asserts the event name is in the output), and `auto_capture` (pure-Python tests that monkey-patch `_capture_for_autocapture` and verify the gate, throttle, and decorator branching without actually invoking viewport capture).

Phase 1 hardening of v1.5. No new tools; everything here is making the existing surface solid.

### Reliability
- **Approval flow race fix**: replaced the single-slot `_pending_permission` / `_permission_response` globals in `bob_sdk_permissions.py` with a dict of `asyncio.Future`s keyed by a monotonic id (`itertools.count`). Two concurrent tool calls in Ask mode used to clobber each other and one coroutine would hang for 120s; now every approval has its own future and `set_permission_decision(id, decision)` resolves the matching one. `id()` of the input dict was also unsafe (Python recycles addresses across deallocations) — switched to a monotonic counter.
- **Mode switch resolves pending approvals**: switching to `bypassPermissions` / `acceptEdits` / `dontAsk` mid-prompt now drains all pending approvals as `allow`; switching to `plan` drains as `deny`. `clear_session()` and `cleanup()` drain to `deny`. No more orphaned coroutines after the user changes their mind.
- **12 new approval-flow tests** in the `/test approvals` category covering single allow/deny on both code paths, sequential and concurrent approvals, mismatched-id resolution (the bug above), mode-switch drains, auto-approved category short-circuit, timeout cleanup, and id-uniqueness churn (100 sequential approvals).
- **Streaming text was completely broken**. We pass `include_partial_messages=True` to the SDK, which causes it to emit `StreamEvent` objects (raw Anthropic API stream events with text deltas) before the final assembled `AssistantMessage`. The Python event loop in `bob_sdk_events._send_and_stream` had no branch for `StreamEvent` so partials were silently dropped — text always appeared all at once when the assistant turn completed. Added a `StreamEvent` branch that accumulates text deltas per content block and emits cumulative text on each chunk; the C++ side already replaces (not appends), so per-chunk emission produces a smooth in-place update. The trailing `AssistantMessage` text re-emit is harmless for single-text-block turns and corrects ordering for the rare text-then-tool-then-text case (the AssistantMessage tool_use event resets `StreamingBotMessageIndex` so the second text becomes a new bot message).
- **Image preview was completely broken**. `BuildInlineImageWidget` constructed `FSlateDynamicImageBrush(FName(*ImagePath), ImageSize)` — that constructor takes a brush *name*, not a file path, and never loaded any pixel data. `HasUObject()` returned false and the text fallback always fired. Even if it had loaded, the local `TSharedPtr<FSlateDynamicImageBrush>` was destroyed on function return while `SImage` held a raw pointer (use-after-free on the next paint). Rewrote with a proper pipeline: `FFileHelper::LoadFileToArray` → `IImageWrapperModule::DetectImageFormat` → `IImageWrapper::SetCompressed` / `GetRaw(BGRA, 8)` → `UTexture2D::CreateTransient` + `BulkData.Lock`/`Memcpy`/`Unlock` + `UpdateResource()` → `Texture->AddToRoot()` (so GC doesn't collect it) → `FSlateDynamicImageBrush(Texture, Size, Name)`. Brushes are stored in a static `TMap<FString, FBobBotImageCacheEntry>` keyed by absolute file path so SImage's raw pointer stays valid for the editor session and repeat references reuse the cached brush. Added aspect-ratio letterboxing into the 480×270 preview area. Required adding `ImageWrapper` to `BobBot.Build.cs`.

### Smarter tools
- **Blueprint `connect_pins` ambiguity diagnostic**. The old code matched node title or unique name with a single overwriting loop and a case-sensitive title compare. Two nodes with the same display title (e.g. two `Print String` calls) silently let the last-found win; case-sensitive title matching also conflicted with the case-insensitive `FindPin` so `"print string"` would fail title lookup but `"then"` would succeed pin lookup on the same call. Two-pass refactor in `BobBotLib::ConnectPins`: collect every match for source and target, error with a list of unique node `GetName()`s when there are 2+ candidates, use case-insensitive title matching while keeping the synthesized `GetName()` (e.g. `K2Node_CallFunction_3`) as the case-sensitive disambiguator. Claude can recover from the ambiguous error by re-calling with the unique name from the diagnostic.

### Live config switching
- **Thinking and effort live switching**: previously the dropdown buttons only set env vars and the change was silently ignored until the user cleared the session, because the SDK reads `--max-thinking-tokens` and `--effort` only at process spawn time and the control protocol exposes no `set_thinking` / `set_effort`. Added `bob_chat.set_thinking_mode()` and `bob_chat.set_effort()` Python helpers that flip a `_client_needs_rebuild` flag in `bob_sdk_client`. `_ensure_client()` now disconnects and reconnects on the next message when the flag is set; resume preserves the session id so no context is lost. Cost: ~1-2s pause on the first message after toggling. C++ chat tab dropdowns and `/thinking` / `/effort` slash commands wired through.

### Security
- **HTTP bridge token auth**: `bob_mcp_bridge_http.py` now reads `BOB_BRIDGE_TOKEN` from the environment and rejects any request without a matching `X-Bobbot-Token` header (`401 Unauthorized` via a Starlette `BaseHTTPMiddleware`). Replaced `mcp.run(transport="streamable-http")` with manual `mcp.streamable_http_app() → add_middleware → uvicorn.serve()` to control the middleware stack.
- **TCP server token gate**: `bob_mcp_server.py` requires the first message on every new connection to be `{"type": "auth", "token": "..."}` matching `BOB_BRIDGE_TOKEN`. Anything else closes the socket. Both bridge variants (`bob_mcp_bridge_http.py` and `bob_mcp_bridge.py`) send the auth handshake automatically after connecting.
- **Per-launch random token**: `FBobBotConfig::RegenerateBridgeAuthToken()` mints a fresh 64-char hex token (two `FGuid`s) on every editor launch. The token is set as `BOB_BRIDGE_TOKEN` for child processes, written into the auto-generated `_bobbot_mcp.json` headers field for BobBot's own SDK client, and **never persisted to the on-disk INI**.
- **External MCP client docs**: new `Docs/MCP-External-Clients.md` explaining the trust model, where to find the token, how to wire Cursor / VS Code / Windsurf, and how to disable auth if needed. The threat model: random local processes are blocked outright; explicitly-configured external editors run in a separate trust context (their own permission UI, not BobBot's).

### Cleanup
- Deleted ~80 lines of dead code from `bob_mcp_server.py`: `_TOOL_CATEGORY_OVERRIDES`, `_CATEGORY_PREFIXES`, `_classify_tool`, `_is_auto_approved`, and the unused `_PERMISSION_MODE` global. These duplicated `bob_sdk_permissions.py` and were never read after the v1.5 SDK refactor moved permission gating into the SDK hooks. Single source of truth restored.
- **Slash command registration carries its own help text.** `FBobBotSlashCommand` is now a struct with `Name`, `UsageSuffix`, `Description`, and `Handler`. Each command registers all four via `RegisterSlashCommand()`. Both `/help` and the Info tab's slash-command card grid render from the same `GetSlashCommands()` accessor — no more parallel arrays drifting out of sync. The Info tab was missing 5 commands (`/effort`, `/thinking`, `/test`, `/rename`, `/tag`) before this refactor; they appear automatically now and any future addition will too.

## [1.5-beta] - 2026-04-03

First distributable release. Complete rewrite from subprocess-based architecture to persistent Agent SDK backend.

### Core
- Claude Agent SDK backend with persistent process (near-zero latency between messages)
- 200 MCP tools across 38 categories
- HTTP MCP bridge (port 13580) with multi-editor support
- Three permission modes: Auto, Ask before edits, Plan
- Session management: list, rename, tag, delete, fork conversations
- Thinking controls (disabled/enabled/adaptive) and effort levels
- Per-message cost budget with configurable limit

### UI
- Five-tab layout: Welcome, Connect, Chat, Context, Info
- Theme system (BobBotTheme.h) with consistent dark surfaces
- 14 SVG icons, selectable text, rich text rendering
- Streaming text display (partial messages update in real-time)
- Quickstart suggestion chips for new conversations
- Inline image preview for viewport captures
- Friendly asset names on drag-and-drop
- Subagent tasks with expandable nested tool calls
- Session forking with tree-view dropdown

### Setup
- Automated FTUE wizard with per-step retry and failure hints
- Auto-creates Python venv from UE's bundled Python
- Auto-installs MCP bridge, Agent SDK dependencies
- Auto-generates .mcp.json for Claude Code, Cursor, VS Code, Windsurf

### Security
- API key stored in Windows Credential Manager (migrated from plaintext INI)
- All servers bound to localhost only
- Tool auto-approve categories with independent toggles

### Blueprint Editing (BobBotLib)
- Variable management: add, remove, set defaults, categories, tooltips, slider ranges
- Component management: add to Blueprint SCS
- Graph creation: functions, custom events
- Graph nodes: function calls, MPC scalar/vector, branch, variable get/set, cast
- Pin wiring: connect any two pins by name
- Compilation with status reporting

### Reliability
- Error handling cleanup: all bare except: replaced with proper Exception handling + logging
- Bridge reconnection: auto-detect death, attempt restart, show status in chat
- Test harness: /test slash command runs smoke tests against live editor
- 120-second step timeout in FTUE wizard

### Docs
- LICENSE.md (MIT) with attribution for claude-agent-sdk, mcp, pywin32
- Updated README reflecting current SDK-only architecture
- Archived pre-refactor planning docs

### Architecture
- `bob_platform.py` — centralized platform abstraction (IS_WINDOWS/IS_MAC/IS_LINUX, subprocess flags, venv paths, process management). All sys.platform checks removed from other files.
- `bob_chat_sdk.py` split into 5 focused modules: `bob_sdk_config` (config/prompt), `bob_sdk_permissions` (tool classification/hooks), `bob_sdk_events` (streaming), `bob_sdk_client` (lifecycle), facade
- `_common.py` upgraded: `actor_exec()` (actor lookup + code), `asset_exec()` (asset load + code), `_exec_ue()` (auto-import unreal). Adopted across all 19 tool files, eliminating 46 inline find-by-label loops.
- `_run_pip_install()` — shared pip helper replacing 3 duplicate install functions in bridge launcher
- `FBobBotPythonBridge::CallPythonJson()` + `PyStr()` — C++ helper for Python function calls with safe escaping
- `FBobBotServerMonitor`, `FBobBotBridgeMonitor`, `FBobBotSdkMonitor` — extracted from PollServerStatus() (110 lines → 3 lines)
- `UI::ConfigCheckbox()` — pointer-to-member helper for config-bound checkboxes with auto-save
- `/help` auto-generated from registered command keys
- Fixed ctypes.windll crash on macOS/Linux, claude.exe hardcoded name, Unix process group killing

### Known Issues
- Image preview depends on FSlateDynamicImageBrush which may not work on all UE versions
- Multi-platform support is guarded but only tested on Windows
- Blueprint pin wiring uses node name matching which may be ambiguous with duplicate names
