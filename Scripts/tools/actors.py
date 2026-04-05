"""Actor tools: inspect, spawn, delete, and modify actors in the current level."""

from _common import _exec, _exec_ue, actor_exec

def register(mcp, send_fn):


    @mcp.tool()
    def get_selected_actors() -> str:
        """Get all currently selected actors in the viewport with their labels, classes, and locations."""
        return _exec_ue("""
actors = unreal.EditorLevelLibrary.get_selected_level_actors()
if not actors:
    print("No actors selected")
else:
    for a in actors:
        loc = a.get_actor_location()
        print(f"{a.get_actor_label()} ({a.get_class().get_name()}) at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
""")

    @mcp.tool()
    def get_level_actors(class_filter: str = "") -> str:
        """Get all actors in the current level. Optionally filter by class name (e.g. 'StaticMeshActor', 'PointLight', 'CameraActor')."""
        return _exec_ue(f"""
actors = unreal.EditorLevelLibrary.get_all_level_actors()
class_filter = "{class_filter}"
count = 0
for a in actors:
    cls = a.get_class().get_name()
    if class_filter and class_filter.lower() not in cls.lower():
        continue
    loc = a.get_actor_location()
    print(f"{{a.get_actor_label()}} ({{cls}}) at ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
    count += 1
print()
print(f"Total: {{count}} actors")
""")

    @mcp.tool()
    def spawn_actor(class_path: str, x: float = 0.0, y: float = 0.0, z: float = 0.0,
                    yaw: float = 0.0, pitch: float = 0.0, roll: float = 0.0) -> str:
        """Spawn an actor at a location. class_path can be a Blueprint path like '/Game/Blueprints/BP_Enemy' or a C++ class like '/Script/Engine.PointLight'."""
        return _exec_ue(f"""
class_path = "{class_path}"
actor_class = unreal.load_class(None, class_path) if class_path.startswith("/") else getattr(unreal, class_path, None)
if actor_class is None:
    # Try loading as Blueprint
    bp = unreal.load_asset(class_path)
    if bp and hasattr(bp, 'generated_class'):
        actor_class = bp.generated_class()
if actor_class is None:
    print(f"ERROR: Could not find class '{{class_path}}'")
else:
    loc = unreal.Vector(x={x}, y={y}, z={z})
    rot = unreal.Rotator(pitch={pitch}, yaw={yaw}, roll={roll})
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, loc, rot)
    if actor:
        print(f"Spawned {{actor.get_actor_label()}} ({{actor_class.get_name()}}) at ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
    else:
        print("ERROR: Failed to spawn actor")
""")

    @mcp.tool()
    def delete_selected_actors() -> str:
        """Delete all currently selected actors in the viewport."""
        return _exec_ue("""
actors = unreal.EditorLevelLibrary.get_selected_level_actors()
if not actors:
    print("No actors selected")
else:
    count = len(actors)
    for a in actors:
        label = a.get_actor_label()
        unreal.EditorLevelLibrary.destroy_actor(a)
    print(f"Deleted {count} actor(s)")
""")

    @mcp.tool()
    def get_actor_properties(actor_label: str) -> str:
        """Get all editable properties of an actor found by its label in the level."""
        return actor_exec(actor_label, f"""
print(f"Actor: {{target.get_actor_label()}} ({{target.get_class().get_name()}})")
print(f"Location: {{target.get_actor_location()}}")
print(f"Rotation: {{target.get_actor_rotation()}}")
print(f"Scale: {{target.get_actor_scale3d()}}")
# List component names
components = target.get_components_by_class(unreal.ActorComponent)
if components:
    print()
    print(f"Components ({{len(components)}}):")
    for c in components:
        print(f"  {{c.get_name()}} ({{c.get_class().get_name()}})")
""")

    @mcp.tool()
    def set_actor_property(actor_label: str, property_name: str, value: str) -> str:
        """Set a property on an actor by label. Common properties: RelativeLocation, RelativeRotation, RelativeScale3D, Mobility, bHidden. Value is a string (e.g. '(X=100,Y=0,Z=0)' for vectors, 'True'/'False' for bools)."""
        return actor_exec(actor_label, f"""
if target.set_editor_property("{property_name}", "{value}"):
    print(f"Set {{target.get_actor_label()}}.{property_name} = {value}")
else:
    print(f"ERROR: Could not set property '{property_name}' on {{target.get_class().get_name()}}")
""")
