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
import subprocess
import bob_platform

# --------------------------------------------------------------------------- #
# SDK dependency — import from the shared venv created by bob_bridge_launcher
# --------------------------------------------------------------------------- #
from bob_sdk_config import _log_sdk


def _setup_venv_imports():
    """Add venv site-packages (and pywin32 DLLs on Windows) to sys.path."""
    try:
        import bob_bridge_launcher
        sp = bob_bridge_launcher.get_venv_site_packages()
        if not sp or not os.path.isdir(sp):
            return False
        if sp not in sys.path:
            sys.path.insert(0, sp)
        bob_platform.process_pth_files(sp)
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
# Import sub-modules
# --------------------------------------------------------------------------- #
import bob_sdk_config
import bob_sdk_permissions
import bob_sdk_events
import bob_sdk_client


# --------------------------------------------------------------------------- #
# Windows: suppress console windows for all subprocesses
# --------------------------------------------------------------------------- #
if bob_platform.IS_WINDOWS:
    _orig_popen = subprocess.Popen.__init__

    def _popen_no_window(self, *args, **kwargs):
        kwargs.setdefault("creationflags", 0)
        kwargs["creationflags"] |= bob_platform.NO_WINDOW_FLAGS
        _orig_popen(self, *args, **kwargs)

    subprocess.Popen.__init__ = _popen_no_window


# --------------------------------------------------------------------------- #
# Detection & Auth (same as bob_chat.py)
# --------------------------------------------------------------------------- #
_claude_path = None


def detect_claude():
    """Check if claude CLI is available. Returns (found: bool, version: str)."""
    global _claude_path
    import subprocess

    path = shutil.which("claude")
    if not path:
        # PATH lookup can miss when the editor is launched from a GUI/desktop
        # launcher with a slim environment. Probe common per-OS install dirs.
        if bob_platform.IS_WINDOWS:
            candidates = [
                os.path.expanduser("~/scoop/apps/nodejs/current/bin/claude"),
                os.path.expanduser("~/scoop/apps/nodejs/current/bin/claude.cmd"),
                os.path.expanduser("~/.npm-global/bin/claude"),
                os.path.expanduser("~/AppData/Roaming/npm/claude.cmd"),
            ]
        else:
            # Linux and macOS: local installs, Homebrew, and Node version managers.
            candidates = [
                os.path.expanduser("~/.local/bin/claude"),
                os.path.expanduser("~/.npm-global/bin/claude"),
                "/usr/local/bin/claude",
                "/opt/homebrew/bin/claude",
                os.path.expanduser("~/.volta/bin/claude"),
                os.path.expanduser("~/bin/claude"),
            ]
            # nvm/fnm keep the binary under a versioned dir — glob for it.
            import glob
            for pattern in (
                os.path.expanduser("~/.nvm/versions/node/*/bin/claude"),
                os.path.expanduser("~/.fnm/node-versions/*/installation/bin/claude"),
                os.path.expanduser("~/.local/share/fnm/node-versions/*/installation/bin/claude"),
            ):
                candidates.extend(sorted(glob.glob(pattern), reverse=True))
        for candidate in candidates:
            if os.path.isfile(candidate):
                path = candidate
                break

    if not path:
        return False, ""

    try:
        result = subprocess.run(
            [path, "--version"],
            capture_output=True, text=True, timeout=10,
            **bob_platform.subprocess_kwargs(),
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
        with open(cred_path, "r", encoding="utf-8") as f:
            creds = json.load(f)
        if creds:
            return True, "Authenticated"
    except (json.JSONDecodeError, IOError):
        pass
    return False, "Credentials file exists but may be invalid. Run 'claude login'."


# --------------------------------------------------------------------------- #
# Configuration (delegated to bob_sdk_config)
# --------------------------------------------------------------------------- #
def set_model(name):
    """Set model. If client is connected, switch live via SDK control protocol."""
    bob_sdk_config.set_model(name)
    # Live switch if connected
    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if connected and loop and loop.is_running():
        async def _do_set_model():
            try:
                if bob_sdk_client._client and bob_sdk_client._client_connected:
                    await bob_sdk_client._client.set_model(name)
                    _log_sdk("BobBot SDK: model switched to {} (live)".format(name))
            except Exception as e:
                _log_sdk("BobBot SDK: live model switch failed: {} "
                         "(will use on reconnect)".format(e))
        asyncio.run_coroutine_threadsafe(_do_set_model(), loop)


def get_model():
    return bob_sdk_config.get_model()


def set_thinking_mode(mode, budget=None):
    """Set extended thinking mode. Reconnects client on next message (resume preserves session).

    Args:
        mode: 'disabled', 'enabled', or 'adaptive'.
        budget: Token budget for 'enabled' mode (defaults to current value or 10000).
    """
    if mode not in ("disabled", "enabled", "adaptive"):
        return
    os.environ["BOB_THINKING_MODE"] = mode
    if budget is not None:
        try:
            os.environ["BOB_THINKING_BUDGET"] = str(int(budget))
        except (ValueError, TypeError):
            pass
    with bob_sdk_events._lock:
        bob_sdk_client._client_needs_rebuild = True
    _log_sdk("BobBot SDK: thinking mode set to {} (will rebuild on next message)".format(mode))


def set_effort(level):
    """Set effort level. Reconnects client on next message (resume preserves session).

    Args:
        level: 'low', 'medium', 'high', or 'max'.
    """
    if level not in ("low", "medium", "high", "max"):
        return
    os.environ["BOB_EFFORT"] = level
    with bob_sdk_events._lock:
        bob_sdk_client._client_needs_rebuild = True
    _log_sdk("BobBot SDK: effort set to {} (will rebuild on next message)".format(level))


def is_thinking():
    with bob_sdk_events._lock:
        return bob_sdk_events._is_thinking


def get_session_cost():
    return bob_sdk_client._total_session_cost


def get_default_prompt():
    """Return the built-in default system prompt string."""
    return bob_sdk_config.get_default_prompt()


# --------------------------------------------------------------------------- #
# Public API (matches bob_chat.py exactly)
# --------------------------------------------------------------------------- #
def send_message(text):
    """Send a user message. Uses persistent ClaudeSDKClient in background loop."""
    global _sdk_available
    with bob_sdk_events._lock:
        available = _sdk_available
    if not available:
        # Retry — venv may have finished building since module load
        _setup_venv_imports()
        try:
            from claude_agent_sdk import ClaudeSDKClient as _c, ClaudeAgentOptions as _o  # noqa: F401
            with bob_sdk_events._lock:
                _sdk_available = True
            _log_sdk("BobBot SDK: now available (venv ready)")
        except ImportError:
            with bob_sdk_events._lock:
                bob_sdk_events._stream_events.append({
                    "type": "error",
                    "message": "claude-agent-sdk not ready. "
                               "Venv may still be building — try again in a moment.",
                })
            return

    if not _claude_path:
        detect_claude()
        if not _claude_path:
            with bob_sdk_events._lock:
                bob_sdk_events._stream_events.append({
                    "type": "error",
                    "message": "Claude CLI not detected. Install Claude Code first.",
                })
            return

    bob_sdk_client._ensure_loop()
    asyncio.run_coroutine_threadsafe(
        bob_sdk_events._send_and_stream(text), bob_sdk_client._loop)


def poll():
    """Poll for stream updates. Called from game thread every 50ms."""
    result = {}
    with bob_sdk_events._lock:
        if bob_sdk_events._stream_events:
            result["events"] = list(bob_sdk_events._stream_events)
            bob_sdk_events._stream_events.clear()
        result["thinking"] = bob_sdk_events._is_thinking
    return result


def clear_session():
    """Clear conversation — disconnect client, next message starts fresh."""
    bob_sdk_permissions._drain_pending("deny")
    with bob_sdk_events._lock:
        bob_sdk_client._session_id = None
        bob_sdk_client._total_session_cost = 0.0
        bob_sdk_client._client_session_id = None
        loop = bob_sdk_client._loop
    # Disconnect client (will be recreated on next send_message)
    if loop and loop.is_running():
        future = asyncio.run_coroutine_threadsafe(
            bob_sdk_client._disconnect_client_safe(), loop)
        try:
            future.result(timeout=15.0)
        except Exception:
            pass


def cleanup():
    """Disconnect client, destroy event loop, clean up."""
    bob_sdk_permissions._drain_pending("deny")
    with bob_sdk_events._lock:
        bob_sdk_client._session_id = None
        bob_sdk_client._total_session_cost = 0.0
        bob_sdk_events._is_thinking = False
        bob_sdk_events._stream_events.clear()
        bob_sdk_client._client_session_id = None
        loop = bob_sdk_client._loop
    # Disconnect client on the event loop
    if loop and loop.is_running():
        future = asyncio.run_coroutine_threadsafe(
            bob_sdk_client._disconnect_client_safe(), loop)
        try:
            future.result(timeout=15.0)
        except Exception:
            pass
    # Stop the event loop
    with bob_sdk_events._lock:
        bob_sdk_client._loop = None
        bob_sdk_client._loop_thread = None
    if loop and loop.is_running():
        loop.call_soon_threadsafe(loop.stop)
    _log_sdk("BobBot SDK: cleanup — client disconnected, loop stopped")


def get_session_id():
    """Return current session ID for C++ to persist."""
    return bob_sdk_client._session_id


def set_session_id(sid):
    """Restore a session ID from C++ (loaded from chat history).
    _ensure_client() will detect the mismatch and reconnect on next send_message.
    """
    bob_sdk_client._session_id = sid if sid else None


def interrupt():
    """Interrupt current response. Called from C++ cancel button."""
    with bob_sdk_events._lock:
        if not bob_sdk_events._is_thinking:
            return
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return

    async def _do_interrupt():
        try:
            if bob_sdk_client._client and bob_sdk_client._client_connected:
                await bob_sdk_client._client.interrupt()
                _log_sdk("BobBot SDK: interrupt sent")
        except Exception as e:
            _log_sdk("BobBot SDK: interrupt failed: {}".format(e))

    asyncio.run_coroutine_threadsafe(_do_interrupt(), loop)


def set_permission_mode(mode):
    """Switch permission mode live on the persistent client.
    Modes: default, acceptEdits, plan, bypassPermissions, dontAsk

    Also resolves any pending approval prompts so coroutines waiting on the
    UI don't hang after a mode switch makes the prompt obsolete:
      - bypassPermissions / acceptEdits / dontAsk: drain to 'allow'
      - plan: drain to 'deny'
    The 'default' mode keeps prompts in flight (the UI still wants an answer).
    """
    if mode in ("bypassPermissions", "acceptEdits", "dontAsk"):
        bob_sdk_permissions._drain_pending("allow")
    elif mode == "plan":
        bob_sdk_permissions._drain_pending("deny")

    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return

    async def _do():
        try:
            if bob_sdk_client._client and bob_sdk_client._client_connected:
                await bob_sdk_client._client.set_permission_mode(mode)
                _log_sdk("BobBot SDK: permission mode set to {}".format(mode))
        except Exception as e:
            _log_sdk("BobBot SDK: set_permission_mode failed: {}".format(e))

    asyncio.run_coroutine_threadsafe(_do(), loop)


def stop_task(task_id):
    """Cancel a running subagent task."""
    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return

    async def _do():
        try:
            if bob_sdk_client._client and bob_sdk_client._client_connected:
                await bob_sdk_client._client.stop_task(task_id)
                _log_sdk("BobBot SDK: stopped task {}".format(task_id))
        except Exception as e:
            _log_sdk("BobBot SDK: stop_task failed: {}".format(e))

    asyncio.run_coroutine_threadsafe(_do(), loop)


def set_permission_decision(request_id, decision):
    """Called by C++ when user clicks Approve/Deny in the approval widget.

    Args:
        request_id: int id from the approval_request event.
        decision: 'allow' or 'deny'.
    """
    bob_sdk_permissions.set_permission_decision(request_id, decision)


# --------------------------------------------------------------------------- #
# Session management (SDK session functions)
# --------------------------------------------------------------------------- #
def list_saved_sessions(limit=50, offset=0):
    """List saved sessions from SDK. Returns list of session info dicts."""
    try:
        from claude_agent_sdk import list_sessions
        sessions = list_sessions(
            directory=bob_sdk_config._BOB_CWD, limit=limit, offset=offset)
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
        info = _get_info(session_id=session_id, directory=bob_sdk_config._BOB_CWD)
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
            session_id=session_id, directory=bob_sdk_config._BOB_CWD,
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
        _rename(session_id=session_id, title=title, directory=bob_sdk_config._BOB_CWD)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def tag_saved_session(session_id, tag):
    """Tag a session for filtering. Pass empty string to clear."""
    try:
        from claude_agent_sdk import tag_session as _tag
        _tag(session_id=session_id, tag=tag if tag else None,
             directory=bob_sdk_config._BOB_CWD)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def delete_saved_session(session_id):
    """Permanently delete a session."""
    try:
        from claude_agent_sdk import delete_session as _delete
        _delete(session_id=session_id, directory=bob_sdk_config._BOB_CWD)
        return {"ok": True}
    except Exception as e:
        return {"ok": False, "error": str(e)}


# --------------------------------------------------------------------------- #
# File checkpointing / undo
# --------------------------------------------------------------------------- #
def rewind_to_message(user_message_id):
    """Undo all file changes back to the state at a given user message."""
    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return {"ok": False, "error": "Not connected"}
    future = asyncio.run_coroutine_threadsafe(
        bob_sdk_client._client.rewind_files(user_message_id), loop)
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
    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return []
    future = asyncio.run_coroutine_threadsafe(
        bob_sdk_client._client.get_mcp_status(), loop)
    try:
        return future.result(timeout=5.0)
    except Exception:
        return []


def reconnect_mcp(server_name):
    """Reconnect a failed MCP server."""
    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return
    asyncio.run_coroutine_threadsafe(
        bob_sdk_client._client.reconnect_mcp_server(server_name), loop)


def toggle_mcp(server_name, enabled):
    """Enable/disable an MCP server at runtime."""
    with bob_sdk_events._lock:
        loop = bob_sdk_client._loop
        connected = bob_sdk_client._client_connected
    if not connected or not loop or not loop.is_running():
        return
    asyncio.run_coroutine_threadsafe(
        bob_sdk_client._client.toggle_mcp_server(server_name, enabled), loop)


def get_status():
    """Return chat system status."""
    return {
        "claude_found": _claude_path is not None,
        "model": bob_sdk_config._model,
        "session_id": bob_sdk_client._session_id,
        "session_cost": bob_sdk_client._total_session_cost,
        "thinking": is_thinking(),
        "backend": "agent-sdk",
        "client_connected": bob_sdk_client._client_connected,
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
    with bob_sdk_events._lock:
        if _sdk_available:
            return True
    _setup_venv_imports()
    try:
        from claude_agent_sdk import ClaudeSDKClient, ClaudeAgentOptions  # noqa: F401
        with bob_sdk_events._lock:
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
    with bob_sdk_events._lock:
        available = _sdk_available
        sid = bob_sdk_client._session_id
    if not available:
        return {"ok": False, "error": "SDK not available"}
    if not sid:
        return {"ok": False, "error": "No active session to fork"}
    try:
        from claude_agent_sdk import fork_session
        result = fork_session(
            session_id=sid, directory=bob_sdk_config._BOB_CWD,
            title=title, up_to_message_id=up_to_message_id)
        return {"ok": True, "session_id": result.session_id}
    except Exception as e:
        return {"ok": False, "error": str(e)}
