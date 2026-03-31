"""
Shared helpers for BobBot MCP tool modules.

Eliminates the duplicated _exec() closure from 41 tool files.
Initialized by the bridge before tool discovery:
    import _common
    _common.init(_send_and_receive)
"""

import json
import threading

_send_fn = None
_send_lock = threading.Lock()


def init(send_fn):
    """Set the send function for tool execution. Called by the bridge on startup."""
    global _send_fn
    with _send_lock:
        _send_fn = send_fn


def _exec(code):
    """Execute Python code in the UE editor and return the output."""
    with _send_lock:
        fn = _send_fn
    if fn is None:
        return "Error: _common not initialized — call init(send_fn) first"
    result = fn({"type": "execute", "code": code})
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
# Actor lookup helper (used by 15+ tools)
# --------------------------------------------------------------------------- #
_FIND_ACTOR = """\
import unreal as _u
_target = None
_label = {label_json}
for _a in _u.EditorLevelLibrary.get_all_level_actors():
    if _a.get_actor_label() == _label:
        _target = _a
        break
if _target is None:
    raise RuntimeError("Actor '{{}}' not found in level".format(_label))
"""


def actor_exec(label, code):
    """Execute code with _target bound to the actor with the given label."""
    safe_label = json.dumps(label)  # Properly escaped string literal
    return _exec(_FIND_ACTOR.format(label_json=safe_label) + code)
