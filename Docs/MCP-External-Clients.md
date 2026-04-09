# Connecting external MCP clients to BobBot

BobBot exposes its 200 Unreal Engine tools as a local MCP server. Other MCP-aware editors (Cursor, VS Code, Windsurf, plain Claude Code in a terminal) can connect and use those tools, but they bypass BobBot's in-editor approval UI by definition: a separate process is making the calls and Claude inside that process owns its own permission flow.

This page describes how to wire an external client to BobBot, what the trust boundary looks like, and what the bridge token does.

## What runs where

- **TCP server** (`bob_mcp_server.py`) lives inside the Unreal Editor process on `127.0.0.1:13579`. It executes Python on the editor's game thread.
- **HTTP bridge** (`bob_mcp_bridge_http.py`) is a separate Python process spawned by BobBot's launcher. It speaks MCP over HTTP on `127.0.0.1:13580/mcp` and forwards every tool call to the TCP server.
- **BobBot's own SDK client** (the chat tab) connects to the HTTP bridge like any other MCP client.

Both ports bind to localhost only. They are never exposed to the network.

## The bridge token

When the editor starts, BobBot generates a fresh random token (`FBobBotConfig::RegenerateBridgeAuthToken`) and writes it to two places:

1. The `BOB_BRIDGE_TOKEN` environment variable. The HTTP bridge subprocess and the in-editor TCP server both read it from there.
2. The URL path in `<Project>/Saved/BobBot/_bobbot_mcp.json`. The MCP endpoint lives at `/mcp/<token>` instead of `/mcp`.

The token is **not** persisted to the on-disk INI. It is regenerated on every editor launch.

The HTTP bridge uses the token as a path segment in its MCP endpoint. Any request to `/mcp` (without the token suffix) gets Starlette's default 404. This replaces the earlier `X-Bobbot-Token` header approach which silently failed because the MCP Python client library deprecated custom headers. The TCP server enforces the token via a handshake: the first message on every connection must be `{"type": "auth", "token": "..."}` or the server closes the socket.

## Trust model

- **Random local processes** (other software on the machine probing localhost) cannot use BobBot's tools. They don't have the token and the bridge rejects them.
- **External editors you wire up explicitly** can use BobBot's tools by reading the token out of `_bobbot_mcp.json` and pasting it into their own MCP config. This is an explicit consent step. Once configured, the external editor's calls are not gated by BobBot's in-editor approval UI; they go through that editor's own permission system.

If you are uncomfortable with an external editor running tools without BobBot's approval UI, do not wire it up. The token gives access; it does not impose policy.

## Configuring an external client

Read `<Project>/Saved/BobBot/_bobbot_mcp.json` after starting the editor. It looks like this:

```json
{
  "mcpServers": {
    "unreal": {
      "type": "http",
      "url": "http://127.0.0.1:13580/mcp/abcdef0123456789..."
    }
  }
}
```

The token is encoded in the URL path (`/mcp/<token>`). Copy the entire URL, not just the token — the path IS the credential.

Copy the entire `unreal` entry into your MCP client's config:

- **Claude Code (terminal)**: paste into your `.mcp.json` (BobBot already writes this to the project root automatically — the token is included).
- **Cursor**: paste into `~/.cursor/mcp.json` under `mcpServers`.
- **Windsurf**: paste into Windsurf's MCP settings file under `mcpServers`.
- **VS Code (Copilot Chat)**: VS Code uses a different schema (`servers` instead of `mcpServers`). BobBot writes `<Project>/Saved/BobBot/_bobbot_mcp_fallback.json` for the stdio variant; for HTTP, copy the URL and headers manually.

The token rotates every editor launch, so you will need to refresh your external client's config whenever you restart the editor. If you find yourself doing this often, keep your external editor config open in a tab and re-paste from `_bobbot_mcp.json`.

## What this is not

- **Not network security.** Both ports bind to `127.0.0.1`. The token does not protect against an attacker who already has code execution on your machine — they can read `_bobbot_mcp.json` themselves.
- **Not a sandbox.** BobBot's tools include `execute_unreal_python` and `run_console_command`. Anyone authorized to call them can do anything the editor process can do. Treat the token like a password to your editor.
- **Not multi-user.** If you start a second instance of the editor on the same machine on a different port, it generates its own token. The two editors do not share state.

## Disabling auth

If you need the pre-1.6 unauthenticated behavior (for example, a script that pokes the bridge without reading the JSON), set `BOB_BRIDGE_TOKEN=` (empty) in the environment before launching the editor. The bridge logs `WITHOUT token auth (open localhost)` on startup when it is in this mode. Do not run in this mode unless you understand the consequences.
