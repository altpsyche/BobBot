"""
BobBot Chat (Agent SDK) — persistent Claude process via claude-agent-sdk.

Drop-in replacement for bob_chat.py. Same public API:
  send_message(), poll(), cleanup(), clear_session(),
  set_model(), get_model(), is_thinking(), get_session_cost(),
  get_session_id(), set_session_id(), get_status(),
  detect_claude(), check_auth(), interrupt()

Uses ClaudeSDKClient which keeps a single persistent Claude process alive
across the entire editor session. Context is maintained naturally in-process.
CLAUDE.md and PROJECT.md are inlined in the system prompt — no tool calls
to read them.

Dependencies auto-installed into Saved/BobBot/.venv/ on first launch.
"""

import os
import sys
import json
import shutil
import asyncio
import threading
import traceback
import subprocess

# --------------------------------------------------------------------------- #
# SDK dependency — import from the shared venv created by bob_bridge_launcher
# --------------------------------------------------------------------------- #
def _log_sdk(msg):
    try:
        import unreal
        unreal.log(msg)
    except Exception:
        print(msg, file=sys.stderr)


def _setup_venv_imports():
    """Add venv site-packages (and pywin32 DLLs on Windows) to sys.path."""
    try:
        import bob_bridge_launcher
        sp = bob_bridge_launcher.get_venv_site_packages()
        if not sp or not os.path.isdir(sp):
            return False
        if sp not in sys.path:
            sys.path.insert(0, sp)
        # pywin32 stores DLLs in a sibling directory that must be registered
        if sys.platform == "win32":
            dll_dir = os.path.join(sp, "pywin32_system32")
            if os.path.isdir(dll_dir):
                os.add_dll_directory(dll_dir)
                if dll_dir not in sys.path:
                    sys.path.insert(0, dll_dir)
        return True
    except Exception:
        return False


_sdk_available = False
_setup_venv_imports()
try:
    from claude_agent_sdk import ClaudeSDKClient, ClaudeAgentOptions  # noqa: F401
    _sdk_available = True
    _log_sdk("BobBot SDK: claude-agent-sdk available (from venv)")
except ImportError as _e:
    _log_sdk("BobBot SDK: not available yet ({}). Will retry after venv setup.".format(_e))

# --------------------------------------------------------------------------- #
# State
# --------------------------------------------------------------------------- #
_claude_path = None
_session_id = None
_model = "sonnet"
_total_session_cost = 0.0

_lock = threading.Lock()
_is_thinking = False
_stream_events = []

# Async event loop in background thread
_loop = None
_loop_thread = None

# Persistent SDK client
_client = None              # ClaudeSDKClient | None
_client_connected = False   # True after connect() succeeds
_client_session_id = None   # Session ID the client was connected with


def _resolve_project_root():
    """Get the absolute project root directory."""
    try:
        import unreal
        uproject = str(unreal.Paths.get_project_file_path())
        if uproject:
            root = os.path.abspath(os.path.dirname(os.path.normpath(uproject)))
            if os.path.isdir(root):
                return root
    except Exception:
        pass
    env_root = os.environ.get("BOB_PROJECT_ROOT", "")
    if env_root:
        return os.path.normpath(env_root)
    return os.getcwd()

_PROJECT_ROOT = _resolve_project_root()
_BOB_CWD = os.path.join(_PROJECT_ROOT, "Saved", "BobBot")
os.makedirs(_BOB_CWD, exist_ok=True)

try:
    import unreal as _unreal_init
    _unreal_init.log("BobBot SDK chat: PROJECT_ROOT = {}".format(_PROJECT_ROOT))
except Exception:
    pass


# --------------------------------------------------------------------------- #
# System prompt — identity only, CLAUDE.md discovered by SDK from cwd
# --------------------------------------------------------------------------- #
_SYSTEM_PROMPT = (
    "You are BobBot, an AI assistant embedded inside the Unreal Engine 5 editor. "
    "You help users with Unreal Engine tasks: creating assets, editing Blueprints, "
    "modifying levels, writing gameplay code, and answering questions about their project.\n\n"
    "You have MCP tools connected to the running UE editor. Prefer them over execute_unreal_python.\n\n"
    "Speak in plain, conversational language. Strictly avoid emojis, dashes, Unicode symbols, "
    "bullet points, or structured formatting. Do not use predictable AI phrasing or robotic "
    "enthusiasm. Keep your responses direct, natural, and human.\n\n"
    "Be concise. Say what you did and the result. No filler."
)

_SYSTEM_PROMPT_PATH = os.path.join(_PROJECT_ROOT, "Saved", "BobBot", "_system_prompt.txt")


# --------------------------------------------------------------------------- #
# Detection & Auth (same as bob_chat.py)
# --------------------------------------------------------------------------- #
def detect_claude():
    """Check if claude CLI is available. Returns (found: bool, version: str)."""
    global _claude_path
    import subprocess

    path = shutil.which("claude")
    if not path:
        for candidate in [
            os.path.expanduser("~/scoop/apps/nodejs/current/bin/claude"),
            os.path.expanduser("~/scoop/apps/nodejs/current/bin/claude.cmd"),
            os.path.expanduser("~/.npm-global/bin/claude"),
        ]:
            if os.path.isfile(candidate):
                path = candidate
                break

    if not path:
        return False, ""

    try:
        result = subprocess.run(
            [path, "--version"],
            capture_output=True, text=True, timeout=10,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        if result.returncode == 0:
            _claude_path = path
            return True, result.stdout.strip()
    except Exception:
        pass

    return False, ""


def check_auth():
    """Check if the user is authenticated with Claude Code."""
    cred_path = os.path.expanduser("~/.claude/.credentials.json")
    if not os.path.isfile(cred_path):
        return False, "Not authenticated. Run 'claude login' in a terminal."
    try:
        with open(cred_path, "r") as f:
            creds = json.load(f)
        if creds:
            return True, "Authenticated"
    except (json.JSONDecodeError, IOError):
        pass
    return False, "Credentials file exists but may be invalid. Run 'claude login'."


# --------------------------------------------------------------------------- #
# Configuration
# --------------------------------------------------------------------------- #
def set_model(name):
    """Set model. If client is connected, switch live via SDK control protocol."""
    global _model
    if name not in ("sonnet", "opus", "haiku"):
        return
    _model = name
    # Live switch if connected
    with _lock:
        loop = _loop
        connected = _client_connected
    if connected and loop and loop.is_running():
        async def _do_set_model():
            try:
                if _client and _client_connected:
                    await _client.set_model(name)
                    _log_sdk("BobBot SDK: model switched to {} (live)".format(name))
            except Exception as e:
                _log_sdk("BobBot SDK: live model switch failed: {} "
                         "(will use on reconnect)".format(e))
        asyncio.run_coroutine_threadsafe(_do_set_model(), loop)


def get_model():
    return _model


def is_thinking():
    with _lock:
        return _is_thinking


def get_session_cost():
    return _total_session_cost


def _get_system_prompt():
    """Get the active system prompt (custom override or inlined default)."""
    if os.path.isfile(_SYSTEM_PROMPT_PATH):
        try:
            with open(_SYSTEM_PROMPT_PATH, "r", encoding="utf-8") as f:
                content = f.read().strip()
            if content:
                return content
        except Exception:
            pass
    return _SYSTEM_PROMPT


def get_default_prompt():
    """Return the built-in default system prompt string."""
    return _SYSTEM_PROMPT


# --------------------------------------------------------------------------- #
# Windows: suppress console windows for all subprocesses
# --------------------------------------------------------------------------- #
if sys.platform == "win32":
    _orig_popen = subprocess.Popen.__init__

    def _popen_no_window(self, *args, **kwargs):
        kwargs.setdefault("creationflags", 0)
        kwargs["creationflags"] |= subprocess.CREATE_NO_WINDOW
        _orig_popen(self, *args, **kwargs)

    subprocess.Popen.__init__ = _popen_no_window


# --------------------------------------------------------------------------- #
# Async event loop management
# --------------------------------------------------------------------------- #
def _ensure_loop():
    """Start the background asyncio event loop if not running."""
    global _loop, _loop_thread
    with _lock:
        if _loop is not None and _loop.is_running():
            return
        _loop = asyncio.new_event_loop()
        _loop_thread = threading.Thread(
            target=_loop.run_forever, daemon=True, name="bobbot-sdk-loop")
        _loop_thread.start()


# --------------------------------------------------------------------------- #
# MCP config for SDK
# --------------------------------------------------------------------------- #
def _build_mcp_servers():
    """Build MCP server config dict for ClaudeAgentOptions."""
    saved_dir = os.path.join(_PROJECT_ROOT, "Saved", "BobBot")

    # Try HTTP config first (persistent bridge)
    http_config = os.path.join(saved_dir, "_bobbot_mcp.json")
    if os.path.isfile(http_config):
        try:
            with open(http_config, "r") as f:
                cfg = json.load(f)
            servers = cfg.get("mcpServers", {})
            unreal = servers.get("unreal", {})
            if unreal.get("type") == "http":
                try:
                    import bob_bridge_launcher
                    if bob_bridge_launcher.is_running():
                        return {"unreal": unreal}
                except Exception:
                    pass
        except Exception:
            pass

    # Fallback to stdio config
    fallback = os.path.join(saved_dir, "_bobbot_mcp_fallback.json")
    config_path = fallback if os.path.isfile(fallback) else os.path.join(
        _PROJECT_ROOT, ".mcp.json")
    if os.path.isfile(config_path):
        try:
            with open(config_path, "r") as f:
                cfg = json.load(f)
            servers = cfg.get("mcpServers", {})
            if "unreal" in servers:
                return {"unreal": servers["unreal"]}
        except Exception:
            pass

    return {}


# --------------------------------------------------------------------------- #
# Tool classification for auto-approve (shared with hooks)
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
    if tool_name in _TOOL_CATEGORY_OVERRIDES:
        return _TOOL_CATEGORY_OVERRIDES[tool_name]
    for prefix, category in _CATEGORY_PREFIXES:
        if tool_name == prefix or tool_name.startswith(prefix):
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
# SDK Permission System
#
# Uses the SDK's native permission architecture:
#   EditAutomatically → bypassPermissions (no prompts)
#   AskBeforeEdits   → acceptEdits + can_use_tool + PermissionRequest hook
#   Plan             → plan (read-only, no tool execution)
#
# can_use_tool: fast callback — auto-approved categories return Allow immediately.
# PermissionRequest: fires when SDK needs user permission — replaces Claude Code's
#   terminal prompt with BobBot's Slate approval widget.
# PreToolUse: logging only — no permission logic.
# --------------------------------------------------------------------------- #
_pending_permission = None   # {id, tool, category} during approval wait
_permission_response = None  # "allow" | "deny" — set by C++ via set_permission_decision()


async def _can_use_tool(tool_name, tool_input, context):
    """SDK can_use_tool callback — auto-approve categories, let others fall through.
    Runs before the permission system. Fast, no blocking.
    """
    from claude_agent_sdk import PermissionResultAllow
    category = _classify_tool(tool_name)
    if _is_auto_approved(category):
        return PermissionResultAllow()
    return None  # fall through to SDK permission system → PermissionRequest hook


async def _on_permission_request(input_data, tool_use_id, context):
    """PermissionRequest hook — replaces Claude Code's terminal prompt.
    Queues approval_request event for BobBot's Slate UI, blocks until user responds.
    """
    global _pending_permission, _permission_response
    tool_name = input_data.get("tool_name", "")
    tool_input = input_data.get("tool_input", {})
    category = _classify_tool(tool_name)

    # Queue for C++ approval widget
    request_id = id(input_data)
    _pending_permission = {
        "id": request_id, "tool": tool_name, "category": category}
    _permission_response = None
    with _lock:
        _stream_events.append({
            "type": "approval_request",
            "id": request_id,
            "tool": tool_name,
            "category": category,
            "input": json.dumps(tool_input, indent=2) if isinstance(
                tool_input, dict) else str(tool_input),
        })

    # Block until user responds via set_permission_decision()
    timeout = 120.0
    start = asyncio.get_event_loop().time()
    while _permission_response is None:
        if asyncio.get_event_loop().time() - start > timeout:
            _pending_permission = None
            return {"hookSpecificOutput": {
                "hookEventName": "PermissionRequest",
                "decision": {"behavior": "deny", "message": "Approval timed out"},
            }}
        await asyncio.sleep(0.1)

    decision = _permission_response
    _pending_permission = None
    _permission_response = None
    return {"hookSpecificOutput": {
        "hookEventName": "PermissionRequest",
        "decision": {"behavior": decision},
    }}


async def _on_pre_tool_use(input_data, tool_use_id, context):
    """PreToolUse hook — log tool calls to chat UI. No permission logic."""
    tool_name = input_data.get("tool_name", "")
    tool_input = input_data.get("tool_input", {})
    with _lock:
        _stream_events.append({
            "type": "hook_tool_start",
            "tool": tool_name,
            "input": json.dumps(tool_input, indent=2) if isinstance(tool_input, dict) else str(tool_input),
        })
    return {"hookSpecificOutput": {"hookEventName": "PreToolUse"}}


async def _on_post_tool_use(input_data, tool_use_id, context):
    """PostToolUse hook — log tool results in chat UI."""
    tool_name = input_data.get("tool_name", "")
    tool_response = input_data.get("tool_response", "")
    with _lock:
        _stream_events.append({
            "type": "hook_tool_result",
            "tool": tool_name,
            "output": str(tool_response)[:2000],
        })
    return {"hookSpecificOutput": {"hookEventName": "PostToolUse"}}


async def _on_post_tool_failure(input_data, tool_use_id, context):
    """PostToolUseFailure hook — log tool errors."""
    with _lock:
        _stream_events.append({
            "type": "hook_tool_error",
            "tool": input_data.get("tool_name", ""),
            "error": input_data.get("error", ""),
        })
    return {"hookSpecificOutput": {"hookEventName": "PostToolUseFailure"}}


async def _on_notification(input_data, tool_use_id, context):
    """Notification hook — display Claude's alerts in BobBot UI."""
    with _lock:
        _stream_events.append({
            "type": "notification",
            "message": input_data.get("message", ""),
            "title": input_data.get("title", ""),
        })
    return {"hookSpecificOutput": {"hookEventName": "Notification"}}


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
        )
    return _sdk_types


# --------------------------------------------------------------------------- #
# Custom agent definitions
# --------------------------------------------------------------------------- #
def _load_agent_definitions():
    """Load custom agent definitions from Saved/BobBot/agents.json."""
    agents_file = os.path.join(_BOB_CWD, "agents.json")
    if not os.path.isfile(agents_file):
        return None
    try:
        with open(agents_file, "r", encoding="utf-8") as f:
            raw = json.load(f)
        from claude_agent_sdk import AgentDefinition
        agents = {}
        for name, defn in raw.items():
            agents[name] = AgentDefinition(
                description=defn["description"],
                prompt=defn["prompt"],
                tools=defn.get("tools"),
                model=defn.get("model"),
                maxTurns=defn.get("maxTurns"),
            )
        return agents
    except Exception as e:
        _log_sdk("BobBot SDK: failed to load agents.json: {}".format(e))
        return None


# --------------------------------------------------------------------------- #
# In-process MCP tools (zero latency, no TCP round-trip)
# --------------------------------------------------------------------------- #
_bobbot_internal_server = None


def _get_internal_mcp_server():
    """Create in-process MCP server with BobBot utility tools."""
    global _bobbot_internal_server
    if _bobbot_internal_server is not None:
        return _bobbot_internal_server
    try:
        from claude_agent_sdk import create_sdk_mcp_server, tool

        @tool(name="ping_unreal",
              description="Check if UE editor is responsive",
              input_schema={})
        async def ping_tool(params):
            try:
                import unreal
                return {"content": [{"type": "text", "text": "pong"}]}
            except Exception:
                return {"content": [{"type": "text", "text": "Editor not responding"}],
                        "is_error": True}

        @tool(name="get_bobbot_status",
              description="Get BobBot diagnostic info (backend, model, session, context)",
              input_schema={})
        async def status_tool(params):
            s = get_status()
            return {"content": [{"type": "text", "text": json.dumps(s, indent=2)}]}

        _bobbot_internal_server = create_sdk_mcp_server(
            "bobbot-internal", "1.0", tools=[ping_tool, status_tool])
        return _bobbot_internal_server
    except Exception as e:
        _log_sdk("BobBot SDK: failed to create internal MCP server: {}".format(e))
        return None


# --------------------------------------------------------------------------- #
# Persistent client lifecycle
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
        _log_sdk("BobBot SDK: client disconnected")
    except asyncio.TimeoutError:
        _log_sdk("BobBot SDK: disconnect timed out, forcing")
        transport = getattr(client, '_transport', None)
        if transport:
            proc = getattr(transport, '_process', None)
            if proc and proc.returncode is None:
                try:
                    proc.kill()
                except Exception:
                    pass
    except Exception as e:
        _log_sdk("BobBot SDK: disconnect error: {}".format(e))


async def _ensure_client():
    """Ensure persistent ClaudeSDKClient is connected. Create/reconnect as needed."""
    global _client, _client_connected, _client_session_id

    sdk = _get_sdk_types()

    # Detect dead process
    if _client and _client_connected:
        transport = getattr(_client, '_transport', None)
        if transport:
            proc = getattr(transport, '_process', None)
            if proc is not None and proc.returncode is not None:
                _log_sdk("BobBot SDK: process died (rc={}), reconnecting".format(
                    proc.returncode))
                _client_connected = False
                _client = None

    # Detect session change (set_session_id was called)
    if _client_connected and _client and _session_id != _client_session_id:
        _log_sdk("BobBot SDK: session changed {} -> {}, reconnecting".format(
            _client_session_id, _session_id))
        await _disconnect_client_safe()

    if _client_connected and _client:
        return  # Already good

    # Build options
    claude_agent_sdk = sdk["module"]
    bundled_name = "claude.exe" if sys.platform == "win32" else "claude"
    bundled_cli = os.path.join(
        os.path.dirname(os.path.abspath(claude_agent_sdk.__file__)),
        "_bundled", bundled_name)
    cli_path = bundled_cli if os.path.isfile(bundled_cli) else None
    _log_sdk("BobBot SDK: using cli_path={}".format(cli_path))

    # Map BobBot permission mode to SDK permission mode (Claude Code style)
    bob_perm = os.environ.get("BOB_PERMISSION_MODE", "edit_automatically")
    if bob_perm == "ask_before_edits":
        sdk_perm = "acceptEdits"       # allows reads, asks before writes → PermissionRequest hook
    elif bob_perm == "plan":
        sdk_perm = "plan"              # read-only, no tool execution
    else:
        sdk_perm = "bypassPermissions"  # edit_automatically → no prompts

    # Plugin root has CLAUDE.md + Config/Rules/ — SDK discovers them via add_dirs
    plugin_root = os.path.join(_PROJECT_ROOT, "Plugins", "BobBot")

    options = sdk["ClaudeAgentOptions"](
        model=_model,
        system_prompt=_get_system_prompt(),
        cwd=_BOB_CWD,
        add_dirs=[plugin_root, _PROJECT_ROOT],
        mcp_servers=_build_mcp_servers(),
        permission_mode=sdk_perm,
        cli_path=cli_path,
        include_partial_messages=True,
    )

    if _session_id:
        options.resume = _session_id

    # Thinking config (from C++ env vars)
    thinking_mode = os.environ.get("BOB_THINKING_MODE", "disabled")
    if thinking_mode == "enabled":
        budget = 10000
        try:
            budget = int(os.environ.get("BOB_THINKING_BUDGET", "10000"))
        except (ValueError, TypeError):
            pass
        options.thinking = {"type": "enabled", "budget_tokens": budget}
    elif thinking_mode == "adaptive":
        options.thinking = {"type": "adaptive"}

    # Effort control
    effort = os.environ.get("BOB_EFFORT", "")
    if effort in ("low", "medium", "high", "max"):
        options.effort = effort

    # Cost budget
    max_budget = os.environ.get("BOB_MAX_BUDGET", "")
    if max_budget:
        try:
            options.max_budget_usd = float(max_budget)
        except (ValueError, TypeError):
            pass

    # Hooks — tool logging + notifications (always), permission request (ask_before_edits only)
    from claude_agent_sdk import HookMatcher
    hooks = {
        "PreToolUse": [HookMatcher(matcher=None, hooks=[_on_pre_tool_use])],
        "PostToolUse": [HookMatcher(matcher=None, hooks=[_on_post_tool_use])],
        "PostToolUseFailure": [HookMatcher(matcher=None, hooks=[_on_post_tool_failure])],
        "Notification": [HookMatcher(matcher=None, hooks=[_on_notification])],
    }

    # AskBeforeEdits: add can_use_tool for auto-approve + PermissionRequest hook for manual approval
    if bob_perm == "ask_before_edits":
        options.can_use_tool = _can_use_tool
        hooks["PermissionRequest"] = [HookMatcher(
            matcher=None, hooks=[_on_permission_request], timeout=130)]

    options.hooks = hooks

    # File checkpointing — enables rewind_files()
    options.enable_file_checkpointing = True

    # Custom agent definitions
    agents = _load_agent_definitions()
    if agents:
        options.agents = agents

    # In-process MCP tools (ping, status — zero latency)
    internal_server = _get_internal_mcp_server()
    if internal_server:
        mcp = options.mcp_servers
        if isinstance(mcp, dict):
            mcp["bobbot-internal"] = internal_server
            options.mcp_servers = mcp

    # Fallback model
    fallback = os.environ.get("BOB_FALLBACK_MODEL", "")
    if fallback:
        options.fallback_model = fallback

    # Disallowed tools
    blocked = os.environ.get("BOB_DISALLOWED_TOOLS", "")
    if blocked:
        options.disallowed_tools = [t.strip() for t in blocked.split(",") if t.strip()]

    # Additional directories for context (append to existing)
    extra_dirs = os.environ.get("BOB_ADD_DIRS", "")
    if extra_dirs:
        options.add_dirs.extend([d.strip() for d in extra_dirs.split(";") if d.strip()])

    # Beta features (1M context)
    if os.environ.get("BOB_BETA_1M_CONTEXT") == "1":
        options.betas = ["interleaved-thinking", "extended-context"]

    # Task budget (token-paced execution)
    task_budget_str = os.environ.get("BOB_TASK_BUDGET", "")
    if task_budget_str:
        try:
            options.task_budget = {"total": int(task_budget_str)}
        except (ValueError, TypeError):
            pass

    # Max turns safety limit
    max_turns_str = os.environ.get("BOB_MAX_TURNS", "")
    if max_turns_str:
        try:
            options.max_turns = int(max_turns_str)
        except (ValueError, TypeError):
            pass

    _client = sdk["ClaudeSDKClient"](options=options)
    await _client.connect()
    _client_connected = True
    _client_session_id = _session_id
    _log_sdk("BobBot SDK: client connected (session={})".format(_session_id))


# --------------------------------------------------------------------------- #
# SDK streaming — persistent client
# --------------------------------------------------------------------------- #
async def _send_and_stream(user_message):
    """Send a message via persistent client and stream events into _stream_events."""
    global _is_thinking, _session_id, _total_session_cost, _client_session_id

    sdk = _get_sdk_types()
    AssistantMessage = sdk["AssistantMessage"]
    UserMessage = sdk["UserMessage"]
    ResultMessage = sdk["ResultMessage"]
    SystemMessage = sdk["SystemMessage"]
    TextBlock = sdk["TextBlock"]
    ToolUseBlock = sdk["ToolUseBlock"]
    ClaudeSDKError = sdk["ClaudeSDKError"]
    TaskStartedMessage = sdk["TaskStartedMessage"]
    TaskProgressMessage = sdk["TaskProgressMessage"]
    TaskNotificationMessage = sdk["TaskNotificationMessage"]

    with _lock:
        _is_thinking = True
        _stream_events.clear()

    try:
        await _ensure_client()

        # Send message to living process
        await _client.query(user_message)

        # Stream response until ResultMessage
        async for message in _client.receive_response():

            # Subagent task messages (check BEFORE SystemMessage — subclasses)
            if isinstance(message, TaskStartedMessage):
                with _lock:
                    _stream_events.append({
                        "type": "subagent_start",
                        "task_id": message.task_id,
                        "description": message.description,
                        "task_type": getattr(message, "task_type", "") or "",
                    })

            elif isinstance(message, TaskProgressMessage):
                usage = message.usage or {}
                with _lock:
                    _stream_events.append({
                        "type": "subagent_progress",
                        "task_id": message.task_id,
                        "description": message.description,
                        "total_tokens": usage.get("total_tokens", 0) if isinstance(usage, dict) else 0,
                        "tool_uses": usage.get("tool_uses", 0) if isinstance(usage, dict) else 0,
                        "duration_ms": usage.get("duration_ms", 0) if isinstance(usage, dict) else 0,
                        "last_tool_name": getattr(message, "last_tool_name", "") or "",
                    })

            elif isinstance(message, TaskNotificationMessage):
                usage = message.usage or {}
                with _lock:
                    _stream_events.append({
                        "type": "subagent_complete",
                        "task_id": message.task_id,
                        "status": message.status,
                        "summary": getattr(message, "summary", "") or "",
                        "total_tokens": usage.get("total_tokens", 0) if isinstance(usage, dict) else 0,
                        "tool_uses": usage.get("tool_uses", 0) if isinstance(usage, dict) else 0,
                        "duration_ms": usage.get("duration_ms", 0) if isinstance(usage, dict) else 0,
                    })

            elif isinstance(message, SystemMessage):
                sid = message.data.get("session_id") if hasattr(message, "data") else None
                if sid:
                    with _lock:
                        _session_id = sid
                        _client_session_id = sid

            elif isinstance(message, AssistantMessage):
                for block in message.content:
                    if isinstance(block, TextBlock):
                        if block.text:
                            with _lock:
                                _stream_events.append({
                                    "type": "text", "text": block.text})
                    elif isinstance(block, ToolUseBlock):
                        tool_input = block.input
                        if isinstance(tool_input, dict):
                            try:
                                input_str = json.dumps(tool_input, indent=2)
                            except (TypeError, ValueError):
                                input_str = str(tool_input)
                        elif isinstance(tool_input, str):
                            try:
                                parsed = json.loads(tool_input)
                                input_str = json.dumps(parsed, indent=2)
                            except (json.JSONDecodeError, ValueError):
                                input_str = tool_input
                        else:
                            input_str = str(tool_input) if tool_input else "{}"
                        with _lock:
                            _stream_events.append({
                                "type": "tool_use",
                                "name": block.name,
                                "input": input_str,
                            })
                    else:
                        btype = getattr(block, "type", "")
                        if btype == "error":
                            error_msg = (getattr(block, "message", "")
                                         or getattr(block, "text", "")
                                         or str(block))
                            with _lock:
                                _stream_events.append({
                                    "type": "error",
                                    "message": "Assistant error: {}".format(
                                        error_msg),
                                })

            elif isinstance(message, UserMessage):
                for block in message.content:
                    btype = getattr(block, "type", "")
                    if btype == "tool_result":
                        output = getattr(block, "content", "")
                        if isinstance(output, list):
                            output = "\n".join(
                                b.get("text", "") for b in output
                                if isinstance(b, dict) and b.get("type") == "text"
                            )
                        with _lock:
                            _stream_events.append({
                                "type": "tool_result",
                                "output": str(output),
                            })

            elif isinstance(message, ResultMessage):
                cost = message.total_cost_usd or 0

                # Get accurate context usage from persistent client
                input_tokens = 0
                output_tokens = 0
                context_window = 0

                try:
                    ctx = await _client.get_context_usage()
                    # ContextUsageResponse is a TypedDict with camelCase keys
                    input_tokens = ctx.get("totalTokens", 0) or 0
                    context_window = ctx.get("maxTokens", 0) or 0
                    # output_tokens not in context_usage — get from ResultMessage
                    usage = message.usage or {}
                    if isinstance(usage, dict):
                        output_tokens = usage.get("output_tokens", 0) or 0
                except Exception:
                    # Fallback: derive from ResultMessage.usage
                    usage = message.usage or {}
                    model_usage = message.model_usage or {}
                    if isinstance(usage, dict):
                        input_tokens = (
                            (usage.get("input_tokens", 0) or 0)
                            + (usage.get("cache_read_input_tokens", 0) or 0)
                            + (usage.get("cache_creation_input_tokens", 0) or 0)
                        )
                        output_tokens = usage.get("output_tokens", 0) or 0
                    if isinstance(model_usage, dict):
                        for _model_name, model_data in model_usage.items():
                            if isinstance(model_data, dict):
                                cw = model_data.get("contextWindow", 0) or 0
                                if cw > 0:
                                    context_window = cw
                                    break

                with _lock:
                    if message.session_id:
                        _session_id = message.session_id
                        _client_session_id = message.session_id
                    _total_session_cost += cost
                    _stream_events.append({
                        "type": "complete",
                        "cost": cost,
                        "duration_ms": message.duration_ms or 0,
                        "num_turns": message.num_turns or 1,
                        "is_error": message.is_error,
                        "result_text": message.result or "",
                        "input_tokens": input_tokens,
                        "output_tokens": output_tokens,
                        "context_window": context_window,
                    })

    except ClaudeSDKError as e:
        # Mark client for reconnection if process died
        _check_client_health()
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "Claude SDK error: {}".format(str(e)),
            })
    except Exception:
        _check_client_health()
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "SDK stream error: {}".format(traceback.format_exc()),
            })
    finally:
        with _lock:
            _is_thinking = False


def _check_client_health():
    """Check if the client process died and mark for reconnection."""
    global _client_connected
    if _client:
        transport = getattr(_client, '_transport', None)
        if transport:
            proc = getattr(transport, '_process', None)
            if proc is not None and proc.returncode is not None:
                _log_sdk("BobBot SDK: client died during stream, "
                         "will reconnect next message")
                _client_connected = False


# --------------------------------------------------------------------------- #
# Public API (matches bob_chat.py exactly)
# --------------------------------------------------------------------------- #
def send_message(text):
    """Send a user message. Uses persistent ClaudeSDKClient in background loop."""
    global _sdk_available
    with _lock:
        available = _sdk_available
    if not available:
        # Retry — venv may have finished building since module load
        _setup_venv_imports()
        try:
            from claude_agent_sdk import ClaudeSDKClient as _c, ClaudeAgentOptions as _o  # noqa: F401
            with _lock:
                _sdk_available = True
            _log_sdk("BobBot SDK: now available (venv ready)")
        except ImportError:
            with _lock:
                _stream_events.append({
                    "type": "error",
                    "message": "claude-agent-sdk not ready. "
                               "Venv may still be building — try again in a moment.",
                })
            return

    if not _claude_path:
        detect_claude()
        if not _claude_path:
            with _lock:
                _stream_events.append({
                    "type": "error",
                    "message": "Claude CLI not detected. Install Claude Code first.",
                })
            return

    _ensure_loop()
    asyncio.run_coroutine_threadsafe(_send_and_stream(text), _loop)


def poll():
    """Poll for stream updates. Called from game thread every 50ms."""
    result = {}
    with _lock:
        if _stream_events:
            result["events"] = list(_stream_events)
            _stream_events.clear()
        result["thinking"] = _is_thinking
    return result


def clear_session():
    """Clear conversation — disconnect client, next message starts fresh."""
    global _session_id, _total_session_cost, _client_session_id
    with _lock:
        _session_id = None
        _total_session_cost = 0.0
        _client_session_id = None
        loop = _loop
    # Disconnect client (will be recreated on next send_message)
    if loop and loop.is_running():
        future = asyncio.run_coroutine_threadsafe(
            _disconnect_client_safe(), loop)
        try:
            future.result(timeout=15.0)
        except Exception:
            pass


def cleanup():
    """Disconnect client, destroy event loop, clean up."""
    global _session_id, _total_session_cost, _is_thinking, _loop, _loop_thread
    global _client_session_id
    with _lock:
        _session_id = None
        _total_session_cost = 0.0
        _is_thinking = False
        _stream_events.clear()
        _client_session_id = None
        loop = _loop
    # Disconnect client on the event loop
    if loop and loop.is_running():
        future = asyncio.run_coroutine_threadsafe(
            _disconnect_client_safe(), loop)
        try:
            future.result(timeout=15.0)
        except Exception:
            pass
    # Stop the event loop
    with _lock:
        _loop = None
        _loop_thread = None
    if loop and loop.is_running():
        loop.call_soon_threadsafe(loop.stop)
    _log_sdk("BobBot SDK: cleanup — client disconnected, loop stopped")


def get_session_id():
    """Return current session ID for C++ to persist."""
    return _session_id


def set_session_id(sid):
    """Restore a session ID from C++ (loaded from chat history).
    _ensure_client() will detect the mismatch and reconnect on next send_message.
    """
    global _session_id
    _session_id = sid if sid else None


def interrupt():
    """Interrupt current response. Called from C++ cancel button."""
    with _lock:
        if not _is_thinking:
            return
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return

    async def _do_interrupt():
        try:
            if _client and _client_connected:
                await _client.interrupt()
                _log_sdk("BobBot SDK: interrupt sent")
        except Exception as e:
            _log_sdk("BobBot SDK: interrupt failed: {}".format(e))

    asyncio.run_coroutine_threadsafe(_do_interrupt(), loop)


def set_permission_mode(mode):
    """Switch permission mode live on the persistent client.
    Modes: default, acceptEdits, plan, bypassPermissions, dontAsk
    """
    with _lock:
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return

    async def _do():
        try:
            if _client and _client_connected:
                await _client.set_permission_mode(mode)
                _log_sdk("BobBot SDK: permission mode set to {}".format(mode))
        except Exception as e:
            _log_sdk("BobBot SDK: set_permission_mode failed: {}".format(e))

    asyncio.run_coroutine_threadsafe(_do(), loop)


def stop_task(task_id):
    """Cancel a running subagent task."""
    with _lock:
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return

    async def _do():
        try:
            if _client and _client_connected:
                await _client.stop_task(task_id)
                _log_sdk("BobBot SDK: stopped task {}".format(task_id))
        except Exception as e:
            _log_sdk("BobBot SDK: stop_task failed: {}".format(e))

    asyncio.run_coroutine_threadsafe(_do(), loop)


def set_permission_decision(decision):
    """Called by C++ when user clicks Approve/Deny in the approval widget.
    decision: 'allow' or 'deny'. Unblocks the PreToolUse hook.
    """
    global _permission_response
    _permission_response = decision


# --------------------------------------------------------------------------- #
# Session management (SDK session functions)
# --------------------------------------------------------------------------- #
def list_saved_sessions(limit=50, offset=0):
    """List saved sessions from SDK. Returns list of session info dicts."""
    try:
        from claude_agent_sdk import list_sessions
        sessions = list_sessions(directory=_BOB_CWD, limit=limit, offset=offset)
        return [
            {
                "session_id": s.session_id,
                "summary": s.summary or "",
                "last_modified": s.last_modified or 0,
                "custom_title": s.custom_title or "",
                "first_prompt": s.first_prompt or "",
                "tag": s.tag or "",
                "created_at": s.created_at or 0,
            }
            for s in sessions
        ]
    except Exception as e:
        _log_sdk("BobBot SDK: list_saved_sessions failed: {}".format(e))
        return []


def get_saved_session_info(session_id):
    """Get metadata for a single session without loading transcript."""
    try:
        from claude_agent_sdk import get_session_info as _get_info
        info = _get_info(session_id=session_id, directory=_BOB_CWD)
        if not info:
            return None
        return {
            "session_id": info.session_id,
            "summary": info.summary or "",
            "last_modified": info.last_modified or 0,
            "custom_title": info.custom_title or "",
            "first_prompt": info.first_prompt or "",
            "tag": info.tag or "",
            "created_at": info.created_at or 0,
        }
    except Exception as e:
        _log_sdk("BobBot SDK: get_saved_session_info failed: {}".format(e))
        return None


def get_saved_session_messages(session_id, limit=100, offset=0):
    """Load conversation transcript with pagination."""
    try:
        from claude_agent_sdk import get_session_messages as _get_msgs
        messages = _get_msgs(
            session_id=session_id, directory=_BOB_CWD,
            limit=limit, offset=offset)
        return [
            {
                "type": m.type,
                "uuid": m.uuid,
                "message": m.message,
            }
            for m in messages
        ]
    except Exception as e:
        _log_sdk("BobBot SDK: get_saved_session_messages failed: {}".format(e))
        return []


def rename_saved_session(session_id, title):
    """Rename a session (custom title)."""
    try:
        from claude_agent_sdk import rename_session as _rename
        _rename(session_id=session_id, title=title, directory=_BOB_CWD)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def tag_saved_session(session_id, tag):
    """Tag a session for filtering. Pass empty string to clear."""
    try:
        from claude_agent_sdk import tag_session as _tag
        _tag(session_id=session_id, tag=tag if tag else None, directory=_BOB_CWD)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def delete_saved_session(session_id):
    """Permanently delete a session."""
    try:
        from claude_agent_sdk import delete_session as _delete
        _delete(session_id=session_id, directory=_BOB_CWD)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


# --------------------------------------------------------------------------- #
# File checkpointing / undo
# --------------------------------------------------------------------------- #
def rewind_to_message(user_message_id):
    """Undo all file changes back to the state at a given user message."""
    with _lock:
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return {"ok": False, "error": "Not connected"}
    future = asyncio.run_coroutine_threadsafe(
        _client.rewind_files(user_message_id), loop)
    try:
        future.result(timeout=30.0)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


# --------------------------------------------------------------------------- #
# MCP runtime control
# --------------------------------------------------------------------------- #
def get_mcp_status():
    """Get MCP server connection status. Returns list of {name, status, tools}."""
    with _lock:
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return []
    future = asyncio.run_coroutine_threadsafe(_client.get_mcp_status(), loop)
    try:
        return future.result(timeout=5.0)
    except Exception:
        return []


def reconnect_mcp(server_name):
    """Reconnect a failed MCP server."""
    with _lock:
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return
    asyncio.run_coroutine_threadsafe(
        _client.reconnect_mcp_server(server_name), loop)


def toggle_mcp(server_name, enabled):
    """Enable/disable an MCP server at runtime."""
    with _lock:
        loop = _loop
        connected = _client_connected
    if not connected or not loop or not loop.is_running():
        return
    asyncio.run_coroutine_threadsafe(
        _client.toggle_mcp_server(server_name, enabled), loop)


def get_status():
    """Return chat system status."""
    return {
        "claude_found": _claude_path is not None,
        "model": _model,
        "session_id": _session_id,
        "session_cost": _total_session_cost,
        "thinking": is_thinking(),
        "backend": "agent-sdk",
        "client_connected": _client_connected,
    }


def check_sdk_available():
    """Return dict with SDK availability info for C++ polling."""
    ver = ""
    if _sdk_available:
        try:
            import claude_agent_sdk
            ver = getattr(claude_agent_sdk, "__version__", "installed")
        except Exception:
            pass
    return {"ok": _sdk_available, "ver": ver}


def ensure_ready():
    """Ensure SDK is loaded. Call after venv is built. Returns True if ready."""
    global _sdk_available
    with _lock:
        if _sdk_available:
            return True
    _setup_venv_imports()
    try:
        from claude_agent_sdk import ClaudeSDKClient, ClaudeAgentOptions  # noqa: F401
        with _lock:
            _sdk_available = True
        _log_sdk("BobBot SDK: now ready")
        if not _claude_path:
            detect_claude()
        return True
    except ImportError as e:
        _log_sdk("BobBot SDK: ensure_ready failed: {}".format(e))
        return False


def fork_current_session(title=None, up_to_message_id=None):
    """Fork the current SDK session. Returns dict with ok, session_id or error."""
    with _lock:
        available = _sdk_available
        sid = _session_id
    if not available:
        return {"ok": False, "error": "SDK not available"}
    if not sid:
        return {"ok": False, "error": "No active session to fork"}
    try:
        from claude_agent_sdk import fork_session
        result = fork_session(
            session_id=sid, directory=_BOB_CWD,
            title=title, up_to_message_id=up_to_message_id)
        return {"ok": True, "session_id": result.session_id}
    except Exception as e:
        return {"ok": False, "error": str(e)}
