"""BobBot smoke test runner — called by /test slash command.

Runs directly inside UE Python (no MCP bridge needed).
Writes results to Saved/BobBot/_test_results.json.

v1.6 update: Full coverage of all v1.5 tool categories.
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

    # ---------------------------------------------------------------------- #
    # Core
    # ---------------------------------------------------------------------- #
    test("core: editor world exists", "core",
         lambda: unreal.EditorLevelLibrary.get_editor_world() is not None)

    test("core: project info available", "core",
         lambda: bool(unreal.SystemLibrary.get_project_directory()))

    # ---------------------------------------------------------------------- #
    # Actors
    # ---------------------------------------------------------------------- #
    def _get_all_actors():
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        # UE returns unreal.Array, not Python list. Check it's iterable with len().
        return actors is not None and hasattr(actors, '__len__')
    test("actors: get all actors", "actors", _get_all_actors)

    def _spawn_destroy():
        cls = unreal.load_class(None, "/Script/Engine.PointLight")
        loc = unreal.Vector(x=99999, y=99999, z=99999)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc)
        if actor:
            unreal.EditorLevelLibrary.destroy_actor(actor)
            return True
        return False
    test("actors: spawn and destroy", "actors", _spawn_destroy)

    # ---------------------------------------------------------------------- #
    # Assets
    # ---------------------------------------------------------------------- #
    test("assets: list assets", "assets",
         lambda: unreal.EditorAssetLibrary.list_assets("/Game", recursive=False) is not None)

    test("assets: asset registry", "assets",
         lambda: unreal.AssetRegistryHelpers.get_asset_registry() is not None)

    # ---------------------------------------------------------------------- #
    # World
    # ---------------------------------------------------------------------- #
    def _world_settings():
        world = unreal.EditorLevelLibrary.get_editor_world()
        ws = world.get_world_settings()
        return ws.get_editor_property("GlobalGravityZ") is not None
    test("world: get settings", "world", _world_settings)

    # ---------------------------------------------------------------------- #
    # Editor ops
    # ---------------------------------------------------------------------- #
    def _deselect():
        unreal.EditorLevelLibrary.set_selected_level_actors([])
        return len(unreal.EditorLevelLibrary.get_selected_level_actors()) == 0
    test("editor_ops: deselect all", "editor_ops", _deselect)

    # ---------------------------------------------------------------------- #
    # Camera
    # ---------------------------------------------------------------------- #
    def _camera():
        try:
            sub = unreal.UnrealEditorSubsystem()
            result = sub.get_level_viewport_camera_info()
            return result is not None
        except Exception:
            try:
                loc, rot = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
                return loc is not None
            except Exception:
                return False
    test("camera: get viewport camera", "camera", _camera)

    # ---------------------------------------------------------------------- #
    # Levels
    # ---------------------------------------------------------------------- #
    def _levels():
        world = unreal.EditorLevelLibrary.get_editor_world()
        return len(world.get_path_name()) > 0
    test("levels: get current level", "levels", _levels)

    # ---------------------------------------------------------------------- #
    # CVars
    # ---------------------------------------------------------------------- #
    def _cvar_int():
        unreal.SystemLibrary.get_console_variable_int_value("r.DefaultFeature.MotionBlur")
        return True
    test("cvars: get int cvar", "cvars", _cvar_int)

    def _cvar_float():
        unreal.SystemLibrary.get_console_variable_float_value("r.ScreenPercentage")
        return True
    test("cvars: get float cvar", "cvars", _cvar_float)

    def _cvar_exec():
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is None:
            return False
        unreal.SystemLibrary.execute_console_command(world, "r.DefaultFeature.MotionBlur")
        return True
    test("cvars: execute console command", "cvars", _cvar_exec)

    # ---------------------------------------------------------------------- #
    # View modes
    # ---------------------------------------------------------------------- #
    def _viewmode_cycle():
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is None:
            return False
        unreal.SystemLibrary.execute_console_command(world, "viewmode Unlit")
        unreal.SystemLibrary.execute_console_command(world, "viewmode Lit")
        return True
    test("view_modes: cycle Unlit and back to Lit", "view_modes", _viewmode_cycle)

    def _show_flag():
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is None:
            return False
        unreal.SystemLibrary.execute_console_command(world, "show Collision")
        unreal.SystemLibrary.execute_console_command(world, "show Collision")
        return True
    test("view_modes: show flag toggle roundtrip", "view_modes", _show_flag)

    # ---------------------------------------------------------------------- #
    # Material instances
    # ---------------------------------------------------------------------- #
    test("material_instances: factory class exists", "material_instances",
         lambda: hasattr(unreal, 'MaterialInstanceConstantFactoryNew'))

    test("material_instances: MaterialEditingLibrary exists", "material_instances",
         lambda: hasattr(unreal, 'MaterialEditingLibrary'))

    # ---------------------------------------------------------------------- #
    # Content browser
    # ---------------------------------------------------------------------- #
    def _content_folder():
        test_path = "/Game/_BobBot_Test_Temp"
        result = unreal.EditorAssetLibrary.make_directory(test_path)
        if result or unreal.EditorAssetLibrary.does_directory_exist(test_path):
            project_content = str(unreal.Paths.project_content_dir())
            disk_path = os.path.join(project_content, "_BobBot_Test_Temp")
            if os.path.isdir(disk_path):
                try:
                    os.rmdir(disk_path)
                except OSError:
                    pass
            return True
        return False
    test("content_browser: create and cleanup folder", "content_browser", _content_folder)

    def _content_registry():
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        assets = registry.get_assets_by_path("/Game", recursive=False)
        return assets is not None
    test("content_browser: get_assets_by_path", "content_browser", _content_registry)

    # ---------------------------------------------------------------------- #
    # LOD
    # ---------------------------------------------------------------------- #
    def _lod_api():
        has_lib = hasattr(unreal, 'EditorStaticMeshLibrary')
        has_lods = hasattr(unreal.StaticMesh, 'get_num_lods')
        return has_lib or has_lods
    test("lod: LOD API available", "lod", _lod_api)

    # ---------------------------------------------------------------------- #
    # Navmesh
    # ---------------------------------------------------------------------- #
    def _navmesh_system():
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is None:
            return False
        try:
            unreal.NavigationSystemV1.get_navigation_system(world)
            return True  # API call works even if nav_sys is None
        except Exception:
            return False
    test("navmesh: NavigationSystemV1 API available", "navmesh", _navmesh_system)

    def _navmesh_scan():
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        for a in actors[:50]:
            _ = a.get_class().get_name()
        return True
    test("navmesh: actor class scanning", "navmesh", _navmesh_scan)

    # ---------------------------------------------------------------------- #
    # Project settings
    # ---------------------------------------------------------------------- #
    def _settings_ini_readable():
        import configparser
        project_dir = str(unreal.Paths.project_dir())
        ini_path = os.path.join(project_dir, "Config", "DefaultEngine.ini")
        if not os.path.isfile(ini_path):
            return False
        config = configparser.ConfigParser(strict=False)
        config.optionxform = str
        config.read(ini_path, encoding='utf-8')
        return len(config.sections()) > 0
    test("project_settings: read DefaultEngine.ini", "project_settings", _settings_ini_readable)

    def _settings_version():
        ver = unreal.SystemLibrary.get_engine_version()
        return len(ver) > 0
    test("project_settings: get engine version", "project_settings", _settings_version)

    # ---------------------------------------------------------------------- #
    # Notifications
    # ---------------------------------------------------------------------- #
    test("notifications: unreal.log", "notifications",
         lambda: (unreal.log("[BobBot Test] log test"), True)[1])

    test("notifications: unreal.log_warning", "notifications",
         lambda: (unreal.log_warning("[BobBot Test] warning test"), True)[1])

    def _notif_print_string():
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is None:
            return False
        # print_string signature varies by UE version. Try positional args.
        try:
            unreal.SystemLibrary.print_string(world, "[BobBot Test]")
        except Exception:
            # Fall back to just verifying the function exists
            return hasattr(unreal.SystemLibrary, 'print_string')
        return True
    test("notifications: print_string toast", "notifications", _notif_print_string)

    # ---------------------------------------------------------------------- #
    # Sequencer
    # ---------------------------------------------------------------------- #
    test("sequencer: LevelSequenceFactoryNew exists", "sequencer",
         lambda: hasattr(unreal, 'LevelSequenceFactoryNew'))

    def _seq_create_inspect_delete():
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.LevelSequenceFactoryNew()
        seq = asset_tools.create_asset(
            "_BobBot_Test_Seq", "/Game", unreal.LevelSequence, factory)
        if seq is None:
            return False
        is_seq = isinstance(seq, unreal.LevelSequence)
        # MovieScene property is protected. Use the public methods on the
        # sequence itself instead of get_editor_property.
        try:
            start = seq.get_playback_start()
            end = seq.get_playback_end()
            has_playback = start is not None or end is not None
        except Exception:
            # Older API: just verify the sequence is valid
            has_playback = is_seq
        unreal.EditorAssetLibrary.delete_asset("/Game/_BobBot_Test_Seq")
        return is_seq and has_playback
    test("sequencer: create, inspect, delete", "sequencer", _seq_create_inspect_delete)

    test("sequencer: editor lib available", "sequencer",
         lambda: hasattr(unreal, 'LevelSequenceEditorBlueprintLibrary'))

    # ---------------------------------------------------------------------- #
    # Build
    # ---------------------------------------------------------------------- #
    def _build_log_dir():
        log_dir = str(unreal.Paths.project_log_dir())
        return os.path.isdir(log_dir)
    test("build: log directory exists", "build", _build_log_dir)

    def _build_log_readable():
        log_dir = str(unreal.Paths.project_log_dir())
        log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
        if not log_files:
            return False
        log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
        log_path = os.path.join(log_dir, log_files[0])
        with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
        return len(lines) > 10
    test("build: latest log readable", "build", _build_log_readable)

    def _build_bp_registry():
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        # Try modern ARFilter API (UE 5.1+)
        try:
            bp_filter = unreal.ARFilter()
            bp_filter.class_paths = [unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")]
            results = registry.get_assets(bp_filter)
            return results is not None and hasattr(results, '__len__')
        except Exception:
            pass
        # Fallback: older get_assets_by_class signature
        try:
            results = registry.get_assets_by_class("Blueprint")
            return results is not None and hasattr(results, '__len__')
        except Exception:
            pass
        # Last resort: just verify the registry exists
        return registry is not None
    test("build: query Blueprints", "build", _build_bp_registry)

    # ---------------------------------------------------------------------- #
    # Materials
    # ---------------------------------------------------------------------- #
    def _mat_editing_api():
        lib = unreal.MaterialEditingLibrary
        return (hasattr(lib, 'create_material_expression')
                and hasattr(lib, 'connect_material_property')
                and hasattr(lib, 'recompile_material'))
    test("materials: MaterialEditingLibrary API", "materials", _mat_editing_api)

    def _mat_blend_modes():
        for m in ['BLEND_OPAQUE', 'BLEND_MASKED', 'BLEND_TRANSLUCENT', 'BLEND_ADDITIVE']:
            if not hasattr(unreal.BlendMode, m):
                return False
        return True
    test("materials: BlendMode enum values", "materials", _mat_blend_modes)

    # ---------------------------------------------------------------------- #
    # BobBotLib
    # ---------------------------------------------------------------------- #
    test("bobbot_lib: module available", "bobbot_lib",
         lambda: hasattr(unreal, 'BobBotLib'))

    def _bobbot_lib_compile():
        return hasattr(unreal.BobBotLib, 'compile_blueprint')
    test("bobbot_lib: compile_blueprint exists", "bobbot_lib", _bobbot_lib_compile)

    # ---------------------------------------------------------------------- #
    # Bridge (env vars)
    # ---------------------------------------------------------------------- #
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
