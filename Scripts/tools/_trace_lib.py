"""Validators, session state, and summary formatters for the Trace toolkit.

Bridge skips modules whose names start with `_`; this file ships private
helpers for `trace.py` only. Keeping them out of `trace.py` lets trace-2
grow `query_trace` + `diff_traces` without bloating the tool surface.
"""

import json
import os
import time
import uuid


# UE 5.4 Trace channel whitelist. Used by start_trace to reject typos before
# they hit the editor (where bad channels silently drop).
KNOWN_CHANNELS = (
    "cpu", "gpu", "frame", "bookmark", "rhi", "loadtime", "memory",
    "task", "stats", "counters", "log", "callstack", "rendercommands",
    "asset", "assetmetadata", "object", "savetime", "net",
)

_TRACE_DIR_ENV = "BOB_TRACE_DIR"
_PROJECT_ROOT_ENV = "BOB_PROJECT_ROOT"
_SESSION_FNAME = ".trace_session.json"


# --------------------------------------------------------------------------- #
# Path resolution
# --------------------------------------------------------------------------- #

def _project_root():
    return os.environ.get(_PROJECT_ROOT_ENV, "")


def resolve_traces_dir():
    """`<root>/Saved/Traces/` (or BOB_TRACE_DIR override). Created if missing.
    Returns absolute path with forward slashes, or "" if root unresolved."""
    override = os.environ.get(_TRACE_DIR_ENV, "").strip()
    if override:
        d = override
    else:
        root = _project_root()
        if not root:
            return ""
        d = os.path.join(root, "Saved", "Traces")
    try:
        os.makedirs(d, exist_ok=True)
    except OSError:
        return ""
    return os.path.abspath(d).replace("\\", "/")


def _bobbot_dir():
    root = _project_root()
    if not root:
        return ""
    d = os.path.join(root, "Saved", "BobBot")
    try:
        os.makedirs(d, exist_ok=True)
    except OSError:
        return ""
    return d


# --------------------------------------------------------------------------- #
# Validation — single source of truth, called at every tool entry
# --------------------------------------------------------------------------- #

_FILENAME_RE = None


def _filename_re():
    global _FILENAME_RE
    if _FILENAME_RE is None:
        import re
        _FILENAME_RE = re.compile(r"^[A-Za-z0-9_.\-]{1,80}$")
    return _FILENAME_RE


def validate_channels(channels):
    """`channels` is a comma-separated string. Returns (ok, parsed_list, err)."""
    if not channels:
        return False, [], "channels must be a non-empty comma-separated string"
    tokens = [c.strip().lower() for c in channels.split(",") if c.strip()]
    if not tokens:
        return False, [], "channels parsed empty"
    bad = [t for t in tokens if t not in KNOWN_CHANNELS]
    if bad:
        return False, tokens, "unknown channel(s): {} (known: {})".format(
            ",".join(bad), ",".join(KNOWN_CHANNELS))
    return True, tokens, None


def validate_filename(file_name):
    """Reject path separators, traversal, non-portable chars. Empty allowed."""
    if not file_name:
        return True, None
    if not _filename_re().match(file_name):
        return False, "file_name must match [A-Za-z0-9_.-]{1,80} (got %r)" % file_name
    return True, None


def is_under(child, parent):
    """True iff resolved `child` lives under resolved `parent`. No symlink escape."""
    try:
        c = os.path.realpath(child)
        p = os.path.realpath(parent)
    except OSError:
        return False
    if not p:
        return False
    try:
        common = os.path.commonpath([c, p])
    except ValueError:
        return False
    return os.path.normcase(common) == os.path.normcase(p)


def validate_trace_path(path, *, must_exist=True):
    """Validates a single .utrace path. Returns (ok, abs_path, err)."""
    if not path or not isinstance(path, str):
        return False, "", "path must be a non-empty string"
    traces = resolve_traces_dir()
    if not traces:
        return False, "", "BOB_PROJECT_ROOT unset; cannot resolve trace dir"
    abs_path = os.path.abspath(path)
    if not is_under(abs_path, traces):
        return False, abs_path, "path not under trace dir ({})".format(traces)
    if not abs_path.lower().endswith(".utrace"):
        return False, abs_path, "not a .utrace file"
    if must_exist and not os.path.isfile(abs_path):
        return False, abs_path, "file not found"
    return True, abs_path.replace("\\", "/"), None


# --------------------------------------------------------------------------- #
# Session state — survives bridge restart via .trace_session.json
# --------------------------------------------------------------------------- #

def _session_path():
    d = _bobbot_dir()
    if not d:
        return ""
    return os.path.join(d, _SESSION_FNAME)


def read_session():
    """Load session record. Returns None for missing, malformed, or
    shape-invalid files so callers (`stop_trace`, `summarize_session`) get
    a clean None rather than a half-broken dict."""
    sp = _session_path()
    if not sp or not os.path.isfile(sp):
        return None
    try:
        with open(sp, "r", encoding="utf-8") as f:
            rec = json.load(f)
        if not isinstance(rec, dict):
            return None
        if not isinstance(rec.get("path"), str) or not rec["path"]:
            return None
        if not isinstance(rec.get("started_at"), (int, float)):
            return None
        if "channels" in rec and not isinstance(rec["channels"], list):
            return None
        if "session_id" in rec and not isinstance(rec["session_id"], str):
            return None
        if "host" in rec and not isinstance(rec["host"], str):
            return None
        return rec
    except (OSError, ValueError):
        return None


def write_session(*, path, channels, file_name):
    sp = _session_path()
    if not sp:
        return None
    sid = uuid.uuid4().hex[:12]
    started = time.time()
    rec = {
        "session_id": sid,
        "path": path.replace("\\", "/"),
        "channels": list(channels),
        "file_name": file_name or "",
        "started_at": started,
        "started_iso": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(started)),
        "host": os.environ.get("COMPUTERNAME", "") or os.environ.get("HOSTNAME", ""),
    }
    try:
        with open(sp, "w", encoding="utf-8") as f:
            json.dump(rec, f, ensure_ascii=False, indent=2)
    except OSError:
        return None
    return rec


def clear_session():
    sp = _session_path()
    if sp and os.path.isfile(sp):
        try:
            os.remove(sp)
        except OSError:
            pass


# --------------------------------------------------------------------------- #
# PC-trace caveat — every summary that *could* be PC-recorded gets prefixed
# unless we have positive evidence the trace came from a device.
# --------------------------------------------------------------------------- #

PC_CAVEAT = "PC trace; not a substitute for device capture. "


def host_is_local(trace_host):
    """Host string from session file matches current machine?

    `BOB_TRACE_HOST` env overrides: when set non-empty, every trace is
    treated as device-pulled (caveat suppressed) — useful when the user
    routinely pulls captures from a target device.
    """
    override = os.environ.get("BOB_TRACE_HOST", "").strip()
    if override:
        return False
    me = os.environ.get("COMPUTERNAME", "") or os.environ.get("HOSTNAME", "")
    if not trace_host or not me:
        return True  # default to "assume PC" if unknown
    return trace_host.strip().lower() == me.strip().lower()


# --------------------------------------------------------------------------- #
# Summary formatters — mirror perf_audit.py style
# --------------------------------------------------------------------------- #

def _human_bytes(n):
    n = float(n or 0)
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024 or unit == "TB":
            return "{:.1f}{}".format(n, unit)
        n /= 1024
    return "{:.1f}TB".format(n)


def summarize_trace_list(d):
    n = len(d.get("traces") or [])
    total = d.get("total_size_bytes", 0)
    if n == 0:
        return "0 traces in {}.".format(d.get("dir", "?"))
    newest = (d.get("traces") or [{}])[0]
    return "{} traces, total {}. Newest: {} ({}, {}).".format(
        n, _human_bytes(total),
        newest.get("name", "?"),
        _human_bytes(newest.get("size_bytes", 0)),
        newest.get("mtime", ""),
    )


def summarize_trace_summary(d):
    ft = d.get("frame_time_ms") or {}
    h = d.get("hitches") or {}
    missing = d.get("channels_missing_recommended") or []
    parts = [
        "{:.1f}s trace".format(d.get("duration_s") or 0),
        "{} frames".format(d.get("frame_count") or 0),
        "avg {:.2f}ms".format(ft.get("avg") or 0),
        "max {:.1f}ms".format(ft.get("max") or 0),
        "{} hitches over {}ms".format(h.get("count", 0), h.get("threshold_ms", 33)),
    ]
    if missing:
        parts.append("missing channels: " + ",".join(missing))
    return ", ".join(parts) + "."


def summarize_session(rec):
    if not rec:
        return "no active trace session."
    age = time.time() - (rec.get("started_at") or 0)
    return "recording {} since {} ({:.0f}s, channels: {}).".format(
        os.path.basename(rec.get("path", "?")),
        rec.get("started_iso", "?"),
        age,
        ",".join(rec.get("channels") or []),
    )


def summarize_delete(d):
    deleted = d.get("deleted") or []
    skipped = d.get("skipped") or []
    return "deleted {}/{} traces, freed {}{}.".format(
        len(deleted),
        len(deleted) + len(skipped),
        _human_bytes(d.get("total_freed_bytes", 0)),
        "" if not skipped else " ({} skipped)".format(len(skipped)),
    )
