"""
BobBot MCP Server - runs inside Unreal Editor's Python environment.

Start: execute `import bob_mcp_server` in the UE Python console.
Stop:  run `bob_mcp_server.stop()`.

Listens for newline-delimited JSON commands over TCP.
Executes Python code on the game thread via Slate post-tick callback.
Supports multiple concurrent clients with isolated namespaces.
"""

import unreal
import socket
import json
import sys
import io
import os
import time
import traceback

# --------------------------------------------------------------------------- #
# Configuration from environment (set by C++ before import)
# --------------------------------------------------------------------------- #
_PORT = int(os.environ.get("BOB_MCP_PORT", "13579"))
_HOST = os.environ.get("BOB_MCP_HOST", "127.0.0.1")
_MAX_CLIENTS = int(os.environ.get("BOB_MCP_MAX_CLIENTS", "2"))
_RATE_LIMIT = int(os.environ.get("BOB_MCP_RATE_LIMIT", "30"))

# --------------------------------------------------------------------------- #
# Server state
# --------------------------------------------------------------------------- #
_server_socket = None
_tick_handle = None
_start_time = None

# Client tracking: socket -> client state dict
_clients = {}


def _new_client_state(addr):
    """Create a fresh state dict for a newly connected client."""
    return {
        "addr": addr,
        "buffer": b"",
        "namespace": {"__builtins__": __builtins__, "unreal": unreal},
        "connected_at": time.time(),
        "tokens": float(_RATE_LIMIT),
        "last_refill": time.time(),
    }


# --------------------------------------------------------------------------- #
# Code execution
# --------------------------------------------------------------------------- #
def _execute_code(code, namespace):
    """Execute Python code in the given namespace, capturing stdout/stderr."""
    old_stdout = sys.stdout
    old_stderr = sys.stderr
    captured_out = io.StringIO()
    captured_err = io.StringIO()

    try:
        sys.stdout = captured_out
        sys.stderr = captured_err
        exec(code, namespace)

        return {
            "success": True,
            "output": captured_out.getvalue(),
            "error": captured_err.getvalue() or None,
        }
    except Exception:
        return {
            "success": False,
            "output": captured_out.getvalue(),
            "error": traceback.format_exc(),
        }
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr


def _send_response(sock, msg):
    """Send a JSON response to a client socket. Returns False if send fails."""
    try:
        data = json.dumps(msg).encode("utf-8") + b"\n"
        sock.sendall(data)
        return True
    except OSError:
        return False


# --------------------------------------------------------------------------- #
# Rate limiting (token bucket)
# --------------------------------------------------------------------------- #
def _check_rate_limit(state):
    """Refill tokens and check if a message is allowed. Returns True if allowed."""
    now = time.time()
    elapsed = now - state["last_refill"]
    state["tokens"] = min(_RATE_LIMIT, state["tokens"] + elapsed * _RATE_LIMIT)
    state["last_refill"] = now

    if state["tokens"] >= 1.0:
        state["tokens"] -= 1.0
        return True
    return False


# --------------------------------------------------------------------------- #
# Tick callback (runs on game thread every frame)
# --------------------------------------------------------------------------- #
def _tick(delta_time):
    try:
        _tick_inner(delta_time)
    except Exception:
        unreal.log_warning("BobBot: tick error: {}".format(traceback.format_exc()))


def _tick_inner(delta_time):
    global _clients

    # --- Accept new connections ---
    try:
        client_sock, addr = _server_socket.accept()
        if len(_clients) >= _MAX_CLIENTS:
            # Reject with a helpful error
            try:
                _send_response(client_sock, {
                    "success": False,
                    "error": "BobBot: max clients ({}) reached. Disconnect another client first.".format(_MAX_CLIENTS),
                })
                client_sock.close()
            except OSError:
                pass
            unreal.log_warning("BobBot: Rejected connection from {} (max clients reached)".format(addr))
        else:
            client_sock.setblocking(False)
            _clients[client_sock] = _new_client_state(addr)
            unreal.log("BobBot: Client connected from {}".format(addr))
    except BlockingIOError:
        pass

    # --- Process each connected client ---
    disconnected = []

    for sock, state in list(_clients.items()):
        # Read available data
        try:
            data = sock.recv(65536)
            if not data:
                raise ConnectionResetError()
            state["buffer"] += data
        except BlockingIOError:
            pass
        except (ConnectionResetError, ConnectionAbortedError, OSError):
            disconnected.append(sock)
            continue

        # Process complete messages (newline-delimited JSON)
        while b"\n" in state["buffer"]:
            line, state["buffer"] = state["buffer"].split(b"\n", 1)
            try:
                msg = json.loads(line.decode("utf-8"))
            except (json.JSONDecodeError, UnicodeDecodeError):
                if not _send_response(sock, {"success": False, "error": "Invalid JSON"}):
                    disconnected.append(sock)
                    break
                continue

            # Rate limit check
            if not _check_rate_limit(state):
                if not _send_response(sock, {
                    "success": False,
                    "error": "Rate limited. Max {} messages/second.".format(_RATE_LIMIT),
                }):
                    disconnected.append(sock)
                    break
                continue

            # Dispatch
            msg_type = msg.get("type", "")

            if msg_type == "execute":
                result = _execute_code(msg.get("code", ""), state["namespace"])
                if not _send_response(sock, result):
                    disconnected.append(sock)
                    break
            elif msg_type == "ping":
                if not _send_response(sock, {"success": True, "output": "pong"}):
                    disconnected.append(sock)
                    break
            elif msg_type == "status":
                if not _send_response(sock, {"success": True, "output": json.dumps(get_status())}):
                    disconnected.append(sock)
                    break
            else:
                if not _send_response(sock, {"success": False, "error": "Unknown type: {}".format(msg_type)}):
                    disconnected.append(sock)
                    break

    # --- Clean up disconnected clients ---
    for sock in disconnected:
        addr = _clients.get(sock, {}).get("addr", "unknown")
        try:
            sock.close()
        except OSError:
            pass
        _clients.pop(sock, None)
        unreal.log("BobBot: Client disconnected: {}".format(addr))


# --------------------------------------------------------------------------- #
# Public API
# --------------------------------------------------------------------------- #
def start(port=None, host=None):
    """Start the BobBot TCP server."""
    global _server_socket, _tick_handle, _PORT, _HOST, _start_time

    if port is not None:
        _PORT = port
    if host is not None:
        _HOST = host

    if _tick_handle is not None:
        unreal.log("BobBot: Already running on {}:{}".format(_HOST, _PORT))
        return

    _server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    _server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    _server_socket.bind((_HOST, _PORT))
    _server_socket.listen(_MAX_CLIENTS + 1)
    _server_socket.setblocking(False)

    _tick_handle = unreal.register_slate_post_tick_callback(_tick)
    _start_time = time.time()
    unreal.log("BobBot: Listening on {}:{} (max {} clients, rate limit {}/s)".format(
        _HOST, _PORT, _MAX_CLIENTS, _RATE_LIMIT))


def stop():
    """Stop the BobBot TCP server and disconnect all clients."""
    global _tick_handle, _server_socket, _clients, _start_time

    if _tick_handle:
        unreal.unregister_slate_post_tick_callback(_tick_handle)
        _tick_handle = None

    for sock in list(_clients.keys()):
        try:
            sock.close()
        except OSError:
            pass
    _clients = {}

    if _server_socket:
        try:
            _server_socket.close()
        except OSError:
            pass
        _server_socket = None

    _start_time = None
    unreal.log("BobBot: Server stopped")


def is_running():
    """Return True if the server is currently running."""
    return _tick_handle is not None


def get_status():
    """Return server status as a dict."""
    clients_info = []
    for sock, state in _clients.items():
        clients_info.append({
            "addr": str(state["addr"]),
            "connected_seconds": int(time.time() - state["connected_at"]),
        })

    return {
        "running": is_running(),
        "host": _HOST,
        "port": _PORT,
        "uptime_seconds": int(time.time() - _start_time) if _start_time else 0,
        "client_count": len(_clients),
        "max_clients": _MAX_CLIENTS,
        "clients": clients_info,
    }


# Auto-start when imported
start()
