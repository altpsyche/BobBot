"""Trace navigator (v1.7-trace-1).

Six MCP tools for managing UE `.utrace` files: list/start/stop/delete/open/summarize.

Trace-1 ships:
  - File-level navigation (no Insights subprocess).
  - Live console-driven recording (Trace.Start File / Trace.Stop).
  - Header-only `summarize_trace` — file metadata + magic check + size-based
    duration estimate. Real frame-time histograms, p50/p95/p99, GPU-pass
    breakdown require the Insights CSV path landing in v1.7-trace-2.

Plan reference: see roadmap §5 and the trace-1 plan file. All tools share
`_validate_trace_args` (path containment + channel whitelist + filename
sanitization) before any UE call.
"""

import os
import subprocess
import time

from _common import _exec, _safe, envelope
from _registry import bob_tool

import _trace_lib as tlib
import _insights_runner as ir


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #

# .utrace files begin with the four-byte magic "TRCE". This is the only piece
# of the file format that's stable across 5.x and worth checking without
# pulling in Insights. Anything past this is a versioned binary stream.
_UTRACE_MAGIC = b"TRCE"


def _maybe_pc_prefix(host):
    """Prepend PC-trace caveat unless we have positive evidence of a device."""
    return tlib.PC_CAVEAT if tlib.host_is_local(host) else ""


def _stat_trace(abs_path):
    """File-system facts only. Caller has already validated the path."""
    st = os.stat(abs_path)
    return {
        "path": abs_path.replace("\\", "/"),
        "name": os.path.basename(abs_path),
        "size_bytes": int(st.st_size),
        "mtime": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(st.st_mtime)),
        "mtime_epoch": st.st_mtime,
    }


def _check_magic(abs_path):
    """Returns (ok, error_str). Cheap header read."""
    try:
        with open(abs_path, "rb") as f:
            head = f.read(4)
    except OSError as e:
        return False, "read failed: {}".format(e)
    if len(head) < 4:
        return False, "trace appears truncated or corrupt (no header)"
    if head != _UTRACE_MAGIC:
        return False, "not a valid .utrace file (magic mismatch)"
    return True, None


# --------------------------------------------------------------------------- #
# Registration
# --------------------------------------------------------------------------- #

def register(mcp, send_fn):

    # ----------------------------------------------------------------- #
    # list_traces
    # ----------------------------------------------------------------- #
    @mcp.tool()
    @bob_tool(category="Trace", output_kind="small", default_timeout=10)
    def list_traces(dir: str = "") -> str:
        """List `.utrace` files in the trace directory.

        Default directory: `<Project>/Saved/Traces/` (or `BOB_TRACE_DIR` env
        override). If `dir` is given, lists that directory instead — but only
        when it resolves under the configured trace dir; arbitrary paths are
        rejected.

        Returns `data.traces` sorted by mtime (newest first), each entry with
        `{path, name, size_bytes, mtime, mtime_epoch}`. `data.dir` and
        `data.total_size_bytes` for the directory roll-up.
        """
        traces_dir = tlib.resolve_traces_dir()
        if not traces_dir:
            return envelope(summary="BOB_PROJECT_ROOT unset", data=None,
                            ok=False, error="cannot resolve trace dir")

        target = traces_dir
        if dir:
            cand = os.path.abspath(dir)
            if not tlib.is_under(cand, traces_dir):
                return envelope(
                    summary="dir not under trace dir ({})".format(traces_dir),
                    data=None, ok=False, error="path containment failed")
            target = cand

        if not os.path.isdir(target):
            return envelope(summary="0 traces in {}.".format(target),
                            data={"dir": target.replace("\\", "/"),
                                  "total_size_bytes": 0, "traces": []})

        rows = []
        total = 0
        try:
            for name in os.listdir(target):
                if not name.lower().endswith(".utrace"):
                    continue
                fp = os.path.join(target, name)
                try:
                    st = os.stat(fp)
                except OSError:
                    continue
                rows.append({
                    "path": fp.replace("\\", "/"),
                    "name": name,
                    "size_bytes": int(st.st_size),
                    "mtime": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(st.st_mtime)),
                    "mtime_epoch": st.st_mtime,
                })
                total += st.st_size
        except OSError as e:
            return envelope(summary="listdir failed: {}".format(e),
                            data=None, ok=False, error=str(e))

        rows.sort(key=lambda r: -r["mtime_epoch"])
        data = {
            "dir": target.replace("\\", "/"),
            "total_size_bytes": total,
            "traces": rows,
        }
        return envelope(summary=tlib.summarize_trace_list(data), data=data)

    # ----------------------------------------------------------------- #
    # start_trace
    # ----------------------------------------------------------------- #
    @mcp.tool()
    @bob_tool(category="Trace", output_kind="small", default_timeout=30)
    def start_trace(channels: str = "cpu,gpu,frame,bookmark",
                    file_name: str = "") -> str:
        """Start a `Trace.Start` recording into the trace dir.

        `channels`: comma-separated subset of cpu, gpu, frame, bookmark, rhi,
        loadtime, memory, task, stats, counters, log, callstack. Unknown
        channel names are rejected before the editor call.

        `file_name`: optional. Defaults to `bobbot_<unix_ms>.utrace`. Must
        match `[A-Za-z0-9_.-]{1,80}`; path separators and traversal rejected.

        Refuses if a session is already active — call `stop_trace` first.
        Persists session state to `Saved/BobBot/.trace_session.json` so
        `stop_trace` can compute accurate duration across bridge restarts.

        Records ~5 MB/min of disk per active channel. Stop promptly.
        """
        existing = tlib.read_session()
        if existing:
            return envelope(
                summary="trace already recording: " + tlib.summarize_session(existing),
                data={"session": existing},
                ok=False, error="trace already recording")

        ok, ch_list, err = tlib.validate_channels(channels)
        if not ok:
            return envelope(summary=err, data=None, ok=False, error=err)

        ok2, err2 = tlib.validate_filename(file_name)
        if not ok2:
            return envelope(summary=err2, data=None, ok=False, error=err2)

        traces_dir = tlib.resolve_traces_dir()
        if not traces_dir:
            return envelope(summary="cannot resolve trace dir", data=None,
                            ok=False, error="BOB_PROJECT_ROOT unset")

        if not file_name:
            file_name = "bobbot_{}.utrace".format(int(time.time() * 1000))
        elif not file_name.lower().endswith(".utrace"):
            file_name += ".utrace"

        out_path = os.path.join(traces_dir, file_name).replace("\\", "/")
        ch_arg = ",".join(ch_list)

        # UE 5.4 console form: `Trace.Start File <abs_path>`. Channel set is
        # configured separately via `Trace.Enable <list>` because some 5.4
        # builds reject the combined `-channels=` flag. Run both.
        cmd = "Trace.Enable " + ch_arg + "\nTrace.Start File " + out_path
        raw = _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, {enable_safe})
unreal.SystemLibrary.execute_console_command(world, {start_safe})
print('OK')
""".format(
            enable_safe=_safe("Trace.Enable " + ch_arg),
            start_safe=_safe("Trace.Start File " + out_path),
        ))

        if not isinstance(raw, str) or "OK" not in raw:
            return envelope(
                summary="Trace.Start console command failed: {}".format(str(raw)[:300]),
                data=None, ok=False, error="console command failed")

        rec = tlib.write_session(path=out_path, channels=ch_list, file_name=file_name)
        if rec is None:
            # Recording started but we couldn't persist session — still report success
            # but warn: stop_trace will fall back to mtime-based duration.
            return envelope(
                summary="recording started; session file not writable, stop_trace duration will be approximate",
                data={"path": out_path, "channels": ch_list,
                      "started_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                      "disk_estimate_mb_per_min": 5},
                ok=True)

        return envelope(
            summary="started {}-channel trace -> {}. ~5MB/min disk. Stop with stop_trace().".format(
                len(ch_list), file_name),
            data={
                "path": out_path,
                "channels": ch_list,
                "started_at": rec["started_iso"],
                "session_id": rec["session_id"],
                "disk_estimate_mb_per_min": 5 * len(ch_list),
            })

    # ----------------------------------------------------------------- #
    # stop_trace
    # ----------------------------------------------------------------- #
    @mcp.tool()
    @bob_tool(category="Trace", output_kind="small", default_timeout=30)
    def stop_trace() -> str:
        """Stop the current `Trace.Start` recording.

        Reads `.trace_session.json` for accurate wall-clock duration. Falls
        back to mtime delta if session file is missing or corrupt;
        `meta.duration_source` distinguishes the two paths.
        """
        rec = tlib.read_session()
        if rec is None:
            # No session file. Still try Trace.Stop in case a recording was
            # started outside BobBot, but report the gap.
            _exec("import unreal\n"
                  "unreal.SystemLibrary.execute_console_command("
                  "unreal.EditorLevelLibrary.get_editor_world(), 'Trace.Stop')\n"
                  "print('OK')")
            return envelope(summary="no active trace session", data=None,
                            ok=False, error="no active trace session")

        raw = _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'Trace.Stop')
print('OK')
""")
        if not isinstance(raw, str) or "OK" not in raw:
            return envelope(
                summary="Trace.Stop failed: {}".format(str(raw)[:300]),
                data=None, ok=False, error="console command failed")

        path = rec.get("path", "")
        size = 0
        duration = 0.0
        source = "session"
        try:
            st = os.stat(path)
            size = int(st.st_size)
            duration = max(0.0, st.st_mtime - float(rec.get("started_at") or st.st_mtime))
        except OSError:
            duration = max(0.0, time.time() - float(rec.get("started_at") or time.time()))
            source = "wallclock_fallback"

        tlib.clear_session()

        env = envelope(
            summary="stopped trace; {:.1f}s, {} bytes -> {}".format(
                duration, size, os.path.basename(path) or "?"),
            data={
                "path": path,
                "size_bytes": size,
                "duration_s": round(duration, 3),
                "session_id": rec.get("session_id"),
            })
        env["meta"]["duration_source"] = source
        return env

    # ----------------------------------------------------------------- #
    # delete_trace
    # ----------------------------------------------------------------- #
    @mcp.tool()
    @bob_tool(category="Trace", output_kind="small", default_timeout=10)
    def delete_trace(paths: list) -> str:
        """Delete one or more `.utrace` files from the trace dir.

        `paths`: list of absolute paths (single delete = single-item list).
        Each path is validated for containment under the trace dir, `.utrace`
        extension, and existence before deletion. Files outside the trace
        dir are skipped with a reason — never deleted.

        Approval is required (the `delete_` name prefix routes this tool to
        the `modify` category in `bob_sdk_permissions.py`). With a list,
        a single approval covers the batch.
        """
        if not isinstance(paths, list) or not paths:
            return envelope(summary="paths must be a non-empty list", data=None,
                            ok=False, error="bad paths argument")

        deleted = []
        skipped = []
        total_freed = 0
        for p in paths:
            ok, abs_path, err = tlib.validate_trace_path(p, must_exist=True)
            if not ok:
                skipped.append({"path": str(p), "reason": err})
                continue
            try:
                size = os.path.getsize(abs_path)
                os.remove(abs_path)
            except OSError as e:
                skipped.append({"path": abs_path, "reason": "remove failed: {}".format(e)})
                continue
            deleted.append({"path": abs_path, "freed_bytes": size})
            total_freed += size

        # Active session pointing at a deleted file? Clear it.
        rec = tlib.read_session()
        if rec:
            sp = (rec.get("path") or "").replace("\\", "/")
            for d in deleted:
                if d["path"].replace("\\", "/") == sp:
                    tlib.clear_session()
                    break

        data = {
            "deleted": deleted,
            "skipped": skipped,
            "total_freed_bytes": total_freed,
        }
        ok_overall = bool(deleted) or not skipped
        return envelope(summary=tlib.summarize_delete(data), data=data, ok=ok_overall,
                        error=None if ok_overall else "all paths skipped")

    # ----------------------------------------------------------------- #
    # open_trace_in_insights
    # ----------------------------------------------------------------- #
    @mcp.tool()
    @bob_tool(category="Trace", output_kind="small", default_timeout=10)
    def open_trace_in_insights(path: str, foreground: bool = True) -> str:
        """Launch UnrealInsights with the given `.utrace` loaded.

        Hard-fails if the Insights binary isn't found; lists every path
        searched in the summary so the user can drop a path into
        `Saved/BobBot/.config.json::insights_binary`.

        Returns `data.pid` — the spawned process's PID. Process-started
        does NOT mean the trace finished loading; verify in the Insights
        window manually. PIE active will be flagged in the summary
        (Risk 5.10).
        """
        ok, abs_path, err = tlib.validate_trace_path(path, must_exist=True)
        if not ok:
            return envelope(summary=err, data=None, ok=False, error=err)

        binary = ir.find_insights_binary()
        if not binary:
            checked = ir.insights_search_paths() or ["<no candidates resolved>"]
            return envelope(
                summary="Insights binary not found. Checked: " + " | ".join(checked[:6]),
                data={"checked": checked},
                ok=False, error="Insights binary not found")

        # Optional PIE warning — non-fatal; just informational.
        pie_note = ""
        try:
            raw = _exec("""
import unreal
gi = unreal.EditorLevelLibrary.get_editor_world()
ws = unreal.SystemLibrary.is_in_editor() if hasattr(unreal.SystemLibrary, 'is_in_editor') else True
print('PIE_ACTIVE=' + ('1' if unreal.EditorLevelLibrary.is_simulating() else '0'))
""")
            if isinstance(raw, str) and "PIE_ACTIVE=1" in raw:
                pie_note = " PIE active; consider stop_pie() before opening Insights."
        except Exception:
            pass

        args = [binary, "-OpenTraceFile=" + abs_path]
        try:
            creationflags = 0
            if hasattr(subprocess, "DETACHED_PROCESS"):
                creationflags = subprocess.DETACHED_PROCESS
            proc = subprocess.Popen(
                args,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
                creationflags=creationflags,
            )
        except OSError as e:
            return envelope(
                summary="failed to launch Insights: {}".format(e),
                data={"binary": binary, "args": args},
                ok=False, error=str(e))

        return envelope(
            summary="launched Insights (pid {}) with {}. process started; verify trace loaded in window manually.{}".format(
                proc.pid, os.path.basename(abs_path), pie_note),
            data={"path": abs_path, "pid": proc.pid, "insights_binary": binary,
                  "foreground_requested": bool(foreground)})

    # ----------------------------------------------------------------- #
    # summarize_trace
    # ----------------------------------------------------------------- #
    @mcp.tool()
    @bob_tool(category="Trace", output_kind="large", default_timeout=180)
    def summarize_trace(path: str) -> str:
        """Header-only summary of a `.utrace` file (trace-1 scope).

        Returns file-level facts: size, mtime, magic-check, size-based
        duration estimate. Frame-time histograms, p50/p95/p99, GPU pass
        breakdown, bookmark/hitch listing require the Insights subprocess
        path landing in v1.7-trace-2 (`meta.summary_mode = "insights_csv"`).

        `meta.summary_mode = "header_only"` for trace-1 results.

        PC-trace caveat is prepended unless session metadata names a
        non-local host (`tlib.host_is_local`).
        """
        ok, abs_path, err = tlib.validate_trace_path(path, must_exist=True)
        if not ok:
            return envelope(summary=err, data=None, ok=False, error=err)

        # Magic check rejects truncated / non-utrace files cleanly.
        magic_ok, magic_err = _check_magic(abs_path)
        if not magic_ok:
            return envelope(summary=magic_err, data=None, ok=False, error=magic_err)

        st = _stat_trace(abs_path)

        # Best-effort recording-host detection from session file (only useful
        # for traces this BobBot instance recorded; device-pulled traces won't
        # match and get the PC caveat by default — safest).
        rec = tlib.read_session() or {}
        host = rec.get("host") if (rec.get("path", "").replace("\\", "/") == st["path"]) else ""

        # Size-based duration estimate. Heuristic only; honest about it.
        # Empirical: ~5 MB/min/channel for default 4-channel set ≈ 333 KB/sec.
        # We expose it as `duration_s_estimate` and never as a hard `duration_s`.
        est_bps = 333_000
        duration_est = max(0.0, st["size_bytes"] / float(est_bps))

        binary = ir.find_insights_binary()
        binary_note = ("Insights present at " + binary) if binary \
            else "Insights binary not found; trace-2 will fall back to subprocess query."

        data = {
            "path": st["path"],
            "name": st["name"],
            "size_bytes": st["size_bytes"],
            "mtime": st["mtime"],
            "duration_s_estimate": round(duration_est, 1),
            "frame_count": None,
            "frame_time_ms": None,
            "channels_active": None,
            "channels_missing_recommended": None,
            "hitches": None,
            "insights_binary": binary,
            "limitations": [
                "header_only mode: real frame stats, hitches, GPU passes require trace-2 (Insights subprocess).",
                "duration_s_estimate is size-based heuristic at ~333KB/s; not authoritative.",
            ],
        }

        prefix = _maybe_pc_prefix(host)
        size_mb = st["size_bytes"] / (1024 * 1024)
        summary = "{}{}, {:.1f}MB, ~{:.0f}s estimated. Frame stats need trace-2. {}".format(
            prefix, st["name"], size_mb, duration_est, binary_note)

        env = envelope(summary=summary, data=data)
        env["meta"]["summary_mode"] = "header_only"
        return env
