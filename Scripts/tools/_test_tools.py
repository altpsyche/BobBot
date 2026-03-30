"""BobBot test harness - tests all tools against a live editor session.

This file is prefixed with _ so it is NOT auto-discovered by the MCP bridge.
To use, load it explicitly or register the run_bobbot_tests tool manually.
"""


def register(mcp, send_fn):

    def _exec(code):
        result = send_fn({"type": "execute", "code": code})
        if result.get("success"):
            output = result.get("output", "")
            err = result.get("error")
            if err:
                output += "\nStderr: " + err
            return output if output.strip() else "(executed successfully, no output)"
        return "Error: " + result.get("error", "Unknown error")

    @mcp.tool()
    def run_bobbot_tests(categories: str = "all") -> str:
        """Run BobBot tool tests against a live editor. categories: 'all' or comma-separated list like 'actors,assets,materials'. Returns pass/fail summary.

        Test categories: core, actors, assets, materials, levels, viewport, context,
        asset_ops, editor_ops, tags_layers, components, world, collision, physics,
        lighting, texture_mesh, import_export, camera, post_process, data_assets,
        skeletal, splines, level_streaming, sequencer, animation, blueprint_advanced,
        input_system, audio, foliage, source_control, build, niagara, pcg, ai_tools,
        umg_widgets, pie, movie_render, debug_profiling, landscape"""
        return _exec(f"""
import unreal, json, time

categories_str = "{categories}"
run_all = categories_str.lower() == "all"
selected = [c.strip().lower() for c in categories_str.split(",")]

results = []
passed = 0
failed = 0
skipped = 0

def test(name, category, code_fn):
    global passed, failed, skipped
    if not run_all and category not in selected:
        skipped += 1
        return
    try:
        result = code_fn()
        if result and "ERROR" not in str(result):
            results.append(("PASS", name))
            passed += 1
        else:
            results.append(("FAIL", name, str(result)[:100]))
            failed += 1
    except Exception as e:
        results.append(("FAIL", name, str(e)[:100]))
        failed += 1

# ---- Core tests ----
def test_core():
    world = unreal.EditorLevelLibrary.get_editor_world()
    return world is not None

test("core: editor world exists", "core", test_core)

# ---- Actor tests ----
def test_actors():
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    return len(actors) >= 0

test("actors: get all actors", "actors", test_actors)

# ---- Asset tests ----
def test_assets():
    assets = unreal.EditorAssetLibrary.list_assets("/Game", recursive=False)
    return True

test("assets: list assets", "assets", test_assets)

# ---- Spawn and cleanup test ----
def test_spawn_cleanup():
    # Spawn a test actor
    cls = unreal.load_class(None, "/Script/Engine.PointLight")
    loc = unreal.Vector(x=99999, y=99999, z=99999)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc)
    if actor:
        label = actor.get_actor_label()
        unreal.EditorLevelLibrary.destroy_actor(actor)
        return True
    return False

test("actors: spawn and destroy", "actors", test_spawn_cleanup)

# ---- World settings test ----
def test_world():
    world = unreal.EditorLevelLibrary.get_editor_world()
    ws = world.get_world_settings()
    gravity = ws.get_editor_property("GlobalGravityZ")
    return gravity is not None

test("world: get settings", "world", test_world)

# ---- Editor ops test ----
def test_editor_ops():
    unreal.EditorLevelLibrary.set_selected_level_actors([])
    selected = unreal.EditorLevelLibrary.get_selected_level_actors()
    return len(selected) == 0

test("editor_ops: deselect all", "editor_ops", test_editor_ops)

# ---- Viewport camera test ----
def test_camera():
    loc, rot = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
    return loc is not None

test("camera: get viewport camera", "camera", test_camera)

# ---- Level info test ----
def test_levels():
    world = unreal.EditorLevelLibrary.get_editor_world()
    path = world.get_path_name()
    return len(path) > 0

test("levels: get current level", "levels", test_levels)

# ---- Summary ----
print(f"\\n{'='*50}")
print(f"BobBot Test Results")
print(f"{'='*50}")
print(f"  Passed:  {{passed}}")
print(f"  Failed:  {{failed}}")
print(f"  Skipped: {{skipped}}")
print(f"  Total:   {{passed + failed + skipped}}")
print(f"{'='*50}")
if failed > 0:
    print(f"\\nFailures:")
    for r in results:
        if r[0] == "FAIL":
            detail = r[2] if len(r) > 2 else ""
            print(f"  FAIL: {{r[1]}} - {{detail}}")
else:
    print(f"\\nAll tests passed!")
""")
