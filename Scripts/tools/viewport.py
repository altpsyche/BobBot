"""Viewport and utility tools: screenshots, console commands, output log."""

from _common import _exec, _exec_ue, _safe


def register(mcp, send_fn):


    @mcp.tool()
    def capture_viewport(filename: str = "viewport_capture.png",
                         width: int = 1920, height: int = 1080) -> str:
        """Take a screenshot of the current viewport. Returns the file path to the saved image."""
        import os
        project_root = os.environ.get("BOB_PROJECT_ROOT", "")
        capture_dir = os.path.join(project_root, "Saved", "BobBot", "Captures")
        capture_path = os.path.join(capture_dir, filename).replace("\\", "/")
        return _exec(f"""
import unreal, os
capture_dir = {_safe(capture_dir)}
os.makedirs(capture_dir, exist_ok=True)
capture_path = {_safe(capture_path)}
result = unreal.AutomationLibrary.take_high_res_screenshot({width}, {height}, capture_path)
if result:
    print(f"Screenshot saved: {{capture_path}}")
else:
    # Fallback: request screenshot via editor command
    unreal.SystemLibrary.execute_console_command(
        unreal.EditorLevelLibrary.get_editor_world(),
        f"HighResShot {width}x{height}")
    print(f"Screenshot requested at {width}x{height} (saved to project Screenshots folder)")
""")

    @mcp.tool()
    def run_console_command(command: str) -> str:
        """Execute a UE console command. Examples: 'stat fps', 'stat unit', 'show collision', 'r.SetRes 1920x1080', 'obj list class=StaticMeshActor'."""
        return _exec(f"""
import unreal
cmd = {_safe(command)}
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, cmd)
print("Executed: " + cmd)
""")

    @mcp.tool()
    def get_output_log(lines: int = 50) -> str:
        """Get the last N lines from the UE output log. Useful for debugging after running tools."""
        return _exec(f"""
import unreal, os
# Read the most recent log file
log_dir = os.path.join(str(unreal.Paths.project_log_dir()))
if not os.path.isdir(log_dir):
    print("Log directory not found")
else:
    # Find the most recent .log file
    log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
    if not log_files:
        print("No log files found")
    else:
        log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
        log_path = os.path.join(log_dir, log_files[0])
        with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
            all_lines = f.readlines()
        tail = all_lines[-{lines}:]
        for line in tail:
            print(line.rstrip())
""")


    @mcp.tool()
    def set_viewport_resolution(width: int = 1920, height: int = 1080) -> str:
        """Set the editor viewport resolution. Common values: 1280x720, 1920x1080, 2560x1440, 3840x2160."""
        return _exec_ue(f"""
world = unreal.EditorLevelLibrary.get_editor_world()
cmd = "r.SetRes {width}x{height}"
unreal.SystemLibrary.execute_console_command(world, cmd)
print(f"Set viewport resolution to {width}x{height}")
""")


    @mcp.tool()
    def toggle_realtime_rendering() -> str:
        """Toggle realtime rendering in the active viewport.
        The UE 'Realtime' console command is a toggle — calling this sends the
        toggle command. Use capture_viewport to visually confirm the state."""
        return _exec_ue("""
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "Realtime")
print("Toggled realtime rendering")
print("Note: 'Realtime' is a toggle command. Call again to flip back.")
""")
