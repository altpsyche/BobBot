"""Debug and profiling tools: frame stats, memory, GPU stats, and benchmarking."""

from _common import _exec_ue

def register(mcp, send_fn):


    @mcp.tool()
    def get_frame_stats() -> str:
        """Get FPS, frame time, draw calls, and triangle count."""
        return _exec_ue("""
world = unreal.EditorLevelLibrary.get_editor_world()
# Enable stat display
unreal.SystemLibrary.execute_console_command(world, "stat fps")
unreal.SystemLibrary.execute_console_command(world, "stat unit")
# Gather what we can programmatically
actors = unreal.EditorLevelLibrary.get_all_level_actors()
mesh_comps = 0
for a in actors:
    mesh_comps += len(a.get_components_by_class(unreal.StaticMeshComponent))
    mesh_comps += len(a.get_components_by_class(unreal.SkeletalMeshComponent))
print("Frame Statistics (enabled stat overlays in viewport):")
print(f"  Total Actors: {len(actors)}")
print(f"  Mesh Components: {mesh_comps}")
print("\\n  stat fps and stat unit enabled in viewport")
print("  Use get_output_log() to read stat data after a few frames")
""")

    @mcp.tool()
    def get_memory_stats() -> str:
        """Get memory usage breakdown."""
        return _exec_ue("""
world = unreal.EditorLevelLibrary.get_editor_world()
# Enable memory stats
unreal.SystemLibrary.execute_console_command(world, "stat memory")
# Read what we can from the log
import os
log_dir = str(unreal.Paths.project_log_dir())
print("Memory Statistics:")
if os.path.isdir(log_dir):
    log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
    if log_files:
        log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
        log_path = os.path.join(log_dir, log_files[0])
        with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
        mem_lines = [l.strip() for l in lines[-100:] if "memory" in l.lower() or "Memory" in l]
        if mem_lines:
            for line in mem_lines[-10:]:
                print(f"  {line}")
        else:
            print("  stat memory enabled in viewport")
            print("  Run get_output_log() to see memory data")
else:
    print("  stat memory enabled in viewport")
""")

    @mcp.tool()
    def get_gpu_stats() -> str:
        """Get GPU time, render target count, and shader complexity info."""
        return _exec_ue("""
world = unreal.EditorLevelLibrary.get_editor_world()
# Enable GPU stats
unreal.SystemLibrary.execute_console_command(world, "stat GPU")
unreal.SystemLibrary.execute_console_command(world, "stat RHI")
# Count scene complexity
actors = unreal.EditorLevelLibrary.get_all_level_actors()
light_count = 0
translucent_count = 0
particle_count = 0
for a in actors:
    light_count += len(a.get_components_by_class(unreal.LightComponentBase))
    particle_count += len(a.get_components_by_class(unreal.NiagaraComponent)) if hasattr(unreal, 'NiagaraComponent') else 0
print("GPU Statistics (enabled stat overlays):")
print(f"  Lights: {light_count}")
print(f"  Particle Systems: {particle_count}")
print(f"  Total Actors: {len(actors)}")
print("\\n  stat GPU and stat RHI enabled in viewport")
print("  Use get_output_log() to read detailed GPU timings")
""")

    @mcp.tool()
    def benchmark_scene(duration_seconds: float = 5.0) -> str:
        """Run a simple scene benchmark by enabling stats for a duration. Check get_output_log() after for results."""
        return _exec_ue(f"""
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    # Enable all relevant stats
    stats = ["stat fps", "stat unit", "stat RHI", "stat SceneRendering"]
    for s in stats:
        unreal.SystemLibrary.execute_console_command(world, s)
    # Count scene elements
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    static_meshes = 0
    skeletal_meshes = 0
    lights = 0
    particles = 0
    for a in actors:
        static_meshes += len(a.get_components_by_class(unreal.StaticMeshComponent))
        skeletal_meshes += len(a.get_components_by_class(unreal.SkeletalMeshComponent))
        lights += len(a.get_components_by_class(unreal.LightComponentBase))
    print(f"Benchmark started ({duration_seconds}s)")
    print(f"\\nScene Complexity:")
    print(f"  Actors: {{len(actors)}}")
    print(f"  Static Meshes: {{static_meshes}}")
    print(f"  Skeletal Meshes: {{skeletal_meshes}}")
    print(f"  Lights: {{lights}}")
    print(f"\\nStat overlays enabled in viewport.")
    print(f"Run get_output_log(50) after {duration_seconds}s to capture frame timing data.")
    print("Run the stat commands again to disable the overlays when done.")
""")
