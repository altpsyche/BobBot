"""
BobBot MCP Bridge (HTTP) - persistent MCP server started once on editor launch.

Unlike bob_mcp_bridge.py (stdio, spawned per claude invocation), this server
runs continuously and accepts connections from any Claude Code instance via
HTTP transport. This eliminates the 3-5s cold start per message.

Endpoint: http://127.0.0.1:{BOB_MCP_BRIDGE_PORT}/mcp
Transport: streamable-http (MCP spec)

Usage:
  uv run --with "mcp[cli]" bob_mcp_bridge_http.py
"""

import socket
import json
import sys
import os
import time
import importlib
import pkgutil

from mcp.server.fastmcp import FastMCP

BRIDGE_PORT = int(os.environ.get("BOB_MCP_BRIDGE_PORT", "13580"))

mcp = FastMCP("unreal-engine", host="127.0.0.1", port=BRIDGE_PORT)

UE_HOST = os.environ.get("BOB_MCP_HOST", "127.0.0.1")
UE_PORT = int(os.environ.get("BOB_MCP_PORT", "13579"))

_socket = None
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
    """Send a JSON message and receive the response, with auto-reconnect."""
    global _socket

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

        except (ConnectionError, OSError, json.JSONDecodeError, UnicodeDecodeError) as e:
            _disconnect()
            if attempt < _MAX_RETRIES:
                time.sleep(_RETRY_DELAY)
                continue
            return {
                "success": False,
                "error": "Connection failed after {} attempts: {}".format(
                    _MAX_RETRIES + 1, str(e)
                ),
            }


# --------------------------------------------------------------------------- #
# Tool auto-discovery (same as stdio bridge)
# --------------------------------------------------------------------------- #
def _scan_tools_dir(tools_dir, package_name):
    """Scan a directory for tool modules and register them."""
    if not os.path.isdir(tools_dir):
        return

    parent_dir = os.path.dirname(tools_dir)
    if parent_dir not in sys.path:
        sys.path.insert(0, parent_dir)

    init_path = os.path.join(tools_dir, "__init__.py")
    if not os.path.isfile(init_path):
        with open(init_path, "w") as f:
            f.write("")

    try:
        pkg = importlib.import_module(package_name)
        count = 0
        for importer, modname, ispkg in pkgutil.iter_modules(pkg.__path__):
            if modname.startswith("_"):
                continue
            try:
                mod = importlib.import_module("{}.{}".format(package_name, modname))
                if hasattr(mod, "register"):
                    mod.register(mcp, _send_and_receive)
                    count += 1
            except Exception as e:
                print("BobBot HTTP: Failed to load tool '{}' from {}: {}".format(
                    modname, tools_dir, e), file=sys.stderr)
        if count > 0:
            print("BobBot HTTP: Loaded {} tool module(s) from {}".format(count, tools_dir),
                  file=sys.stderr)
    except Exception as e:
        print("BobBot HTTP: Failed to scan tools dir {}: {}".format(tools_dir, e),
              file=sys.stderr)


def _register_all_tools():
    """Discover and register tools from built-in and project directories."""
    builtin_dir = os.path.join(os.path.dirname(__file__), "tools")
    _scan_tools_dir(builtin_dir, "tools")

    project_root = os.environ.get("BOB_PROJECT_ROOT", "")
    if project_root:
        project_tools = os.path.join(project_root, "BobBot", "tools")
        if os.path.isdir(project_tools):
            _scan_tools_dir(project_tools, "bobbot_project_tools")


# Initialize shared helpers before tool discovery
from tools import _common
_common.init(_send_and_receive)

_register_all_tools()

if __name__ == "__main__":
    print("BobBot HTTP bridge starting on http://{}:{}/mcp".format("127.0.0.1", BRIDGE_PORT),
          file=sys.stderr)
    mcp.run(transport="streamable-http")
