"""Editor notification tools: show toast messages and write to the output log."""

from _common import _exec_ue


def register(mcp, send_fn):


    @mcp.tool()
    def show_editor_notification(message: str, severity: str = "info",
                                 duration: float = 5.0) -> str:
        """Show a notification in the UE editor output log.
        severity: 'info', 'warning', 'error'.
        duration: how long a toast notification stays visible (seconds, default 5)."""
        safe_msg = message.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        return _exec_ue(f"""
sev = "{severity}".lower()
msg = "{safe_msg}"
if sev == "error":
    unreal.log_error(msg)
elif sev == "warning":
    unreal.log_warning(msg)
else:
    unreal.log(msg)

# Also show an on-screen notification toast if available
try:
    duration = {duration}
    if sev == "error":
        color = unreal.AppReturnType.NO  # just for the toast color
        unreal.SystemLibrary.print_string(
            unreal.EditorLevelLibrary.get_editor_world(),
            msg, screen_duration=duration)
    else:
        unreal.SystemLibrary.print_string(
            unreal.EditorLevelLibrary.get_editor_world(),
            msg, screen_duration=duration)
except Exception:
    pass

print(f"Notification ({{sev}}): {{msg}}")
""")


    @mcp.tool()
    def log_to_output(message: str, category: str = "BobBot",
                      verbosity: str = "log") -> str:
        """Write a message to the UE Output Log with a category prefix.
        verbosity: 'log', 'warning', 'error'."""
        safe_msg = message.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        safe_cat = category.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        return _exec_ue(f"""
msg = "[{safe_cat}] " + "{safe_msg}"
v = "{verbosity}".lower()
if v == "error":
    unreal.log_error(msg)
elif v == "warning":
    unreal.log_warning(msg)
else:
    unreal.log(msg)
print(f"Logged ({{v}}): {{msg}}")
""")
