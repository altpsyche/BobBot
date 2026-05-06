"""
BobBot SDK — Tool classification & permission hooks.

Contains the tool category system (overrides, prefix matching),
auto-approve logic, and the permission decision bridge for C++.
"""

import os
import itertools
import threading


# --------------------------------------------------------------------------- #
# Tool classification for auto-approve
# --------------------------------------------------------------------------- #
_TOOL_CATEGORY_OVERRIDES = {
    "ping_unreal": "read_only",
    "get_bobbot_status": "read_only",
    "validate_assets": "read_only",
    "benchmark_scene": "read_only",
    "focus_on_actor": "viewport",
    "deselect_all": "modify",
    "select_actors": "modify",
    "undo": "modify",
    "redo": "modify",
    "start_pie": "modify",
    "stop_pie": "modify",
    "play_sequence": "modify",
    "save_current_level": "modify",
    "open_level": "modify",
    "compile_blueprints": "modify",
    "build_lighting": "modify",
    "render_sequence_to_images": "modify",
    "export_asset": "modify",
    "execute_pcg_graph": "modify",
    "import_asset": "create",
    "import_fbx": "create",
}

_CATEGORY_PREFIXES = [
    ("execute_unreal_python", "code_exec"),
    ("run_console_command", "code_exec"),
    ("execute_pie_console_command", "code_exec"),
    ("capture_", "viewport"),
    ("set_viewport_camera", "viewport"),
    ("get_active_viewport_camera", "viewport"),
    ("get_output_log", "viewport"),
    ("get_", "read_only"),
    ("search_", "read_only"),
    ("is_", "read_only"),
    ("list_", "read_only"),
    ("read_", "read_only"),
    ("audit_", "read_only"),
    ("spawn_", "create"),
    ("create_", "create"),
    ("add_", "create"),
    ("set_", "modify"),
    ("delete_", "modify"),
    ("remove_", "modify"),
    ("rename_", "modify"),
    ("duplicate_", "modify"),
    ("move_", "modify"),
    ("connect_", "modify"),
    ("attach_", "modify"),
    ("check_out_", "modify"),
    ("check_in_", "modify"),
    ("revert_", "modify"),
]


def _classify_tool(tool_name):
    """Classify a tool by category. Returns: read_only, viewport, create, modify, code_exec."""
    # Strip MCP server prefix if present (e.g., "mcp__bobbot-internal__ping_unreal" -> "ping_unreal")
    short_name = tool_name
    if "__" in tool_name:
        short_name = tool_name.rsplit("__", 1)[-1]
    if short_name in _TOOL_CATEGORY_OVERRIDES:
        return _TOOL_CATEGORY_OVERRIDES[short_name]
    for prefix, category in _CATEGORY_PREFIXES:
        if short_name == prefix or short_name.startswith(prefix):
            return category
    return "code_exec"


def _is_auto_approved(category):
    """Check if a tool category is auto-approved based on env var config."""
    env_map = {
        "read_only": "BOB_AUTO_APPROVE_READ_ONLY",
        "viewport": "BOB_AUTO_APPROVE_VIEWPORT",
        "create": "BOB_AUTO_APPROVE_CREATE",
        "modify": "BOB_AUTO_APPROVE_MODIFY",
        "code_exec": "BOB_AUTO_APPROVE_CODE_EXEC",
    }
    key = env_map.get(category, "")
    return os.environ.get(key, "0") == "1"


# --------------------------------------------------------------------------- #
# Permission decision bridge (C++ <-> async hooks)
#
# Each pending approval gets a unique id and an asyncio.Future. The hook
# coroutines block on `await future.with_timeout(...)` and the C++ UI
# resolves a specific id via set_permission_decision(id, decision).
#
# This avoids the single-slot global mailbox bug where two concurrent
# approvals would clobber each other's state.
# --------------------------------------------------------------------------- #
_lock = threading.Lock()
_id_counter = itertools.count(1)
_pending_approvals = {}  # int id -> {"future": asyncio.Future, "loop": loop, "tool": str, "category": str}


def _next_request_id():
    """Return the next monotonic request id (thread-safe)."""
    return next(_id_counter)


def _register_pending(request_id, future, loop, tool, category):
    """Record a pending approval. Called by the hook coroutine."""
    with _lock:
        _pending_approvals[request_id] = {
            "future": future,
            "loop": loop,
            "tool": tool,
            "category": category,
        }


def _pop_pending(request_id):
    """Remove and return a pending approval entry. Returns None if absent."""
    with _lock:
        return _pending_approvals.pop(request_id, None)


def _drain_pending(decision):
    """Resolve every pending approval with the given decision.
    Used when permission mode flips to a non-prompting state, on session
    clear, and on cleanup. Safe to call from any thread.
    """
    with _lock:
        entries = list(_pending_approvals.items())
        _pending_approvals.clear()
    for request_id, entry in entries:
        future = entry["future"]
        loop = entry["loop"]
        if future.done():
            continue
        try:
            loop.call_soon_threadsafe(_safe_set_result, future, decision)
        except RuntimeError:
            # Loop is closed — nothing to wake
            pass


def _safe_set_result(future, value):
    """Set a future result, ignoring InvalidStateError if already resolved."""
    if not future.done():
        try:
            future.set_result(value)
        except Exception:
            pass


def set_permission_decision(request_id, decision):
    """Called by C++ when user clicks Approve/Deny in the approval widget.

    Args:
        request_id: int id from the approval_request stream event.
        decision: 'allow' or 'deny'. Unblocks the matching hook coroutine.
    """
    try:
        rid = int(request_id)
    except (TypeError, ValueError):
        try:
            import unreal
            unreal.log_warning(
                "BobBot: set_permission_decision got non-int id {!r}".format(request_id))
        except Exception:
            pass
        return

    entry = _pop_pending(rid)
    if entry is None:
        try:
            import unreal
            unreal.log_warning(
                "BobBot: set_permission_decision({}, '{}') — no matching pending request".format(
                    rid, decision))
        except Exception:
            pass
        return

    future = entry["future"]
    loop = entry["loop"]
    try:
        loop.call_soon_threadsafe(_safe_set_result, future, decision)
    except RuntimeError:
        pass

    try:
        import unreal
        unreal.log("BobBot: set_permission_decision({}, '{}') resolved {}".format(
            rid, decision, entry["tool"]))
    except Exception:
        pass
