"""
BobBot HTTP Bridge Launcher — manages the persistent MCP bridge process.

Uses a venv created from UE's own Python so the bridge, SDK, and editor
all share one interpreter version (no system Python conflicts).

Called from C++ on editor startup/shutdown:
  import bob_bridge_launcher
  bob_bridge_launcher.start()   # Ensures venv, spawns bridge, waits for health
  bob_bridge_launcher.stop()    # Kills bridge process
  bob_bridge_launcher.is_running()  # Health check
"""

import os
import sys
import subprocess
import time
import threading
import bob_platform

_process = None
_log_file = None
_lock = threading.Lock()

_HEALTH_RETRIES = 30
_HEALTH_DELAY = 1.0

# --------------------------------------------------------------------------- #
# UE Python venv management
# --------------------------------------------------------------------------- #
_venv_ready = False


def _resolve_project_root():
    """Get the project root directory."""
    try:
        import unreal
        uproject = str(unreal.Paths.get_project_file_path())
        if uproject:
            return os.path.dirname(os.path.normpath(uproject))
    except Exception:
        pass
    return os.environ.get("BOB_PROJECT_ROOT", os.getcwd())


def _get_venv_dir():
    """Return the venv path: <Project>/Saved/BobBot/.venv/"""
    return os.path.join(_resolve_project_root(), "Saved", "BobBot", ".venv")


def _get_venv_python():
    """Return the venv's Python executable path."""
    return bob_platform.venv_python(_get_venv_dir())


def _get_venv_pip():
    """Return the venv's pip executable path."""
    return bob_platform.venv_pip(_get_venv_dir())


def _find_ue_python():
    """Find UE's bundled Python interpreter for the *current* platform.

    All three platform binaries (Win64/Linux/Mac) are present in a synced
    engine tree, so we must pick the one matching this OS — not just the first
    file that exists. Selecting the Windows python.exe on Linux and trying to
    exec it fails with [Errno 13] Permission denied (it lacks the +x bit).
    """
    try:
        import unreal
        engine_dir = str(unreal.Paths.engine_dir())
        py3 = os.path.join(engine_dir, "Binaries", "ThirdParty", "Python3")
        if bob_platform.IS_WINDOWS:
            candidates = [os.path.join(py3, "Win64", "python.exe")]
        elif bob_platform.IS_MAC:
            candidates = [os.path.join(py3, "Mac", "bin", "python3")]
        else:  # Linux and other Unix
            candidates = [os.path.join(py3, "Linux", "bin", "python3")]
        for c in candidates:
            if os.path.isfile(c):
                return c
    except Exception:
        pass

    return None


def _ensure_venv():
    """Create the venv and install dependencies if needed. Returns True on success."""
    global _venv_ready
    if _venv_ready:
        return True

    venv_dir = _get_venv_dir()
    venv_python = _get_venv_python()
    venv_pip = _get_venv_pip()

    # If venv already exists and has pip, just verify deps
    if os.path.isfile(venv_python) and os.path.isfile(venv_pip):
        _venv_ready = True
        return True

    # Create venv from UE's Python
    ue_python = _find_ue_python()
    if not ue_python:
        _log("ERROR: Cannot find UE's Python interpreter")
        return False

    _log("Creating venv from UE Python ({})...".format(ue_python))
    try:
        subprocess.run(
            [ue_python, "-m", "venv", venv_dir],
            capture_output=True, text=True, timeout=30,
            **bob_platform.subprocess_kwargs(),
        )
    except (OSError, subprocess.SubprocessError) as e:
        _log("ERROR: Failed to create venv: {}".format(e))
        return False

    if not os.path.isfile(venv_python):
        _log("ERROR: venv created but python.exe not found at {}".format(venv_python))
        return False

    # Install dependencies
    _log("Installing bridge dependencies (mcp[cli])...")
    r = _run_pip_install("mcp[cli]")
    if not r["ok"]:
        _log("ERROR: pip install mcp[cli] failed: {}".format(r["message"]))
        return False

    _log("Installing SDK (claude-agent-sdk)...")
    r = _run_pip_install("claude-agent-sdk")
    if not r["ok"]:
        _log("WARNING: pip install claude-agent-sdk failed: {}".format(r["message"]))
        # Non-fatal — bridge works without SDK, only chat needs it

    # pywin32 DLL loading is handled at runtime by bob_chat_sdk._setup_venv_imports()
    # via os.add_dll_directory(). No post-install script needed — and running it while
    # UE has the DLLs loaded causes "file in use" errors.

    _venv_ready = True
    _log("Venv ready at {}".format(venv_dir))
    return True


def get_venv_site_packages():
    """Return the venv's site-packages directory for UE Python to import from."""
    return bob_platform.venv_site_packages(_get_venv_dir())


# --------------------------------------------------------------------------- #
# Bridge path and health check
# --------------------------------------------------------------------------- #
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


# Env vars the bridge subprocess needs, read from the LIVE process env (C++
# sets these via setenv after Python started, so os.environ may be stale/empty).
_BRIDGE_ENV_KEYS = (
    "BOB_MCP_BRIDGE_PORT", "BOB_MCP_PORT", "BOB_MCP_HOST", "BOB_BRIDGE_TOKEN",
    "BOB_PERMISSION_MODE", "BOB_MCP_MAX_CLIENTS", "BOB_MCP_RATE_LIMIT",
    "BOB_PROJECT_ROOT",
)


def _build_bridge_env():
    """Environment for the bridge subprocess: inherit ours, then overlay the
    live C-env values for the BOB_* keys so the child never gets a stale/empty
    token or port (the cause of the open-/mcp vs token-gated-config mismatch)."""
    env = dict(os.environ)
    for k in _BRIDGE_ENV_KEYS:
        v = bob_platform.live_env(k, "")
        if v:
            env[k] = v
    return env


def _get_port():
    """Return the bridge port from the live environment."""
    raw = bob_platform.live_env("BOB_MCP_BRIDGE_PORT", "").strip()
    try:
        return int(raw)
    except (TypeError, ValueError):
        return 13580


def _health_check(port):
    """Check if the HTTP bridge is accepting connections."""
    try:
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect(("127.0.0.1", port))
        s.close()
        return True
    except Exception:
        return False


def _kill_process_tree(proc):
    """Kill a process and all its children."""
    bob_platform.kill_process_tree(proc)


def _kill_port(port):
    """Kill whatever process is listening on a TCP port (cross-platform)."""
    _log("_kill_port({}): {}".format(port, bob_platform.kill_port(port)))


# --------------------------------------------------------------------------- #
# Start / Stop / Status
# --------------------------------------------------------------------------- #
def start():
    """Ensure venv, launch HTTP bridge, and wait for it to become healthy."""
    global _process

    with _lock:
        if _process and _process.poll() is None:
            if _health_check(_get_port()):
                _log("HTTP bridge already running (pid {})".format(_process.pid))
                return True

    # Ensure venv + deps are installed
    if not _ensure_venv():
        return False

    bridge_path = _resolve_bridge_path()
    if not bridge_path:
        _log("ERROR: Cannot find bob_mcp_bridge_http.py")
        return False

    port = _get_port()

    # Check if something else is already on this port.  If it's a stale
    # bridge from a previous editor session (before a code change or token
    # rotation), we must kill it — otherwise the new token-gated path
    # won't match and the SDK will get 404 on every tool call.  We detect
    # staleness by checking the _process handle: if _process is None (we
    # didn't spawn it) but the port is occupied, something else is there.
    if _health_check(port):
        with _lock:
            we_own_it = _process is not None and _process.poll() is None
        if we_own_it:
            _log("HTTP bridge already running (pid {})".format(_process.pid))
            return True
        # Stale bridge from a prior editor session — kill it so we can
        # rebind with the current token path.
        _log("Stale bridge detected on port {}. Killing it.".format(port))
        try:
            _kill_port(port)
        except Exception as e:
            _log("WARNING: could not kill stale bridge: {}".format(e))
        import time as _t
        _t.sleep(1.0)  # Give the OS a moment to release the port

    # Run bridge using venv Python (bridge has SO_REUSEADDR for instant rebind)
    venv_python = _get_venv_python()
    cmd = [venv_python, bridge_path]

    _log("Starting HTTP bridge: {}".format(" ".join(cmd)))

    # Redirect stderr to a log file (not PIPE — PIPE deadlocks when buffer fills)
    log_path = os.path.join(_resolve_project_root(), "Saved", "BobBot", "_bridge.log")
    global _log_file
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    if _log_file:
        try: _log_file.close()
        except Exception: pass
    _log_file = open(log_path, "w", encoding="utf-8")
    log_file = _log_file

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=log_file,
            # Pass an env with the live C-env BOB_* values overlaid, so the
            # bridge gets the real token/port even when Python's os.environ is
            # a stale startup snapshot (see _build_bridge_env).
            env=_build_bridge_env(),
            # Put the bridge in its own session/process group. On Unix this is
            # essential: without it the child shares the editor's process group,
            # so kill_process_tree()'s os.killpg() would signal (and terminate)
            # the Unreal editor itself. Ignored on Windows.
            start_new_session=True,
            **bob_platform.subprocess_kwargs(),
        )
    except OSError as e:
        _log("ERROR: Failed to spawn bridge: {}".format(e))
        log_file.close()
        _log_file = None
        return False

    with _lock:
        _process = proc

    def _read_bridge_log():
        """Read the bridge log file for diagnostics."""
        try:
            with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                return f.read()[-2000:]
        except Exception:
            return "(no log)"

    # Wait for bridge to become healthy
    for i in range(_HEALTH_RETRIES):
        if proc.poll() is not None:
            _log("ERROR: Bridge exited early (code {}): {}".format(
                proc.returncode, _read_bridge_log()))
            with _lock:
                _process = None
            return False

        if _health_check(port):
            _log("HTTP bridge started on port {} (pid {})".format(port, proc.pid))
            return True

        time.sleep(_HEALTH_DELAY)

    _log("ERROR: Bridge not healthy after {}s. Log: {}".format(
        _HEALTH_RETRIES * _HEALTH_DELAY, _read_bridge_log()))
    _kill_process_tree(proc)
    with _lock:
        _process = None
    return False


def stop():
    """Stop the HTTP bridge process."""
    global _process, _log_file

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

    if _log_file:
        try: _log_file.close()
        except Exception: pass
        _log_file = None


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


# --------------------------------------------------------------------------- #
# Granular setup steps (used by Welcome tab for live progress)
# --------------------------------------------------------------------------- #
def setup_create_venv():
    """Create the venv from UE Python (no installs). Returns dict with ok, message."""
    venv_dir = _get_venv_dir()
    venv_python = _get_venv_python()

    if os.path.isfile(venv_python):
        return {"ok": True, "message": "Venv already exists"}

    ue_python = _find_ue_python()
    if not ue_python:
        return {"ok": False, "message": "Cannot find UE Python interpreter"}

    try:
        result = subprocess.run(
            [ue_python, "-m", "venv", venv_dir],
            capture_output=True, text=True, timeout=30,
            **bob_platform.subprocess_kwargs(),
        )
        if not os.path.isfile(venv_python):
            return {"ok": False, "message": "Venv created but python.exe not found"}
        return {"ok": True, "message": "Python environment ready"}
    except (OSError, subprocess.SubprocessError) as e:
        return {"ok": False, "message": str(e)}


def _run_pip_install(package_spec, timeout=120):
    """Run pip install for a package. Returns {ok: bool, message: str}."""
    venv_pip = _get_venv_pip()
    if not os.path.isfile(venv_pip):
        return {"ok": False, "message": "Venv pip not found — create venv first"}
    try:
        result = subprocess.run(
            [venv_pip, "install", package_spec],
            capture_output=True, text=True, timeout=timeout,
            **bob_platform.subprocess_kwargs(),
        )
        if result.returncode != 0:
            return {"ok": False, "message": result.stderr[:300]}
        return {"ok": True, "message": "{} installed".format(package_spec)}
    except (OSError, subprocess.SubprocessError) as e:
        return {"ok": False, "message": str(e)}


def setup_install_mcp():
    """Install mcp[cli] into the venv. Returns dict with ok, message."""
    return _run_pip_install("mcp[cli]")


def setup_install_sdk():
    """Install claude-agent-sdk into the venv. Returns dict with ok, message."""
    return _run_pip_install("claude-agent-sdk")


def check_health():
    """Return dict with bridge health info for C++ polling."""
    port = _get_port()
    ok = _health_check(port)
    return {"ok": ok, "port": port, "status": "Connected" if ok else "Stopped"}


def _log(msg):
    """Log to UE output if available, otherwise stderr."""
    try:
        import unreal
        unreal.log("BobBot bridge: {}".format(msg))
    except Exception:
        print("BobBot bridge: {}".format(msg), file=sys.stderr)
