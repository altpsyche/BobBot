"""
BobBot SDK — Tool classification & permission hooks.

Contains the tool category system (overrides, prefix matching),
auto-approve logic, and the permission decision bridge for C++.
"""

import os


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
# --------------------------------------------------------------------------- #
_pending_permission = None   # {id, tool, category} during approval wait
_permission_response = None  # "allow" | "deny" — set by C++ via set_permission_decision()


def set_permission_decision(decision):
    """Called by C++ when user clicks Approve/Deny in the approval widget.
    decision: 'allow' or 'deny'. Unblocks the can_use_tool callback.
    """
    global _permission_response
    _permission_response = decision
    try:
        import unreal
        unreal.log("BobBot: set_permission_decision('{}') — pending={}, response={}".format(
            decision, _pending_permission, _permission_response))
    except Exception:
        pass
