"""Level streaming tools: add, remove, and inspect streaming sub-levels."""


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
    def add_streaming_level(level_path: str) -> str:
        """Add a sub-level as a streaming level to the current persistent level."""
        return _exec(f"""
import unreal
level_path = "{level_path}"
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
    def remove_streaming_level(level_path: str) -> str:
        """Remove a streaming level from the current persistent level."""
        return _exec(f"""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    streaming_levels = world.get_editor_property("StreamingLevels")
    if not streaming_levels:
        print("No streaming levels to remove")
    else:
        found = None
        for sl in streaming_levels:
            pkg_name = sl.get_world_asset_package_name()
            if "{level_path}" in pkg_name or pkg_name.endswith("{level_path}".rsplit("/", 1)[-1]):
                found = sl
                break
        if found is None:
            print(f"ERROR: Streaming level '{level_path}' not found")
            print("Current streaming levels:")
            for sl in streaming_levels:
                print(f"  {{sl.get_world_asset_package_name()}}")
        else:
            unreal.EditorLevelUtils.remove_level_from_world(found.get_loaded_level())
            print(f"Removed streaming level: {level_path}")
""")

    @mcp.tool()
    def get_streaming_levels() -> str:
        """List all streaming levels with their load status."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    print(f"Persistent Level: {world.get_path_name()}")
    streaming_levels = world.get_editor_property("StreamingLevels")
    if not streaming_levels:
        print("\\nNo streaming sub-levels")
    else:
        print(f"\\nStreaming Levels ({len(streaming_levels)}):")
        for sl in streaming_levels:
            pkg_name = sl.get_world_asset_package_name()
            loaded = sl.get_loaded_level() is not None
            visible = sl.get_editor_property("bShouldBeVisibleInEditor") if hasattr(sl, "bShouldBeVisibleInEditor") else "N/A"
            print(f"  {pkg_name}")
            print(f"    Loaded: {loaded}, Visible: {visible}")
            print(f"    Class: {sl.get_class().get_name()}")
""")
