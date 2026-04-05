"""LOD tools: inspect and manage Levels of Detail on static meshes."""

from _common import _exec_ue, asset_exec

def register(mcp, send_fn):


    @mcp.tool()
    def get_lod_info(mesh_path: str) -> str:
        """Get LOD information for a static mesh: LOD count, triangle counts, and screen sizes."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    print(f"Static Mesh: {mesh_path}")
    num_lods = asset.get_num_lods()
    print(f"LOD Count: {{num_lods}}")
    for i in range(num_lods):
        print()
        print(f"  LOD {{i}}:")
        try:
            num_sections = asset.get_num_sections(i)
            print(f"    Sections: {{num_sections}}")
        except Exception:
            print("    Sections: (unable to read)")
        try:
            num_tris = asset.get_num_triangles(i)
            print(f"    Triangles: {{num_tris}}")
        except Exception:
            pass
        try:
            num_verts = asset.get_num_vertices(i)
            print(f"    Vertices: {{num_verts}}")
        except Exception:
            pass
    # Try to read screen sizes from SourceModels
    try:
        source_models = asset.get_editor_property("SourceModels")
        if source_models:
            print()
            print(f"Screen Sizes:")
            for idx, sm in enumerate(source_models):
                try:
                    screen_size = sm.get_editor_property("ScreenSize")
                    print(f"  LOD {{idx}}: {{screen_size}}")
                except Exception:
                    try:
                        ss = sm.screen_size
                        print(f"  LOD {{idx}}: {{ss}}")
                    except Exception:
                        pass
    except Exception:
        print("\\nScreen sizes: (SourceModels not accessible on this UE version)")
""")

    @mcp.tool()
    def set_lod_screen_size(mesh_path: str, lod_index: int, screen_size: float) -> str:
        """Set the screen size threshold for a specific LOD on a static mesh. Screen size is a 0-1 value where 1 = full screen."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    lod_index = {lod_index}
    screen_size = {screen_size}
    try:
        source_models = asset.get_editor_property("SourceModels")
        if source_models is None or len(source_models) == 0:
            print("ERROR: No SourceModels found on this mesh")
        elif lod_index < 0 or lod_index >= len(source_models):
            print(f"ERROR: LOD index {{lod_index}} out of range (mesh has {{len(source_models)}} LODs)")
        else:
            sm = source_models[lod_index]
            try:
                sm.set_editor_property("ScreenSize", screen_size)
            except Exception:
                # Some UE versions use a PerPlatform wrapper
                try:
                    ss_prop = sm.get_editor_property("ScreenSize")
                    ss_prop.set_editor_property("Default", screen_size)
                except Exception as e2:
                    print(f"ERROR: Could not set ScreenSize: {{e2}}")
                    print("Try using execute_unreal_python with direct property manipulation")
                    raise
            unreal.EditorAssetLibrary.save_asset("{mesh_path}")
            print(f"Set LOD {{lod_index}} screen size to {{screen_size}} on {mesh_path}")
    except Exception as e:
        if "ERROR" not in str(e):
            print(f"ERROR: Could not modify SourceModels: {{e}}")
            print("This property may not be writable on your UE version. Try using execute_unreal_python with EditorStaticMeshLibrary.")
""")

    @mcp.tool()
    def auto_generate_lods(mesh_path: str, num_lods: int = 3) -> str:
        """Auto-generate LODs for a static mesh. Uses EditorStaticMeshLibrary if available, otherwise tries direct manipulation."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    num_lods = {num_lods}
    generated = False
    # Try EditorStaticMeshLibrary (UE 5.x preferred path)
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
    # Fallback: try setting LODs via SourceModels manipulation
    if not generated:
        try:
            source_models = asset.get_editor_property("SourceModels")
            current = len(source_models) if source_models else 1
            if num_lods <= current:
                print(f"Mesh already has {{current}} LODs (requested {{num_lods}})")
                generated = True
            else:
                # Try adding reduction settings
                if hasattr(unreal, 'StaticMeshSourceModel'):
                    for i in range(current, num_lods):
                        new_model = unreal.StaticMeshSourceModel()
                        try:
                            reduction = new_model.get_editor_property("ReductionSettings")
                            # Reduce triangles progressively
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
        unreal.EditorAssetLibrary.save_asset("{mesh_path}")
        final_count = asset.get_num_lods()
        print(f"Mesh now has {{final_count}} LODs")
    else:
        print("ERROR: Could not auto-generate LODs. Use execute_unreal_python for manual LOD setup.")
""")

    @mcp.tool()
    def get_nanite_status(mesh_path: str) -> str:
        """Check whether Nanite is enabled on a static mesh and display available Nanite settings."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    print(f"Static Mesh: {mesh_path}")
    found_settings = False
    try:
        nanite_settings = asset.get_editor_property("NaniteSettings")
        found_settings = True
        print("Nanite Settings found:")
        try:
            enabled = nanite_settings.get_editor_property("bEnabled")
            print(f"  Enabled: {{enabled}}")
        except Exception:
            print("  Enabled: (could not read bEnabled)")
        # Try common Nanite settings properties
        for prop_name in ["FallbackPercentTriangles", "FallbackRelativeError",
                          "TrimRelativeError", "MaxEdgesPerVertex",
                          "PositionPrecision", "NormalPrecision"]:
            try:
                val = nanite_settings.get_editor_property(prop_name)
                print(f"  {{prop_name}}: {{val}}")
            except Exception:
                pass
    except Exception:
        pass
    if not found_settings:
        # Try alternate property path
        try:
            enabled = asset.get_editor_property("bIsNaniteEnabled")
            print(f"Nanite Enabled (bIsNaniteEnabled): {{enabled}}")
            found_settings = True
        except Exception:
            pass
    if not found_settings:
        print("Nanite settings not available (pre-Nanite UE version or not a Nanite-capable mesh)")
""")
