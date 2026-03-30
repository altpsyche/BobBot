"""
BobBot HTTP Bridge Launcher — manages the persistent MCP bridge process.

Called from C++ on editor startup/shutdown:
  import bob_bridge_launcher
  bob_bridge_launcher.start()   # Spawns HTTP bridge, waits for health
  bob_bridge_launcher.stop()    # Kills bridge process
  bob_bridge_launcher.is_running()  # Health check
"""

import os
import sys
import subprocess
import time
import threading

_process = None
_lock = threading.Lock()

_HEALTH_RETRIES = 15
_HEALTH_DELAY = 0.5


def _resolve_bridge_path():
    """Locate the HTTP bridge script."""
    try:
        import unreal
        uproject = str(unreal.Paths.get_project_file_path())
        if uproject:
            project_dir = os.path.dirname(os.path.normpath(uproject))
            path = os.path.join(project_dir, "Plugins", "BobBot", "Scripts", "bob_mcp_bridge_http.py")
            if os.path.isfile(path):
                return path
    except Exception:
        pass

    env_root = os.environ.get("BOB_PROJECT_ROOT", "")
    if env_root:
        path = os.path.join(env_root, "Plugins", "BobBot", "Scripts", "bob_mcp_bridge_http.py")
        if os.path.isfile(path):
            return path

    return None


def _get_port():
    """Return the bridge port from environment."""
    return int(os.environ.get("BOB_MCP_BRIDGE_PORT", "13580"))


def _health_check(port):
    """Check if the HTTP bridge is responding."""
    try:
        import urllib.request
        url = "http://127.0.0.1:{}/mcp".format(port)
        req = urllib.request.Request(url, method="GET")
        resp = urllib.request.urlopen(req, timeout=3)
        return resp.status < 500
    except Exception:
        return False


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


def start():
    """Launch the HTTP bridge and wait for it to become healthy."""
    global _process

    with _lock:
        if _process and _process.poll() is None:
            # Already running
            if _health_check(_get_port()):
                _log("HTTP bridge already running (pid {})".format(_process.pid))
                return True

    bridge_path = _resolve_bridge_path()
    if not bridge_path:
        _log("ERROR: Cannot find bob_mcp_bridge_http.py")
        return False

    port = _get_port()

    # Check if something else is already on this port
    if _health_check(port):
        _log("HTTP bridge already responding on port {}".format(port))
        return True

    cmd = [
        "uv", "run", "--with", "mcp[cli]",
        bridge_path,
    ]

    _log("Starting HTTP bridge: {}".format(" ".join(cmd)))

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
    except Exception as e:
        _log("ERROR: Failed to spawn bridge: {}".format(e))
        return False

    with _lock:
        _process = proc

    # Wait for bridge to become healthy
    for i in range(_HEALTH_RETRIES):
        if proc.poll() is not None:
            stderr = proc.stderr.read().decode("utf-8", errors="replace") if proc.stderr else ""
            _log("ERROR: Bridge exited early (code {}): {}".format(proc.returncode, stderr[:500]))
            with _lock:
                _process = None
            return False

        if _health_check(port):
            _log("HTTP bridge started on port {} (pid {})".format(port, proc.pid))
            return True

        time.sleep(_HEALTH_DELAY)

    _log("ERROR: Bridge not healthy after {}s".format(_HEALTH_RETRIES * _HEALTH_DELAY))
    _kill_process_tree(proc)
    with _lock:
        _process = None
    return False


def stop():
    """Stop the HTTP bridge process."""
    global _process

    with _lock:
        proc = _process
        _process = None

    if proc and proc.poll() is None:
        _kill_process_tree(proc)
        try:
            proc.wait(timeout=5)
        except Exception:
            pass
        _log("HTTP bridge stopped")


def is_running():
    """Check if the bridge is running and healthy."""
    with _lock:
        proc = _process
    if proc and proc.poll() is not None:
        return False
    return _health_check(_get_port())


def get_port():
    """Return the active bridge port."""
    return _get_port()


def _log(msg):
    """Log to UE output if available, otherwise stderr."""
    try:
        import unreal
        unreal.log("BobBot bridge: {}".format(msg))
    except Exception:
        print("BobBot bridge: {}".format(msg), file=sys.stderr)
