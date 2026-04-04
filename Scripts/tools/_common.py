"""
Shared helpers for BobBot MCP tool modules.

Eliminates the duplicated _exec() closure from 41 tool files.
Initialized by the bridge before tool discovery:
    import _common
    _common.init(_send_and_receive)
"""

import inspect
import json
import threading

_send_fn = None
_send_lock = threading.Lock()


def init(send_fn):
    """Set the send function for tool execution. Called by the bridge on startup."""
    global _send_fn
    with _send_lock:
        _send_fn = send_fn


def _get_tool_name():
    """Walk the call stack to find the MCP tool function name.
    The chain is: tool_function -> _exec (or tool_function -> actor_exec -> _exec).
    Returns the first non-internal function name."""
    frame = inspect.currentframe()
    try:
        caller = frame.f_back  # _exec's caller
        if caller:
            name = caller.f_code.co_name
            # If called via actor_exec, go one more level up
            if name in ("_exec", "actor_exec"):
                caller = caller.f_back
                name = caller.f_code.co_name if caller else ""
            return name
    finally:
        del frame
    return ""


def _exec(code):
    """Execute Python code in the UE editor and return the output.
    Automatically detects the calling tool name for permission classification."""
    tool_name = _get_tool_name()
    with _send_lock:
        fn = _send_fn
        if fn is None:
            return "Error: _common not initialized — call init(send_fn) first"
        result = fn({"type": "execute", "code": code, "tool_name": tool_name})
    if not isinstance(result, dict) or "success" not in result:
        return "Error: Malformed response from UE"
    if result.get("success"):
        output = result.get("output", "")
        err = result.get("error")
        if err:
            output += "\nStderr: " + err
        return output if output.strip() else "(executed successfully, no output)"
    return "Error: " + result.get("error", "Unknown error")


# --------------------------------------------------------------------------- #
# Helpers: eliminate boilerplate from tool files
# --------------------------------------------------------------------------- #

def _exec_ue(code):
    """Execute code with `import unreal` pre-applied."""
    return _exec("import unreal\n" + code)


# --- Actor lookup (used by 20+ tools) ---

_FIND_ACTOR = """\
import unreal
target = None
for _a in unreal.EditorLevelLibrary.get_all_level_actors():
    if _a.get_actor_label() == {label_json}:
        target = _a
        break
if target is None:
    print("ERROR: Actor '{{}}' not found".format({label_json}))
"""

def actor_exec(label, code):
    """Execute code with `target` bound to the actor with the given label.

    If the actor isn't found, prints an error and skips the code.
    The code string can be an f-string for additional parameter substitution.
    `import unreal` is already done. Use `target` to reference the found actor.
    """
    safe_label = json.dumps(label)
    preamble = _FIND_ACTOR.format(label_json=safe_label)
    # Indent the user code under "else:" so it only runs if found
    indented = "\n".join("    " + line for line in code.strip().splitlines())
    return _exec(preamble + "else:\n" + indented + "\n")


# --- Asset lookup (used by 10+ tools) ---

_FIND_ASSET = """\
import unreal
asset = unreal.EditorAssetLibrary.load_asset({path_json})
if asset is None:
    print("ERROR: Asset '{{}}' not found".format({path_json}))
"""

def asset_exec(path, code):
    """Execute code with `asset` bound to the loaded asset at `path`.

    If the asset doesn't exist, prints an error and skips the code.
    `import unreal` is already done. Use `asset` to reference the loaded asset.
    """
    safe_path = json.dumps(path)
    preamble = _FIND_ASSET.format(path_json=safe_path)
    indented = "\n".join("    " + line for line in code.strip().splitlines())
    return _exec(preamble + "else:\n" + indented + "\n")
