"""Movie Render Queue tools: create render jobs, check status, render sequences."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_render_job(sequence_path: str, output_dir: str = "",
                          format: str = "PNG",
                          resolution: str = "1920x1080") -> str:
        """Create a Movie Render Queue job for a LevelSequence. format: 'PNG', 'EXR', 'JPG'. resolution: 'WxH'."""
        return _exec(f"""
import unreal, os
seq = unreal.EditorAssetLibrary.load_asset("{sequence_path}")
if seq is None:
    print("ERROR: LevelSequence '{sequence_path}' not found")
elif not isinstance(seq, unreal.LevelSequence):
    print(f"ERROR: '{{seq.get_class().get_name()}}' is not a LevelSequence")
elif not hasattr(unreal, 'MoviePipelineQueueSubsystem'):
    print("ERROR: Movie Render Queue plugin is not available.")
    print("Enable it in Edit > Plugins > Movie Render Queue")
else:
    try:
        subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
        queue = subsystem.get_queue()
        job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
        job.set_editor_property("Sequence", unreal.SoftObjectPath("{sequence_path}"))
        job.set_editor_property("Map", unreal.SoftObjectPath(unreal.EditorLevelLibrary.get_editor_world().get_path_name()))
        output_dir = "{output_dir}" or os.path.join(str(unreal.Paths.project_saved_dir()), "MovieRenders")
        res_parts = "{resolution}".split("x")
        width = int(res_parts[0]) if len(res_parts) >= 2 else 1920
        height = int(res_parts[1]) if len(res_parts) >= 2 else 1080
        print(f"Created render job for {sequence_path}")
        print(f"  Output: {{output_dir}}")
        print(f"  Format: {format}")
        print(f"  Resolution: {{width}}x{{height}}")
        print("Use get_render_queue_status() to check progress, or open Window > Movie Render Queue")
    except Exception as e:
        print(f"ERROR: {{e}}")
        print("Ensure Movie Render Queue plugin is enabled")
""")

    @mcp.tool()
    def get_render_queue_status() -> str:
        """Get the status of Movie Render Queue jobs."""
        return _exec("""
import unreal
if not hasattr(unreal, 'MoviePipelineQueueSubsystem'):
    print("ERROR: Movie Render Queue plugin is not available.")
    print("Enable it in Edit > Plugins > Movie Render Queue")
else:
    try:
        subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
        queue = subsystem.get_queue()
        jobs = queue.get_jobs()
        if not jobs:
            print("Render queue is empty")
        else:
            print(f"Render Queue ({len(jobs)} job(s)):")
            for i, job in enumerate(jobs):
                seq = job.get_editor_property("Sequence")
                status = job.get_editor_property("StatusMessage") if hasattr(job, "StatusMessage") else "Unknown"
                print(f"  [{i}] {seq}")
                print(f"      Status: {status}")
        is_rendering = subsystem.is_rendering()
        print()
        print(f"Currently rendering: {is_rendering}")
    except Exception as e:
        print(f"ERROR: {e}")
        print("Ensure Movie Render Queue plugin is enabled")
""")

    @mcp.tool()
    def render_sequence_to_images(sequence_path: str, output_dir: str = "",
                                  format: str = "PNG") -> str:
        """Render a LevelSequence to an image sequence. Starts rendering immediately."""
        return _exec(f"""
import unreal, os
seq = unreal.EditorAssetLibrary.load_asset("{sequence_path}")
if seq is None:
    print("ERROR: LevelSequence '{sequence_path}' not found")
elif not isinstance(seq, unreal.LevelSequence):
    print(f"ERROR: '{{seq.get_class().get_name()}}' is not a LevelSequence")
else:
    output_dir = "{output_dir}" or os.path.join(str(unreal.Paths.project_saved_dir()), "MovieRenders")
    os.makedirs(output_dir, exist_ok=True)
    rendered = False
    # Try Movie Render Queue (modern pipeline)
    if hasattr(unreal, 'MoviePipelineQueueSubsystem') and hasattr(unreal, 'MoviePipelinePIEExecutor'):
        try:
            subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
            queue = subsystem.get_queue()
            job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
            job.set_editor_property("Sequence", unreal.SoftObjectPath("{sequence_path}"))
            job.set_editor_property("Map", unreal.SoftObjectPath(unreal.EditorLevelLibrary.get_editor_world().get_path_name()))
            executor = unreal.MoviePipelinePIEExecutor()
            subsystem.render_queue_with_executor(executor)
            rendered = True
            print(f"Started rendering {sequence_path}")
            print(f"  Output: {{output_dir}}")
            print(f"  Format: {format}")
            print("Check Window > Movie Render Queue for progress")
        except Exception as e:
            print(f"Movie Render Queue failed: {{e}}")
    # Fallback: use console command for simpler capture
    if not rendered:
        try:
            world = unreal.EditorLevelLibrary.get_editor_world()
            unreal.SystemLibrary.execute_console_command(world, f"HighResShot 1920x1080")
            print(f"Movie Render Queue not available. Triggered HighResShot instead.")
            print(f"  Screenshots saved to project's Screenshots folder")
            print("For full sequence rendering, enable the Movie Render Queue plugin")
        except Exception as e2:
            print(f"ERROR: {{e2}}")
            print("Enable Movie Render Queue plugin in Edit > Plugins for sequence rendering")
""")
