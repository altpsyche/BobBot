"""Viewport and utility tools: screenshots, console commands, output log."""

import os


def register(mcp, send_fn):

    def _exec(code):
        result = send_fn({"type": "execute", "code": code})
        if result.get("success"):
            output = result.get("output", "")
            err = result.get("error")
            if err:
                output += "\nStderr: " + err
            return output if output.strip() else "(executed successfully, no output)"
        return "Error: " + result.get("error", "Unknown error")

    @mcp.tool()
    def capture_viewport(filename: str = "viewport_capture.png",
                         width: int = 1920, height: int = 1080) -> str:
        """Take a screenshot of the current viewport. Returns the file path to the saved image."""
        project_root = os.environ.get("BOB_PROJECT_ROOT", "")
        capture_dir = os.path.join(project_root, "Saved", "BobBot", "Captures")
        capture_path = os.path.join(capture_dir, filename).replace("\\", "/")
        return _exec(f"""
import unreal, os
capture_dir = r"{capture_dir}"
os.makedirs(capture_dir, exist_ok=True)
capture_path = r"{capture_path}"
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
        safe_cmd = command.replace('"', '\\"')
        return _exec(f"""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "{safe_cmd}")
print("Executed: {safe_cmd}")
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
