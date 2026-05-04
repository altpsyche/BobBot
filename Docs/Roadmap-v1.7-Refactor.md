# BobBot review + implementation plan

This plan has two parts. **Part 1** is the implementation work the user originally asked for (fix the protected-`StreamingLevels` issue, expand to all 11 broken-introspection subsystems, add runtime tool discovery). **Part 2** is the holistic codebase review the user asked for after Part 1 was drafted, across the seven axes they named. Recommendations from Part 2 are folded into a prioritized backlog at the end so you can pick what to ship next.

---

## Part 1 — Implementation: unblock map inspection

### Context

BobBot's Python tools call `obj.get_editor_property("X")` ~120 times across 13 files. Many `X`s are **protected** UPROPERTYs (`UWorld::StreamingLevels`, `UMaterial::Expressions`, `UStaticMesh::SourceModels`, `UMovieScene::MasterTracks`, `UNiagaraSystem::EmitterHandles`, `UWidgetBlueprint::WidgetTree`, `USkeleton::Sockets`, …). UE's Python binding gates `get_editor_property` on `BlueprintReadable`; those calls silently fail. Every one is a blind spot for the agent.

User wants BobBot to deeply inspect maps and flag perf outliers. Each blind spot masks a perf signal: meshes with no LODs, materials with too many instructions, Niagara systems with runaway emitters, streaming levels stuck loaded, widget trees with deep hierarchies, etc.

Secondary problem: tool catalog duplicated across **four** locations (`CLAUDE.md` says 203, `Docs/ToolReference.md` says 200, `README.md` says 200, `Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp::GToolCategories[]` has 202). A runtime `list_tools` MCP endpoint eliminates the agent's need to read those docs at all.

### Confirmed decisions

- Helper style: **Generic + typed**.
- Scope: **All 11 subsystems** flagged in the audit.
- Runtime discovery: **Add `list_tools` now**.

### Approach

Add a "Reflection / Engine Property Access" section to [`UBobBotLib`](Source/BobBot/Public/BobBotLib.h). The Python binding's `BlueprintReadable` gate is what fails — `FProperty` reflection itself does not check access modifiers, so a C++ helper in our own UFUNCTION reads protected fields by name and re-exports them.

#### Generic helpers (force multiplier)

```cpp
// Returns ExportText serialization of any UPROPERTY by name. Bypasses
// BlueprintReadable. Empty string if not found / null / unsupported.
UFUNCTION(BlueprintCallable, Category = "BobBot|Reflection")
static FString ReadPropertyAsString(UObject* Object, FName PropertyName);

// Returns the contents of a TArray<UObject*>-typed UPROPERTY. Empty array on
// miss / type mismatch / null. Covers TArray<TObjectPtr<T>> too.
UFUNCTION(BlueprintCallable, Category = "BobBot|Reflection")
static TArray<UObject*> ReadObjectArrayProperty(UObject* Object, FName PropertyName);
```

`ReadPropertyAsString` — `FindFProperty<FProperty>(Object->GetClass(), Name)` + `Property->ExportText_InContainer(0, Out, Object, Object, nullptr, PPF_None)`.

`ReadObjectArrayProperty` — `CastField<FArrayProperty>` → `Inner = CastField<FObjectPropertyBase>` → `FScriptArrayHelper::Num()` + `GetRawPtr(i)` → cast to `UObject*`.

#### Typed array getters (real Python pointers for hot paths)

| Function | Wraps |
|---|---|
| `GetWorldStreamingLevels(UWorld*) -> TArray<ULevelStreaming*>` | `UWorld::GetStreamingLevels()` |
| `GetMaterialExpressions(UMaterial*) -> TArray<UMaterialExpression*>` | `UMaterial::GetExpressionCollection()` (5.1+) |
| `GetMovieSceneTracks(UMovieScene*) -> TArray<UMovieSceneTrack*>` | `UMovieScene::GetTracks()` |
| `GetMovieSceneBindings(UMovieScene*) -> TArray<FMovieSceneBinding>` | `UMovieScene::GetBindings()` |
| `GetSkeletonSockets(USkeleton*) -> TArray<USkeletalMeshSocket*>` | `USkeleton::Sockets` |
| `GetNiagaraEmitterHandles(UNiagaraSystem*) -> TArray<FNiagaraEmitterHandle>` | `UNiagaraSystem::GetEmitterHandles()` |
| `GetWidgetTreeRoot(UWidgetBlueprint*) -> UWidget*` | `UWidgetBlueprint::WidgetTree->RootWidget` |
| `GetStaticMeshSourceModels(UStaticMesh*) -> TArray<FStaticMeshSourceModel>` | `UStaticMesh::GetSourceModels()` |
| `GetStaticMeshNaniteSettings(UStaticMesh*) -> FMeshNaniteSettings` | `UStaticMesh::NaniteSettings` |
| `GetBlackboardKeys(UBlackboardData*) -> TArray<FBlackboardEntry>` | `UBlackboardData::Keys` |
| `GetInputContextMappings(UInputMappingContext*) -> TArray<FEnhancedActionKeyMapping>` | `UInputMappingContext::GetMappings()` |
| `GetBlueprintFunctionGraphs(UBlueprint*) -> TArray<UEdGraph*>` | `UBlueprint::FunctionGraphs` |
| `GetBlueprintUbergraphPages(UBlueprint*) -> TArray<UEdGraph*>` | `UBlueprint::UbergraphPages` |
| `GetBlueprintSCSNodes(UBlueprint*) -> TArray<USCS_Node*>` | `UBlueprint::SimpleConstructionScript->GetAllNodes()` |
| `GetLandscapeEditorLayers(ALandscape*) -> TArray<FLandscapeEditorLayerSettings>` | `ALandscapeProxy::EditorLayerSettings` |

Verification step 2 below confirms whether `TArray<USTRUCT>` reflects to Python with member access. If not, fall back to per-field typed getters per struct (e.g. `GetSourceModelScreenSize(UStaticMesh*, int32) -> float`).

### Phases

**Phase 0 — `list_tools` runtime discovery.** New `Scripts/tools/_meta.py` registering `list_tools(category="")`. Snapshot the FastMCP registry at scan time in [`bob_mcp_bridge.py`](Scripts/bob_mcp_bridge.py) (lines 156–189) into a module-level dict, expose via `_meta.py`. Returns `[{name, category, params, doc_first_line}, ...]`. Update CLAUDE.md to say *"call `list_tools` for the live catalog."*

**Phase 1 — Streaming levels (original task).** Add `GetWorldStreamingLevels`. Rewrite [level_streaming.py:39, 69](Scripts/tools/level_streaming.py).

**Phase 2 — Perf-critical typed getters.** LOD/Nanite, Materials, Niagara, Skeletal. Plus three derived summary tools — `get_lod_summary`, `get_material_complexity`, `get_niagara_summary` — that flag outliers directly. These three are what turns inspection into actionable signal.

**Phase 3 — Cinematic / UI inspection.** Sequencer, UMG widgets. Add `get_widget_tree_summary` (depth + node count).

**Phase 4 — Lower-priority subsystems.** Blueprint internals, AI/Blackboard, Input, Landscape. Read paths get rewritten; struct-mutation writes get typed C++ helpers (`AddBlackboardKey`, `AddInputContextMapping`) instead of struct-by-reflection rewrites.

**Phase 5 — Doc/list synchronization.** For every NEW MCP tool added (`list_tools`, `get_lod_summary`, `get_material_complexity`, `get_niagara_summary`, `get_widget_tree_summary`), update **all four** catalogs in the same commit: [CLAUDE.md](CLAUDE.md), [Docs/ToolReference.md](Docs/ToolReference.md), [README.md](README.md), [SBobBotInfoTab.cpp](Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp). Add `## Adding a tool` checklist to CLAUDE.md.

### Critical files

**C++ (modify):**
- [Source/BobBot/Public/BobBotLib.h](Source/BobBot/Public/BobBotLib.h) — new `// -- Reflection --` and `// -- Asset/Level Inspection --` sections.
- [Source/BobBot/Private/BobBotLib.cpp](Source/BobBot/Private/BobBotLib.cpp) — implementations + new includes (`Engine/World.h`, `Engine/LevelStreaming.h`, `Materials/Material.h`, `Engine/StaticMesh.h`, `Animation/Skeleton.h`, `Engine/SkeletalMeshSocket.h`, `MovieScene.h`, `NiagaraSystem.h`, `WidgetBlueprint.h`, `Blueprint/WidgetTree.h`, `BehaviorTree/BlackboardData.h`, `InputMappingContext.h`, `Landscape.h`, `LandscapeProxy.h`).
- [Source/BobBot/BobBot.Build.cs](Source/BobBot/BobBot.Build.cs) — add `MovieScene`, `Niagara`, `UMG`, `UMGEditor`, `AIModule`, `EnhancedInput`, `Landscape` to `PrivateDependencyModuleNames`.
- [Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp](Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp) — add new entries to `GToolCategories[]`.

**Python (modify):** [level_streaming.py](Scripts/tools/level_streaming.py), [lod.py](Scripts/tools/lod.py), [materials.py](Scripts/tools/materials.py), [niagara.py](Scripts/tools/niagara.py), [skeletal.py](Scripts/tools/skeletal.py), [sequencer.py](Scripts/tools/sequencer.py), [umg_widgets.py](Scripts/tools/umg_widgets.py), [blueprint_advanced.py](Scripts/tools/blueprint_advanced.py), [ai_tools.py](Scripts/tools/ai_tools.py), [input_system.py](Scripts/tools/input_system.py), [landscape.py](Scripts/tools/landscape.py).

**Python (create):** [Scripts/tools/_meta.py](Scripts/tools/_meta.py).

**Bridge (modify if needed):** [Scripts/bob_mcp_bridge.py](Scripts/bob_mcp_bridge.py) — only if the FastMCP registry doesn't already expose names.

**Docs (sync):** [CLAUDE.md](CLAUDE.md), [Docs/ToolReference.md](Docs/ToolReference.md), [README.md](README.md), [Docs/BobBotLib.md](Docs/BobBotLib.md).

### Reused existing utilities

- `unreal.BobBotLib.*` reflection convention (CamelCase → snake_case) — established by `add_function_graph`, `compile_blueprint`, etc.
- `_exec` / `_safe` from [Scripts/tools/_common.py](Scripts/tools/_common.py:65).
- Tool self-registration — `register(mcp, send_fn)` + bridge `_scan_tools_dir()`.
- Public C++ getters where they exist (table above).
- `GetBlueprintVariableNames` ([BobBotLib.cpp](Source/BobBot/Private/BobBotLib.cpp)) — template for null-check + return-array shape.

### Verification

1. **Build**: editor hot-reload via Live Coding (or close + rebuild). `compile_blueprints` MCP tool to confirm BobBotLib functions reachable.
2. **Reflection helpers**:
   - `unreal.BobBotLib.read_property_as_string(world, "StreamingLevels")` returns non-empty.
   - `len(read_object_array_property(world, "StreamingLevels"))` matches `len(get_world_streaming_levels(world))`.
   - `read_property_as_string` on a known-OK property (light Intensity) returns the numeric.
   - **Verify struct-array Python binding**: `len(get_niagara_emitter_handles(ns))` matches editor UI; iterating exposes `.Name` and `.bIsEnabled`.
3. **Phase 1**: open level with sub-levels. `get_streaming_levels` lists; `remove_streaming_level` removes; relist confirms.
4. **Phase 2**: `get_lod_summary` returns LOD count + Nanite flag; `get_material_complexity` returns expression count; `get_niagara_summary` returns emitter count. Cross-check vs editor UIs.
5. **Phase 3**: `get_sequence_tracks` non-empty on a known sequence; `get_widget_tree` returns real root + children.
6. **Phase 4**: `get_blackboard_keys` lists keys with names + types; `get_input_actions` returns mappings.
7. **Phase 0 `list_tools`**: returns ≥200 entries; `category=` filter works; count matches `GToolCategories[]` ±2.
8. **End-to-end perf smoke test**: BobBot walks the test map, calls the new summary tools per actor, prints outliers. Confirm a known heavy mesh / material gets flagged.
9. **Regression**: `get_actor_properties`, `spawn_actor`, `capture_viewport`, `compile_blueprints` unaffected.

### Risks

- New build deps (`MovieScene`, `Niagara`, `UMG`, `UMGEditor`, `AIModule`, `EnhancedInput`, `Landscape`) enlarge link surface. All are editor-only and already common — confirm BobBot module Type stays `Editor` (it is).
- Struct-array Python binding may not fully reflect for some structs. Verification step 2 catches; fall back to per-field typed getters where needed.
- Engine-version drift on a few public getters; project floor is UE 5.4 per [CLAUDE.md](CLAUDE.md). Use `#if ENGINE_MINOR_VERSION` only where required.
- Write paths intentionally deferred — Phase 4 mutations get typed `Add*` C++ helpers; everything else stays read-only first.
- Doc drift continues until `list_tools` is universally adopted; the CLAUDE.md banner mitigates.

### Out of scope (Part 1)

- Migrating the four-list catalog to a single source-of-truth manifest (worth doing later; not blocking perf inspection).
- Replacing all `set_editor_property`-based struct mutations across the 13 files.
- World Partition / runtime streaming inspection (`UWorldPartition`).

---

## Part 2 — Codebase review

Findings from a focused audit of `Source/BobBot/` (~4.5K LOC C++) and `Scripts/` (~50 tool modules + bridge). Each axis below: **what's good → what's weak → what to do.**

### Code Quality

**Good.**
- UE conventions held throughout: F/U/A/E/S/b prefixes, CamelCase, `LogBobBot` as a single named log category.
- Strong const-correctness on getters (`GetHistory() const`, `IsThinking() const`).
- Header hygiene: forward declarations dominate; heavy includes confined to `.cpp`.
- Multi-channel error reporting that matches caller needs: `bool` for boolean operations, `FString` for human-readable status (`CompileBlueprintWithStatus`), user-visible chat messages via `AddMessage(..., ESender::Error, ...)`.
- Disciplined logging — no spam in tight polling loops.

**Weak.**
- **No `TObjectPtr` adoption** despite UE 5.4 floor. UPROPERTY-tracked `UObject*` fields (notably none in BobBot's non-UObject classes — actually safe, but *will* matter if any UObject sub-classes are added in BobBotLib growth).
- **F-string-of-Python pattern is pervasive in tool files** ([materials.py:16–33](Scripts/tools/materials.py), [sequencer.py:11–21](Scripts/tools/sequencer.py)): tools build a multi-line Python string with `{_safe(arg)}` substitutions. Risks: syntax errors blame the f-string not the call site; IDE lint/typecheck doesn't see the embedded code; escaping burden falls on every tool author.
- **Type hints are surface-only**: every tool returns `-> str` even when `autocaptured` ([_common.py:245–274](Scripts/tools/_common.py)) returns `[text, Image, ...]`. Lint suppresses, but the contract is misstated.
- **Silent exception swallowing**: ~338 try/except blocks across tools, most catching bare `Exception` and either falling back or printing `ERROR:` without preserving context. Original exception is lost — debugging needs UE output log.

**Action.**
1. Add a `_codegen.py` helper to `Scripts/tools/` that builds the Python-in-Python source as concatenated `textwrap.dedent` + parameter-binding via a small `Snippet` class — gives line numbers, IDE syntax, and centralised escaping. Migrate one tool (e.g. `level_streaming.py`) as proof, then incremental.
2. Standardise error returns: define `ToolError(message, *, where=None, original=None)` in `_common.py`; tools `raise ToolError(...)` and a top-level catch in `_exec` formats it with original traceback for the agent. Replaces `print(f"ERROR: ...")`.
3. Tighten `-> str` to `-> ToolResult` (a typed union: text, list-of-content, or error) — update FastMCP's content unpacker accordingly.

### Code Reliability

**Good.**
- Defensive null-checks early-return in C++ ([BobBotChatController.cpp](Source/BobBot/Private/BobBotChatController.cpp)).
- Threading is sane: C++ on game thread, Python work on daemon threads, polling is tick-based with short timeouts (50ms active / 1000ms idle).
- GC/lifetime safety: chat history in plain `TArray`, JSON via `TSharedPtr<FJsonObject>`, no cached raw `UObject*` in TMaps.
- Reconnection logic: 3 attempts + 5s cooldown for the HTTP bridge ([BobBotStatusMonitors.cpp:76–91](Source/BobBot/Private/BobBotStatusMonitors.cpp)).
- Subagent tracking by UUID string keys — no dangling refs.

**Weak.**
- **Python state leaks across calls** (documented in [CLAUDE.md](CLAUDE.md): *"Variables persist across `execute_unreal_python` calls"*). Any tool that does `result = ...` leaves it in the interpreter; later tools can read or accidentally collide. Fine for power users, hostile to designer use.
- **No editor transaction wrapping**. BobBot edits are not inside `FScopedTransaction` — a 5-actor spawn requires Ctrl+Z 5 times. This is the single biggest reliability gap from a designer's POV.
- **No hot-reload guards** on C++ singletons (`FBobBotConfig::Get()`, `FBobBotPythonBridge::Get()`, `UBobBotLib::TypeCache`). Live-coding a typo into BobBot leaves stale state.
- **Stale chat index** if backend session deleted externally — controller doesn't validate the cached index against bridge state.
- **Socket timeout fixed at 60–120s** with no per-tool override. A long mesh build inside `auto_generate_lods` can exceed it; the bridge returns "Connection failed" instead of "still working."
- **No bridge-level tests**. Python `_test_runner.py` covers tool semantics but not the socket / JSON / auth layer.

**Action.**
1. Wrap every Python tool call from C++ in `FScopedTransaction`. Group transactions by chat turn — a single user message → single undo step. Highest-leverage reliability+UX fix.
2. Add a `__pre_tool` hook in `_common.py` that snapshots the interpreter globals; restore after each tool unless the tool opts in via `@stateful`.
3. Add a 5-test bridge harness: connect / send / oversized response / timeout / disconnect-recover. Gate releases on it.
4. Add per-tool timeout override (default 60s, override via decorator `@tool(timeout=300)`).
5. Add `RegisterStartupCallback` / `OnEnginePreExit` cleanup of singletons; clear `UBobBotLib::TypeCache` on `FCoreDelegates::OnPostHotReload`.

### System Design

**Good.**
- Single `BobBot` editor module, `LoadingPhase: PostEngineInit` — clean.
- File-based JSON IPC for one-shot Python eval is unconventional but appropriate (transient output, no dependency on a serialization lib).
- Long-running HTTP bridge (`bob_bridge_launcher`) handles the chat session; status monitors poll its health independently from chat traffic.
- Five-tab UI is clearly separated by role: Chat, Connect (setup + diagnostics), Context (system prompt + memory), Welcome (FTUE state machine), Info (slash commands + tool catalog).
- Multicast delegates decouple widgets from the chat controller (`OnMessageAdded`, `OnApprovalStateChanged`, `OnThinkingStateChanged`).
- Approval flow with category toggles (`bAutoApproveReadOnly`, `bAutoApproveModify`, `bAutoApproveCodeExec`) is well-thought.

**Weak.**
- **Two transport mechanisms** with overlapping responsibility — file-based JSON eval (for direct Python from C++) and the TCP bridge (for the MCP server). Diagnostics report both, but failures can be confusing (which is down? which retries? which has auth?).
- **No formal Python⇄C++ contract version**. UE Python API drift (5.4 vs 5.5) is handled tool-by-tool with try/except chains ([niagara.py:91–121](Scripts/tools/niagara.py)). No version probe at boot.
- **No structured event log**. All Python output is `print()` to UE's output log; no JSON event stream, no per-tool timing, no hierarchy.
- **`UBobBotLib` is becoming the catch-all** (25 UFUNCTIONs already; this plan adds ~17 more). Cohesion is decent — all "fill UE Python API gaps" — but past ~50 it stops being a library and starts being a manifest. No discoverable subdivision.
- **`SBobBotInfoTab.cpp::GToolCategories[]` is the source of truth** for the human-facing catalog but is hand-edited and drifts from CLAUDE.md / README.md / ToolReference.md. The four-way drift is observable today (203 / 200 / 200 / 202).

**Action.**
1. Treat the TCP MCP bridge as the **only** transport long-term; deprecate file-based eval after migrating BobBotLib calls to MCP tool calls. Single transport, single auth surface, single failure mode.
2. Add a **boot-time version probe**: bridge calls `unreal.SystemLibrary.get_engine_version()` + `sys.version` and stores in a `BridgeManifest` object that tools can read. Replace per-tool try/except with `if manifest.engine_minor >= 5: ...`.
3. Replace `print()` with structured `bob_log(event, **fields)` that emits one-line JSON to UE log; the chat controller can render it as a tree.
4. Split BobBotLib by domain into multiple BlueprintFunctionLibraries: `UBobBotBlueprintLib`, `UBobBotReflectionLib`, `UBobBotInspectionLib`. Same module, separate translation units, lower cognitive load.
5. Single source of truth for the tool catalog: a JSON manifest emitted from `bob_mcp_bridge.py`'s scan, consumed by `SBobBotInfoTab.cpp` at editor startup (cached) and by docs build (CI step generates the markdown sections from the JSON). Eliminates the four-way drift permanently.

### Scalability and Modularity

**Good.**
- Python tool files are independently self-registering; adding a tool is one new file.
- `<ProjectRoot>/BobBot/tools/` is also scanned, so projects can ship their own tools without touching the plugin.
- Slate widgets coupled to the controller via raw pointer + delegate subscription, not direct method ownership.
- `FBobBotConfig` is a singleton with INI persistence; reasonable for plugin-internal config.

**Weak.**
- **No conflict resolution** for tools with the same name across the plugin and project tool dirs — first-loaded wins, silently.
- **No per-project tool authoring UI**. A designer must drop a `.py` into `<ProjectRoot>/BobBot/tools/` and restart the editor. No editor utility widget to scaffold a tool.
- **No tool dependency declarations**. Tools assume helpers are present; if a tool needs another tool's output, there's no contract.
- **Settings live in `FBobBotConfig` (a plain singleton), not `UDeveloperSettings`**. Means project leads can't override per-project via DefaultEngine.ini, and there's no Project Settings UI page — designers configure BobBot only inside its own panel.
- **Hot-reload churn** on `UBobBotLib::TypeCache` is unguarded; live-coding a tool while a chat is open can corrupt the cache.

**Action.**
1. Convert `FBobBotConfig` to a `UDeveloperSettings` (`UBobBotProjectSettings : UDeveloperSettings`). Keeps the INI persistence, gains Project Settings UI integration + per-project overrides.
2. Add explicit conflict-resolution policy for tool dirs: project tools override plugin tools by name, log a warning at scan time.
3. Add `bob_create_tool` editor utility: prompts for tool name + category, scaffolds `<ProjectRoot>/BobBot/tools/<name>.py` from a template, hot-reloads the bridge.
4. Add a `manifest_version` field to each tool module; bridge logs incompatibility at scan time.

### Scope for Improvement

Highest-leverage quick wins, ranked.

1. **Editor transactions around tool calls** (designer-critical). One `FScopedTransaction` per chat turn → single Ctrl+Z undoes the whole AI action. Implementation is small (wrap the C++ call site in `FBobBotPythonBridge::ExecPython*`); upside is huge.
2. **Activity log / change journal**. Every tool call logs `{timestamp, tool, args, affected_assets[], affected_actors[]}` to a JSONL in `Saved/BobBot/activity.jsonl`. New "History" tab in the panel renders it. Designers gain an audit trail; engineers gain debug evidence.
3. **`list_tools` runtime endpoint** (already in Part 1). Eliminates four-way doc drift permanently.
4. **Reflection helpers** (already in Part 1). Unblocks the perf-inspection blind spots.
5. **Bridge test harness**. Five tests against the socket/JSON/auth layer prevent silent regressions.
6. **Per-tool timeout override**. `auto_generate_lods` and similar long-running tools no longer false-alarm.
7. **Structured logging (`bob_log` JSON events)**. Lets the chat controller render a tree, makes incidents diagnosable.
8. **Auto-capture default = on for read tools** (currently opt-in). Vision-capable models self-correct better when they always see the result; throttle is already implemented, so cost is bounded.
9. **`UDeveloperSettings` migration** for project-level overrides.
10. **BobBotLib subdivision**. Mostly cosmetic; defer unless growth justifies.

### Scope for Editor Tooling for Designers

This is the biggest open product surface. BobBot today is a "power-user CLI in a Slate panel"; turning it into something a designer reaches for daily needs more than the chat tab.

**Concrete additions, ranked by leverage.**

1. **Right-click context menus.** Actor outliner → "Ask BobBot about this actor"; content browser → "Ask BobBot about this asset"; level menu → "Audit map for perf outliers." Each pre-fills the chat with a tool invocation. Implementation: register `FToolMenuOwner` entries in `FBobBotModule::StartupModule`. Low-medium effort, high adoption.
2. **Pre-built designer recipes** ("scripted actions" in BobBot terminology). One-click buttons in a new "Recipes" section of the Info tab: *Audit map perf*, *Find unused assets*, *Suggest LOD chains for selected meshes*, *Validate lighting*, *Generate test variant*. Each is a saved chat seed plus expected tools. Backed by the new perf-inspection tools from Part 1.
3. **Visual diff in approval UI.** Today's approval shows Python code. Replace with: (a) the tool name + human description from the registry, (b) a *predicted* before/after viewport screenshot when feasible (tool reports a `PreviewMode` capability), (c) the code as a collapsible "details." For non-previewable tools (e.g. Blueprint edits), show the affected asset list + a "diff" of touched UPROPERTYs.
4. **"What did BobBot do?" panel.** A History tab listing every action this session: tool name, target, timestamp, undo button. Single Ctrl+Z handles the latest, but the panel lets designers selectively revert any prior step (uses the activity log JSONL).
5. **Plan mode polish.** Plan mode is read-only today; show the planned tool sequence as a checklist preview before any approval. Designers can scratch out steps, then "Approve plan" to execute the rest.
6. **Tool browser / search.** Info tab is a static reference. Replace with: search box, category filter, "favorites" star, "recently used" list. Backed by the same `list_tools` JSON manifest from Phase 0.
7. **In-editor tool authoring**. `bob_create_tool` editor utility scaffolds a new Python tool from a template and hot-reloads. Lets a TD on a project add a custom tool without leaving the editor.
8. **First-run sample chat.** Welcome tab today is FTUE. After completion, drop the user into a guided "first chat" — preloaded prompt: *"Audit this map and tell me the three most expensive things"*. Demonstrates value in 30 seconds.
9. **Cost / budget HUD on every action.** Currently shown in chat header. Promote to a persistent inline indicator on the approval card (this action: ~$0.003; session: $0.04 of $5 budget). Builds trust.
10. **Context-menu over Output Log entries.** Right-click on a UE warning → "Ask BobBot to investigate." Pre-fills with the log line.

### Gaps and Misses

The under-the-radar items — neither bugs nor obvious features, but things you'd notice in a code review.

- **No C++ unit tests / automation specs.** `_test_runner.py` covers Python tool semantics inside UE; nothing covers BobBotLib functions or the C++ controller. Type-resolution code in `UBobBotLib::ResolveVarType` is exactly the kind of thing that should have a Spec test.
- **No CI files visible.** No `.github/workflows`, no build scripts that run the test runner, no version bump automation. Releases drift.
- **No CHANGELOG discipline tied to commits.** [CHANGELOG.md](CHANGELOG.md) is hand-maintained; recent commits (`Port fixes`, `Streaming text fixed - ConnectPins ambiguity fixed`) don't show up there.
- **No release notes in UI.** First launch after upgrade should highlight what's new — currently silent.
- **`bAutoApproveModify` defaults to off, which is right; `bAutoCaptureAfterEdits` defaults to off, which is wrong** — the agent is markedly better with self-vision and the throttle bounds cost.
- **No bulk approval.** Approving a 10-step refactor needs 10 clicks. "Approve next 5" or "Approve all this category for this turn" would reduce friction.
- **No outbound MCP examples for Cursor/VS Code/Windsurf** beyond the Connect tab one-click setup. Integration testing across clients isn't visible.
- **No telemetry / opt-in usage analytics.** Hard to know which tools are unused vs which power-users live in. Even local-only counts in the activity log would inform pruning.
- **Tool naming inconsistencies.** `get_actors_by_tag` (singular) vs `get_actor_tags` (plural target); `set_actor_property` vs `set_component_property` — different verb shapes for "set field on a thing." A naming style guide + a deprecation pass before 2.0 would pay off.
- **No formal versioning of the Python⇄C++ contract.** Tools fall back via try/except chains across UE versions; a bridge manifest (engine version, BobBotLib version, tool count, schema hash) would let `_common.py` skip unsupported tools cleanly.
- **`pie.py:start_pie/stop_pie/get_pie_actors`** don't validate state ([pie.py](Scripts/tools/pie.py)) — calling `stop_pie` when PIE isn't running silently returns "executed successfully, no output."
- **`_test_runner.py` doesn't run automatically.** It's a manual harness; should run on plugin load in development builds and report any failures in the Connect tab.
- **No editor-side rate limiting.** `RateLimitPerSecond` exists in config but is enforced at the bridge; a misbehaving session can still spam the chat controller.
- **Chat history is in-memory only.** Closing the editor loses the session. Persisting (JSON in `Saved/BobBot/sessions/`) + a "resume" flow would be a big quality-of-life win.
- **API key migration path** assumes Windows Credential Manager; macOS/Linux story isn't tested in code paths present today.
- **`SBobBotInfoTab.cpp::GToolCategories[]`** is hand-maintained and drifts from the runtime tool list (today: 202 vs 203/200/200). The single-source-of-truth manifest fix in System Design above resolves this.

---

## Prioritized backlog (recommendation)

Sequence the implementation as follows. Ship Part 1 first because it directly answers the user's stated need (perf inspection). Then layer designer surface on top.

### Sprint 1 — Unblock inspection + close the doc-drift hole (Part 1, ~2–3 days)

1. Reflection helpers (`ReadPropertyAsString`, `ReadObjectArrayProperty`).
2. Phase 1: streaming levels.
3. Phase 0: `list_tools` + CLAUDE.md banner.
4. Phase 2: LOD/Nanite, Materials, Niagara, Skeletal — including the three derived `*_summary` tools that are the actual perf-flag surface.
5. Phase 5: doc sync + `## Adding a tool` checklist.

### Sprint 2 — Reliability + designer foundation (~3–5 days)

6. `FScopedTransaction` per chat turn (Improvement #1).
7. Activity log JSONL + new "History" tab (Improvement #2).
8. Bridge test harness (Improvement #5).
9. Phases 3 + 4 of Part 1 (Sequencer, UMG, BP, AI, Input, Landscape).
10. Per-tool timeout override (Improvement #6).

### Sprint 3 — Designer UX (~5–7 days)

11. Right-click context menus on actors / assets / output log (Designer #1).
12. Recipes section in Info tab — "Audit map perf" first (Designer #2).
13. Visual diff in approval UI (Designer #3).
14. Tool browser/search in Info tab, backed by `list_tools` (Designer #6).
15. First-run sample chat (Designer #8).

### Backlog (later)

16. `UDeveloperSettings` migration.
17. Single-source tool manifest (eliminates `GToolCategories[]`).
18. Bridge transport consolidation (drop file-eval).
19. Structured logging (`bob_log`).
20. In-editor tool authoring (`bob_create_tool`).
21. CI: test runner + version bump + changelog enforcement.
22. Persistent chat history.
23. Naming style guide + 2.0 deprecation pass.

This ordering keeps each sprint shippable on its own, prioritises the user's stated goal, and front-loads the highest-leverage reliability + designer fixes.

---

## Part 3 — Risks & Mitigations

Per-change risk register. Format: **Risk → Likelihood × Impact → Mitigation → Detection**. Likelihood/Impact are L/M/H. Anything M×H or H×H gets pre-flight gating.

### Part 1 implementation risks

| # | Risk | L × I | Mitigation | Detection |
|---|------|-------|------------|-----------|
| 1.1 | `ReadPropertyAsString` exposes private engine state and a future engine update changes serialization shape, breaking Python parsers downstream. | M × M | Use the helper for *agent inspection* only (human-readable). For machine-parsed paths, always go through a typed getter. Document this in the helper's UFUNCTION comment. | A tool that parses ExportText output starts failing on engine upgrade — caught by the perf-smoke test in verification step 8. |
| 1.2 | Adding `MovieScene`, `Niagara`, `UMG/UMGEditor`, `AIModule`, `EnhancedInput`, `Landscape` as plugin deps inflates load order and can trigger cyclic-module errors in some project setups. | L × H | Add deps incrementally, one phase at a time. After each phase, do a clean rebuild with `-noubtmakefiles` and confirm editor cold-start succeeds. Keep BobBot module Type=Editor and LoadingPhase=PostEngineInit (already set). | Editor fails to load with module-init error. Reproduce on a clean checkout before each phase merges. |
| 1.3 | `TArray<USTRUCT>` returned from a UFUNCTION may not bind to Python with full member access for some structs (`FStaticMeshSourceModel`, `FNiagaraEmitterHandle`, `FBlackboardEntry`). | M × M | Verification step 2 explicitly probes this on a known struct *before* writing any tool that depends on it. If it fails, fall back to per-field typed getters (e.g. `GetSourceModelScreenSize(UStaticMesh*, int32) -> float`). | Verification step 2 fails — gate Phase 2/3 on it. |
| 1.4 | `unreal.BobBotLib.read_property_as_string` becomes the agent's hammer and gets used everywhere instead of typed getters, regressing type-safety across tools. | M × M | Lint rule in `_test_runner.py`: a tool that calls `read_property_as_string` must declare `@uses_reflection` or fail review. Document "use typed getter when one exists" in the helper's docstring. | New tools land that bypass typed getters — caught at review by the lint rule. |
| 1.5 | `list_tools` snapshot drifts from FastMCP registry if a tool is registered after `_meta.py` runs. | L × L | Defer the snapshot until `_scan_tools_dir` completes; expose via a callable, not a frozen list. Re-snapshot on bridge restart. | `list_tools` count != `len(mcp._tools)` — assert at startup. |
| 1.6 | `BlueprintReadable` bypass via `FProperty::ExportText_InContainer` reads `EditDefaultsOnly` / `EditAnywhere` properties that the asset designer assumed were not Python-visible — minor security/expectation surprise on shared projects. | L × L | The plugin is editor-only and runs with full editor privileges already; this is no broader than `execute_unreal_python`. Document the helper's reach in [Docs/BobBotLib.md](Docs/BobBotLib.md). | None needed — informational. |
| 1.7 | Doc-drift continues mid-Sprint-1: the four catalogs disagree until the manifest fix lands, and the new tools added in this sprint themselves drift. | M × L | Phase 5 of Part 1 mandates updating all four in the same commit. CLAUDE.md gets a banner pointing to `list_tools` so the agent stops trusting the static lists. | A grep on the four files shows different counts — add a pre-commit grep check. |

### Sprint 2 risks (reliability + designer foundation)

| # | Risk | L × I | Mitigation | Detection |
|---|------|-------|------------|-----------|
| 2.1 | `FScopedTransaction` per chat turn collides with engine-internal transactions (asset save, compile-on-load) and produces malformed undo stacks. Corrupting the undo buffer is hard to recover from in-session. | M × H | Wrap only at the **C++ tool-call boundary** in `FBobBotPythonBridge::ExecPython*`, not around the chat send. Skip wrapping for read-only tools (already classified by `bAutoApproveReadOnly`). Add a feature flag `bob.transactions.enabled=true` so it can be killed without a rebuild. Test against asset-save, compile-on-load, world-partition save flows before merging. | Editor undo behaviour regresses — covered by a manual QA matrix added to `Docs/QA-checklist.md`. |
| 2.2 | One transaction per chat turn becomes huge for long agentic sessions (100+ tool calls), exhausting transaction memory or making undo unbearably slow. | M × M | Cap transactions at N steps (configurable, default 25); when exceeded, close the current transaction and open a new one. Surface "Transaction split here" in the History tab. | Memory stats `unreal.SystemLibrary.get_memory_stats` jumps; UE log warns "Transaction buffer nearing limit." |
| 2.3 | Activity log JSONL grows unbounded, eventually filling `Saved/`. | L × M | Rotate at 50MB or 30 days, whichever first. Settings knob to disable. Compress on rotate. | `Saved/BobBot/activity.jsonl*` total size monitor shown in Connect tab. |
| 2.4 | Activity log captures tool *args* that include secrets (API keys passed to a custom tool, file contents in `import_asset`). | M × H | Tool registry declares `@sensitive_args=[...]` for tools whose params shouldn't be logged. Default-redact any param matching `*_key`, `*_token`, `password`, `secret` regardless of declaration. Audit existing tools for sensitivity at sprint start. | Code review checklist + a unit test that logs a known-sensitive call and asserts redaction. |
| 2.5 | Bridge test harness uses real ports and clashes with a running editor on the dev machine. | M × L | Tests bind to ephemeral ports (port 0). Use a fixture bridge instance, not the live one. | Test fails with "address in use" — caught immediately. |
| 2.6 | Per-tool timeout override is set too high in a tool that genuinely hangs, blocking the chat thread. | L × M | Bridge enforces a hard ceiling (300s) regardless of tool override. Long-running tools get a "still working" heartbeat back to the chat every 10s. | Chat thread shows "still working" but no progress for >5 minutes — promote to user-visible warning with cancel button. |
| 2.7 | Phase 4 typed write helpers (`AddBlackboardKey`, `AddInputContextMapping`) introduce data-shape bugs that corrupt assets (the very risk that motivated avoiding `set_editor_property` mutations). | L × H | Each typed write goes through the engine's existing add API where possible (`USkeleton::AddSocket()` style). For Blackboard specifically, use `UBlackboardData::AddKey()` if exposed; if not, ship the read-only Phase 4 first and defer writes to Sprint 3 with a separate review. | New tool runs against a test asset in `_test_runner.py`; before/after asset diff shows expected delta only. |

### Sprint 3 risks (designer UX)

| # | Risk | L × I | Mitigation | Detection |
|---|------|-------|------------|-----------|
| 3.1 | Right-click context menus pre-fill prompts that the agent then misinterprets and runs destructive tools — a designer who clicked "ask BobBot about this" gets surprise edits. | M × H | Context menus *always* drop into Plan mode for that turn, regardless of the user's default permission setting. The pre-filled prompt is read-only by default; user must explicitly switch to AskBeforeEdits. | First-run flow shows a one-time tooltip explaining the behavior. |
| 3.2 | "Audit map perf" recipe walks every actor and runs the new summary tools, exceeding the cost budget on a large map. | M × M | Recipes declare an estimated cost; the recipe panel shows an estimate before launch and refuses to start if `MaxBudgetUsd` would be exceeded. Bound the walk by a configurable `MaxActors` (default 500) with "continue" button if the user wants more. | Cost estimator rejects the launch — designer sees the estimate up-front. |
| 3.3 | Visual diff requires a viewport capture before *and* after, doubling capture cost and tail latency. | M × M | Capture only on tools whose category includes "modify" + the affected area is in viewport (use `focus_on_actor` heuristic). Diff the two captures and only show the diff if non-trivial pixel delta exists. Cache "before" captures across multi-step turns. | Capture rate per tool tracked in activity log; alert if >2× expected. |
| 3.4 | Tool browser surfaces the full ~200 tools to designers, who pick destructive tools (`delete_asset`, `execute_unreal_python`) without understanding the consequences. | M × H | Hide `execute_unreal_python` and `set_*` tools behind an "Advanced" toggle by default. Each tool card shows its category badge (Read / Modify / Code) prominently. | Telemetry on tool selection, if/when added; until then, manual review of which tools designers reach for. |
| 3.5 | First-run sample chat hits a fresh project with no actors and produces an underwhelming demo, hurting first impression. | L × M | Sample chat checks for actors first; if none, switches to *"create a starter scene and audit it"* recipe. Always ends with a visible value moment (e.g. one flagged outlier). | QA on a fresh project before each release. |

### Backlog risks (worth flagging now even though shipped later)

| # | Risk | L × I | Mitigation | Detection |
|---|------|-------|------------|-----------|
| B.1 | Bridge transport consolidation (drop file-eval) breaks any external scripts that relied on the file path. | L × M | Add a deprecation log warning for one minor version, then remove. Document in CHANGELOG. | Field reports of broken external integrations. |
| B.2 | `UDeveloperSettings` migration loses settings for users with custom `BobBot.ini` files. | L × M | One-time migration shim on first launch reads the old INI and writes into the new settings; archives the old file as `BobBot.ini.bak`. | Connect tab shows "Settings migrated" notice once. |
| B.3 | Single-source tool manifest generation runs at editor startup and slows cold-start by O(scan time). | L × L | Cache the manifest hash on disk; regenerate only on tool-file mtime change. | Cold-start telemetry comparison. |
| B.4 | Persistent chat history exposes sensitive prompts/answers to anyone with disk access. | M × M | Encrypt at rest with a key derived from the user's OS keychain entry (same as API key). Provide "Clear history" button + auto-purge after configurable retention. | None initially — deliberate trust model. |
| B.5 | Naming-style guide deprecation pass breaks existing user macros / pre-recorded chats. | M × M | Two-version deprecation: old names log a warning + alias to new for one version, then remove in 2.0. | Telemetry on aliased-name calls; targeted outreach before removal. |

### Cross-cutting risks (apply to multiple sprints)

| # | Risk | L × I | Mitigation | Detection |
|---|------|-------|------------|-----------|
| X.1 | UE 5.5 / 5.6 release lands mid-implementation and shifts public-getter signatures (`UMaterial::GetExpressionCollection`, `UStaticMesh::GetSourceModels`). | M × M | `#if ENGINE_MINOR_VERSION` guards on the typed getters that wrap version-dependent APIs. Add a CI matrix run against the next UE preview when available. | Build break on UE preview. |
| X.2 | The plan's scope (Part 1 alone covers 11 subsystems + ~17 new C++ functions + 5 doc files) underestimates time, and Sprint 1 slips into Sprint 2. | M × M | Each phase of Part 1 is independently shippable. If Sprint 1 stalls, ship Phases 0+1+2 (the perf-critical subset that delivers the user's stated goal) and roll Phases 3+4 into Sprint 2. Keep the sprint-1 verification gate as the merge bar. | Daily sprint check; if Phase 2 isn't done by day 3 of 5, descope Phases 3+4. |
| X.3 | Live-coding during agent sessions corrupts BobBotLib state (already-flagged hot-reload concern). | M × M | Add `FCoreDelegates::OnPostHotReload` handler that calls `UBobBotLib::ClearTypeCache` and bumps a "session generation" counter; chat controller refuses to dispatch tools on stale generation until user acks. | Hot-reload triggered while a tool is mid-flight — surfaced as a chat warning. |
| X.4 | Plan changes accumulate technical debt in the existing tool files (rewrites + new helpers + transaction wrapping all touch the same files), making each subsequent change more conflict-prone. | M × M | Land Part 1 phases first as additive (new C++ + minimal Python rewrites). Defer transaction wrapping (Sprint 2) until after Part 1's Python rewrites merge. Avoid stacking unrelated edits in the same PR. | PR conflict rate; merge-queue stalls. |
| X.5 | The agent (BobBot itself) used during development relies on tools that are mid-rewrite — self-bootstrap risk where a broken tool kills the dev loop. | M × M | Keep `add_streaming_level` and the read-only chain (`get_*`) on the additive path — they keep working through Phase 1 even before old code is deleted. For each phase, ship the new tool *and* leave the old one functional until verification passes; only then remove. | Dev loop on the same machine as the changes; failures surface immediately. |
| X.6 | Unmitigated risks in this register itself — items rated L × L that turn out to be M+. | L × M | Re-rate after Sprint 1 retro using actual incidents. Treat the register as a living document, not a one-shot artifact. | Retro at sprint boundary. |

### Pre-flight gates

Before merging Part 1:
- Verification steps 1–3 must pass (build, reflection helpers, Phase 1 streaming).
- Risk 1.3 explicitly probed (struct-array binding) — Phases 2/3 blocked otherwise.
- All four tool catalogs synced for any new tools (Risk 1.7).
- Pre-commit grep gate on tool-count consistency.

Before merging Sprint 2:
- Risk 2.1 manual QA matrix run (transaction interaction with asset-save / compile / WP-save).
- Risk 2.4 redaction unit test green.
- Bridge test harness covers all five scenarios (Risk 2.5).

Before merging Sprint 3:
- Risk 3.1 plan-mode-by-default verified for context menus.
- Risk 3.4 advanced-toggle hides destructive tools by default.
- First-run sample chat tested on empty + populated projects (Risk 3.5).

---

## Part 4 — What's actually left + cutoff mitigation

This section is the post-Sprint-2 picture. Sprints 1 and 2 shipped the perf-inspection toolkit, atomic signals, activity log, bridge tests, per-tool timeouts. Real-world use surfaced two issues the prior plan didn't anticipate: silent agent stops mid-stream and visible socket timeouts on long audits. This Part covers both, plus the work explicitly carried over.

### 4.1 Cutoff failure modes — verified

Investigation of the output path between tool execution and the chat (see `Scripts/tools/_common.py`, `Scripts/bob_mcp_bridge.py`, `Content/Python/bob_sdk_events.py`, `Content/Python/bob_mcp_server.py`, `Source/BobBot/Private/BobBotChatController.cpp`) maps the truncation/stop layers as:

| # | Layer | File:Line | Behavior |
|---|---|---|---|
| 1 | UE-side stdout capture | [bob_mcp_server.py:67–104](Content/Python/bob_mcp_server.py) | unlimited |
| 2 | TCP recv loop | [bob_mcp_bridge.py:113–121](Scripts/bob_mcp_bridge.py) | unlimited; 65KB chunks until `\n` |
| 3 | `_exec` cap | [`_common.py:147–225`](Scripts/tools/_common.py) | **32KB hard cap** with visible truncation marker |
| 4 | Activity-log snapshot | [`_common.py:124–145`](Scripts/tools/_common.py) | 500 chars (diagnostic only — not the data path) |
| 5 | `_on_post_tool_use` hook (chat-UI events) | [bob_sdk_events.py:154–164](Content/Python/bob_sdk_events.py) | **2000 chars** — affects the chat-panel preview only; does NOT affect what the model sees |
| 6 | C++ chat history append | [`BobBotChatController.cpp::HandleStreamEvent_ToolResult`](Source/BobBot/Private/BobBotChatController.cpp) | unlimited |

Conclusions:
- The `[:2000]` slice in `_on_post_tool_use` is **chat-UI cosmetic only** (the user sees the truncated text in the chat panel for the tool-result card). The model receives the full result via the `UserMessage`/`tool_result` path. Not the cause of silent stops.
- Layer 3 (32KB cap) is visible to the agent, marker explicit. Not silent.
- The most likely real cause of "agent stops mid-stream" is **context-window saturation**: a deep audit calls 30–60 tools, each returning 5–30KB. Even with truncation markers, total context blows past the model budget; the model emits a stop without a final report. No error surfaces in the bridge or chat — it looks like a hang, then nothing.
- Secondary cause: bridge socket timeout (60s default, 120s in `ask_before_edits`). `audit_map_perf` on a 10k-actor map can exceed it. Already mitigatable via the `tool_timeout` decorator from Sprint 2 — but no audit tool has been decorated yet.

### 4.2 Structural refactor — one pass, all tools

User decision: principled refactor over patch. Migrate every tool to a structured envelope + registry-driven catalog. Cutoff issue resolves as a side effect because every output has a typed `summary` separate from its `data`, and oversized `data` always spills to a uniform path the agent fetches via one tool.

**Goal**: every tool result is a predictable, parseable envelope. Every tool's metadata (category, output kind, timeout, sensitive args) is data, not docstring. Doc drift eliminated. Hooks no longer slice silently.

#### 4.2.1 Output envelope

New schema, applied to every tool return:

```json
{
  "ok": true,
  "summary": "Top 3 outliers: M_UGW_Landscape (269 exprs OVERCOMPLEX); ... ",
  "data": { /* arbitrary structured payload */ },
  "spill_path": null,
  "error": null,
  "meta": { "duration_s": 1.23, "truncated": false, "tool": "audit_map_perf" }
}
```

Rules:
- `summary` is small, always present, human-readable. The chat-UI renders this.
- `data` is structured. Agent reads to drive decisions. No regex parsing.
- `spill_path` populated when serialized envelope exceeds the inline budget for the tool's `output_kind`. Full envelope written to `<BOB_PROJECT_ROOT>/Saved/BobBot/output_overflow/<ts>_<tool>.json`. The `data` field in the inline reply is replaced with `null` and `meta.truncated=true`.
- `error` populated on failure. `ok=false`. `summary` carries the human message.
- `meta.truncated` lets the agent know to fetch from `spill_path` if it wants the full `data`.

New helpers in [Scripts/tools/_common.py](Scripts/tools/_common.py):
- `envelope(summary, data=None, *, error=None, ok=True, tool_name=None)` — builds the dict.
- `_serialize_envelope(env, output_kind)` — measures, spills if oversized, returns the wire dict.
- `_OVERFLOW_DIR` reusing the same `_activity_log_path` directory pattern.

Replaces: `_MAX_OUTPUT_BYTES` magic threshold, the marker-string truncation path, and the activity-log 500-char snapshot (which records `summary` instead — always small, always accurate).

#### 4.2.2 Tool registry as data

New file [Scripts/tools/_registry.py](Scripts/tools/_registry.py):

```python
def bob_tool(*, category, output_kind="small", default_timeout=60, sensitive_args=()):
    """Decorator. Replaces bare @mcp.tool().
       output_kind: 'small' (≤4KB inline budget) | 'large' (≤16KB) | 'huge' (always spill).
       default_timeout: seconds; bridge clamps to [1, 300].
       sensitive_args: tuple of arg names to redact in activity log."""
```

Records metadata into `_TOOL_REGISTRY: dict[str, ToolSpec]` and applies `@mcp.tool()` underneath. Each tool's function gets wrapped so:
1. Args matching `sensitive_args` are redacted before logging.
2. Stdout from the tool body is captured and used as `summary` if the function returns a plain string or `None`. Tools that already return an envelope keep it.
3. `with_timeout(default_timeout)` is applied automatically. Per-call override still works.
4. Activity log records `category`, `output_kind`, `truncated`, `spill_path` alongside `summary`.

Migration: every `@mcp.tool()` call site across the 50 tool files becomes `@bob_tool(category=..., output_kind=...)`. Mechanical. Pre-existing print-style tools keep working — stdout becomes `summary`, `data` defaults to `None`. High-value tools (perf audit set, list_tools, get_*_info readers) get explicit `data` payloads with structure the agent can consume programmatically.

#### 4.2.3 Single tool manifest — kills 4-way doc drift

At bridge startup, after `_register_all_tools()`, write `<BOB_PROJECT_ROOT>/Saved/BobBot/.tool_manifest.json` containing every entry from `_TOOL_REGISTRY`. Format:

```json
[
  {"name":"audit_map_perf","category":"Perf Audit","params":["max_meshes","heavy_tris",...],"output_kind":"huge","timeout":180,"doc":"Walk the open level..."},
  ...
]
```

Consumers:
- [Scripts/tools/meta.py::list_tools](Scripts/tools/meta.py) — already reads from FastMCP registry; switch to read `_TOOL_REGISTRY` directly so output_kind/timeout/category travel with the listing.
- [Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp](Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp) — replace the hand-maintained `GToolCategories[]` static with a load of the JSON manifest at editor startup. Fall back to "manifest not found — restart bridge" message rather than crashing.
- New `Scripts/build_docs.py` — reads the manifest, emits the per-category sections of [Docs/ToolReference.md](Docs/ToolReference.md), the `## Tools (NNN)` section of [CLAUDE.md](CLAUDE.md), and the count line in [README.md](README.md). Run as a pre-commit hook. CI fails if regenerated content differs from committed content.

Net effect: the four-place edit becomes a single `bob_tool(...)` change. Doc drift impossible.

#### 4.2.4 Bridge & chat-controller updates

- [Scripts/bob_mcp_bridge.py](Scripts/bob_mcp_bridge.py) and [Scripts/bob_mcp_bridge_http.py](Scripts/bob_mcp_bridge_http.py) — no protocol change; they already pass through whatever the tool returns. The wire format is now JSON envelope instead of plain string. They serialize as before.
- [Content/Python/bob_sdk_events.py:154–164](Content/Python/bob_sdk_events.py) `_on_post_tool_use` — drop the `[:2000]` slice. If the tool_response is a JSON envelope, store `envelope.summary` (small by construction) into the `hook_tool_result` event. If it's a legacy string (shouldn't happen post-migration), keep up to 4KB with explicit marker.
- [Source/BobBot/Private/BobBotChatController.cpp::HandleStreamEvent_ToolResult](Source/BobBot/Private/BobBotChatController.cpp) — try-parse the incoming output as JSON. If envelope, render `summary` as the message body and stash `data` in a collapsible "Details" disclosure (renders as JSON tree or a table when `data` is a list of dicts). If parse fails, render verbatim (back-compat path; unreachable post-migration).

#### 4.2.5 Overflow fetch tool

New `Scripts/tools/overflow.py`:
- `read_overflow(spill_path: str, offset: int = 0, max_bytes: int = 8000) -> envelope` — reads the spilled JSON file in slices. Returns `data={"chunk": "...", "eof": true|false, "size": N}`. The agent navigates without ever holding the whole thing in context.

Single tool, single protocol. Agent's prompt template tells it: "if `meta.truncated=true`, call `read_overflow(spill_path)` until `data.eof=true`."

#### 4.2.6 Migration scope — every tool, one pass

50 tool files in [Scripts/tools/](Scripts/tools/). Each gets:
1. `@mcp.tool()` → `@bob_tool(category=..., output_kind=..., default_timeout=...)`.
2. Output review: high-value tools (perf-audit set, get_*_info readers, audit_map_perf, list_tools, get_lightmap_density_summary, get_foliage_density_report, get_actor_perf_signal, get_lod_summary, get_material_complexity, get_niagara_summary, get_streaming_levels, get_lod_info, get_static_mesh_info, get_skeleton_info, get_blackboard_keys, get_input_context_mappings, get_landscape_info, get_widget_tree, get_sequence_info, get_sequence_tracks, get_blueprint_functions, get_blueprint_components) return explicit `envelope(summary, data)` instead of printing.
3. Print-style write tools (`spawn_actor`, `set_*`, `delete_*`, `create_*`) keep print → captured-as-summary. `data` stays `None`. These are status-line returns; no structured payload makes sense.

Mechanical part is the decorator swap (~50 files, ~5 lines each). High-value rewrites are ~25 reader tools. Estimate 1.5–2 days for one engineer.

Critical files (modify):
- [Scripts/tools/_common.py](Scripts/tools/_common.py) — envelope helpers, spill helpers, drop `_MAX_OUTPUT_BYTES`, `_log_activity` records `summary`/`spill_path` not raw output snippet.
- [Scripts/tools/_registry.py](Scripts/tools/_registry.py) — new file, `bob_tool` + `_TOOL_REGISTRY`.
- [Scripts/tools/meta.py](Scripts/tools/meta.py) — switch to `_TOOL_REGISTRY`.
- [Scripts/tools/overflow.py](Scripts/tools/overflow.py) — new, `read_overflow`.
- All 50 [Scripts/tools/*.py](Scripts/tools/) — decorator swap + envelope where high-value.
- [Scripts/bob_mcp_bridge.py](Scripts/bob_mcp_bridge.py), [Scripts/bob_mcp_bridge_http.py](Scripts/bob_mcp_bridge_http.py) — emit manifest at startup.
- [Content/Python/bob_sdk_events.py](Content/Python/bob_sdk_events.py) — drop `[:2000]`, summary-aware.
- [Source/BobBot/Private/BobBotChatController.cpp](Source/BobBot/Private/BobBotChatController.cpp) — envelope rendering.
- [Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp](Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp) — read manifest, drop `GToolCategories[]`.
- New [Scripts/build_docs.py](Scripts/build_docs.py) — generates doc sections from manifest.
- Doc files: keep the prose (Quickstart, Configuration, Architecture). Replace tool-list sections with `<!-- AUTOGEN START --> ... <!-- AUTOGEN END -->` markers populated by the build script.

#### 4.2.7 Texture-size aggregator (folded into the migration)

Originally a carryover; build it on top of the new envelope from day one. New tool `audit_textures` in [Scripts/tools/texture_audit.py](Scripts/tools/texture_audit.py). Walks materials referenced by in-use meshes, follows texture parameters, rolls up dimensions and compressed size. Returns `envelope(summary="Top 5 textures by bytes: ...", data={"textures":[{path, w, h, format, bytes, refs}], "totals":{...}})`. `output_kind="large"` so it always spills if the texture set is huge.

#### 4.2.8 Execution order (single PR, logical phases)

1. Envelope + registry helpers + `bob_tool` decorator. Stdout-capture fallback. Tests.
2. Bridge manifest emission + `meta.list_tools` switched to registry.
3. `bob_sdk_events.py` summary-aware. C++ `HandleStreamEvent_ToolResult` envelope rendering with structured `data` panel.
4. Tool migration: decorator swap on all 50 files. Re-run bridge tests.
5. High-value reader-tool rewrites to explicit envelopes (the ~25 tools above).
6. `read_overflow` tool. Spill-path agent prompt template.
7. `Scripts/build_docs.py` + autogen markers in CLAUDE.md / README.md / Docs/ToolReference.md.
8. SBobBotInfoTab manifest reader.
9. Add `audit_textures`.
10. Verification (4.7 below).

### 4.4 Sprint 3 — Designer UX (5 items, planned earlier, status unchanged)

Carried over verbatim from the original plan. None started.

1. **Right-click context menus** — actor outliner / content browser / output log → "Ask BobBot about this". Pre-fills chat with a structured prompt referencing the right-clicked object. Drops to Plan mode by default (Risk 3.1). Implementation: `FToolMenuOwner` registration in `FBobBotModule::StartupModule()`.
2. **Recipes section** in Info tab — one-click buttons backed by Sprint 1/2 perf tools. First three recipes: *Audit map perf* (calls `audit_map_perf` + cross-checks with the atomic getters), *Find unused assets*, *Suggest LOD chains for selected meshes*. Each recipe declares an estimated cost ceiling (Risk 3.2).
3. **Visual diff in approval UI** — capture before + after viewport on tools tagged `modify`. Diff and only show if non-trivial pixel delta. Cache before-captures within a turn (Risk 3.3).
4. **Tool browser/search** in Info tab — backed by `list_tools` from Phase 0. Hide `execute_unreal_python` and `set_*` behind an Advanced toggle by default (Risk 3.4).
5. **First-run sample chat** — drop into a guided "first chat" after FTUE completion. Sample prompt: *"Audit this map and tell me the three most expensive things"*. Falls back to *"create a starter scene and audit it"* on empty maps (Risk 3.5).

Critical files (per item, condensed):
- New `FToolMenuOwner` wiring in [Source/BobBot/Private/BobBot.cpp](Source/BobBot/Private/BobBot.cpp) `StartupModule`.
- [Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp](Source/BobBot/Private/Widgets/SBobBotInfoTab.cpp) — Recipes section + Tool browser/search UI.
- [Source/BobBot/Private/BobBotChatController.cpp](Source/BobBot/Private/BobBotChatController.cpp) — approval-card visual diff.
- [Source/BobBot/Private/Widgets/SBobBotWelcomeTab.cpp](Source/BobBot/Private/Widgets/SBobBotWelcomeTab.cpp) — first-run sample chat trigger.

### 4.5 Backlog (deferred — pick up after Sprint 3)

Status unchanged from earlier plan. No work yet on:
- `FScopedTransaction` per chat turn + History tab (the highest-impact reliability item but the riskiest one — deliberately deferred until Risk 2.1 QA matrix is run).
- `UDeveloperSettings` migration.
- Single-source tool manifest (kills `GToolCategories[]` drift).
- Bridge transport consolidation (drop file-eval).
- Structured logging (`bob_log` JSON events).
- In-editor `bob_create_tool`.
- CI: test runner + version bump + CHANGELOG enforcement.
- Persistent chat history.
- Naming style guide + 2.0 deprecation pass.

### 4.6 Recommended execution order

1. **Refactor (4.2)** — single PR, single rebuild. ~1.5–2 days. Lands envelope, registry, manifest, doc autogen, tool migration, overflow fetch, texture aggregator. Cutoff issue resolves as a side effect.
2. **Designer UX Sprint 3 (4.4)** — multi-day, after refactor. Right-click menus first (highest adoption), recipes second. Recipes can read tool metadata from the new manifest (categories, output_kind) for richer UI.
3. **`FScopedTransaction` + History tab** — only after Risk 2.1 QA matrix is run. History tab now reads `data` envelopes from activity log, not raw text — much better surface than originally planned.
4. **Remaining backlog (4.5)** — opportunistic.

### 4.7 Verification

Refactor:
- `list_tools` returns ~214 entries with `category`, `output_kind`, `default_timeout` populated. None marked `category=""`.
- A small tool (`ping_unreal`, `get_current_level`) returns `{ok:true, summary:"...", data:null}` inline; no spill path.
- A large tool (`audit_map_perf` on BR_2) returns `{ok:true, summary:"4-line headline", data:null, spill_path:".../audit_map_perf_*.json", meta:{truncated:true}}`. Confirm the spill file exists and contains the full structured `data`.
- `read_overflow(spill_path)` returns `{ok:true, data:{chunk:..., eof:false, size:N}}`. Repeating with `offset` advances; final call returns `eof=true`.
- C++ chat panel renders `summary` as the message body and shows the structured `data` (table or JSON tree) under a collapsible Details disclosure. Confirm by triggering `audit_map_perf` from BobBot's in-editor chat.
- `_on_post_tool_use` event in the chat shows `summary`, not a raw 2000-char slice. Confirm by inspecting `_stream_events` payloads via debug log.
- `Saved/BobBot/.tool_manifest.json` exists after bridge startup and matches `_TOOL_REGISTRY`.
- SBobBotInfoTab's tool tab is populated from the manifest (not the deleted `GToolCategories[]`). Restart with manifest deleted: tab shows "manifest not found — restart bridge" and does not crash.
- Run `python Scripts/build_docs.py` — diff against committed `Docs/ToolReference.md`, `CLAUDE.md`, `README.md` is empty. Edit one tool's `bob_tool(category=...)`; rerun; diff is exactly the expected change.
- All five bridge tests still pass (`python Scripts/tests/test_bridge.py`).
- `audit_textures` on BR_2: top-5 textures by bytes match the manual eyeball of the texture pool (`get_texture_pool_status` → output log → `Memreport`).
- Activity log records `summary`/`spill_path`/`category`/`truncated` fields.
- 30 sequential `get_actor_perf_signal` calls on BR_2 actors: agent completes a final summary message. No silent stop.

Sprint 3 designer UX (per-item, when reached):
- Right-click context menu launches chat with pre-filled prompt; chat lands in Plan mode regardless of default permission setting.
- "Audit map perf" recipe runs end-to-end. Recipe panel shows estimated cost from manifest (per-tool `output_kind` weighted).
- Visual diff approval card shows before/after on a `spawn_actor` invocation.

### 4.8 New risks introduced by Part 4

| # | Risk | L × I | Mitigation | Detection |
|---|------|-------|------------|-----------|
| 4.1 | Spill files accumulate without bound, fill `Saved/`. | M × M | Reuse the activity-log rotation pattern: cap `Saved/BobBot/output_overflow/` at 200MB, prune oldest first. Settings knob to disable spilling entirely. | Connect tab shows `Saved/BobBot/output_overflow` byte total. |
| 4.2 | Spilled JSON exposes secrets a tool dumped (env values, file contents). | L × M | Apply existing `_redact` regex to `summary` + serialized `data` before spill. Document that redaction is best-effort. Tool authors can declare `sensitive_args=("api_key",)` on `bob_tool` for deterministic redaction. | Unit test: tool with `api_key` arg, assert redaction in spill file. |
| 4.3 | Agent loops on `read_overflow` chasing data that isn't there. | M × M | `data.eof=true` when slice reaches file end. Agent prompt template explicitly says: "stop when eof". | Activity-log trace shows repeat `read_overflow` calls — alert. |
| 4.4 | Refactor breaks one of the 50 tool files; missed during one-pass migration. | M × H | (a) Bridge test harness exercises a representative tool from each category. (b) `_register_all_tools()` logs every tool that fails to load — fail-loud at bridge startup. (c) Pre-merge: smoke each tool category with `list_tools(category=...)` to ensure all show up. | Tool count drops in `list_tools` after refactor. |
| 4.5 | C++ chat panel chokes on huge `data` payloads when it renders the JSON tree. | M × M | Tree widget paginates / virtualizes. If `len(data) > 100KB` (already truncated by spill), render `data` as "spilled to disk; use read_overflow" only. | Editor frame stats spike on tool-result render. |
| 4.6 | Doc autogen overwrites human edits in the prose sections. | L × M | Autogen only inside `<!-- AUTOGEN START --> / <!-- AUTOGEN END -->` markers. Pre-commit hook errors if a hand edit drifts inside the markers. | CI failure on tool-list diff. |
| 4.7 | Manifest file path tied to `BOB_PROJECT_ROOT`; CI / external clients without the env var don't get a manifest. | L × L | Bridge falls back to printing the manifest to stderr at startup if it can't write. SBobBotInfoTab's "manifest not found" message points the user to restart. | Bridge log on startup. |
| 4.8 | `audit_map_perf` orchestrator now duplicates atomic-getter logic if not refactored to a façade. | M × M | Explicitly rewrite `audit_map_perf` to call the atomic getters and concatenate their `data` payloads. Single code path. | Code review of `perf_audit.py` post-migration. |
| 4.9 | Stdout-capture fallback for `summary` produces multi-MB summaries when a tool prints in a hot loop (existing tools that print one line per actor). | M × M | Stdout-capture is opt-in: tools without explicit `summary=` and without explicit return get a stdout-driven summary capped at 4KB with "[... truncated, full output in spill]" marker. The full output goes to `data["stdout"]` which spills normally. | Audit log shows `meta.summary_truncated` field; spot-check during migration. |

### 4.9 Pre-flight gates

Before merging Part 4 (refactor):
- All five bridge tests pass against the new envelope + registry path.
- `list_tools` returns ≥214 entries; no `category=""`.
- A representative tool from each of the ~25 categories (`Actors`, `Materials`, `Niagara`, `Sequencer`, `LOD`, `Foliage`, `Landscape`, `Perf Audit`, `Meta`, etc.) executes end-to-end with envelope output. Activity log records each.
- `Scripts/build_docs.py` round-trip clean: regenerate produces empty diff against committed docs.
- BR_2 `audit_map_perf` produces a `summary` ≤4KB inline + a populated `spill_path`. `read_overflow` retrieves the full payload across multiple slices, last slice has `eof=true`.
- C++ chat panel renders `summary` + `data` Details panel without throwing. Verify on an empty `data`, a list-of-dicts `data`, and a deeply-nested `data`.
- Risk 4.4 mitigation in place: bridge logs failed tools at startup and `list_tools` count is asserted ≥214 in a smoke test.
- Risk 4.6 markers present in [CLAUDE.md](CLAUDE.md), [README.md](README.md), [Docs/ToolReference.md](Docs/ToolReference.md). Pre-commit hook catches edits inside markers.

Before merging Part 4 designer UX:
- Same gates as Sprint 3 in Part 3 (3.1 plan-mode default, 3.4 advanced toggle, 3.5 first-run on empty + populated maps).
- Recipes UI reads from the manifest, not from a hardcoded list.

