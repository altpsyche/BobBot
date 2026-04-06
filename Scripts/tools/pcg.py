"""PCG (Procedural Content Generation) tools: create graphs, inspect, and execute."""

from _common import _exec_ue, actor_exec, asset_exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def create_pcg_graph(name: str, path: str = "/Game/PCG") -> str:
        """Create a PCG graph asset."""
        return _exec_ue(f"""
name = {_safe(name)}
path = {_safe(path)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    # PCG graph creation - try available factory
    factory = unreal.PCGGraphFactory() if hasattr(unreal, 'PCGGraphFactory') else None
    if factory:
        graph = asset_tools.create_asset(name, path, unreal.PCGGraph if hasattr(unreal, 'PCGGraph') else None, factory)
        if graph:
            unreal.EditorAssetLibrary.save_asset(f"{{path}}/{{name}}")
            print(f"Created PCG Graph: {{path}}/{{name}}")
        else:
            print(f"ERROR: Failed to create PCG Graph")
    else:
        print("ERROR: PCG Graph factory not available. Ensure the PCG plugin is enabled.")
        print("Enable it in Edit > Plugins > Procedural Content Generation Framework")
except Exception as e:
    print(f"ERROR: {{e}}")
    print("PCG has limited Python API support in UE 5.4")
""")

    @mcp.tool()
    def get_pcg_graph_info(graph_path: str) -> str:
        """Get nodes, settings, and basic info for a PCG graph asset."""
        return asset_exec(graph_path, f"""
graph_path = {_safe(graph_path)}
print(f"PCG Graph: {{graph_path}}")
print(f"Class: {{asset.get_class().get_name()}}")
# PCG graph inspection is limited in Python
# List what properties we can access
try:
    if hasattr(asset, 'get_editor_property'):
        print("\\n(PCG graph node inspection has limited Python API support)")
        print("Use execute_unreal_python for detailed graph analysis")
except Exception as e:
    print(f"Could not inspect graph: {{e}}")
""")

    @mcp.tool()
    def execute_pcg_graph(actor_label: str) -> str:
        """Execute/regenerate a PCG graph on an actor that has a PCGComponent."""
        return actor_exec(actor_label, """
# Find PCG component
all_comps = target.get_components_by_class(unreal.ActorComponent)
pcg_comp = None
for c in all_comps:
    if "PCG" in c.get_class().get_name():
        pcg_comp = c
        break
if pcg_comp is None:
    print(f"ERROR: {target.get_actor_label()} has no PCG component")
    print("Available components:")
    for c in all_comps:
        print(f"  {c.get_name()} ({c.get_class().get_name()})")
else:
    try:
        pcg_comp.generate(True)
        print(f"Executed PCG graph on {target.get_actor_label()}")
    except Exception as e:
        print(f"ERROR: Could not execute PCG graph: {e}")
        print("Try using execute_unreal_python with the PCGComponent API directly")
""")

    @mcp.tool()
    def get_pcg_volumes() -> str:
        """List all actors with PCG components in the current level."""
        return _exec_ue("""
actors = unreal.EditorLevelLibrary.get_all_level_actors()
pcg_actors = []
for a in actors:
    all_comps = a.get_components_by_class(unreal.ActorComponent)
    for c in all_comps:
        if "PCG" in c.get_class().get_name():
            pcg_actors.append((a, c))
            break
if pcg_actors:
    print(f"PCG Actors ({len(pcg_actors)}):")
    for actor, comp in pcg_actors:
        loc = actor.get_actor_location()
        print(f"  {actor.get_actor_label()} ({actor.get_class().get_name()})")
        print(f"    Component: {comp.get_class().get_name()}")
        print(f"    Location: ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
else:
    print("No PCG actors in current level")
""")
