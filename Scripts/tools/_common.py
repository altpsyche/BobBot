"""
Shared helpers for BobBot MCP tool modules.

Eliminates the duplicated _exec() closure from 41 tool files.
Initialized by the bridge before tool discovery:
    import _common
    _common.init(_send_and_receive)
"""

import inspect
import json
import os
import threading
import time

_send_fn = None
_send_lock = threading.Lock()

# Maximum output size returned to the AI client (bytes).
# Larger outputs are truncated to avoid eating conversation context.
_MAX_OUTPUT_BYTES = 32 * 1024  # 32 KB

# --------------------------------------------------------------------------- #
# Auto-capture state (Phase 2.4)
#
# When BOB_AUTO_CAPTURE_AFTER_EDITS=1, the @autocaptured decorator
# wraps eligible write tools and appends a viewport screenshot to the
# tool result as a multi-content response. The throttle prevents
# rapid-fire actor edits from generating one capture per call.
# --------------------------------------------------------------------------- #
_AUTO_CAPTURE_THROTTLE_S = 2.0
_last_capture_ts = 0.0
_capture_lock = threading.Lock()


def init(send_fn):
    """Set the send function for tool execution. Called by the bridge on startup."""
    global _send_fn
    with _send_lock:
        _send_fn = send_fn


_INTERNAL_FRAMES = ("_exec", "_exec_ue", "actor_exec", "asset_exec")


def _get_tool_name():
    """Walk the call stack to find the MCP tool function name.
    The chain may be:
        tool_function -> _exec
        tool_function -> actor_exec -> _exec
        tool_function -> asset_exec -> _exec
        tool_function -> _exec_ue -> _exec
    Skip every internal frame and return the first non-internal function name."""
    frame = inspect.currentframe()
    try:
        caller = frame.f_back  # _exec's caller
        # Walk past every internal helper frame
        while caller and caller.f_code.co_name in _INTERNAL_FRAMES:
            caller = caller.f_back
        return caller.f_code.co_name if caller else ""
    finally:
        del frame


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
        if not output.strip():
            return "(executed successfully, no output)"
        # Truncate oversized output to avoid eating conversation context
        if len(output) > _MAX_OUTPUT_BYTES:
            truncated_bytes = len(output) - _MAX_OUTPUT_BYTES
            output = (
                output[:_MAX_OUTPUT_BYTES]
                + f"\n\n... ({truncated_bytes:,} bytes truncated). "
                + "Narrow the query or use get_output_log to see full output."
            )
        return output
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


# --------------------------------------------------------------------------- #
# Auto-capture (Phase 2.4)
#
# Decorator for write tools. When BOB_AUTO_CAPTURE_AFTER_EDITS=1 is set
# and the throttle window allows, captures the viewport after the
# wrapped tool runs and returns a multi-content list:
#     [tool_result_text, fastmcp.Image(path=...), "Auto-captured: <path>"]
#
# FastMCP's _convert_to_content unpacks this into one TextContent block,
# one ImageContent block (which vision-capable models read directly), and
# a trailing TextContent block whose path triggers the in-chat image
# preview pipeline (the C++ ExtractImagePath scanner).
#
# Decorator-based rather than baked into _exec because FastMCP runs
# Pydantic validation on a tool function's annotated return type. Tools
# annotated `-> str` returning a list would fail validation. The
# decorator returns Any (no annotation forcing) and is applied per-tool.
# --------------------------------------------------------------------------- #
def _is_auto_capture_enabled():
    return os.environ.get("BOB_AUTO_CAPTURE_AFTER_EDITS", "0") == "1"


def _check_throttle():
    """Returns True if enough time has passed since the last capture.
    Updates _last_capture_ts as a side effect on success."""
    global _last_capture_ts
    with _capture_lock:
        now = time.monotonic()
        if now - _last_capture_ts < _AUTO_CAPTURE_THROTTLE_S:
            return False
        _last_capture_ts = now
        return True


def _capture_for_autocapture():
    """Capture the viewport via the bridge's send_fn.
    Returns the absolute file path on success, None on failure.
    Mirrors viewport.py's capture_viewport but with a unique filename."""
    if _send_fn is None:
        return None

    project_root = os.environ.get("BOB_PROJECT_ROOT", "")
    if not project_root:
        return None

    captures_dir = os.path.join(project_root, "Saved", "BobBot", "Captures")
    filename = "_autocapture_{}.png".format(int(time.time() * 1000))
    capture_path = os.path.join(captures_dir, filename).replace("\\", "/")

    code = (
        "import unreal, os\n"
        "os.makedirs({captures_dir}, exist_ok=True)\n"
        "try:\n"
        "    unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, {capture_path})\n"
        "except Exception:\n"
        "    world = unreal.EditorLevelLibrary.get_editor_world()\n"
        "    unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 ' + {capture_path})\n"
    ).format(captures_dir=json.dumps(captures_dir), capture_path=json.dumps(capture_path))

    try:
        with _send_lock:
            result = _send_fn({"type": "execute", "code": code, "tool_name": "_autocapture"})
    except Exception:
        return None

    if not isinstance(result, dict) or not result.get("success"):
        return None
    if not os.path.isfile(capture_path):
        return None
    return capture_path


def autocaptured(func):
    """Decorator: after the wrapped tool runs, optionally append a viewport
    screenshot to the result as a FastMCP multi-content response.

    No-op (returns plain str) when:
        - BOB_AUTO_CAPTURE_AFTER_EDITS != "1"
        - The 2-second throttle window hasn't elapsed
        - The capture itself fails
    Otherwise returns [text_str, Image(path=...), "Auto-captured: <path>"].
    """
    def _wrapped(*args, **kwargs):
        result = func(*args, **kwargs)
        if not _is_auto_capture_enabled():
            return result
        if not _check_throttle():
            return result
        cap_path = _capture_for_autocapture()
        if not cap_path:
            return result
        # Lazy import — FastMCP's Image type lives in mcp.server.fastmcp
        try:
            from mcp.server.fastmcp import Image
        except Exception:
            return result
        return [result, Image(path=cap_path), "\nAuto-captured: " + cap_path]

    _wrapped.__name__ = func.__name__
    _wrapped.__doc__ = func.__doc__
    _wrapped.__wrapped__ = func
    return _wrapped
