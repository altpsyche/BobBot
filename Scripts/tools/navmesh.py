"""Navigation mesh tools: build, inspect, and configure navmesh."""

from _common import _exec_ue
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Navigation", output_kind="small", default_timeout=180)
    def build_navigation() -> str:
        """Rebuild the navigation mesh for the current level."""
        return _exec_ue("""
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    try:
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
        print("Navigation rebuild initiated. This may take a moment for large levels.")
    except Exception as e:
        # Fallback: try NavigationSystemV1 approach
        try:
            nav_sys = unreal.NavigationSystemV1.get_navigation_system(world)
            if nav_sys:
                nav_sys.build()
                print("Navigation rebuild initiated via NavigationSystemV1.")
            else:
                print("ERROR: No NavigationSystem found in the current world.")
        except Exception as e2:
            print(f"ERROR: Could not rebuild navigation: {e2}")
            print("Try running 'RebuildNavigation' via run_console_command tool.")
""")

    @mcp.tool()
    @bob_tool(category="Navigation", output_kind="large", default_timeout=60)
    def get_navmesh_info() -> str:
        """Get navigation mesh information: NavMeshBoundsVolumes, RecastNavMesh settings (AgentRadius, AgentHeight, CellSize)."""
        return _exec_ue("""
actors = unreal.EditorLevelLibrary.get_all_level_actors()

# Count NavMeshBoundsVolumes
bounds_volumes = []
for a in actors:
    cls = a.get_class().get_name()
    if "NavMeshBoundsVolume" in cls:
        loc = a.get_actor_location()
        bounds_volumes.append(f"  {a.get_actor_label()} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")

print(f"NavMeshBoundsVolumes ({len(bounds_volumes)}):")
if bounds_volumes:
    for bv in bounds_volumes:
        print(bv)
else:
    print("  None found. Navigation requires at least one NavMeshBoundsVolume.")

# Find RecastNavMesh
print("")
recast = None
for a in actors:
    cls = a.get_class().get_name()
    if "RecastNavMesh" in cls:
        recast = a
        break

if recast is None:
    print("RecastNavMesh: Not found in level")
    print("  (Auto-generated when navigation is built with a NavMeshBoundsVolume)")
else:
    print(f"RecastNavMesh: {recast.get_actor_label()} ({recast.get_class().get_name()})")
    for prop_name in ["AgentRadius", "AgentHeight", "CellSize", "CellHeight",
                      "AgentMaxSlope", "AgentMaxStepHeight", "TileSizeUU",
                      "MinRegionArea", "MergeRegionSize"]:
        try:
            val = recast.get_editor_property(prop_name)
            print(f"  {prop_name}: {val}")
        except Exception:
            pass

# Also check NavigationSystem settings
print("")
try:
    world = unreal.EditorLevelLibrary.get_editor_world()
    nav_sys = unreal.NavigationSystemV1.get_navigation_system(world)
    if nav_sys:
        print("NavigationSystem: Active")
        try:
            auto_build = nav_sys.get_editor_property("bAutoCreateNavigationData")
            print(f"  AutoCreateNavigationData: {auto_build}")
        except Exception:
            pass
    else:
        print("NavigationSystem: Not active")
except Exception as e:
    print(f"NavigationSystem: Could not query ({e})")
""")

    @mcp.tool()
    @bob_tool(category="Navigation", output_kind="small", default_timeout=60)
    def set_navmesh_settings(agent_radius: float = -1.0, agent_height: float = -1.0, cell_size: float = -1.0) -> str:
        """Set properties on the RecastNavMesh actor. Only values >= 0 are applied. Call build_navigation() after to regenerate."""
        return _exec_ue(f"""
agent_radius = {agent_radius}
agent_height = {agent_height}
cell_size = {cell_size}

actors = unreal.EditorLevelLibrary.get_all_level_actors()
recast = None
for a in actors:
    cls = a.get_class().get_name()
    if "RecastNavMesh" in cls:
        recast = a
        break

if recast is None:
    print("ERROR: No RecastNavMesh actor found in level.")
    print("Place a NavMeshBoundsVolume and build navigation first.")
else:
    changes = []
    if agent_radius >= 0:
        try:
            recast.set_editor_property("AgentRadius", agent_radius)
            changes.append(f"AgentRadius = {{agent_radius}}")
        except Exception as e:
            print(f"WARNING: Could not set AgentRadius: {{e}}")
    if agent_height >= 0:
        try:
            recast.set_editor_property("AgentHeight", agent_height)
            changes.append(f"AgentHeight = {{agent_height}}")
        except Exception as e:
            print(f"WARNING: Could not set AgentHeight: {{e}}")
    if cell_size >= 0:
        try:
            recast.set_editor_property("CellSize", cell_size)
            changes.append(f"CellSize = {{cell_size}}")
        except Exception as e:
            print(f"WARNING: Could not set CellSize: {{e}}")
    if changes:
        print(f"Updated RecastNavMesh ({{recast.get_actor_label()}}):")
        for c in changes:
            print(f"  {{c}}")
        print("\\nReminder: Call build_navigation() to regenerate the navmesh with new settings.")
    else:
        print("No changes applied (all values < 0). Pass agent_radius, agent_height, or cell_size >= 0 to modify.")
""")
