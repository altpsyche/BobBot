"""LOD tools: inspect and manage Levels of Detail on static meshes."""

from _common import _exec_ue, asset_exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="LOD", output_kind="large", default_timeout=60)
    def get_lod_info(mesh_path: str) -> str:
        """Get LOD information for a static mesh: LOD count, triangle counts, and screen sizes."""
        return asset_exec(mesh_path, """
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{asset.get_class().get_name()}' is not a StaticMesh")
else:
    print(f"Static Mesh: {_asset_path}")
    num_lods = asset.get_num_lods()
    print(f"LOD Count: {num_lods}")
    for i in range(num_lods):
        print()
        print(f"  LOD {i}:")
        try:
            num_sections = asset.get_num_sections(i)
            print(f"    Sections: {num_sections}")
        except Exception:
            print("    Sections: (unable to read)")
        try:
            num_tris = asset.get_num_triangles(i)
            print(f"    Triangles: {num_tris}")
        except Exception:
            pass
        try:
            num_verts = asset.get_num_vertices(i)
            print(f"    Vertices: {num_verts}")
        except Exception:
            pass
    src_count = unreal.BobBotLib.get_static_mesh_lod_count(asset)
    if src_count > 0:
        print()
        print("Screen Sizes:")
        for idx in range(src_count):
            ss = unreal.BobBotLib.get_static_mesh_lod_screen_size(asset, idx)
            print(f"  LOD {idx}: {ss}")
""")

    @mcp.tool()
    @bob_tool(category="LOD", output_kind="large", default_timeout=60)
    def set_lod_screen_size(mesh_path: str, lod_index: int, screen_size: float) -> str:
        """Set the screen size threshold for a specific LOD on a static mesh. Screen size is a 0-1 value where 1 = full screen."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    lod_index = {lod_index}
    screen_size = {screen_size}
    src_count = unreal.BobBotLib.get_static_mesh_lod_count(asset)
    if src_count <= 0:
        print("ERROR: No source models found on this mesh")
    elif lod_index < 0 or lod_index >= src_count:
        print(f"ERROR: LOD index {{lod_index}} out of range (mesh has {{src_count}} LODs)")
    else:
        try:
            source_models = asset.get_editor_property("SourceModels")
            sm = source_models[lod_index]
            try:
                sm.set_editor_property("ScreenSize", screen_size)
            except Exception:
                ss_prop = sm.get_editor_property("ScreenSize")
                ss_prop.set_editor_property("Default", screen_size)
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
            print(f"Set LOD {{lod_index}} screen size to {{screen_size}} on {{_asset_path}}")
        except Exception as e:
            print(f"ERROR: Could not set ScreenSize: {{e}}")
            print("This property write path may need EditorStaticMeshLibrary on your UE version.")
""")

    @mcp.tool()
    @bob_tool(category="LOD", output_kind="large", default_timeout=180)
    def auto_generate_lods(mesh_path: str, num_lods: int = 3) -> str:
        """Auto-generate LODs for a static mesh. Uses EditorStaticMeshLibrary if available, otherwise tries direct manipulation."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    num_lods = {num_lods}
    generated = False
    if hasattr(unreal, 'EditorStaticMeshLibrary'):
        try:
            lib = unreal.EditorStaticMeshLibrary
            result = lib.set_lod_count(asset, num_lods)
            if result > 0:
                print(f"Generated {{result}} LODs via EditorStaticMeshLibrary")
                generated = True
            else:
                print("EditorStaticMeshLibrary.set_lod_count returned 0, trying alternative...")
        except Exception as e:
            print(f"EditorStaticMeshLibrary failed: {{e}}, trying alternative...")
    if not generated:
        try:
            current = unreal.BobBotLib.get_static_mesh_lod_count(asset)
            if current < 0:
                current = 1
            if num_lods <= current:
                print(f"Mesh already has {{current}} LODs (requested {{num_lods}})")
                generated = True
            else:
                source_models = asset.get_editor_property("SourceModels")
                if hasattr(unreal, 'StaticMeshSourceModel'):
                    for i in range(current, num_lods):
                        new_model = unreal.StaticMeshSourceModel()
                        try:
                            reduction = new_model.get_editor_property("ReductionSettings")
                            pct = max(0.1, 1.0 - (i * 0.3))
                            reduction.set_editor_property("PercentTriangles", pct)
                        except Exception:
                            pass
                        source_models.append(new_model)
                    asset.set_editor_property("SourceModels", source_models)
                    asset.build()
                    print(f"Added LODs via SourceModels ({{current}} -> {{num_lods}})")
                    generated = True
                else:
                    print("StaticMeshSourceModel class not available")
        except Exception as e:
            print(f"SourceModels manipulation failed: {{e}}")
    if generated:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        final_count = asset.get_num_lods()
        print(f"Mesh now has {{final_count}} LODs")
    else:
        print("ERROR: Could not auto-generate LODs. Use execute_unreal_python for manual LOD setup.")
""")

    @mcp.tool()
    @bob_tool(category="LOD", output_kind="large", default_timeout=60)
    def get_nanite_status(mesh_path: str) -> str:
        """Check whether Nanite is enabled on a static mesh and display available Nanite settings."""
        return asset_exec(mesh_path, """
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{asset.get_class().get_name()}' is not a StaticMesh")
else:
    print(f"Static Mesh: {_asset_path}")
    enabled = unreal.BobBotLib.get_static_mesh_nanite_enabled(asset)
    print(f"  Nanite enabled: {enabled}")
    if enabled:
        fallback_pct = unreal.BobBotLib.get_static_mesh_nanite_fallback_percent(asset)
        print(f"  Fallback percent triangles: {fallback_pct}")
    else:
        print("  (Nanite disabled — no fallback settings)")
""")

    @mcp.tool()
    @bob_tool(category="LOD", output_kind="large", default_timeout=60)
    def get_lod_summary(mesh_path: str) -> str:
        """Mobile-aware perf summary for a static mesh. Reports runtime LOD count,
        per-LOD triangle counts, lightmap resolution, material slots, Nanite flag.
        Flags outliers using mobile-friendly thresholds (no Nanite assumption).

        Thresholds (defaults):
          HEAVY_TRIS    : LOD0 tris > 20000
          HIGH_TRIS_NO_LOD : LOD0 tris > 5000 and runtime LODs < 2
          HIGH_LIGHTMAP : lightmap resolution > 256
          MANY_MATERIALS: material slots > 4
        """
        return asset_exec(mesh_path, """
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{asset.get_class().get_name()}' is not a StaticMesh")
else:
    lib = unreal.BobBotLib
    runtime_lods = lib.get_static_mesh_runtime_lod_count(asset)
    source_lods = lib.get_static_mesh_lod_count(asset)
    nanite = lib.get_static_mesh_nanite_enabled(asset)
    mat_count = lib.get_static_mesh_material_count(asset)
    lightmap = lib.get_static_mesh_lightmap_resolution(asset)
    tris = []
    for i in range(max(runtime_lods, 0)):
        tris.append(lib.get_static_mesh_num_triangles(asset, i))
    tris0 = tris[0] if tris else -1
    flags = []
    if tris0 > 20000:
        flags.append("HEAVY_TRIS")
    if tris0 > 5000 and runtime_lods < 2:
        flags.append("HIGH_TRIS_NO_LOD")
    if lightmap > 256:
        flags.append(f"HIGH_LIGHTMAP({lightmap})")
    if mat_count > 4:
        flags.append(f"MANY_MATERIALS({mat_count})")
    flag_str = ",".join(flags) if flags else "OK"
    tris_str = "/".join(str(t) for t in tris) if tris else "?"
    print(f"{_asset_path} | runtimeLODs={runtime_lods} sourceLODs={source_lods} | tris={tris_str} | mats={mat_count} | lightmap={lightmap} | nanite={nanite} | {flag_str}")
""")
