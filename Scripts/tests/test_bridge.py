"""Bridge test harness — 5 scenarios validating bob_mcp_bridge.py against
a fake UE TCP server.

Run from the BobBot venv:
    python Scripts/tests/test_bridge.py

Fake server binds to an ephemeral port (0) so it never clashes with a real
editor on 13579. Auth handshake disabled by setting BOB_BRIDGE_TOKEN="".
Each scenario asserts the expected behavior; a non-zero exit code means
something regressed.
"""

import json
import os
import socket
import sys
import threading
import time

# Ensure the bridge module can be imported.
HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.dirname(HERE)
if SCRIPTS not in sys.path:
    sys.path.insert(0, SCRIPTS)


# --------------------------------------------------------------------------- #
# Fake UE server: accepts JSON-newline messages, replies per-scenario
# --------------------------------------------------------------------------- #

class FakeUEServer:
    def __init__(self, mode="echo"):
        """mode: 'echo' | 'oversized' | 'timeout' | 'drop'"""
        self.mode = mode
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(1)
        self.port = self.sock.getsockname()[1]
        self._stop = False
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def start(self):
        self.thread.start()

    def stop(self):
        self._stop = True
        try:
            self.sock.close()
        except OSError:
            pass

    def _serve(self):
        try:
            self.sock.settimeout(2.0)
            while not self._stop:
                try:
                    conn, _ = self.sock.accept()
                except (socket.timeout, OSError):
                    continue
                threading.Thread(target=self._handle, args=(conn,), daemon=True).start()
        except OSError:
            pass

    def _handle(self, conn):
        try:
            buf = b""
            while b"\n" not in buf:
                chunk = conn.recv(65536)
                if not chunk:
                    return
                buf += chunk
            line = buf.split(b"\n", 1)[0]
            req = json.loads(line.decode("utf-8"))

            if self.mode == "drop":
                conn.close()
                return
            if self.mode == "timeout":
                # Sleep longer than test timeout
                time.sleep(5)
                conn.close()
                return
            if self.mode == "oversized":
                payload = "X" * (40 * 1024)
                resp = {"success": True, "output": payload}
            else:  # echo
                resp = {"success": True, "output": "echo: " + (req.get("code", "")[:50])}
            conn.sendall(json.dumps(resp).encode("utf-8") + b"\n")
        except (ConnectionError, json.JSONDecodeError, OSError):
            pass
        finally:
            try:
                conn.close()
            except OSError:
                pass


# --------------------------------------------------------------------------- #
# Test runner
# --------------------------------------------------------------------------- #

def _run_with_server(mode, *, timeout_s=2):
    """Boot a fake server, freshly import bridge against it, return _send_and_receive."""
    srv = FakeUEServer(mode=mode)
    srv.start()
    os.environ["BOB_MCP_HOST"] = "127.0.0.1"
    os.environ["BOB_MCP_PORT"] = str(srv.port)
    os.environ["BOB_BRIDGE_TOKEN"] = ""
    # Drop any previous bridge module cache
    for mod in list(sys.modules):
        if mod.startswith("bob_mcp_bridge"):
            del sys.modules[mod]
    import bob_mcp_bridge  # noqa: F401  pylint: disable=import-outside-toplevel
    bob_mcp_bridge._SOCKET_TIMEOUT = timeout_s  # type: ignore[attr-defined]
    return bob_mcp_bridge, srv


_FAILED = []


def _check(name, cond, detail=""):
    if cond:
        print(f"  PASS  {name}")
    else:
        print(f"  FAIL  {name}: {detail}")
        _FAILED.append(name)


def test_connect():
    print("scenario 1: connect")
    bridge, srv = _run_with_server("echo")
    try:
        sock = bridge._get_connection()
        _check("connect produces socket", sock is not None)
        bridge._disconnect()
    finally:
        srv.stop()


def test_roundtrip():
    print("scenario 2: send/receive roundtrip")
    bridge, srv = _run_with_server("echo")
    try:
        result = bridge._send_and_receive({"type": "execute", "code": "ping"})
        _check("roundtrip success flag", result.get("success") is True, str(result))
        _check("roundtrip echoes code", "ping" in result.get("output", ""), str(result))
        bridge._disconnect()
    finally:
        srv.stop()


def test_oversized_response():
    print("scenario 3: oversized response handled")
    bridge, srv = _run_with_server("oversized")
    try:
        result = bridge._send_and_receive({"type": "execute", "code": "fat"})
        out = result.get("output", "")
        _check("oversized parses successfully", result.get("success") is True, str(result)[:200])
        _check("oversized payload received intact", len(out) >= 40 * 1024, f"len={len(out)}")
        bridge._disconnect()
    finally:
        srv.stop()


def test_timeout():
    print("scenario 4: per-call timeout override")
    bridge, srv = _run_with_server("timeout", timeout_s=10)
    try:
        # Use 1-second per-call override; server sleeps 5s
        result = bridge._send_and_receive({"type": "execute", "code": "hang", "timeout": 1})
        _check("timeout returns failure", result.get("success") is False, str(result))
        bridge._disconnect()
    finally:
        srv.stop()


def test_disconnect_recover():
    print("scenario 5: disconnect-recover")
    bridge, srv = _run_with_server("drop")
    try:
        # First call hits a server that drops; bridge retries and eventually fails
        result = bridge._send_and_receive({"type": "execute", "code": "first"})
        _check("first call surfaces error", result.get("success") is False, str(result))
        # Switch server to echo mode and retry
        srv.mode = "echo"
        bridge._disconnect()
        result2 = bridge._send_and_receive({"type": "execute", "code": "after"})
        _check("recovers after server flip", result2.get("success") is True, str(result2))
        bridge._disconnect()
    finally:
        srv.stop()


def main():
    test_connect()
    test_roundtrip()
    test_oversized_response()
    test_timeout()
    test_disconnect_recover()
    print()
    if _FAILED:
        print(f"FAILED: {len(_FAILED)} test(s): {', '.join(_FAILED)}")
        sys.exit(1)
    print("All bridge tests passed")


if __name__ == "__main__":
    main()
