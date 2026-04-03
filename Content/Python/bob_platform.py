"""BobBot platform abstraction.

Single source of truth for all OS-specific paths, process management,
and subprocess flags. Import this instead of sprinkling sys.platform
checks throughout the codebase.
"""

import os
import sys
import subprocess


# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
IS_WINDOWS = sys.platform == "win32"
IS_MAC = sys.platform == "darwin"
IS_LINUX = sys.platform.startswith("linux")


# ---------------------------------------------------------------------------
# Subprocess flags
# ---------------------------------------------------------------------------
# Suppresses console windows on Windows; no-op on Unix.
NO_WINDOW_FLAGS = subprocess.CREATE_NO_WINDOW if IS_WINDOWS else 0


def subprocess_kwargs():
    """Return common kwargs for subprocess.run/Popen to suppress console windows."""
    return {"creationflags": NO_WINDOW_FLAGS} if IS_WINDOWS else {}


# ---------------------------------------------------------------------------
# Virtual environment paths
# ---------------------------------------------------------------------------
def venv_python(venv_dir):
    """Return the Python executable inside a venv."""
    if IS_WINDOWS:
        return os.path.join(venv_dir, "Scripts", "python.exe")
    return os.path.join(venv_dir, "bin", "python")


def venv_pip(venv_dir):
    """Return the pip executable inside a venv."""
    if IS_WINDOWS:
        return os.path.join(venv_dir, "Scripts", "pip.exe")
    return os.path.join(venv_dir, "bin", "pip")


def venv_site_packages(venv_dir):
    """Return the site-packages directory inside a venv."""
    if IS_WINDOWS:
        return os.path.join(venv_dir, "Lib", "site-packages")
    pyver = "python{}.{}".format(sys.version_info.major, sys.version_info.minor)
    return os.path.join(venv_dir, "lib", pyver, "site-packages")


# ---------------------------------------------------------------------------
# Claude Code CLI
# ---------------------------------------------------------------------------
CLAUDE_EXE_NAME = "claude.exe" if IS_WINDOWS else "claude"


def bundled_claude_path(sdk_module):
    """Return the path to the bundled Claude CLI inside the SDK package, or None."""
    sdk_dir = os.path.dirname(os.path.abspath(sdk_module.__file__))
    path = os.path.join(sdk_dir, "_bundled", CLAUDE_EXE_NAME)
    return path if os.path.isfile(path) else None


# ---------------------------------------------------------------------------
# pywin32 DLL registration (Windows-only, safe no-op on Unix)
# ---------------------------------------------------------------------------
def register_pywin32_dlls(site_packages_dir):
    """Register pywin32 DLLs so they can be loaded. No-op on non-Windows."""
    if not IS_WINDOWS:
        return
    dll_dir = os.path.join(site_packages_dir, "pywin32_system32")
    if os.path.isdir(dll_dir):
        os.add_dll_directory(dll_dir)
        if dll_dir not in sys.path:
            sys.path.insert(0, dll_dir)


# ---------------------------------------------------------------------------
# Process management
# ---------------------------------------------------------------------------
def kill_process_tree(proc):
    """Kill a process and all its children.

    Windows: taskkill /F /T /PID
    Unix:    os.killpg (SIGTERM to the process group)
    Fallback: proc.kill() (parent only)
    """
    if IS_WINDOWS:
        try:
            subprocess.run(
                ["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                capture_output=True, timeout=10,
                creationflags=NO_WINDOW_FLAGS,
            )
            return
        except Exception:
            pass
    else:
        try:
            import signal
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            return
        except Exception:
            pass
    # Final fallback — kills parent only
    try:
        proc.kill()
    except Exception:
        pass
