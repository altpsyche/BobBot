"""BobBot system diagnostics: backend status, bridge health, MCP config."""

import json
import os
import socket

from _common import _exec


def register(mcp, send_fn):

    @mcp.tool()
    def get_bobbot_status() -> str:
        """Get BobBot system status: backend type, bridge health, session info, MCP config, and environment. Useful for diagnosing connection or configuration issues."""
        lines = []

        # Backend type
        use_sdk = os.environ.get("BOB_USE_SDK", "0")
        backend = "Agent SDK" if use_sdk == "1" else "Subprocess"
        lines.append("Backend: {}".format(backend))

        # Auth mode
        auth_mode = os.environ.get("BOB_AUTH_MODE", "subscription")
        lines.append("Auth mode: {}".format(auth_mode))
        if auth_mode == "api_key":
            provider = os.environ.get("BOB_API_PROVIDER", "anthropic")
            has_key = bool(os.environ.get("BOB_API_KEY", ""))
            lines.append("  Provider: {}".format(provider))
            lines.append("  API key set: {}".format("yes" if has_key else "no"))

        # Bridge health (TCP connect check)
        bridge_port = os.environ.get("BOB_MCP_BRIDGE_PORT", "13580")
        lines.append("\nBridge port: {}".format(bridge_port))
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(2)
            s.connect(("127.0.0.1", int(bridge_port)))
            s.close()
            lines.append("Bridge health: Connected")
        except Exception:
            lines.append("Bridge health: Unreachable")

        # MCP server config
        mcp_port = os.environ.get("BOB_MCP_PORT", "13579")
        mcp_host = os.environ.get("BOB_MCP_HOST", "127.0.0.1")
        max_clients = os.environ.get("BOB_MCP_MAX_CLIENTS", "4")
        lines.append("\nMCP server: {}:{}".format(mcp_host, mcp_port))
        lines.append("Max clients: {}".format(max_clients))

        # Session info — query via UE Python (bob_chat runs inside the editor)
        session_output = _exec(
            "import bob_chat, json\n"
            "print(json.dumps(bob_chat.get_status()))\n"
        )
        if session_output and not session_output.startswith("Error:"):
            try:
                status = json.loads(session_output.strip())
                lines.append("\nSession:")
                lines.append("  Model: {}".format(status.get("model", "unknown")))
                lines.append("  Session ID: {}".format(status.get("session_id") or "none"))
                lines.append("  Cost: ${:.2f}".format(status.get("session_cost", 0)))
                lines.append("  Thinking: {}".format(status.get("thinking", False)))
                lines.append("  Backend active: {}".format(status.get("backend", "unknown")))
            except (ValueError, KeyError):
                lines.append("\nSession: unable to parse response")
        else:
            lines.append("\nSession: {}".format(session_output or "unable to query"))

        # Budget
        budget = os.environ.get("BOB_MAX_BUDGET", "0")
        lines.append("\nCost budget: ${} per message{}".format(
            budget, " (unlimited)" if budget == "0" or budget == "0.00" else ""))

        # Permission mode
        perm = os.environ.get("BOB_PERMISSION_MODE", "ask_me")
        lines.append("Permission mode: {}".format(perm))

        return "\n".join(lines)
