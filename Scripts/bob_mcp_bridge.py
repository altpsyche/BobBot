"""
BobBot MCP Bridge - standalone MCP server that AI clients launch.

Connects to the UE editor's TCP server (bob_mcp_server.py) on localhost.
Tools are auto-registered from the tools/ directory.

Usage in .mcp.json:
  "command": "uv", "args": ["run", "--with", "mcp[cli]", "<path>/Scripts/bob_mcp_bridge.py"]
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
# Longer timeout in ask_me mode — user needs time to review code
_SOCKET_TIMEOUT = 120 if os.environ.get("BOB_PERMISSION_MODE") == "ask_me" else 30


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

        except (ConnectionError, OSError, json.JSONDecodeError) as e:
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
def _register_all_tools():
    """Import all tool modules from tools/ and call their register() function."""
    tools_dir = os.path.join(os.path.dirname(__file__), "tools")
    if not os.path.isdir(tools_dir):
        return

    # Add Scripts dir to path so tools package is importable
    scripts_dir = os.path.dirname(__file__)
    if scripts_dir not in sys.path:
        sys.path.insert(0, scripts_dir)

    import tools
    for importer, modname, ispkg in pkgutil.iter_modules(tools.__path__):
        if modname.startswith("_"):
            continue
        try:
            mod = importlib.import_module("tools." + modname)
            if hasattr(mod, "register"):
                mod.register(mcp, _send_and_receive)
        except Exception as e:
            print("BobBot: Failed to load tool module '{}': {}".format(modname, e), file=sys.stderr)


_register_all_tools()

if __name__ == "__main__":
    mcp.run()
