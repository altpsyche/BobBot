"""Landscape tools: inspect landscape info, set materials, and query paint layers."""

from _common import _exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Landscape", output_kind="large", default_timeout=60)
    def get_landscape_info() -> str:
        """Get landscape bounds, component count, layer names, and material."""
        return _exec("""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
landscapes = [a for a in actors if isinstance(a, unreal.Landscape) or isinstance(a, unreal.LandscapeProxy)]
if not landscapes:
    print("No landscape in current level")
else:
    for ls in landscapes:
        print(f"Landscape: {ls.get_actor_label()} ({ls.get_class().get_name()})")
        loc = ls.get_actor_location()
        print(f"  Location: ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
        # Material
        mat = ls.get_editor_property("LandscapeMaterial")
        print(f"  Material: {mat.get_path_name() if mat else 'None'}")
        # Components
        comps = ls.get_components_by_class(unreal.LandscapeComponent)
        print(f"  Components: {len(comps)}")
        # Layer info via BobBotLib (EditorLayerSettings is editor-only + protected).
        count = unreal.BobBotLib.get_landscape_editor_layer_count(ls)
        if count > 0:
            print(f"  Layers ({count}):")
            for i in range(count):
                print(f"    {unreal.BobBotLib.get_landscape_editor_layer_name(ls, i)}")
""")

    @mcp.tool()
    @bob_tool(category="Landscape", output_kind="small", default_timeout=60)
    def set_landscape_material(material_path: str) -> str:
        """Assign a material to the landscape in the current level."""
        return _exec(f"""
import unreal
material_path = {_safe(material_path)}
actors = unreal.EditorLevelLibrary.get_all_level_actors()
landscapes = [a for a in actors if isinstance(a, unreal.Landscape) or isinstance(a, unreal.LandscapeProxy)]
if not landscapes:
    print("ERROR: No landscape in current level")
else:
    mat = unreal.EditorAssetLibrary.load_asset(material_path)
    if mat is None:
        print(f"ERROR: Material '{{material_path}}' not found")
    else:
        for ls in landscapes:
            ls.set_editor_property("LandscapeMaterial", mat)
            print(f"Set material on {{ls.get_actor_label()}} to {{material_path}}")
""")

    @mcp.tool()
    @bob_tool(category="Landscape", output_kind="large", default_timeout=60)
    def get_landscape_layers() -> str:
        """List all paint layers on the landscape and their info."""
        return _exec("""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
landscapes = [a for a in actors if isinstance(a, unreal.Landscape) or isinstance(a, unreal.LandscapeProxy)]
if not landscapes:
    print("No landscape in current level")
else:
    for ls in landscapes:
        print(f"Landscape: {ls.get_actor_label()}")
        count = unreal.BobBotLib.get_landscape_editor_layer_count(ls)
        if count <= 0:
            print("\\nNo paint layers configured")
        else:
            print()
            print(f"Paint Layers ({count}):")
            for i in range(count):
                print(f"  {unreal.BobBotLib.get_landscape_editor_layer_name(ls, i)}")
""")
