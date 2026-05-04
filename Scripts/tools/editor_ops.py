"""Editor operations: selection, undo/redo, focus, rename, and organize actors."""

from _common import _exec, _exec_ue, actor_exec, _safe, autocaptured
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def select_actors(actor_labels: str) -> str:
        """Select actors in the viewport by label. Comma-separated list of actor labels."""
        return _exec_ue(f"""
labels = {_safe(actor_labels)}.split(",")
labels = [l.strip() for l in labels if l.strip()]
all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
to_select = []
not_found = []
for label in labels:
    found = False
    for a in all_actors:
        if a.get_actor_label() == label:
            to_select.append(a)
            found = True
            break
    if not found:
        not_found.append(label)
if to_select:
    unreal.EditorLevelLibrary.set_selected_level_actors(to_select)
    print(f"Selected {{len(to_select)}} actor(s): {{', '.join(a.get_actor_label() for a in to_select)}}")
if not_found:
    print(f"Not found: {{', '.join(not_found)}}")
if not to_select and not not_found:
    print("No labels provided")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def deselect_all() -> str:
        """Clear all viewport selection."""
        return _exec_ue("""
unreal.EditorLevelLibrary.set_selected_level_actors([])
print("Selection cleared")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def undo() -> str:
        """Trigger editor undo."""
        return _exec_ue("""
unreal.EditorLevelLibrary.editor_undo()
print("Undo executed")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def redo() -> str:
        """Trigger editor redo."""
        return _exec_ue("""
unreal.EditorLevelLibrary.editor_redo()
print("Redo executed")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    @autocaptured
    def focus_on_actor(actor_label: str):
        """Focus the viewport camera on an actor by label."""
        return actor_exec(actor_label, """
# Select the actor and use the editor focus command
unreal.EditorLevelLibrary.set_selected_level_actors([target])
# Use console command to focus on selection
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "CAMERA ALIGN")
loc = target.get_actor_location()
print(f"Focused on {target.get_actor_label()} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    @autocaptured
    def set_actor_label(actor_label: str, new_label: str):
        """Rename an actor in the level."""
        return actor_exec(actor_label, f"""
_new = {_safe(new_label)}
old = target.get_actor_label()
target.set_actor_label(_new)
print(f"Renamed: {{old}} -> {{_new}}")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def set_actor_folder(actor_label: str, folder_path: str) -> str:
        """Organize an actor into a World Outliner folder. Use '/' for subfolders like 'Lighting/Dynamic'."""
        return actor_exec(actor_label, f"""
_folder = {_safe(folder_path)}
target.set_folder_path(_folder)
print(f"Moved {{target.get_actor_label()}} to folder '{{_folder}}'")
""")

    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="large", default_timeout=60)
    def get_editor_selection() -> str:
        """Get full selection info: selected actors in viewport and selected assets in Content Browser."""
        return _exec_ue("""
# Viewport selection
selected = unreal.EditorLevelLibrary.get_selected_level_actors()
if selected:
    print(f"Selected actors ({len(selected)}):")
    for a in selected:
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        scale = a.get_actor_scale3d()
        print(f"  {a.get_actor_label()} ({a.get_class().get_name()})")
        print(f"    Location: ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
        print(f"    Rotation: ({rot.pitch:.1f}, {rot.yaw:.1f}, {rot.roll:.1f})")
        print(f"    Scale: ({scale.x:.2f}, {scale.y:.2f}, {scale.z:.2f})")
        comps = a.get_components_by_class(unreal.ActorComponent)
        if comps:
            print(f"    Components: {', '.join(c.get_class().get_name() for c in comps)}")
else:
    print("No actors selected in viewport")

# Content Browser selection
try:
    sel_assets = unreal.EditorUtilityLibrary.get_selected_assets()
    if sel_assets:
        print()
        print(f"Selected assets ({len(sel_assets)}):")
        for a in sel_assets:
            print(f"  {a.get_path_name()} ({a.get_class().get_name()})")
except Exception as e:
    unreal.log_warning(f'get_editor_selection selected assets: {e}')

# Current level
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    print()
    print(f"Current level: {world.get_path_name()}")
""")


    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="large", default_timeout=60)
    def list_plugins() -> str:
        """List all plugins referenced in the .uproject file and any additional plugins found in the Plugins/ directory."""
        return _exec("""
import unreal, os, json

project_path = str(unreal.Paths.get_project_file_path())
project_dir = os.path.dirname(project_path)

# 1. Read .uproject file
listed_plugins = {}
try:
    with open(project_path, 'r', encoding='utf-8') as f:
        uproject = json.load(f)
    plugins_arr = uproject.get("Plugins", [])
    for p in plugins_arr:
        name = p.get("Name", "Unknown")
        enabled = p.get("Enabled", False)
        listed_plugins[name] = enabled
    print(f"Plugins in .uproject ({len(plugins_arr)}):")
    for p in sorted(plugins_arr, key=lambda x: x.get("Name", "")):
        name = p.get("Name", "Unknown")
        enabled = p.get("Enabled", False)
        status = "enabled" if enabled else "disabled"
        print(f"  {name}: {status}")
except Exception as e:
    print(f"ERROR: Failed to read .uproject: {e}")

# 2. Scan Plugins/ directory for unlisted plugins
plugins_dir = os.path.join(project_dir, "Plugins")
if os.path.isdir(plugins_dir):
    unlisted = []
    for entry in os.listdir(plugins_dir):
        entry_path = os.path.join(plugins_dir, entry)
        if os.path.isdir(entry_path):
            # Check for .uplugin file
            uplugin_files = [f for f in os.listdir(entry_path) if f.endswith('.uplugin')]
            if uplugin_files and entry not in listed_plugins:
                unlisted.append(entry)
    if unlisted:
        print()
        print(f"Plugins in Plugins/ dir but not in .uproject ({len(unlisted)}):")
        for name in sorted(unlisted):
            print(f"  {name} (local plugin, not listed in .uproject)")
else:
    print("\\nNo Plugins/ directory found")
""")


    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def set_editor_bookmark(slot: int, x: float = None, y: float = None,
                            z: float = None, yaw: float = None,
                            pitch: float = None) -> str:
        """Save an editor viewport bookmark to a slot (0-9). If no location is given, saves the current viewport camera position. If location is given, moves the camera there first."""
        if x is not None and y is not None and z is not None:
            # Move camera first, then set bookmark
            yaw_val = yaw if yaw is not None else 0.0
            pitch_val = pitch if pitch is not None else 0.0
            return _exec_ue(f"""
world = unreal.EditorLevelLibrary.get_editor_world()
# Move viewport camera to position
subsystem = unreal.UnrealEditorSubsystem()
loc = unreal.Vector(x={x}, y={y}, z={z})
rot = unreal.Rotator(pitch={pitch_val}, yaw={yaw_val}, roll=0.0)
subsystem.set_level_viewport_camera_info(loc, rot)

# Set bookmark at current position
unreal.SystemLibrary.execute_console_command(world, "SetBookmark {slot}")
print(f"Saved bookmark {slot} at ({x}, {y}, {z})")
""")
        else:
            return _exec_ue(f"""
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "SetBookmark {slot}")
# Read back current camera to confirm
try:
    subsystem = unreal.UnrealEditorSubsystem()
    result = subsystem.get_level_viewport_camera_info()
    if result:
        loc = result[1]  # location
        rot = result[2]  # rotation
        print(f"Saved bookmark {slot} at ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
    else:
        print(f"Saved bookmark {slot} (could not read camera position)")
except Exception:
    print(f"Saved bookmark {slot} at current viewport camera position")
""")


    @mcp.tool()
    @bob_tool(category="Editor Operations", output_kind="small", default_timeout=60)
    def load_editor_bookmark(slot: int) -> str:
        """Jump the viewport camera to a previously saved bookmark (slot 0-9). Returns the camera position after jumping."""
        return _exec_ue(f"""
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "JumpToBookmark {slot}")

# Read back camera position
try:
    subsystem = unreal.UnrealEditorSubsystem()
    result = subsystem.get_level_viewport_camera_info()
    if result:
        loc = result[1]
        rot = result[2]
        print(f"Jumped to bookmark {slot}: ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}}) rot ({{rot.pitch:.1f}}, {{rot.yaw:.1f}}, {{rot.roll:.1f}})")
    else:
        print(f"Jumped to bookmark {slot} (could not read camera position)")
except Exception:
    print(f"Jumped to bookmark {slot}")
""")
