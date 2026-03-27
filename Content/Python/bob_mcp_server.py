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
_PERMISSION_MODE = os.environ.get("BOB_PERMISSION_MODE", "allow_always")

# --------------------------------------------------------------------------- #
# Server state
# --------------------------------------------------------------------------- #
_server_socket = None
_tick_handle = None
_start_time = None

# Client tracking: socket -> client state dict
_clients = {}

# --------------------------------------------------------------------------- #
# Approval queue for "ask_me" mode
# --------------------------------------------------------------------------- #
_pending_approval = None   # dict: {id, tool, code, sock, state, timestamp}
_approval_counter = 0
_APPROVAL_TIMEOUT = 120    # seconds

_SAVED_DIR = os.path.join(
    os.environ.get("BOB_PROJECT_ROOT", os.getcwd()), "Saved", "BobBot"
)
_APPROVAL_PENDING_FILE = os.path.join(_SAVED_DIR, "_approval_pending.json")
_APPROVAL_RESPONSE_FILE = os.path.join(_SAVED_DIR, "_approval_response.json")


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
# Approval queue ("ask_me" mode)
# --------------------------------------------------------------------------- #
def _enqueue_approval(sock, state, code):
    """Queue an execute request for user approval. Does NOT send a TCP response yet."""
    global _pending_approval, _approval_counter
    _approval_counter += 1
    _pending_approval = {
        "id": _approval_counter,
        "tool": "execute_unreal_python",
        "code": code,
        "sock": sock,
        "state": state,
        "timestamp": time.time(),
    }
    # Write the pending file for C++ to discover
    try:
        os.makedirs(_SAVED_DIR, exist_ok=True)
        with open(_APPROVAL_PENDING_FILE, "w", encoding="utf-8") as f:
            json.dump({
                "id": _approval_counter,
                "tool": "execute_unreal_python",
                "code": code,
                "timestamp": time.time(),
            }, f)
    except Exception as e:
        unreal.log_warning("BobBot: Failed to write approval file: {}".format(e))


def _check_approval_response():
    """Check if the user has responded to a pending approval. Called every tick.
    Note: _pending_approval is declared global in _tick_inner which calls us,
    but we also need it here since we assign to it."""
    global _pending_approval

    if _pending_approval is None:
        return

    # Check for timeout
    if time.time() - _pending_approval["timestamp"] > _APPROVAL_TIMEOUT:
        sock = _pending_approval["sock"]
        _send_response(sock, {
            "success": False,
            "error": "Approval timed out after {}s. User did not respond.".format(
                _APPROVAL_TIMEOUT),
        })
        _pending_approval = None
        _cleanup_approval_files()
        return

    # Check for response file from C++
    if not os.path.isfile(_APPROVAL_RESPONSE_FILE):
        return

    try:
        with open(_APPROVAL_RESPONSE_FILE, "r", encoding="utf-8") as f:
            response = json.load(f)
    except (json.JSONDecodeError, IOError):
        return

    # Verify the response matches the pending request
    if response.get("id") != _pending_approval["id"]:
        return

    sock = _pending_approval["sock"]
    state = _pending_approval["state"]
    code = _pending_approval["code"]
    decision = response.get("decision", "denied")

    # Clear state and files BEFORE executing — execution may trigger modal
    # dialogs that pump the message loop, re-entering this tick callback.
    # Without clearing first, the same code would execute again each tick.
    _pending_approval = None
    _cleanup_approval_files()

    if decision == "approved":
        result = _execute_code(code, state["namespace"])
        _send_response(sock, result)
    else:
        _send_response(sock, {
            "success": False,
            "error": "User denied tool execution.",
        })


def _cleanup_approval_files():
    """Remove both approval files."""
    for f in (_APPROVAL_PENDING_FILE, _APPROVAL_RESPONSE_FILE):
        try:
            os.remove(f)
        except OSError:
            pass


# --------------------------------------------------------------------------- #
# Tick callback (runs on game thread every frame)
# --------------------------------------------------------------------------- #
def _tick(delta_time):
    try:
        _tick_inner(delta_time)
    except Exception:
        unreal.log_warning("BobBot: tick error: {}".format(traceback.format_exc()))


def _tick_inner(delta_time):
    global _clients, _pending_approval, _PERMISSION_MODE

    # Check for approval responses first (before processing new messages)
    _check_approval_response()
    _PERMISSION_MODE = os.environ.get("BOB_PERMISSION_MODE", "allow_always")

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
                if _PERMISSION_MODE == "ask_me":
                    # Queue for user approval — don't respond yet
                    _enqueue_approval(sock, state, msg.get("code", ""))
                    break  # Stop processing this client's messages until approved
                else:
                    result = _execute_code(msg.get("code", ""), state["namespace"])
                    if not _send_response(sock, result):
                        disconnected.append(sock)
                        break
            elif msg_type == "ping":
                # ping is always auto-approved (harmless connectivity check)
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
        # If this client had a pending approval, cancel it
        if _pending_approval and _pending_approval.get("sock") is sock:
            _pending_approval = None
            _cleanup_approval_files()

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

    # Clean up stale approval files from previous session
    _cleanup_approval_files()

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
    global _tick_handle, _server_socket, _clients, _start_time, _pending_approval

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

    _pending_approval = None
    _cleanup_approval_files()
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
        "has_pending_approval": _pending_approval is not None,
    }


# Auto-start when imported
start()
