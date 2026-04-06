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

def _safe(s):
    """JSON-quote a string for safe embedding in generated Python code.

    Handles quotes, backslashes, newlines, and all special chars.
    Result includes surrounding double-quotes::

        f'name = {_safe(user_input)}'  ->  name = "properly escaped"
    """
    return json.dumps(str(s))


def _error(msg):
    """Print a standardized error message. Use instead of print('ERROR: ...')."""
    print("ERROR: " + str(msg))


def _exec_ue(code):
    """Execute code with `import unreal` pre-applied."""
    return _exec("import unreal\n" + code)


# --- Actor lookup (used by 20+ tools) ---

_FIND_ACTOR = """\
import unreal
_actor_label = {label_json}
target = None
for _a in unreal.EditorLevelLibrary.get_all_level_actors():
    if _a.get_actor_label() == _actor_label:
        target = _a
        break
if target is None:
    print("ERROR: Actor " + _actor_label + " not found")
"""

def actor_exec(label, code):
    """Execute code with `target` bound to the actor with the given label.

    If the actor isn't found, prints an error. The user code runs after the
    lookup and should check `target` as needed (or just use it — an
    AttributeError on None is caught by _exec's error handling and the
    error message from the preamble still reaches the output).

    `import unreal` is already done. Use `target` to reference the found actor.
    """
    safe_label = json.dumps(label)
    preamble = _FIND_ACTOR.format(label_json=safe_label)
    # User code runs at top level after the preamble. No indentation.
    # If target is None, user code will fail on first target.xxx() call,
    # but the error message was already printed by the preamble.
    return _exec(preamble + code)


# --- Asset lookup (used by 10+ tools) ---

_FIND_ASSET = """\
import unreal
_asset_path = {path_json}
asset = unreal.EditorAssetLibrary.load_asset(_asset_path)
if asset is None:
    print("ERROR: Asset " + _asset_path + " not found")
"""

def asset_exec(path, code):
    """Execute code with `asset` bound to the loaded asset at `path`.

    If the asset doesn't exist, prints an error. Same behavior as actor_exec.
    `import unreal` is already done. Use `asset` to reference the loaded asset.
    """
    safe_path = json.dumps(path)
    preamble = _FIND_ASSET.format(path_json=safe_path)
    return _exec(preamble + code)
