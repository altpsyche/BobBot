"""
BobBot SDK — Client lifecycle.

Manages the persistent ClaudeSDKClient: connection, disconnection,
dead-process detection, event loop, and SDK type caching.
"""

import os
import asyncio
import threading

import bob_platform
import bob_sdk_config
import bob_sdk_events


# --------------------------------------------------------------------------- #
# State
# --------------------------------------------------------------------------- #
_session_id = None
_total_session_cost = 0.0

# Async event loop in background thread
_loop = None
_loop_thread = None

# Persistent SDK client
_client = None              # ClaudeSDKClient | None
_client_connected = False   # True after connect() succeeds
_client_session_id = None   # Session ID the client was connected with
_client_needs_rebuild = False  # True after thinking/effort/etc env changes — forces reconnect on next message


# --------------------------------------------------------------------------- #
# SDK types — lazily cached after first successful import
# --------------------------------------------------------------------------- #
_sdk_types = {}


def _get_sdk_types():
    """Return cached dict of SDK classes. Import once, reuse forever."""
    if not _sdk_types:
        import claude_agent_sdk
        from claude_agent_sdk import (
            ClaudeSDKClient, ClaudeAgentOptions,
            AssistantMessage, UserMessage, ResultMessage, SystemMessage,
            TextBlock, ToolUseBlock,
            ClaudeSDKError,
            TaskStartedMessage, TaskProgressMessage, TaskNotificationMessage,
            StreamEvent,
        )
        _sdk_types.update(
            module=claude_agent_sdk,
            ClaudeSDKClient=ClaudeSDKClient,
            ClaudeAgentOptions=ClaudeAgentOptions,
            AssistantMessage=AssistantMessage, UserMessage=UserMessage,
            ResultMessage=ResultMessage, SystemMessage=SystemMessage,
            TextBlock=TextBlock, ToolUseBlock=ToolUseBlock,
            ClaudeSDKError=ClaudeSDKError,
            TaskStartedMessage=TaskStartedMessage,
            TaskProgressMessage=TaskProgressMessage,
            TaskNotificationMessage=TaskNotificationMessage,
            StreamEvent=StreamEvent,
        )
    return _sdk_types


# --------------------------------------------------------------------------- #
# Async event loop management
# --------------------------------------------------------------------------- #
def _ensure_loop():
    """Start the background asyncio event loop if not running."""
    global _loop, _loop_thread
    with bob_sdk_events._lock:
        if _loop is not None and _loop.is_running():
            return
        _loop = asyncio.new_event_loop()
        _loop_thread = threading.Thread(
            target=_loop.run_forever, daemon=True, name="bobbot-sdk-loop")
        _loop_thread.start()


# --------------------------------------------------------------------------- #
# Client lifecycle
# --------------------------------------------------------------------------- #
async def _disconnect_client_safe():
    """Disconnect the client with timeout. Safe to call even if not connected."""
    global _client, _client_connected, _client_session_id
    client = _client
    _client = None
    _client_connected = False
    _client_session_id = None
    if client is None:
        return
    try:
        await asyncio.wait_for(client.disconnect(), timeout=12.0)
        bob_sdk_config._log_sdk("BobBot SDK: client disconnected")
    except asyncio.TimeoutError:
        bob_sdk_config._log_sdk("BobBot SDK: disconnect timed out, forcing")
        transport = getattr(client, '_transport', None)
        if transport:
            proc = getattr(transport, '_process', None)
            if proc and proc.returncode is None:
                try:
                    proc.kill()
                except Exception:
                    pass
    except Exception as e:
        bob_sdk_config._log_sdk("BobBot SDK: disconnect error: {}".format(e))


async def _ensure_client(get_status_fn=None):
    """Ensure persistent ClaudeSDKClient is connected. Create/reconnect as needed.

    Args:
        get_status_fn: callable for the internal MCP server's status tool.
    """
    global _client, _client_connected, _client_session_id, _client_needs_rebuild

    sdk = _get_sdk_types()

    # Detect dead process
    if _client and _client_connected:
        transport = getattr(_client, '_transport', None)
        if transport:
            proc = getattr(transport, '_process', None)
            if proc is not None and proc.returncode is not None:
                bob_sdk_config._log_sdk(
                    "BobBot SDK: process died (rc={}), reconnecting".format(
                        proc.returncode))
                _client_connected = False
                _client = None

    # Detect rebuild request (thinking/effort/other spawn-time options changed)
    if _client_connected and _client and _client_needs_rebuild:
        bob_sdk_config._log_sdk(
            "BobBot SDK: options changed, reconnecting (resume preserves session)")
        await _disconnect_client_safe()

    # Detect session change (set_session_id was called)
    if _client_connected and _client and _session_id != _client_session_id:
        bob_sdk_config._log_sdk(
            "BobBot SDK: session changed {} -> {}, reconnecting".format(
                _client_session_id, _session_id))
        await _disconnect_client_safe()

    if _client_connected and _client:
        return  # Already good

    # Build options
    claude_agent_sdk = sdk["module"]
    cli_path = bob_platform.bundled_claude_path(claude_agent_sdk)
    bob_sdk_config._log_sdk("BobBot SDK: using cli_path={}".format(cli_path))

    # Read env-based configuration
    env_opts = bob_sdk_config.get_sdk_options_from_env()
    sdk_perm = env_opts["permission_mode"]
    bob_perm = env_opts["bob_permission_mode"]

    # Plugin root has CLAUDE.md + Config/Rules/ — SDK discovers them via add_dirs
    plugin_root = os.path.join(bob_sdk_config._PROJECT_ROOT, "Plugins", "BobBot")

    options = sdk["ClaudeAgentOptions"](
        model=bob_sdk_config._model,
        system_prompt=bob_sdk_config._get_system_prompt(),
        cwd=bob_sdk_config._BOB_CWD,
        add_dirs=[plugin_root, bob_sdk_config._PROJECT_ROOT],
        mcp_servers=bob_sdk_config._build_mcp_servers(),
        permission_mode=sdk_perm,
        cli_path=cli_path,
        include_partial_messages=True,
    )

    if _session_id:
        options.resume = _session_id

    # Apply env-based options
    if "thinking" in env_opts:
        options.thinking = env_opts["thinking"]

    if "effort" in env_opts:
        options.effort = env_opts["effort"]

    if "max_budget_usd" in env_opts:
        options.max_budget_usd = env_opts["max_budget_usd"]

    # Hooks — tool logging + notifications (always), permission request (ask_before_edits only)
    from claude_agent_sdk import HookMatcher
    hooks = {
        "PreToolUse": [HookMatcher(matcher=None, hooks=[bob_sdk_events._on_pre_tool_use])],
        "PostToolUse": [HookMatcher(matcher=None, hooks=[bob_sdk_events._on_post_tool_use])],
        "PostToolUseFailure": [HookMatcher(matcher=None, hooks=[bob_sdk_events._on_post_tool_failure])],
        "Notification": [HookMatcher(matcher=None, hooks=[bob_sdk_events._on_notification])],
    }

    # can_use_tool: gates internal MCP tools (ping_unreal, get_bobbot_status).
    # Checks BOB_PERMISSION_MODE at runtime so mode switches work without reconnecting.
    options.can_use_tool = bob_sdk_events._can_use_tool

    # PermissionRequest hook: gates external MCP tool calls (the 200 tools).
    # Claude Code fires this hook when it wants to run a tool in acceptEdits mode.
    # Without it, acceptEdits auto-approves everything and Ask mode doesn't ask.
    hooks["PermissionRequest"] = [HookMatcher(
        matcher=None, hooks=[bob_sdk_events._on_permission_request], timeout=130)]

    options.hooks = hooks

    # File checkpointing — enables rewind_files()
    options.enable_file_checkpointing = True

    # Custom agent definitions
    agents = bob_sdk_config._load_agent_definitions()
    if agents:
        options.agents = agents

    # In-process MCP tools (ping, status — zero latency)
    internal_server = bob_sdk_config._get_internal_mcp_server(get_status_fn)
    if internal_server:
        mcp = options.mcp_servers
        if isinstance(mcp, dict):
            mcp["bobbot-internal"] = internal_server
            options.mcp_servers = mcp

    # Apply remaining env options
    if "fallback_model" in env_opts:
        options.fallback_model = env_opts["fallback_model"]

    if "disallowed_tools" in env_opts:
        options.disallowed_tools = env_opts["disallowed_tools"]

    if "extra_add_dirs" in env_opts:
        options.add_dirs.extend(env_opts["extra_add_dirs"])

    if "betas" in env_opts:
        options.betas = env_opts["betas"]

    if "task_budget" in env_opts:
        options.task_budget = env_opts["task_budget"]

    if "max_turns" in env_opts:
        options.max_turns = env_opts["max_turns"]

    _client = sdk["ClaudeSDKClient"](options=options)
    await _client.connect()
    _client_connected = True
    _client_session_id = _session_id
    _client_needs_rebuild = False
    bob_sdk_config._log_sdk(
        "BobBot SDK: client connected (session={})".format(_session_id))


def _check_client_health():
    """Check if the client process died and mark for reconnection."""
    global _client_connected
    if _client:
        transport = getattr(_client, '_transport', None)
        if transport:
            proc = getattr(transport, '_process', None)
            if proc is not None and proc.returncode is not None:
                bob_sdk_config._log_sdk(
                    "BobBot SDK: client died during stream, "
                    "will reconnect next message")
                _client_connected = False
