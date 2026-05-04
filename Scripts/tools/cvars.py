"""Console variable tools: read, write, list, and query CVars."""

from _common import _exec_ue, _safe
from _registry import bob_tool


def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Console Variables", output_kind="large", default_timeout=60)
    def get_cvar(name: str) -> str:
        """Get the current value of a console variable (CVar). Tries int and float accessors first, then falls back to executing the CVar name as a console command to read the value from the log."""
        return _exec_ue(f"""
name = {_safe(name)}
int_val = unreal.SystemLibrary.get_console_variable_int_value(name)
float_val = unreal.SystemLibrary.get_console_variable_float_value(name)
if int_val != 0:
    print(f"{{name}} = {{int_val}} (int)")
elif float_val != 0.0:
    print(f"{{name}} = {{float_val}} (float)")
else:
    # Both returned 0 - could be actual 0 or unrecognized. Execute as console command to read output.
    import os
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        unreal.SystemLibrary.execute_console_command(world, name)
        # Read the last few lines of the log to find the printed value
        log_dir = str(unreal.Paths.project_log_dir())
        if os.path.isdir(log_dir):
            log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
            if log_files:
                log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
                log_path = os.path.join(log_dir, log_files[0])
                with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
                    lines = f.readlines()
                # Look for the cvar value in the last 10 lines
                found = False
                for line in lines[-10:]:
                    if name.lower() in line.lower():
                        print(line.strip())
                        found = True
                if not found:
                    print(f"{{name}} = 0 (both int and float returned 0; no log output found)")
        else:
            print(f"{{name}} = 0 (both int and float returned 0; log dir not found)")
    else:
        print(f"{{name}} = 0 (both int and float returned 0; no world available)")
""")

    @mcp.tool()
    @bob_tool(category="Console Variables", output_kind="small", default_timeout=60)
    def set_cvar(name: str, value: str) -> str:
        """Set a console variable (CVar) to a new value. Executes '{name} {value}' as a console command."""
        return _exec_ue(f"""
name = {_safe(name)}
value = {_safe(value)}
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    unreal.SystemLibrary.execute_console_command(world, name + " " + value)
    print(f"Set {{name}} = {{value}}")
""")

    @mcp.tool()
    @bob_tool(category="Console Variables", output_kind="large", default_timeout=60)
    def list_cvars(pattern: str = "") -> str:
        """List console variables matching a pattern. Executes 'cvarlist {pattern}' and reads matching lines from the log. Limited to 50 results."""
        return _exec_ue(f"""
import os
pattern = {_safe(pattern)}
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    cmd = "cvarlist " + pattern if pattern else "cvarlist"
    unreal.SystemLibrary.execute_console_command(world, cmd)
    log_dir = str(unreal.Paths.project_log_dir())
    if os.path.isdir(log_dir):
        log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
        if log_files:
            log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
            log_path = os.path.join(log_dir, log_files[0])
            with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
            # Filter last 100 lines for cvar entries matching the pattern
            matches = []
            for line in lines[-100:]:
                stripped = line.strip()
                if pattern and pattern.lower() in stripped.lower():
                    matches.append(stripped)
                elif not pattern and ("CVar" in stripped or "cvar" in stripped.lower()):
                    matches.append(stripped)
            if matches:
                for m in matches[:50]:
                    print(m)
                if len(matches) > 50:
                    print()
                    print(f"... and {{len(matches) - 50}} more (showing first 50)")
                print()
                print(f"Total matches: {{min(len(matches), 50)}} shown")
            else:
                print(f"No CVars found matching '{{pattern}}'")
        else:
            print("ERROR: No log files found")
    else:
        print("ERROR: Log directory not found")
""")

    @mcp.tool()
    @bob_tool(category="Console Variables", output_kind="small", default_timeout=60)
    def reset_cvar(name: str) -> str:
        """Query a console variable's current value and help text. Executes the CVar name without a value, which causes UE to print its current/default value to the log.

        Note: This does not truly reset the CVar to its engine default.
        Use set_cvar() to explicitly set it back if you know the default value."""
        return _exec_ue(f"""
import os
name = {_safe(name)}
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No world available")
else:
    # Executing a CVar name without value prints its current value and help text
    unreal.SystemLibrary.execute_console_command(world, name)
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
                if name.lower() in line.lower():
                    print(line.strip())
                    found = True
            if not found:
                print(f"Executed '{{name}}' (no value) - check Output Log for default value")
        else:
            print("ERROR: No log files found")
    else:
        print("ERROR: Log directory not found")
""")
