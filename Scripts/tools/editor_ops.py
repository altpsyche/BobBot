"""Editor operations: selection, undo/redo, focus, rename, and organize actors."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def select_actors(actor_labels: str) -> str:
        """Select actors in the viewport by label. Comma-separated list of actor labels."""
        return _exec(f"""
import unreal
labels = [l.strip() for l in "{actor_labels}".split(",") if l.strip()]
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
    def deselect_all() -> str:
        """Clear all viewport selection."""
        return _exec("""
import unreal
unreal.EditorLevelLibrary.set_selected_level_actors([])
print("Selection cleared")
""")

    @mcp.tool()
    def undo() -> str:
        """Trigger editor undo."""
        return _exec("""
import unreal
unreal.EditorLevelLibrary.editor_undo()
print("Undo executed")
""")

    @mcp.tool()
    def redo() -> str:
        """Trigger editor redo."""
        return _exec("""
import unreal
unreal.EditorLevelLibrary.editor_redo()
print("Redo executed")
""")

    @mcp.tool()
    def focus_on_actor(actor_label: str) -> str:
        """Focus the viewport camera on an actor by label."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    # Select the actor and use the editor focus command
    unreal.EditorLevelLibrary.set_selected_level_actors([target])
    # Use console command to focus on selection
    world = unreal.EditorLevelLibrary.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "CAMERA ALIGN")
    loc = target.get_actor_location()
    print(f"Focused on {{target.get_actor_label()}} at ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
""")

    @mcp.tool()
    def set_actor_label(actor_label: str, new_label: str) -> str:
        """Rename an actor in the level."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    target.set_actor_label("{new_label}")
    print(f"Renamed: {actor_label} -> {new_label}")
""")

    @mcp.tool()
    def set_actor_folder(actor_label: str, folder_path: str) -> str:
        """Organize an actor into a World Outliner folder. Use '/' for subfolders like 'Lighting/Dynamic'."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    target.set_folder_path("{folder_path}")
    print(f"Moved {{target.get_actor_label()}} to folder '{folder_path}'")
""")

    @mcp.tool()
    def get_editor_selection() -> str:
        """Get full selection info: selected actors in viewport and selected assets in Content Browser."""
        return _exec("""
import unreal
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
        print(f"\\nSelected assets ({len(sel_assets)}):")
        for a in sel_assets:
            print(f"  {a.get_path_name()} ({a.get_class().get_name()})")
except:
    pass

# Current level
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    print(f"\\nCurrent level: {world.get_path_name()}")
""")
