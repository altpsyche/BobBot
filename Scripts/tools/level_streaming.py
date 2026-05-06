"""Level streaming tools: add, remove, and inspect streaming sub-levels."""

from _common import _exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Level Streaming", output_kind="small", default_timeout=60)
    def add_streaming_level(level_path: str) -> str:
        """Add a sub-level as a streaming level to the current persistent level."""
        return _exec(f"""
import unreal
level_path = {_safe(level_path)}
if not unreal.EditorAssetLibrary.does_asset_exist(level_path):
    print(f"ERROR: Level '{{level_path}}' not found")
else:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        print("ERROR: No level open")
    else:
        result = unreal.EditorLevelUtils.add_level_to_world(
            world, level_path, unreal.LevelStreamingDynamic)
        if result:
            print(f"Added streaming level: {{level_path}}")
        else:
            print(f"ERROR: Failed to add streaming level '{{level_path}}'")
""")

    @mcp.tool()
    @bob_tool(category="Level Streaming", output_kind="small", default_timeout=60)
    def remove_streaming_level(level_path: str) -> str:
        """Remove a streaming level from the current persistent level."""
        return _exec(f"""
import unreal
level_path = {_safe(level_path)}
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    streaming_levels = unreal.BobBotLib.get_world_streaming_levels(world)
    if not streaming_levels:
        print("No streaming levels to remove")
    else:
        found = None
        for sl in streaming_levels:
            pkg_name = str(sl.get_world_asset_package_f_name())
            if level_path in pkg_name or pkg_name.endswith(level_path.rsplit("/", 1)[-1]):
                found = sl
                break
        if found is None:
            print(f"ERROR: Streaming level '{{level_path}}' not found")
            print("Current streaming levels:")
            for sl in streaming_levels:
                print(f"  {{str(sl.get_world_asset_package_f_name())}}")
        else:
            unreal.EditorLevelUtils.remove_level_from_world(found.get_loaded_level())
            print(f"Removed streaming level: {{level_path}}")
""")

    @mcp.tool()
    @bob_tool(category="Level Streaming", output_kind="large", default_timeout=60)
    def get_streaming_levels() -> str:
        """List all streaming levels with their load status."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    print(f"Persistent Level: {world.get_path_name()}")
    streaming_levels = unreal.BobBotLib.get_world_streaming_levels(world)
    if not streaming_levels:
        print("\\nNo streaming sub-levels")
    else:
        print()
        print(f"Streaming Levels ({len(streaming_levels)}):")
        for sl in streaming_levels:
            pkg_name = str(sl.get_world_asset_package_f_name())
            loaded = sl.get_loaded_level() is not None
            try:
                visible = sl.should_be_visible_in_editor
            except Exception:
                visible = "N/A"
            print(f"  {pkg_name}")
            print(f"    Loaded: {loaded}, Visible: {visible}")
            print(f"    Class: {sl.get_class().get_name()}")
""")
