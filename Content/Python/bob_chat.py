"""
BobBot Chat — routes in-editor AI chat through Claude Code CLI.

Spawns `claude -p --output-format stream-json` as a subprocess, using the user's
existing Claude Code OAuth subscription. Claude Code handles tool use
(execute_unreal_python) via the MCP bridge automatically.

Stream events arrive as newline-delimited JSON on stdout. Each event is queued
and drained by poll() on the game thread every 100ms.

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
_is_thinking = False
_process = None

# Stream event queue: list of dicts consumed by poll().
# Each entry is one of:
#   {"type": "text", "text": "full text of this turn segment"}
#   {"type": "tool_use", "name": "...", "input": "..."}
#   {"type": "tool_result", "output": "..."}
#   {"type": "complete", "cost": 0.05, "duration_ms": 1234, "num_turns": 2}
_stream_events = []


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
    _unreal_init.log("BobBot chat: PROJECT_ROOT = {}".format(_PROJECT_ROOT))
except Exception:
    pass

def _build_system_prompt():
    """Build system prompt with CLAUDE.md and PROJECT.md content inlined."""
    claude_md_path = os.path.join(_PROJECT_ROOT, "Plugins", "BobBot", "CLAUDE.md")
    claude_md_content = ""
    try:
        with open(claude_md_path, "r", encoding="utf-8") as f:
            claude_md_content = f.read().strip()
    except Exception:
        claude_md_content = "(Could not read {})".format(claude_md_path)

    project_md_path = os.path.join(_PROJECT_ROOT, "PROJECT.md")
    project_md_content = ""
    try:
        if os.path.isfile(project_md_path):
            with open(project_md_path, "r", encoding="utf-8") as f:
                project_md_content = f.read().strip()
    except Exception:
        pass

    rules_dir = os.path.join(
        _PROJECT_ROOT, "Plugins", "BobBot", "Config", "Rules"
    ).replace("\\", "/")

    prompt = (
        "You are BobBot, an AI assistant embedded inside the Unreal Engine 5 editor. "
        "You help users with Unreal Engine tasks: creating assets, editing Blueprints, "
        "modifying levels, writing gameplay code, and answering questions about their project. "
        "You have 159 MCP tools connected to the running UE editor for actors, assets, materials, "
        "levels, viewport, lighting, animation, AI, physics, sequencer, and more. "
        "Prefer these tools over execute_unreal_python.\n\n"
        "=== TOOL REFERENCE (already loaded — do NOT read CLAUDE.md via tools) ===\n"
        "{claude_md}\n"
        "=== END TOOL REFERENCE ===\n\n"
        "Scope-triggered rules for specific domains (actors, materials, blueprints, animation, "
        "lighting, AI, physics, etc.) are at {rules_dir} — read the relevant rule file when "
        "working in that domain.\n"
    ).format(claude_md=claude_md_content, rules_dir=rules_dir)

    if project_md_content:
        prompt += (
            "\n=== PROJECT CONTEXT (already loaded — do NOT read PROJECT.md via tools) ===\n"
            "{}\n=== END PROJECT CONTEXT ===\n"
        ).format(project_md_content)
    else:
        prompt += "If {} exists, read it for project-specific context.\n".format(
            project_md_path.replace("\\", "/"))

    prompt += "Be concise. Show what you did and the result."
    return prompt


_SYSTEM_PROMPT = _build_system_prompt()

_SYSTEM_PROMPT_PATH = os.path.join(_PROJECT_ROOT, "Saved", "BobBot", "_system_prompt.txt")


# --------------------------------------------------------------------------- #
# Detection & Auth
# --------------------------------------------------------------------------- #
def detect_claude():
    """Check if claude CLI is available. Returns (found: bool, version: str)."""
    global _claude_path

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
# Subprocess: command building
# --------------------------------------------------------------------------- #
def _find_mcp_config():
    """Find BobBot's MCP config, preferring HTTP transport if bridge is running."""
    saved_dir = os.path.join(_PROJECT_ROOT, "Saved", "BobBot")

    # Prefer HTTP config if bridge is running
    http_config = os.path.join(saved_dir, "_bobbot_mcp.json")
    if os.path.isfile(http_config):
        try:
            with open(http_config, "r") as f:
                cfg = json.load(f)
            # Check if this is an HTTP config and bridge is actually responding
            servers = cfg.get("mcpServers", {})
            unreal = servers.get("unreal", {})
            if unreal.get("type") == "http":
                try:
                    import bob_bridge_launcher
                    if bob_bridge_launcher.is_running():
                        return os.path.normpath(http_config).replace("\\", "/")
                except Exception:
                    pass
                # HTTP config but bridge is down — fall through to fallback
            else:
                # Stdio config — use it directly
                return os.path.normpath(http_config).replace("\\", "/")
        except (json.JSONDecodeError, IOError):
            pass

    # Fallback: stdio config
    fallback_config = os.path.join(saved_dir, "_bobbot_mcp_fallback.json")
    if os.path.isfile(fallback_config):
        return os.path.normpath(fallback_config).replace("\\", "/")

    root_config = os.path.join(_PROJECT_ROOT, ".mcp.json")
    if os.path.isfile(root_config):
        return os.path.normpath(root_config).replace("\\", "/")

    return None


def _ensure_system_prompt_file():
    """Write default system prompt to disk if no file exists. Return the path."""
    if not os.path.isfile(_SYSTEM_PROMPT_PATH):
        os.makedirs(os.path.dirname(_SYSTEM_PROMPT_PATH), exist_ok=True)
        with open(_SYSTEM_PROMPT_PATH, "w", encoding="utf-8") as f:
            f.write(_SYSTEM_PROMPT)
    return _SYSTEM_PROMPT_PATH


def get_default_prompt():
    """Return the built-in default system prompt string (for C++ Reset to Default)."""
    return _SYSTEM_PROMPT


def _build_command():
    """Build the claude CLI command with streaming and session isolation."""
    mcp_config = _find_mcp_config()
    prompt_file = _ensure_system_prompt_file()
    permission_mode = os.environ.get("BOB_PERMISSION_MODE", "allow_always")

    cmd = [
        _claude_path, "-p",
        "--output-format", "stream-json",
        "--verbose",
        "--include-partial-messages",
        "--model", _model,
    ]

    if permission_mode in ("allow_always", "ask_me"):
        cmd.extend(["--dangerously-skip-permissions"])

    if mcp_config:
        cmd.extend(["--strict-mcp-config", "--mcp-config", mcp_config])

    cmd.extend(["--system-prompt-file", prompt_file.replace("\\", "/")])

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


# --------------------------------------------------------------------------- #
# Stream event handling
# --------------------------------------------------------------------------- #
def _handle_stream_event(event):
    """Parse a stream-json event and queue it for poll()."""
    global _session_id, _total_session_cost

    etype = event.get("type", "")

    if etype == "system":
        sid = event.get("session_id")
        with _lock:
            if sid:
                _session_id = sid

    elif etype == "assistant":
        message = event.get("message", {})
        content_blocks = message.get("content", [])
        for block in content_blocks:
            btype = block.get("type", "")

            if btype == "text":
                text = block.get("text", "")
                if text:
                    with _lock:
                        _stream_events.append({"type": "text", "text": text})

            elif btype == "tool_use":
                tool_name = block.get("name", "unknown")
                tool_input = block.get("input", {})
                input_str = json.dumps(tool_input, indent=2) if isinstance(tool_input, dict) else str(tool_input)
                with _lock:
                    _stream_events.append({
                        "type": "tool_use",
                        "name": tool_name,
                        "input": input_str,
                    })

            elif btype == "tool_result":
                output = block.get("content", "")
                if isinstance(output, list):
                    output = "\n".join(
                        b.get("text", "") for b in output if b.get("type") == "text"
                    )
                with _lock:
                    _stream_events.append({
                        "type": "tool_result",
                        "output": str(output),
                    })

    elif etype == "result":
        sid = event.get("session_id")
        cost = event.get("total_cost_usd", 0)
        with _lock:
            if sid:
                _session_id = sid
            _total_session_cost += cost
            _stream_events.append({
                "type": "complete",
                "cost": cost,
                "duration_ms": event.get("duration_ms", 0),
                "num_turns": event.get("num_turns", 1),
                "is_error": event.get("is_error", False),
                "result_text": event.get("result", ""),
            })


def _stream_thread(user_message):
    """Background thread: spawns claude CLI, reads stream events line by line."""
    global _is_thinking, _process

    try:
        with _lock:
            _is_thinking = True
            _stream_events.clear()

        cmd = _build_command()

        try:
            import unreal
            unreal.log("BobBot chat cmd: {}".format(
                " ".join('"{}"'.format(c) if " " in c else c for c in cmd)))
        except Exception:
            pass

        os.makedirs(_BOB_CWD, exist_ok=True)

        # Hide console window on Windows (CREATE_NO_WINDOW + STARTUPINFO)
        kwargs = {}
        if sys.platform == "win32":
            kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            si = subprocess.STARTUPINFO()
            si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            si.wShowWindow = 0  # SW_HIDE
            kwargs["startupinfo"] = si

        _process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            cwd=_BOB_CWD,
            **kwargs,
        )

        # Write user message to stdin, then close to signal "go"
        _process.stdin.write(user_message)
        _process.stdin.close()

        # Read stdout line by line (each line is a JSON event)
        for raw_line in _process.stdout:
            line = raw_line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                _handle_stream_event(event)
            except json.JSONDecodeError:
                continue

        # Wait for process to finish
        _process.wait(timeout=10)

        if _process.returncode != 0:
            stderr = _process.stderr.read() if _process.stderr else ""
            with _lock:
                # Only set error if no complete event was received
                has_complete = any(e.get("type") == "complete" for e in _stream_events)
                if not has_complete:
                    _stream_events.append({
                        "type": "error",
                        "message": _classify_error(stderr, _process.returncode),
                    })

    except Exception:
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "Stream error: {}".format(traceback.format_exc()),
            })
    finally:
        with _lock:
            _is_thinking = False
            _process = None


# --------------------------------------------------------------------------- #
# Public API
# --------------------------------------------------------------------------- #
def send_message(text):
    """Send a user message. Spawns claude CLI in background thread."""
    if not _claude_path:
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "Claude CLI not detected. Install Claude Code first.",
            })
        return

    t = threading.Thread(target=_stream_thread, args=(text,), daemon=True)
    t.start()


def poll():
    """
    Poll for stream updates. Called from game thread every 100ms.
    Returns dict with:
      events: list of stream events since last poll
      thinking: bool
    """
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


def _kill_process_tree(proc):
    """Kill a process and all its children (Windows: taskkill /T)."""
    if sys.platform == "win32":
        try:
            subprocess.run(
                ["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                capture_output=True, timeout=10,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            return
        except Exception:
            pass
    try:
        proc.kill()
    except Exception:
        pass


def cleanup():
    """Kill subprocess and clean up. Called from C++ destructor."""
    global _process, _is_thinking, _session_id, _total_session_cost
    with _lock:
        proc = _process
        _process = None
        _is_thinking = False
        _session_id = None
        _total_session_cost = 0.0
        _stream_events.clear()
    # Kill outside lock — kill can block
    if proc:
        _kill_process_tree(proc)
        try:
            proc.wait(timeout=5)
        except Exception:
            pass


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
        "backend": "subprocess",
    }


# --------------------------------------------------------------------------- #
# SDK backend swap: checked at call time so toggling takes effect immediately
# --------------------------------------------------------------------------- #

# Save subprocess implementations before overriding
_sub = {
    "send_message": send_message, "poll": poll, "cleanup": cleanup,
    "clear_session": clear_session, "is_thinking": is_thinking,
    "get_status": get_status, "get_session_cost": get_session_cost,
    "get_session_id": get_session_id, "set_session_id": set_session_id,
    "set_model": set_model, "get_model": get_model,
    "interrupt": lambda: None,  # no-op for subprocess
    "set_permission_mode": lambda mode: None,  # no-op for subprocess
    "stop_task": lambda task_id: None,  # no-op for subprocess
    "set_permission_decision": lambda decision: None,  # no-op for subprocess
    "list_saved_sessions": lambda limit=50, offset=0: [],  # no-op for subprocess
    "get_saved_session_info": lambda session_id: None,  # no-op for subprocess
    "get_saved_session_messages": lambda session_id, limit=100, offset=0: [],  # no-op
    "rename_saved_session": lambda session_id, title: {"ok": False, "error": "Requires SDK"},
    "tag_saved_session": lambda session_id, tag: {"ok": False, "error": "Requires SDK"},
    "delete_saved_session": lambda session_id: {"ok": False, "error": "Requires SDK"},
    "fork_current_session": lambda title=None, up_to_message_id=None: {"ok": False, "error": "Fork requires Agent SDK"},
}

_sdk_mod = None  # None = untried, False = tried and failed, module = ready


def _backend():
    """Return the active backend module (SDK or subprocess fallback)."""
    global _sdk_mod
    if os.environ.get("BOB_USE_SDK") != "1":
        return None  # subprocess mode
    if _sdk_mod is None:
        try:
            import bob_chat_sdk
            _sdk_mod = bob_chat_sdk
        except ImportError:
            _sdk_mod = False
    return _sdk_mod or None


def _dispatch(name, *args):
    """Route a call to SDK backend if active, otherwise subprocess."""
    sdk = _backend()
    if sdk:
        return getattr(sdk, name)(*args)
    fn = _sub.get(name)
    if fn is None:
        raise AttributeError("No backend function: {}".format(name))
    return fn(*args)


def send_message(text):    return _dispatch("send_message", text)      # noqa: F811,E704
def poll():                return _dispatch("poll")                    # noqa: F811,E704
def cleanup():             return _dispatch("cleanup")                 # noqa: F811,E704
def clear_session():       return _dispatch("clear_session")           # noqa: F811,E704
def is_thinking():         return _dispatch("is_thinking")             # noqa: F811,E704
def get_status():          return _dispatch("get_status")              # noqa: F811,E704
def get_session_cost():    return _dispatch("get_session_cost")        # noqa: F811,E704
def get_session_id():      return _dispatch("get_session_id")          # noqa: F811,E704
def set_session_id(sid):   return _dispatch("set_session_id", sid)     # noqa: F811,E704
def set_model(name):       return _dispatch("set_model", name)         # noqa: F811,E704
def get_model():           return _dispatch("get_model")               # noqa: F811,E704
def interrupt():           return _dispatch("interrupt")               # noqa: E704
def set_permission_mode(mode): return _dispatch("set_permission_mode", mode)  # noqa: E704
def stop_task(task_id):    return _dispatch("stop_task", task_id)      # noqa: E704
def set_permission_decision(d): return _dispatch("set_permission_decision", d)  # noqa: E704
def list_saved_sessions(limit=50, offset=0): return _dispatch("list_saved_sessions", limit, offset)  # noqa: E704
def get_saved_session_info(sid): return _dispatch("get_saved_session_info", sid)  # noqa: E704
def get_saved_session_messages(sid, limit=100, offset=0): return _dispatch("get_saved_session_messages", sid, limit, offset)  # noqa: E704
def rename_saved_session(sid, title): return _dispatch("rename_saved_session", sid, title)  # noqa: E704
def tag_saved_session(sid, tag): return _dispatch("tag_saved_session", sid, tag)  # noqa: E704
def delete_saved_session(sid): return _dispatch("delete_saved_session", sid)  # noqa: E704
def fork_current_session(title=None, up_to_message_id=None): return _dispatch("fork_current_session", title, up_to_message_id)  # noqa: E704
