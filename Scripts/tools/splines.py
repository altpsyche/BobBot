"""Spline tools: create spline actors, add points, inspect, and set meshes."""

from _common import _exec, _exec_ue, actor_exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_spline_actor(points: str, closed: bool = False) -> str:
        """Create a spline actor with given points. points is semicolon-separated 'x,y,z' like '0,0,0;100,0,0;100,100,0'. closed=True to close the loop."""
        return _exec_ue(f"""
# Parse points
point_strs = "{points}".split(";")
spline_points = []
for ps in point_strs:
    parts = [float(p.strip()) for p in ps.strip().split(",")]
    if len(parts) >= 3:
        spline_points.append(unreal.Vector(x=parts[0], y=parts[1], z=parts[2]))
if len(spline_points) < 2:
    print("ERROR: Need at least 2 points. Format: 'x,y,z;x,y,z;...'")
else:
    # Spawn an empty actor and add a spline component
    actor_class = unreal.load_class(None, "/Script/Engine.Actor")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, spline_points[0])
    if actor:
        actor.set_actor_label("SplineActor")
        spline = actor.add_component_by_class(unreal.SplineComponent, False, unreal.Transform(), False)
        if spline:
            spline.clear_spline_points()
            for i, pt in enumerate(spline_points):
                spline.add_spline_point_at_index(pt, i, unreal.SplineCoordinateSpace.WORLD, True)
            spline.set_closed_loop({closed})
            spline.update_spline()
            closed_str = " (closed)" if {closed} else ""
            print(f"Created SplineActor with {{len(spline_points)}} points{{closed_str}}")
        else:
            print("ERROR: Failed to add SplineComponent")
    else:
        print("ERROR: Failed to spawn actor")
""")

    @mcp.tool()
    def get_spline_info(actor_label: str) -> str:
        """Get point count, length, and closed state of a spline on an actor."""
        return actor_exec(actor_label, f"""
spline_comps = target.get_components_by_class(unreal.SplineComponent)
if not spline_comps:
    print(f"ERROR: {{target.get_actor_label()}} has no SplineComponent")
else:
    sp = spline_comps[0]
    num_points = sp.get_number_of_spline_points()
    length = sp.get_spline_length()
    closed = sp.is_closed_loop()
    print(f"Spline on {{target.get_actor_label()}}:")
    print(f"  Points: {{num_points}}")
    print(f"  Length: {{length:.0f}} cm")
    print(f"  Closed: {{closed}}")
    for i in range(num_points):
        loc = sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
        print(f"  Point {{i}}: ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
""")

    @mcp.tool()
    def add_spline_point(actor_label: str, x: float = 0.0, y: float = 0.0,
                         z: float = 0.0, index: int = -1) -> str:
        """Add a point to a spline. index=-1 appends at the end."""
        return actor_exec(actor_label, f"""
spline_comps = target.get_components_by_class(unreal.SplineComponent)
if not spline_comps:
    print(f"ERROR: {{target.get_actor_label()}} has no SplineComponent")
else:
    sp = spline_comps[0]
    point = unreal.Vector(x={x}, y={y}, z={z})
    idx = {index}
    if idx < 0:
        idx = sp.get_number_of_spline_points()
    sp.add_spline_point_at_index(point, idx, unreal.SplineCoordinateSpace.WORLD, True)
    sp.update_spline()
    print(f"Added spline point at ({x}, {y}, {z}) index={{idx}} on {{target.get_actor_label()}}")
""")

    @mcp.tool()
    def set_spline_mesh(actor_label: str, mesh_path: str) -> str:
        """Set a mesh that follows the spline by creating SplineMeshComponents along its length."""
        return actor_exec(actor_label, f"""
spline_comps = target.get_components_by_class(unreal.SplineComponent)
if not spline_comps:
    print(f"ERROR: {{target.get_actor_label()}} has no SplineComponent")
else:
    mesh = unreal.EditorAssetLibrary.load_asset("{mesh_path}")
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        print("ERROR: Static mesh '{mesh_path}' not found")
    else:
        sp = spline_comps[0]
        num_points = sp.get_number_of_spline_points()
        segments = max(1, num_points - 1)
        count = 0
        for i in range(segments):
            start = sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.LOCAL)
            start_tan = sp.get_tangent_at_spline_point(i, unreal.SplineCoordinateSpace.LOCAL)
            end = sp.get_location_at_spline_point(i + 1, unreal.SplineCoordinateSpace.LOCAL)
            end_tan = sp.get_tangent_at_spline_point(i + 1, unreal.SplineCoordinateSpace.LOCAL)
            smc = target.add_component_by_class(unreal.SplineMeshComponent, False, unreal.Transform(), False)
            if smc:
                smc.set_static_mesh(mesh)
                smc.set_start_and_end(start, start_tan, end, end_tan, True)
                count += 1
        print(f"Created {{count}} SplineMeshComponent(s) along spline using {mesh_path}")
""")
