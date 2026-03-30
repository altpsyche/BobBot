"""Level tools: inspect, open, and save levels."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def get_current_level() -> str:
        """Get info about the currently open level: name, path, and actor count."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    level_path = world.get_path_name()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    print(f"Level: {level_path}")
    print(f"Actors: {len(actors)}")
    # Count by type
    type_counts = {}
    for a in actors:
        cls = a.get_class().get_name()
        type_counts[cls] = type_counts.get(cls, 0) + 1
    print("\\nBy type:")
    for cls, count in sorted(type_counts.items(), key=lambda x: -x[1])[:20]:
        print(f"  {cls}: {count}")
""")

    @mcp.tool()
    def open_level(level_path: str) -> str:
        """Open a level by asset path (e.g. '/Game/Maps/MainMenu')."""
        return _exec(f"""
import unreal
path = "{level_path}"
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorLoadingAndSavingUtils.load_map(path)
    print(f"Opened level: {{path}}")
else:
    print(f"ERROR: Level '{{path}}' not found")
""")

    @mcp.tool()
    def save_current_level() -> str:
        """Save the currently open level."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    print(f"Saved level: {world.get_path_name()}")
""")
