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
def process_pth_files(site_packages_dir):
    """Process .pth files in site-packages, the way standard site.py does.

    UE's embedded Python skips site.py, so packages that rely on .pth files
    for path setup (pywin32, setuptools, etc.) don't work out of the box.
    This reads each .pth file, adds the listed directories to sys.path,
    and executes any import lines. Then on Windows, registers any directories
    containing .dll files so native extensions can load their dependencies.
    """
    if not os.path.isdir(site_packages_dir):
        return

    # Pass 1: process .pth files (add paths, run import lines)
    for filename in sorted(os.listdir(site_packages_dir)):
        if not filename.endswith(".pth"):
            continue
        pth_path = os.path.join(site_packages_dir, filename)
        try:
            with open(pth_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    if line.startswith("import ") or line.startswith("import\t"):
                        try:
                            exec(line)
                        except Exception:
                            pass
                    else:
                        full = os.path.join(site_packages_dir, line)
                        if os.path.isdir(full) and full not in sys.path:
                            sys.path.insert(0, full)
        except Exception:
            pass

    # Pass 2 (Windows only): register directories that contain .dll files
    # so native Python extensions can find their dependencies.
    # This is generic — works for pywin32, any other package with native DLLs.
    if IS_WINDOWS:
        for entry in os.listdir(site_packages_dir):
            dirpath = os.path.join(site_packages_dir, entry)
            if not os.path.isdir(dirpath):
                continue
            if any(f.endswith(".dll") for f in os.listdir(dirpath)):
                try:
                    os.add_dll_directory(dirpath)
                except Exception:
                    pass
                if dirpath not in sys.path:
                    sys.path.insert(0, dirpath)


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
