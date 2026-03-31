"""
BobBot Chat (Agent SDK) — persistent Claude process via claude-agent-sdk.

Drop-in replacement for bob_chat.py. Same public API:
  send_message(), poll(), cleanup(), clear_session(),
  set_model(), get_model(), is_thinking(), get_session_cost(),
  get_session_id(), set_session_id(), get_status(),
  detect_claude(), check_auth()

Key difference: instead of spawning a new `claude -p` subprocess per message,
this uses ClaudeSDKClient which keeps a single persistent Claude process alive
across the entire editor session. Subsequent messages have ~0s overhead.

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
    from claude_agent_sdk import query, ClaudeAgentOptions  # noqa: F401
    _sdk_available = True
    _log_sdk("BobBot SDK: claude-agent-sdk available (from venv)")
except ImportError as _e:
    _log_sdk("BobBot SDK: not available yet ({}). Will retry after venv setup.".format(_e))

# --------------------------------------------------------------------------- #
# State (same interface as bob_chat.py)
# --------------------------------------------------------------------------- #
_claude_path = None
_session_id = None
_model = "sonnet"
_total_session_cost = 0.0

_lock = threading.Lock()
_is_thinking = False
_process = None  # Not used directly, but kept for API compat
_stream_events = []

# Async event loop in background thread
_loop = None
_loop_thread = None
_client = None  # ClaudeSDKClient instance


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

try:
    import unreal as _unreal_init
    _unreal_init.log("BobBot SDK chat: PROJECT_ROOT = {}".format(_PROJECT_ROOT))
except Exception:
    pass

_SYSTEM_PROMPT = (
    "You are BobBot, an AI assistant embedded inside the Unreal Engine 5 editor. "
    "You help users with Unreal Engine tasks: creating assets, editing Blueprints, "
    "modifying levels, writing gameplay code, and answering questions about their project. "
    "You have 158 MCP tools connected to the running UE editor for actors, assets, materials, "
    "levels, viewport, lighting, animation, AI, physics, sequencer, and more. "
    "Prefer these tools over execute_unreal_python. "
    "Your full tool reference is at {claude_md} - read it at the start of a conversation. "
    "Scope-triggered rules for specific domains (actors, materials, blueprints, animation, "
    "lighting, AI, physics, etc.) are at {rules_dir} - read the relevant rule file when "
    "working in that domain. "
    "If {project_md} exists, read it for project-specific context. "
    "Be concise. Show what you did and the result."
).format(
    claude_md=os.path.join(_PROJECT_ROOT, "Plugins", "BobBot", "CLAUDE.md").replace("\\", "/"),
    rules_dir=os.path.join(_PROJECT_ROOT, "Plugins", "BobBot", "Config", "Rules").replace("\\", "/"),
    project_md=os.path.join(_PROJECT_ROOT, "PROJECT.md").replace("\\", "/"),
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
# Configuration (same as bob_chat.py)
# --------------------------------------------------------------------------- #
def set_model(name):
    global _model
    if name in ("sonnet", "opus", "haiku"):
        _model = name


def get_model():
    return _model


def is_thinking():
    with _lock:
        return _is_thinking


def get_session_cost():
    return _total_session_cost


def _ensure_system_prompt_file():
    """Write default system prompt to disk if no file exists. Return the path."""
    if not os.path.isfile(_SYSTEM_PROMPT_PATH):
        os.makedirs(os.path.dirname(_SYSTEM_PROMPT_PATH), exist_ok=True)
        with open(_SYSTEM_PROMPT_PATH, "w", encoding="utf-8") as f:
            f.write(_SYSTEM_PROMPT)
    return _SYSTEM_PROMPT_PATH


def get_default_prompt():
    """Return the built-in default system prompt string."""
    return _SYSTEM_PROMPT


# --------------------------------------------------------------------------- #
# Windows: suppress console windows for all subprocesses
#
# On Windows, any executable built with the "console" subsystem (including the
# bundled claude.exe) creates a visible console window unless CREATE_NO_WINDOW
# is passed to CreateProcess. The Agent SDK spawns claude via anyio.open_process
# which doesn't set this flag. This patch adds it to all Popen calls from this
# interpreter. It's safe because UE's embedded Python has no reason to show
# console windows.
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
    if _loop is not None and _loop.is_running():
        return

    _loop = asyncio.new_event_loop()
    _loop_thread = threading.Thread(target=_loop.run_forever, daemon=True, name="bobbot-sdk-loop")
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
    config_path = fallback if os.path.isfile(fallback) else os.path.join(_PROJECT_ROOT, ".mcp.json")
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


def _get_system_prompt():
    """Get the active system prompt (custom override or default)."""
    if os.path.isfile(_SYSTEM_PROMPT_PATH):
        try:
            with open(_SYSTEM_PROMPT_PATH, "r", encoding="utf-8") as f:
                content = f.read().strip()
            if content:
                return content
        except Exception:
            pass
    return _SYSTEM_PROMPT


# --------------------------------------------------------------------------- #
# SDK types — lazily cached after first successful import
# --------------------------------------------------------------------------- #
_sdk_types = {}


def _get_sdk_types():
    """Return cached dict of SDK classes. Import once, reuse forever."""
    if not _sdk_types:
        import claude_agent_sdk
        from claude_agent_sdk import (
            query, ClaudeAgentOptions,
            AssistantMessage, UserMessage, ResultMessage, SystemMessage,
            TextBlock, ToolUseBlock,
            ClaudeSDKError,
        )
        _sdk_types.update(
            module=claude_agent_sdk, query=query, ClaudeAgentOptions=ClaudeAgentOptions,
            AssistantMessage=AssistantMessage, UserMessage=UserMessage,
            ResultMessage=ResultMessage, SystemMessage=SystemMessage,
            TextBlock=TextBlock, ToolUseBlock=ToolUseBlock,
            ClaudeSDKError=ClaudeSDKError,
        )
    return _sdk_types


# --------------------------------------------------------------------------- #
# SDK streaming
# --------------------------------------------------------------------------- #
async def _send_and_stream(user_message):
    """Send a message via Agent SDK and stream events into _stream_events queue."""
    global _is_thinking, _session_id, _total_session_cost

    sdk = _get_sdk_types()
    query = sdk["query"]
    ClaudeAgentOptions = sdk["ClaudeAgentOptions"]
    AssistantMessage = sdk["AssistantMessage"]
    UserMessage = sdk["UserMessage"]
    ResultMessage = sdk["ResultMessage"]
    SystemMessage = sdk["SystemMessage"]
    TextBlock = sdk["TextBlock"]
    ToolUseBlock = sdk["ToolUseBlock"]
    ClaudeSDKError = sdk["ClaudeSDKError"]
    claude_agent_sdk = sdk["module"]

    with _lock:
        _is_thinking = True
        _stream_events.clear()

    try:
        # Use SDK's bundled claude.exe (native binary) with CREATE_NO_WINDOW
        # instead of claude.cmd which goes through cmd.exe and flashes a terminal
        bundled_cli = os.path.join(
            os.path.dirname(os.path.abspath(claude_agent_sdk.__file__)),
            "_bundled", "claude.exe")
        cli_path = bundled_cli if os.path.isfile(bundled_cli) else None
        _log_sdk("BobBot SDK: using cli_path={}".format(cli_path))

        options = ClaudeAgentOptions(
            model=_model,
            system_prompt=_get_system_prompt(),
            cwd=_BOB_CWD,
            mcp_servers=_build_mcp_servers(),
            permission_mode="bypassPermissions",
            cli_path=cli_path,
        )

        if _session_id:
            options.resume = _session_id

        async for message in query(prompt=user_message, options=options):

            if isinstance(message, SystemMessage):
                sid = message.data.get("session_id") if hasattr(message, "data") else None
                if sid:
                    with _lock:
                        _session_id = sid

            elif isinstance(message, AssistantMessage):
                for block in message.content:
                    if isinstance(block, TextBlock):
                        if block.text:
                            with _lock:
                                _stream_events.append({"type": "text", "text": block.text})
                    elif isinstance(block, ToolUseBlock):
                        # Validate input — may be partial JSON during streaming
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
                                input_str = tool_input  # Show raw partial
                        else:
                            input_str = str(tool_input) if tool_input else "{}"
                        with _lock:
                            _stream_events.append({
                                "type": "tool_use",
                                "name": block.name,
                                "input": input_str,
                            })
                    else:
                        # Handle error blocks or other content types
                        btype = getattr(block, "type", "")
                        if btype == "error":
                            error_msg = getattr(block, "message", "") or getattr(block, "text", "") or str(block)
                            with _lock:
                                _stream_events.append({
                                    "type": "error",
                                    "message": "Assistant error: {}".format(error_msg),
                                })

            elif isinstance(message, UserMessage):
                # Tool results come as UserMessages
                for block in message.content:
                    btype = getattr(block, "type", "")
                    if btype == "tool_result":
                        output = getattr(block, "content", "")
                        if isinstance(output, list):
                            output = "\n".join(
                                b.get("text", "") for b in output if isinstance(b, dict) and b.get("type") == "text"
                            )
                        with _lock:
                            _stream_events.append({
                                "type": "tool_result",
                                "output": str(output),
                            })

            elif isinstance(message, ResultMessage):
                cost = message.total_cost_usd or 0
                with _lock:
                    if message.session_id:
                        _session_id = message.session_id
                    _total_session_cost += cost
                    _stream_events.append({
                        "type": "complete",
                        "cost": cost,
                        "duration_ms": message.duration_ms or 0,
                        "num_turns": message.num_turns or 1,
                        "is_error": message.is_error,
                        "result_text": message.result or "",
                    })

    except ClaudeSDKError as e:
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "Claude SDK error: {}".format(str(e)),
            })
    except Exception:
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "SDK stream error: {}".format(traceback.format_exc()),
            })
    finally:
        with _lock:
            _is_thinking = False


# --------------------------------------------------------------------------- #
# Public API (matches bob_chat.py exactly)
# --------------------------------------------------------------------------- #
def send_message(text):
    """Send a user message. Uses Agent SDK in background async loop."""
    global _sdk_available
    if not _sdk_available:
        # Retry — venv may have finished building since module load
        _setup_venv_imports()
        try:
            from claude_agent_sdk import query as _q, ClaudeAgentOptions as _o  # noqa: F401
            _sdk_available = True
            _log_sdk("BobBot SDK: now available (venv ready)")
        except ImportError:
            with _lock:
                _stream_events.append({
                    "type": "error",
                    "message": "claude-agent-sdk not ready. Venv may still be building — try again in a moment.",
                })
            return

    if not _claude_path:
        # Try detecting Claude now (may not have been called yet via this module)
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
    """Poll for stream updates. Called from game thread every 100ms."""
    result = {}
    with _lock:
        if _stream_events:
            result["events"] = list(_stream_events)
            _stream_events.clear()
        result["thinking"] = _is_thinking
    return result


def clear_session():
    """Clear conversation — next message starts a new session."""
    global _session_id, _total_session_cost
    _session_id = None
    _total_session_cost = 0.0


def cleanup():
    """Kill any running query and clean up."""
    global _session_id, _total_session_cost, _is_thinking
    # The SDK manages subprocess lifecycle — just clear our state
    with _lock:
        _session_id = None
        _total_session_cost = 0.0
        _is_thinking = False
        _stream_events.clear()


def get_session_id():
    """Return current session ID for C++ to persist."""
    return _session_id


def set_session_id(sid):
    """Restore a session ID from C++ (loaded from chat history)."""
    global _session_id
    _session_id = sid if sid else None


def get_status():
    """Return chat system status."""
    return {
        "claude_found": _claude_path is not None,
        "model": _model,
        "session_id": _session_id,
        "session_cost": _total_session_cost,
        "thinking": is_thinking(),
        "backend": "agent-sdk",
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
