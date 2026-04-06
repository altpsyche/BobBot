"""View mode tools: switch viewport rendering modes and toggle show flags."""

from _common import _exec_ue, _safe


def register(mcp, send_fn):


    @mcp.tool()
    def set_view_mode(mode: str) -> str:
        """Set the viewport rendering mode. Supported modes: Lit, Unlit, Wireframe, DetailLighting, LightingOnly, ReflectionOverride, CollisionPawn, CollisionVisibility, PathTracing."""
        return _exec_ue(f"""
mode = {_safe(mode)}
mode_map = {{
    "Lit": "Lit",
    "Unlit": "Unlit",
    "Wireframe": "BrushWireframe",
    "DetailLighting": "Lit_DetailLighting",
    "LightingOnly": "LightingOnly",
    "ReflectionOverride": "ReflectionOverride",
    "CollisionPawn": "CollisionPawn",
    "CollisionVisibility": "CollisionVis",
    "PathTracing": "PathTracing",
}}
mapped = mode_map.get(mode)
if mapped is None:
    print(f"ERROR: Unknown view mode '{{mode}}'. Valid modes: {{', '.join(mode_map.keys())}}")
else:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        print("ERROR: No world available")
    else:
        unreal.SystemLibrary.execute_console_command(world, f"viewmode {{mapped}}")
        print(f"View mode set to {{mode}} ({{mapped}})")
""")

    @mcp.tool()
    def get_view_mode() -> str:
        """Get the current viewport rendering mode. Reads the output of the 'viewmode' console command from the log."""
        return _exec_ue("""
import os
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    unreal.SystemLibrary.execute_console_command(world, "viewmode")
    log_dir = str(unreal.Paths.project_log_dir())
    if os.path.isdir(log_dir):
        log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
        if log_files:
            log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
            log_path = os.path.join(log_dir, log_files[0])
            with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
            found = False
            for line in lines[-10:]:
                stripped = line.strip()
                if "viewmode" in stripped.lower():
                    print(stripped)
                    found = True
            if not found:
                print("Could not determine current view mode from log.")
                print("Use capture_viewport() to see the current view mode visually.")
        else:
            print("ERROR: No log files found")
    else:
        print("ERROR: Log directory not found")
""")

    @mcp.tool()
    def set_show_flag(flag: str) -> str:
        """Toggle a show flag in the viewport. The UE 'show' command is always a toggle — calling this twice flips it back. Common flags: Collision, Bounds, Grid, Navigation, Volumes, BSP, Fog, Particles, Lighting, PostProcessing."""
        return _exec_ue(f"""
flag = {_safe(flag)}
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    unreal.SystemLibrary.execute_console_command(world, "show " + flag)
    print(f"Toggled show flag '{{flag}}'")
    print("Note: 'show' is a toggle. Call again to flip back.")
""")

    @mcp.tool()
    def get_show_flags() -> str:
        """Get the current show flags state. Executes 'show' with no arguments and reads the output from the log."""
        return _exec_ue("""
import os
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    unreal.SystemLibrary.execute_console_command(world, "show")
    log_dir = str(unreal.Paths.project_log_dir())
    if os.path.isdir(log_dir):
        log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
        if log_files:
            log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
            log_path = os.path.join(log_dir, log_files[0])
            with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
            # Read last 100 lines for show flag output
            show_lines = []
            for line in lines[-100:]:
                stripped = line.strip()
                if "show" in stripped.lower() or "flag" in stripped.lower():
                    show_lines.append(stripped)
            if show_lines:
                print(f"Show flags output ({len(show_lines)} lines):")
                for sl in show_lines:
                    print(f"  {sl}")
            else:
                print("No show flag output found in recent log lines.")
                print("The 'show' command output may vary by UE version.")
        else:
            print("ERROR: No log files found")
    else:
        print("ERROR: Log directory not found")
""")
