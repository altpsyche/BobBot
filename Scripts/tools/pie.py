"""Play-In-Editor tools: start/stop PIE, check status, inspect game world."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def start_pie() -> str:
        """Start Play-In-Editor session."""
        return _exec("""
import unreal
try:
    # Use EditorLevelLibrary or console command
    unreal.EditorLevelLibrary.editor_play_simulate()
    print("Started Play-In-Editor")
except Exception as e:
    # Fallback to console command
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "PIE")
        print("Started Play-In-Editor via console")
    except Exception as e2:
        print(f"ERROR: Could not start PIE: {e2}")
""")

    @mcp.tool()
    def stop_pie() -> str:
        """Stop the current Play-In-Editor session."""
        return _exec("""
import unreal
try:
    unreal.EditorLevelLibrary.editor_end_play()
    print("Stopped Play-In-Editor")
except Exception as e:
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "EXIT")
        print("Stopped PIE via console")
    except Exception as e2:
        print(f"ERROR: Could not stop PIE: {e2}")
""")

    @mcp.tool()
    def is_pie_running() -> str:
        """Check if a Play-In-Editor session is currently active."""
        return _exec("""
import unreal
try:
    is_playing = unreal.EditorLevelLibrary.editor_get_play_world() is not None
    if is_playing:
        print("PIE is RUNNING")
    else:
        print("PIE is NOT running")
except:
    print("PIE status: unknown (API not available)")
""")

    @mcp.tool()
    def get_pie_actors(class_filter: str = "") -> str:
        """Get actors in the game world during PIE. Optional class_filter like 'Character', 'Pawn'."""
        return _exec(f"""
import unreal
try:
    pie_world = unreal.EditorLevelLibrary.editor_get_play_world()
    if pie_world is None:
        print("ERROR: PIE is not running. Start PIE first with start_pie()")
    else:
        actors = unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.Actor)
        class_filter = "{class_filter}".lower()
        count = 0
        for a in actors:
            cls = a.get_class().get_name()
            if class_filter and class_filter not in cls.lower():
                continue
            loc = a.get_actor_location()
            print(f"{{a.get_name()}} ({{cls}}) at ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
            count += 1
            if count >= 100:
                print("... (truncated at 100)")
                break
        print(f"\\nTotal: {{count}} actors")
except Exception as e:
    print(f"ERROR: Could not access PIE world: {{e}}")
""")

    @mcp.tool()
    def execute_pie_console_command(command: str) -> str:
        """Execute a console command in the game world during PIE."""
        safe_cmd = command.replace('"', '\\"')
        return _exec(f"""
import unreal
try:
    pie_world = unreal.EditorLevelLibrary.editor_get_play_world()
    if pie_world is None:
        print("ERROR: PIE is not running. Start PIE first with start_pie()")
    else:
        unreal.SystemLibrary.execute_console_command(pie_world, "{safe_cmd}")
        print(f"Executed in PIE: {safe_cmd}")
except Exception as e:
    print(f"ERROR: {{e}}")
""")
