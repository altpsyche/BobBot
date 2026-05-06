"""Editor notification tools: show toast messages and write to the output log."""

from _common import _exec_ue, _safe
from _registry import bob_tool


def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Notifications", output_kind="small", default_timeout=60)
    def show_editor_notification(message: str, severity: str = "info",
                                 duration: float = 5.0) -> str:
        """Show a notification in the UE editor output log.
        severity: 'info', 'warning', 'error'.
        duration: how long a toast notification stays visible (seconds, default 5)."""
        return _exec_ue(f"""
sev = {_safe(severity)}.lower()
msg = {_safe(message)}
if sev == "error":
    unreal.log_error(msg)
elif sev == "warning":
    unreal.log_warning(msg)
else:
    unreal.log(msg)

# Also show an on-screen notification toast if available
try:
    duration = {duration}
    world = unreal.EditorLevelLibrary.get_editor_world()
    if sev == "error":
        unreal.SystemLibrary.print_string(
            world, msg, screen_duration=duration,
            text_color=unreal.LinearColor(r=1.0, g=0.2, b=0.2, a=1.0))
    elif sev == "warning":
        unreal.SystemLibrary.print_string(
            world, msg, screen_duration=duration,
            text_color=unreal.LinearColor(r=1.0, g=0.8, b=0.0, a=1.0))
    else:
        unreal.SystemLibrary.print_string(
            world, msg, screen_duration=duration)
except Exception:
    pass

print(f"Notification ({{sev}}): {{msg}}")
""")


    @mcp.tool()
    @bob_tool(category="Notifications", output_kind="small", default_timeout=60)
    def log_to_output(message: str, category: str = "BobBot",
                      verbosity: str = "log") -> str:
        """Write a message to the UE Output Log with a category prefix.
        verbosity: 'log', 'warning', 'error'."""
        return _exec_ue(f"""
cat = {_safe(category)}
msg = "[" + cat + "] " + {_safe(message)}
v = {_safe(verbosity)}.lower()
if v == "error":
    unreal.log_error(msg)
elif v == "warning":
    unreal.log_warning(msg)
else:
    unreal.log(msg)
print(f"Logged ({{v}}): {{msg}}")
""")
