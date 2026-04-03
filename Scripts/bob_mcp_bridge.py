"""
BobBot MCP Bridge - standalone MCP server that AI clients launch.

Connects to the UE editor's TCP server (bob_mcp_server.py) on localhost.
Tools are auto-registered from two directories:
  1. Plugins/BobBot/Scripts/tools/  (built-in, ships with plugin)
  2. <ProjectRoot>/BobBot/tools/    (project-specific, user-created)

Usage in .mcp.json (via venv):
  "command": "<Saved/BobBot/.venv/Scripts/python.exe>", "args": ["<path>/Scripts/bob_mcp_bridge.py"]
"""

import socket
import json
import sys
import os
import time
import importlib
import pkgutil

from mcp.server.fastmcp import FastMCP

mcp = FastMCP("unreal-engine")

UE_HOST = os.environ.get("BOB_MCP_HOST", "127.0.0.1")
UE_PORT = int(os.environ.get("BOB_MCP_PORT", "13579"))

_socket = None
_MAX_RETRIES = 2
_RETRY_DELAY = 0.5
# Longer timeout in ask_before_edits mode — user needs time to review
_SOCKET_TIMEOUT = 120 if os.environ.get("BOB_PERMISSION_MODE") == "ask_before_edits" else 60


# --------------------------------------------------------------------------- #
# Connection management
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
# Tool auto-discovery
# --------------------------------------------------------------------------- #
def _scan_tools_dir(tools_dir, package_name):
    """Scan a directory for tool modules and register them."""
    if not os.path.isdir(tools_dir):
        return

    parent_dir = os.path.dirname(tools_dir)
    if parent_dir not in sys.path:
        sys.path.insert(0, parent_dir)

    # Ensure __init__.py exists so the directory is importable as a package
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
                print("BobBot: Failed to load tool '{}' from {}: {}".format(
                    modname, tools_dir, e), file=sys.stderr)
        if count > 0:
            print("BobBot: Loaded {} tool module(s) from {}".format(count, tools_dir),
                  file=sys.stderr)
    except Exception as e:
        print("BobBot: Failed to scan tools dir {}: {}".format(tools_dir, e),
              file=sys.stderr)


def _register_all_tools():
    """Discover and register tools from built-in and project directories."""

    # 1. Built-in tools from plugin (Scripts/tools/)
    builtin_dir = os.path.join(os.path.dirname(__file__), "tools")
    _scan_tools_dir(builtin_dir, "tools")

    # 2. Project-specific tools (<ProjectRoot>/BobBot/tools/)
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
    mcp.run()
