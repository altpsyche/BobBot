"""
BobBot MCP Bridge (HTTP) - persistent MCP server started once on editor launch.

Unlike bob_mcp_bridge.py (stdio, spawned per claude invocation), this server
runs continuously and accepts connections from any Claude Code instance via
HTTP transport. This eliminates the 3-5s cold start per message.

Endpoint: http://127.0.0.1:{BOB_MCP_BRIDGE_PORT}/mcp
Transport: streamable-http (MCP spec)

Usage (via venv):
  <Saved/BobBot/.venv/Scripts/python.exe> bob_mcp_bridge_http.py
"""

import socket
import json
import sys
import os
import time
import importlib
import importlib.util

# Ensure both Scripts/ and Scripts/tools/ are on sys.path.
# Scripts/ is needed for `from tools import _common` (package import).
# Scripts/tools/ is needed for `from _common import _exec` (bare import inside tool modules).
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_TOOLS_DIR = os.path.join(_SCRIPT_DIR, "tools")
for _d in (_SCRIPT_DIR, _TOOLS_DIR):
    if _d not in sys.path:
        sys.path.insert(0, _d)

from mcp.server.fastmcp import FastMCP

BRIDGE_PORT = int(os.environ.get("BOB_MCP_BRIDGE_PORT", "13580"))

mcp = FastMCP("unreal-engine", host="127.0.0.1", port=BRIDGE_PORT)

UE_HOST = os.environ.get("BOB_MCP_HOST", "127.0.0.1")
UE_PORT = int(os.environ.get("BOB_MCP_PORT", "13579"))

import threading as _threading
_socket = None
_socket_lock = _threading.Lock()
_MAX_RETRIES = 3
_RETRY_DELAY = 1.0
_SOCKET_TIMEOUT = 120 if os.environ.get("BOB_PERMISSION_MODE") == "ask_me" else 60


# --------------------------------------------------------------------------- #
# Connection management (same as stdio bridge)
# --------------------------------------------------------------------------- #
def _get_connection():
    """Get or create a TCP connection to the UE server."""
    global _socket
    if _socket is None:
        _socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        _socket.settimeout(_SOCKET_TIMEOUT)
        _socket.connect((UE_HOST, UE_PORT))
    return _socket


def _disconnect():
    """Close the TCP connection."""
    global _socket
    if _socket:
        try:
            _socket.close()
        except OSError:
            pass
        _socket = None


def _send_and_receive(msg: dict) -> dict:
    """Send a JSON message and receive the response, with auto-reconnect.
    Lock ensures concurrent tool calls don't corrupt socket state."""
    with _socket_lock:
        for attempt in range(_MAX_RETRIES + 1):
            try:
                sock = _get_connection()
                data = json.dumps(msg).encode("utf-8") + b"\n"
                sock.sendall(data)

                buf = b""
                while b"\n" not in buf:
                    chunk = sock.recv(65536)
                    if not chunk:
                        raise ConnectionError("UE server closed connection")
                    buf += chunk

                line = buf.split(b"\n", 1)[0]
                return json.loads(line.decode("utf-8"))

            except ConnectionRefusedError:
                _disconnect()
                if attempt < _MAX_RETRIES:
                    time.sleep(_RETRY_DELAY)
                    continue
                return {
                    "success": False,
                    "error": (
                        "Cannot connect to Unreal Engine on {}:{}. "
                        "Make sure the editor is open and the BobBot plugin is loaded. "
                        "Check the BobBot panel in UE (Window > BobBot) for server status."
                    ).format(UE_HOST, UE_PORT),
                }

            except (ConnectionError, OSError) as e:
                _disconnect()
                if attempt < _MAX_RETRIES:
                    time.sleep(_RETRY_DELAY)
                    continue
                return {
                    "success": False,
                    "error": "Connection failed after {} attempts: {}".format(
                        _MAX_RETRIES + 1, str(e)),
                }

            except (json.JSONDecodeError, UnicodeDecodeError) as e:
                _disconnect()
                return {
                    "success": False,
                    "error": "Invalid response from UE server: {}".format(str(e)),
                }


# --------------------------------------------------------------------------- #
# Tool auto-discovery
# --------------------------------------------------------------------------- #
def _load_tools_from(tools_dir, label=""):
    """Scan a directory for .py files with a register() function and load them."""
    if not os.path.isdir(tools_dir):
        return

    count = 0
    for fname in sorted(os.listdir(tools_dir)):
        if not fname.endswith(".py") or fname.startswith("_"):
            continue
        modname = fname[:-3]
        filepath = os.path.join(tools_dir, fname)
        try:
            spec = importlib.util.spec_from_file_location(
                "bobbot_tools.{}".format(modname), filepath)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            if hasattr(mod, "register"):
                mod.register(mcp, _send_and_receive)
                count += 1
        except Exception as e:
            print("BobBot HTTP: Failed to load tool '{}' from {}: {}".format(
                modname, tools_dir, e), file=sys.stderr)
    if count > 0:
        print("BobBot HTTP: Loaded {} {}tool(s) from {}".format(
            count, label, tools_dir), file=sys.stderr)


def _register_all_tools():
    """Discover and register tools from built-in and project directories."""
    _load_tools_from(os.path.join(_SCRIPT_DIR, "tools"))

    project_root = os.environ.get("BOB_PROJECT_ROOT", "")
    if project_root:
        project_tools = os.path.join(os.path.abspath(project_root), "BobBot", "tools")
        _load_tools_from(project_tools, label="project ")


# Initialize shared helpers before tool discovery
import _common
_common.init(_send_and_receive)

_register_all_tools()

import atexit
atexit.register(_disconnect)

if __name__ == "__main__":
    # Enable SO_REUSEADDR so bridge can rebind immediately after restart
    # (without waiting for TIME_WAIT to expire)
    import socket as _sock
    _orig_bind = _sock.socket.bind
    def _reuse_bind(self, address):
        try:
            self.setsockopt(_sock.SOL_SOCKET, _sock.SO_REUSEADDR, 1)
        except Exception:
            pass
        return _orig_bind(self, address)
    _sock.socket.bind = _reuse_bind

    print("BobBot HTTP bridge starting on http://{}:{}/mcp".format("127.0.0.1", BRIDGE_PORT),
          file=sys.stderr, flush=True)
    try:
        mcp.run(transport="streamable-http")
    except SystemExit as e:
        if e.code != 0:
            print("BobBot HTTP bridge exited with code {}".format(e.code), file=sys.stderr, flush=True)
    except Exception as e:
        print("BobBot HTTP bridge FATAL: {}".format(e), file=sys.stderr, flush=True)
        import traceback
        traceback.print_exc(file=sys.stderr)
        sys.exit(1)
