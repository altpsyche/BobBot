"""UnrealInsights binary lookup + tool-side runtime config.

Trace-1: only `find_insights_binary()` and `read_runtime_config()` are used.
Trace-2 will extend this module with `run_insights_query(...)` for the
subprocess-based query path. The placeholder here reserves the API surface.

Cross-platform: candidate filenames vary per OS so a Mac/Linux dev box
doesn't hard-fail before getting a clean error envelope.
"""

import json
import os
import sys


_RUNTIME_CONFIG_FNAME = ".config.json"


def _project_root():
    return os.environ.get("BOB_PROJECT_ROOT", "")


def runtime_config_path():
    root = _project_root()
    if not root:
        return ""
    return os.path.join(root, "Saved", "BobBot", _RUNTIME_CONFIG_FNAME)


def read_runtime_config():
    """Returns dict (empty if missing/unreadable). Tool-side runtime config —
    distinct from `bob_sdk_config` (chat SDK). Schema documented in README."""
    cp = runtime_config_path()
    if not cp or not os.path.isfile(cp):
        return {}
    try:
        with open(cp, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def _engine_dir_from_unreal():
    """Best-effort engine dir resolution. Returns "" if unreal isn't importable
    (running outside the editor) — in that case the caller falls back to env."""
    try:
        import unreal
        return unreal.Paths.engine_dir().rstrip("/\\")
    except Exception:
        return os.environ.get("UE_ENGINE_DIR", "").rstrip("/\\")


def _platform_subdir():
    return {
        "win32": "Win64",
        "darwin": "Mac",
    }.get(sys.platform, "Linux")


def _platform_candidates():
    if sys.platform == "win32":
        return ("UnrealInsights-Cmd.exe", "UnrealInsights.exe")
    if sys.platform == "darwin":
        return (
            "UnrealInsights-Cmd",
            "UnrealInsights.app/Contents/MacOS/UnrealInsights",
            "UnrealInsights",
        )
    return ("UnrealInsights-Cmd", "UnrealInsights")


def insights_search_paths():
    """Ordered list of paths checked. Returned for diagnostics regardless of
    whether the binary was found."""
    paths = []
    cfg = read_runtime_config()
    override = (cfg.get("insights_binary") or "").strip()
    if override:
        paths.append(override)
    eng = _engine_dir_from_unreal()
    if eng:
        bin_dir = os.path.join(eng, "Binaries", _platform_subdir())
        # Some engine layouts put Binaries directly under engine root, others
        # under `Engine/Binaries`. Cover both.
        bin_dir2 = os.path.join(eng, "Engine", "Binaries", _platform_subdir())
        for d in (bin_dir, bin_dir2):
            for cand in _platform_candidates():
                paths.append(os.path.join(d, cand))
    # Dedup, preserve order
    seen = set()
    out = []
    for p in paths:
        n = os.path.normcase(os.path.normpath(p))
        if n in seen:
            continue
        seen.add(n)
        out.append(p)
    return out


def find_insights_binary():
    """Returns absolute path string of the first existing candidate, or None."""
    for p in insights_search_paths():
        if os.path.isfile(p):
            return os.path.abspath(p).replace("\\", "/")
    return None


def run_insights_query(*args, **kwargs):
    """Reserved for trace-2. Spawns Insights with -ExecCmds and parses CSV.
    Trace-1 uses `Trace.SaveStats` only — no subprocess."""
    raise NotImplementedError("run_insights_query lands in v1.7-trace-2")
