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
import re
import threading
import time

_send_fn = None
_send_lock = threading.Lock()

# Per-call timeout override (seconds). Set via `tool_timeout` decorator or
# `with_timeout` context manager. Bridge default applies when None.
_TIMEOUT_CEILING_S = 300
_call_state = threading.local()


def _current_timeout():
    return getattr(_call_state, "timeout", None)


def with_timeout(seconds):
    """Context manager: override per-call socket timeout for nested _exec calls.
    Bridge clamps to [1, _TIMEOUT_CEILING_S]."""
    class _Ctx:
        def __enter__(self_inner):
            self_inner._prev = getattr(_call_state, "timeout", None)
            _call_state.timeout = max(1, min(int(seconds), _TIMEOUT_CEILING_S))
            return self_inner
        def __exit__(self_inner, exc_type, exc, tb):
            _call_state.timeout = self_inner._prev
            return False
    return _Ctx()


def tool_timeout(seconds):
    """Decorator: apply with_timeout(seconds) around the wrapped tool body."""
    import functools
    def deco(fn):
        @functools.wraps(fn)
        def wrapped(*args, **kwargs):
            with with_timeout(seconds):
                return fn(*args, **kwargs)
        return wrapped
    return deco


# --------------------------------------------------------------------------- #
# Activity log (Sprint 2 — Improvement #2)
#
# Append-only JSONL of every tool call: timestamp, tool, code preview, output
# preview, duration. Stored in <ProjectRoot>/Saved/BobBot/activity.jsonl.
# Rotated at 50MB.
#
# Redaction (Risk 2.4): the `code` field is run through regex substitution
# matching common secret-shaped patterns (api_key=, password=, bearer tokens,
# Bridge tokens) before write. Match is conservative — false negatives possible
# for novel formats; do not rely on this as a security boundary, only as a
# noise filter against accidental capture.
# --------------------------------------------------------------------------- #

_ACTIVITY_LOG_LOCK = threading.Lock()
_ACTIVITY_LOG_PATH = None
_ACTIVITY_MAX_BYTES = 50 * 1024 * 1024
_ACTIVITY_DISABLED = os.environ.get("BOB_ACTIVITY_LOG", "1") == "0"

_REDACT_PATTERNS = [
    # key/token/secret/password values in code text or env-style assignments
    re.compile(r"((?:api[_-]?key|access[_-]?token|secret|password|passwd|pwd|token|auth)\s*[:=]\s*)([\"']?)([A-Za-z0-9_\-+/=]{8,})", re.IGNORECASE),
    # standalone hex/base64 blobs ≥40 chars (likely tokens)
    re.compile(r"\b([A-Fa-f0-9]{40,})\b"),
]


def _redact(text):
    if not text:
        return text
    for pat in _REDACT_PATTERNS:
        if pat.groups >= 3:
            text = pat.sub(lambda m: m.group(1) + m.group(2) + "<REDACTED>", text)
        else:
            text = pat.sub("<REDACTED>", text)
    return text


def _activity_log_path():
    global _ACTIVITY_LOG_PATH
    if _ACTIVITY_LOG_PATH is not None:
        return _ACTIVITY_LOG_PATH
    root = os.environ.get("BOB_PROJECT_ROOT", "")
    if not root:
        _ACTIVITY_LOG_PATH = ""
        return ""
    log_dir = os.path.join(root, "Saved", "BobBot")
    try:
        os.makedirs(log_dir, exist_ok=True)
    except OSError:
        _ACTIVITY_LOG_PATH = ""
        return ""
    _ACTIVITY_LOG_PATH = os.path.join(log_dir, "activity.jsonl")
    return _ACTIVITY_LOG_PATH


def _maybe_rotate(path):
    try:
        if os.path.getsize(path) >= _ACTIVITY_MAX_BYTES:
            backup = path + ".1"
            try:
                os.replace(path, backup)
            except OSError:
                pass
    except OSError:
        pass


def _log_activity(tool_name, code, output, success, duration_s, *, summary=None, spill_path=None, category=None, truncated=False):
    """Append a single tool call to the activity log.

    `summary` (preferred): a short, redacted, human-readable digest of what the
    tool did. Always small. Stored verbatim — this is what a History tab would
    render.

    `output` (legacy fallback): used only when `summary` is None. Truncated to
    500 chars and redacted. Pre-envelope tools land here.
    """
    if _ACTIVITY_DISABLED:
        return
    path = _activity_log_path()
    if not path:
        return
    if summary is None:
        summary = _redact(output or "")[:500]
    else:
        summary = _redact(summary)[:1500]
    record = {
        "ts": time.time(),
        "tool": tool_name,
        "ok": bool(success),
        "dur_s": round(duration_s, 4),
        "code": _redact(code or "")[:500],
        "summary": summary,
    }
    if category:
        record["category"] = category
    if spill_path:
        record["spill_path"] = spill_path
    if truncated:
        record["truncated"] = True
    line = json.dumps(record, ensure_ascii=False) + "\n"
    with _ACTIVITY_LOG_LOCK:
        _maybe_rotate(path)
        try:
            with open(path, "a", encoding="utf-8") as f:
                f.write(line)
        except OSError:
            pass


# --------------------------------------------------------------------------- #
# Output envelope + spill (Refactor v1.7)
#
# Every tool returns a structured envelope:
#     {ok, summary, data, spill_path, error, meta}
# - summary: small, always present, human-readable.
# - data:    structured payload (dict/list/None).
# - spill_path: populated when serialized envelope exceeds inline budget.
# - meta.truncated: true if data was spilled.
#
# Inline budgets per output_kind:
#   small : 4 KB
#   large : 16 KB
#   huge  : 1 KB (always spill data)
# --------------------------------------------------------------------------- #

_OUTPUT_BUDGETS = {
    "small": 4 * 1024,
    "large": 16 * 1024,
    "huge": 1 * 1024,
}
_DEFAULT_OUTPUT_KIND = "small"
_OVERFLOW_DIR_NAME = "output_overflow"
_OVERFLOW_LOCK = threading.Lock()
_OVERFLOW_MAX_DIR_BYTES = 200 * 1024 * 1024  # cap directory at 200MB


def envelope(summary="", data=None, *, ok=True, error=None, tool_name=None):
    """Build a tool-result envelope. Tool functions wrapped by `bob_tool` should
    return one of these directly. Plain-string returns are auto-wrapped by the
    decorator.
    """
    return {
        "ok": bool(ok),
        "summary": str(summary) if summary is not None else "",
        "data": data,
        "spill_path": None,
        "error": error,
        "meta": {"tool": tool_name},
    }


def _overflow_dir():
    root = os.environ.get("BOB_PROJECT_ROOT", "")
    if not root:
        return ""
    d = os.path.join(root, "Saved", "BobBot", _OVERFLOW_DIR_NAME)
    try:
        os.makedirs(d, exist_ok=True)
    except OSError:
        return ""
    return d


def _prune_overflow_dir(d):
    """Best-effort: keep total bytes under _OVERFLOW_MAX_DIR_BYTES, oldest first."""
    try:
        files = []
        total = 0
        for name in os.listdir(d):
            fp = os.path.join(d, name)
            try:
                st = os.stat(fp)
                files.append((st.st_mtime, st.st_size, fp))
                total += st.st_size
            except OSError:
                continue
        if total <= _OVERFLOW_MAX_DIR_BYTES:
            return
        files.sort()
        for _, sz, fp in files:
            try:
                os.remove(fp)
                total -= sz
            except OSError:
                pass
            if total <= _OVERFLOW_MAX_DIR_BYTES:
                return
    except OSError:
        pass


def _spill_envelope(env, tool_name):
    """Write the full envelope to disk; return the spill path. Caller is
    responsible for replacing env['data'] with None after spilling."""
    d = _overflow_dir()
    if not d:
        return None
    fname = "{}_{}.json".format(int(time.time() * 1000), tool_name or "tool")
    fname = re.sub(r"[^A-Za-z0-9_.\-]", "_", fname)
    fp = os.path.join(d, fname)
    try:
        with _OVERFLOW_LOCK:
            with open(fp, "w", encoding="utf-8") as f:
                json.dump(env, f, ensure_ascii=False)
            _prune_overflow_dir(d)
    except (OSError, TypeError, ValueError):
        return None
    return fp.replace("\\", "/")


def _measure_envelope(env):
    try:
        return len(json.dumps(env, ensure_ascii=False).encode("utf-8"))
    except (TypeError, ValueError):
        return -1


def serialize_envelope(env, output_kind="small", *, tool_name=None):
    """Measure envelope; if it exceeds the inline budget for output_kind,
    write the full envelope to a spill file and replace env['data'] with
    None plus meta.truncated=true. Returns the (possibly modified) envelope."""
    budget = _OUTPUT_BUDGETS.get(output_kind, _OUTPUT_BUDGETS[_DEFAULT_OUTPUT_KIND])
    size = _measure_envelope(env)
    if size < 0:
        # Not JSON-serializable. Drop data, surface error.
        env["data"] = None
        env["error"] = (env.get("error") or "") + "; data not JSON-serializable"
        env["meta"]["truncated"] = True
        return env
    needs_spill = (output_kind == "huge" and env.get("data") is not None) or size > budget
    if not needs_spill:
        env["meta"]["bytes"] = size
        env["meta"]["truncated"] = False
        return env
    spill_path = _spill_envelope(env, tool_name or env.get("meta", {}).get("tool"))
    env["spill_path"] = spill_path
    env["data"] = None
    env["meta"]["truncated"] = True
    env["meta"]["bytes_full"] = size
    if spill_path is None:
        env["error"] = (env.get("error") or "") + "; spill failed (BOB_PROJECT_ROOT unset?)"
    return env

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
    """Execute Python code in the UE editor and return its captured stdout.

    Returns the raw string from UE — no size cap here. Size enforcement lives
    in the `bob_tool` decorator, which spills oversized envelopes to disk.
    Activity-log entry written by the decorator with the final summary; this
    layer only logs malformed responses and bridge errors.
    """
    tool_name = _get_tool_name()
    started = time.time()
    with _send_lock:
        fn = _send_fn
        if fn is None:
            return "Error: _common not initialized — call init(send_fn) first"
        payload = {"type": "execute", "code": code, "tool_name": tool_name}
        timeout = _current_timeout()
        if timeout is not None:
            payload["timeout"] = timeout
        result = fn(payload)
    duration = time.time() - started
    if not isinstance(result, dict) or "success" not in result:
        _log_activity(tool_name, code, "malformed response", False, duration)
        return "Error: Malformed response from UE"
    if result.get("success"):
        output = result.get("output", "")
        err = result.get("error")
        if err:
            output += "\nStderr: " + err
        if not output.strip():
            return "(executed successfully, no output)"
        return output
    err_msg = result.get("error", "Unknown error")
    _log_activity(tool_name, code, err_msg, False, duration)
    return "Error: " + err_msg


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
