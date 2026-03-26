"""
BobBot Chat — routes in-editor AI chat through Claude Code CLI.

Spawns `claude -p --output-format json` as a subprocess, using the user's
existing Claude Code OAuth subscription. Claude Code handles tool use
(execute_unreal_python) via the MCP bridge automatically.

No API key required.
"""

import os
import sys
import json
import shutil
import subprocess
import threading
import traceback

# --------------------------------------------------------------------------- #
# State
# --------------------------------------------------------------------------- #
_claude_path = None  # resolved path to claude CLI
_session_id = None   # conversation session UUID
_model = "sonnet"
_total_session_cost = 0.0

_lock = threading.Lock()
_pending_response = None   # dict: {text, cost, duration_ms, num_turns, is_error}
_is_thinking = False
_error_message = None
_process = None

_PROJECT_ROOT = os.environ.get("BOB_PROJECT_ROOT", os.getcwd())

_SYSTEM_PROMPT = (
    "You are BobBot, an AI assistant embedded inside the Unreal Engine 5 editor. "
    "You help users with Unreal Engine tasks: creating assets, editing Blueprints, "
    "modifying levels, writing gameplay code, and answering questions about their project.\n\n"
    "You have MCP tools connected to the running UE editor:\n"
    "- execute_unreal_python: Run Python code inside the editor with access to the `unreal` module "
    "and full UE Python API, including `unreal.BobBotLib` for Blueprint manipulation.\n"
    "- ping_unreal: Check if the editor is responding.\n\n"
    "When the user asks you to do something in the editor, use execute_unreal_python. "
    "Be concise. Show what you did and the result."
)


# --------------------------------------------------------------------------- #
# Detection & Auth
# --------------------------------------------------------------------------- #
def detect_claude():
    """Check if claude CLI is available. Returns (found: bool, version: str)."""
    global _claude_path

    # Try to find claude
    path = shutil.which("claude")
    if not path:
        # Windows: check common locations
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
    """Check if the user is authenticated with Claude Code. Returns (authenticated: bool, status: str)."""
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
    """Set the model: 'sonnet', 'opus', or 'haiku'."""
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


# --------------------------------------------------------------------------- #
# Subprocess chat
# --------------------------------------------------------------------------- #
def _build_command():
    """Build the claude CLI command."""
    mcp_config = os.path.join(_PROJECT_ROOT, ".mcp.json").replace("\\", "/")

    cmd = [
        _claude_path, "-p",
        "--output-format", "json",
        "--model", _model,
        "--system-prompt", _SYSTEM_PROMPT,
        "--permission-mode", "bypassPermissions",
    ]

    # Only add MCP config if the file exists
    if os.path.isfile(mcp_config):
        cmd.extend(["--mcp-config", mcp_config])

    # Session continuity
    if _session_id:
        cmd.extend(["--resume", _session_id])

    return cmd


def _classify_error(stderr, returncode):
    """Classify subprocess errors into user-friendly messages."""
    s = stderr.lower() if stderr else ""
    if "not authenticated" in s or "login" in s or "oauth" in s or "not logged in" in s:
        return "Not authenticated. Run 'claude login' in a terminal to sign in."
    if "rate limit" in s or "429" in s:
        return "Rate limited. Wait a moment and try again."
    if "budget" in s:
        return "Cost budget exceeded for this request."
    if returncode == 127 or "not found" in s:
        return "Claude CLI not found. Install: npm install -g @anthropic-ai/claude-code"
    return "Claude CLI error (exit {}): {}".format(returncode, stderr[:500] if stderr else "unknown")


def _chat_thread(user_message):
    """Background thread: spawns claude CLI, waits for response."""
    global _pending_response, _is_thinking, _error_message, _session_id, _process, _total_session_cost

    try:
        with _lock:
            _is_thinking = True

        cmd = _build_command()

        _process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            cwd=_PROJECT_ROOT,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )

        stdout, stderr = _process.communicate(input=user_message, timeout=300)

        if _process.returncode != 0:
            with _lock:
                _error_message = _classify_error(stderr, _process.returncode)
                _is_thinking = False
            return

        # Parse JSON response
        result = json.loads(stdout)

        # Capture session ID
        sid = result.get("session_id")
        if sid:
            _session_id = sid

        cost = result.get("total_cost_usd", 0)
        _total_session_cost += cost

        response_text = result.get("result", "(no response)")
        is_error = result.get("is_error", False)

        if is_error:
            with _lock:
                _error_message = response_text
                _is_thinking = False
            return

        with _lock:
            _pending_response = {
                "text": response_text,
                "cost": cost,
                "duration_ms": result.get("duration_ms", 0),
                "num_turns": result.get("num_turns", 1),
            }
            _is_thinking = False

    except subprocess.TimeoutExpired:
        if _process:
            _process.kill()
        with _lock:
            _error_message = "Request timed out after 5 minutes."
            _is_thinking = False
    except json.JSONDecodeError as e:
        with _lock:
            _error_message = "Failed to parse Claude response: {}".format(e)
            _is_thinking = False
    except Exception as e:
        with _lock:
            _error_message = "Error: {}".format(traceback.format_exc())
            _is_thinking = False
    finally:
        _process = None


# --------------------------------------------------------------------------- #
# Public API (called from C++ via game thread Python exec)
# --------------------------------------------------------------------------- #
def send_message(text):
    """Send a user message. Spawns claude CLI in background thread."""
    if not _claude_path:
        global _error_message
        with _lock:
            _error_message = "Claude CLI not detected. Install Claude Code first."
        return

    t = threading.Thread(target=_chat_thread, args=(text,), daemon=True)
    t.start()


def poll():
    """
    Poll for updates. Called from game thread tick.
    Returns dict with keys: response, cost, duration_ms, num_turns, error, thinking
    """
    global _pending_response, _error_message

    result = {}

    with _lock:
        if _pending_response is not None:
            result["response"] = _pending_response["text"]
            result["cost"] = _pending_response.get("cost", 0)
            result["duration_ms"] = _pending_response.get("duration_ms", 0)
            result["num_turns"] = _pending_response.get("num_turns", 1)
            _pending_response = None
        if _error_message is not None:
            result["error"] = _error_message
            _error_message = None
        result["thinking"] = _is_thinking

    return result


def clear_session():
    """Clear conversation — next message starts a new session."""
    global _session_id, _total_session_cost
    _session_id = None
    _total_session_cost = 0.0


def get_status():
    """Return chat system status."""
    return {
        "claude_found": _claude_path is not None,
        "model": _model,
        "session_id": _session_id,
        "session_cost": _total_session_cost,
        "thinking": is_thinking(),
    }
