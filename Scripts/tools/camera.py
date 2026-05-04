"""Camera tools: create cameras, set properties, control viewport camera."""

from _common import _exec, _exec_ue, actor_exec
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Camera", output_kind="small", default_timeout=60)
    def create_camera(x: float = 0.0, y: float = 0.0, z: float = 200.0,
                      yaw: float = 0.0, pitch: float = 0.0, fov: float = 90.0) -> str:
        """Create a CameraActor at a location with optional FOV."""
        return _exec_ue(f"""
cam_class = unreal.load_class(None, "/Script/Engine.CameraActor")
loc = unreal.Vector(x={x}, y={y}, z={z})
rot = unreal.Rotator(pitch={pitch}, yaw={yaw}, roll=0)
cam = unreal.EditorLevelLibrary.spawn_actor_from_class(cam_class, loc, rot)
if cam:
    cam_comp = cam.get_components_by_class(unreal.CameraComponent)
    if cam_comp:
        cam_comp[0].set_editor_property("FieldOfView", {fov})
    print(f"Created {{cam.get_actor_label()}} at ({x}, {y}, {z}) yaw={yaw} pitch={pitch} FOV={fov}")
else:
    print("ERROR: Failed to create camera")
""")

    @mcp.tool()
    @bob_tool(category="Camera", output_kind="small", default_timeout=60)
    def set_camera_properties(actor_label: str, fov: float = -1.0,
                              aperture: float = -1.0,
                              focus_distance: float = -1.0) -> str:
        """Set camera lens properties. Pass -1 to leave unchanged. aperture is f-stop (e.g. 2.8), focus_distance in cm."""
        return actor_exec(actor_label, f"""
cam_comp = target.get_components_by_class(unreal.CameraComponent)
if not cam_comp:
    print(f"ERROR: {{target.get_actor_label()}} has no camera component")
else:
    cc = cam_comp[0]
    changes = []
    if {fov} > 0:
        cc.set_editor_property("FieldOfView", {fov})
        changes.append(f"FOV={fov}")
    if {aperture} > 0:
        cc.set_editor_property("PostProcessSettings.DepthOfFieldFstop", {aperture})
        changes.append(f"aperture={aperture}")
    if {focus_distance} > 0:
        cc.set_editor_property("PostProcessSettings.DepthOfFieldFocalDistance", {focus_distance})
        changes.append(f"focus_distance={focus_distance}")
    if changes:
        print(f"Updated {{target.get_actor_label()}}: {{', '.join(changes)}}")
    else:
        print("No properties changed")
""")

    @mcp.tool()
    @bob_tool(category="Camera", output_kind="large", default_timeout=60)
    def get_active_viewport_camera() -> str:
        """Get the editor viewport camera location, rotation, and FOV."""
        return _exec_ue("""
loc, rot = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
print(f"Viewport Camera:")
print(f"  Location: ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
print(f"  Rotation: pitch={rot.pitch:.1f} yaw={rot.yaw:.1f} roll={rot.roll:.1f}")
""")

    @mcp.tool()
    @bob_tool(category="Camera", output_kind="small", default_timeout=60)
    def set_viewport_camera(x: float = 0.0, y: float = 0.0, z: float = 0.0,
                            yaw: float = 0.0, pitch: float = 0.0) -> str:
        """Teleport the editor viewport camera to a position and rotation."""
        return _exec_ue(f"""
loc = unreal.Vector(x={x}, y={y}, z={z})
rot = unreal.Rotator(pitch={pitch}, yaw={yaw}, roll=0)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
print(f"Viewport camera moved to ({x}, {y}, {z}) pitch={pitch} yaw={yaw}")
""")
