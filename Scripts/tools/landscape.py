"""Landscape tools: inspect landscape info, set materials, and query paint layers."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
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
        # Try to get layer info
        try:
            layers = ls.get_editor_property("EditorLayerSettings")
            if layers:
                print(f"  Layers ({len(layers)}):")
                for layer in layers:
                    info = layer.get_editor_property("LayerInfoObj")
                    if info:
                        print(f"    {info.get_editor_property('LayerName')}")
        except:
            pass
""")

    @mcp.tool()
    def set_landscape_material(material_path: str) -> str:
        """Assign a material to the landscape in the current level."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
landscapes = [a for a in actors if isinstance(a, unreal.Landscape) or isinstance(a, unreal.LandscapeProxy)]
if not landscapes:
    print("ERROR: No landscape in current level")
else:
    mat = unreal.EditorAssetLibrary.load_asset("{material_path}")
    if mat is None:
        print("ERROR: Material '{material_path}' not found")
    else:
        for ls in landscapes:
            ls.set_editor_property("LandscapeMaterial", mat)
            print(f"Set material on {{ls.get_actor_label()}} to {material_path}")
""")

    @mcp.tool()
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
        try:
            layers = ls.get_editor_property("EditorLayerSettings")
            if layers:
                print(f"\\nPaint Layers ({len(layers)}):")
                for layer_setting in layers:
                    info = layer_setting.get_editor_property("LayerInfoObj")
                    if info:
                        layer_name = info.get_editor_property("LayerName")
                        layer_type = info.get_class().get_name()
                        print(f"  {layer_name} ({layer_type})")
            else:
                print("\\nNo paint layers configured")
        except Exception as e:
            print(f"\\nCould not read layers: {e}")
            print("Try using execute_unreal_python for detailed landscape inspection")
""")
