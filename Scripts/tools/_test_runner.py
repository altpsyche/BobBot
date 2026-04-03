"""BobBot smoke test runner — called by /test slash command.

Runs directly inside UE Python (no MCP bridge needed).
Writes results to Saved/BobBot/_test_results.json.
"""

import unreal
import json
import os
import time


def run_tests(categories="all"):
    """Run smoke tests and return results dict."""
    run_all = categories.lower() == "all"
    selected = [c.strip().lower() for c in categories.split(",")]

    results = []
    passed = 0
    failed = 0
    skipped = 0
    start_time = time.time()

    def test(name, category, code_fn):
        nonlocal passed, failed, skipped
        if not run_all and category not in selected:
            skipped += 1
            return
        try:
            result = code_fn()
            if result:
                results.append({"status": "PASS", "name": name})
                passed += 1
            else:
                results.append({"status": "FAIL", "name": name, "detail": "returned falsy"})
                failed += 1
        except Exception as e:
            results.append({"status": "FAIL", "name": name, "detail": str(e)[:200]})
            failed += 1

    # ---- Core ----
    test("core: editor world exists", "core",
         lambda: unreal.EditorLevelLibrary.get_editor_world() is not None)

    test("core: project info available", "core",
         lambda: bool(unreal.SystemLibrary.get_project_directory()))

    # ---- Actors ----
    test("actors: get all actors", "actors",
         lambda: isinstance(unreal.EditorLevelLibrary.get_all_level_actors(), list))

    def _spawn_destroy():
        cls = unreal.load_class(None, "/Script/Engine.PointLight")
        loc = unreal.Vector(x=99999, y=99999, z=99999)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc)
        if actor:
            unreal.EditorLevelLibrary.destroy_actor(actor)
            return True
        return False
    test("actors: spawn and destroy", "actors", _spawn_destroy)

    # ---- Assets ----
    test("assets: list assets", "assets",
         lambda: unreal.EditorAssetLibrary.list_assets("/Game", recursive=False) is not None)

    test("assets: asset registry", "assets",
         lambda: unreal.AssetRegistryHelpers.get_asset_registry() is not None)

    # ---- World ----
    def _world_settings():
        world = unreal.EditorLevelLibrary.get_editor_world()
        ws = world.get_world_settings()
        return ws.get_editor_property("GlobalGravityZ") is not None
    test("world: get settings", "world", _world_settings)

    # ---- Editor ops ----
    def _deselect():
        unreal.EditorLevelLibrary.set_selected_level_actors([])
        return len(unreal.EditorLevelLibrary.get_selected_level_actors()) == 0
    test("editor_ops: deselect all", "editor_ops", _deselect)

    # ---- Camera ----
    def _camera():
        loc, rot = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
        return loc is not None
    test("camera: get viewport camera", "camera", _camera)

    # ---- Levels ----
    def _levels():
        world = unreal.EditorLevelLibrary.get_editor_world()
        return len(world.get_path_name()) > 0
    test("levels: get current level", "levels", _levels)

    # ---- BobBotLib ----
    def _bobbot_lib():
        return hasattr(unreal, 'BobBotLib')
    test("bobbot_lib: module available", "bobbot_lib", _bobbot_lib)

    def _bobbot_lib_compile():
        # Just check the function exists, don't actually compile
        return hasattr(unreal.BobBotLib, 'compile_blueprint')
    test("bobbot_lib: compile_blueprint exists", "bobbot_lib", _bobbot_lib_compile)

    # ---- Bridge (via env vars) ----
    def _bridge_env():
        return bool(os.environ.get('BOB_MCP_PORT'))
    test("bridge: environment configured", "bridge", _bridge_env)

    elapsed = time.time() - start_time

    return {
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "total": passed + failed + skipped,
        "elapsed_ms": int(elapsed * 1000),
        "results": results,
    }


def run_and_save(categories="all"):
    """Run tests and write results JSON to Saved/BobBot/_test_results.json."""
    result = run_tests(categories)
    out_dir = os.path.join(os.environ.get('BOB_PROJECT_ROOT', '.'), 'Saved', 'BobBot')
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, '_test_results.json')
    with open(out_path, 'w') as f:
        json.dump(result, f)
    return result
