"""Foliage tools: inspect foliage types, register meshes, and get placement stats."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def get_foliage_types() -> str:
        """List all foliage types in the current level and their settings."""
        return _exec("""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
foliage_actors = [a for a in actors if a.get_class().get_name() == "InstancedFoliageActor"]
if not foliage_actors:
    print("No foliage in current level")
else:
    for fa in foliage_actors:
        print(f"Foliage Actor: {fa.get_actor_label()}")
        # List foliage types by checking the component hierarchy
        comps = fa.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        if comps:
            print(f"  Foliage Mesh Types ({len(comps)}):")
            for comp in comps:
                mesh = comp.get_editor_property("StaticMesh")
                count = comp.get_instance_count()
                print(f"    {mesh.get_path_name() if mesh else 'None'}: {count} instances")
        else:
            print("  No foliage mesh components")
""")

    @mcp.tool()
    def add_foliage_type(static_mesh_path: str) -> str:
        """Register a static mesh as a foliage type so it can be painted."""
        return _exec(f"""
import unreal
mesh = unreal.EditorAssetLibrary.load_asset("{static_mesh_path}")
if mesh is None:
    print("ERROR: Static mesh '{static_mesh_path}' not found")
elif not isinstance(mesh, unreal.StaticMesh):
    print(f"ERROR: '{{mesh.get_class().get_name()}}' is not a StaticMesh")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mesh_name = mesh.get_name()
    ft_name = f"FT_{{mesh_name}}"
    ft = None
    # Try factory approach first
    if hasattr(unreal, 'FoliageType_InstancedStaticMeshFactory'):
        try:
            factory = unreal.FoliageType_InstancedStaticMeshFactory()
            ft = asset_tools.create_asset(ft_name, "/Game/Foliage", unreal.FoliageType_InstancedStaticMesh, factory)
        except Exception as e:
            unreal.log_warning(f'add_foliage_type factory: {{e}}')
    # Fallback: create asset directly without factory
    if ft is None and hasattr(unreal, 'FoliageType_InstancedStaticMesh'):
        try:
            ft = asset_tools.create_asset(ft_name, "/Game/Foliage", unreal.FoliageType_InstancedStaticMesh, None)
        except Exception as e:
            unreal.log_warning(f'add_foliage_type direct: {{e}}')
    if ft:
        ft.set_editor_property("Mesh", mesh)
        unreal.EditorAssetLibrary.save_asset(f"/Game/Foliage/{{ft_name}}")
        print(f"Created foliage type: /Game/Foliage/{{ft_name}} (mesh: {static_mesh_path})")
    else:
        print("ERROR: Could not create foliage type.")
        print("FoliageType_InstancedStaticMesh may not be available in this UE version.")
        print("Workaround: Create a FoliageType asset manually in Content Browser")
""")

    @mcp.tool()
    def get_foliage_stats() -> str:
        """Get total foliage instance count and breakdown by mesh type."""
        return _exec("""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
foliage_actors = [a for a in actors if a.get_class().get_name() == "InstancedFoliageActor"]
if not foliage_actors:
    print("No foliage in current level")
else:
    total = 0
    breakdown = []
    for fa in foliage_actors:
        comps = fa.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
        for comp in comps:
            mesh = comp.get_editor_property("StaticMesh")
            count = comp.get_instance_count()
            total += count
            mesh_name = mesh.get_name() if mesh else "Unknown"
            breakdown.append((mesh_name, count))
    print(f"Foliage Statistics:")
    print(f"  Total Instances: {total}")
    if breakdown:
        print()
        print(f"Breakdown by mesh type:")
        for mesh_name, count in sorted(breakdown, key=lambda x: -x[1]):
            pct = (count / total * 100) if total > 0 else 0
            print(f"    {mesh_name}: {count} ({pct:.1f}%)")
""")
