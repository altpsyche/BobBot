# BobBot UX Audit — Consolidated

## The Core Problem

BobBot works technically but its UI assumes users understand MCP, TCP, authentication flows, and Python prerequisites. A game developer just wants to chat with Claude in their editor. The FTUE dumps too much information and hides the critical path.

---

## Current FTUE (What Happens Today)

1. Install plugin, restart editor
2. Open BobBot panel -> Connect tab
3. See a wall of information: server status, Claude status, model buttons, 4 MCP client configs, 3 prerequisites, connected clients
4. User thinks: "What is MCP? Why do I need Python? What do these buttons do?"
5. Maybe they click Chat tab and type something
6. Nothing happens for 10-30 seconds (no thinking indicator in chat messages)
7. Response appears -- it works! But user doesn't know what they configured

**Clicks to first chat:** ~4-5, but **minutes of confusion** figuring out what matters.

---

## Ideal FTUE (What Should Happen)

1. Install plugin, restart editor
2. Open BobBot -> See a simple setup checklist:
   - Claude Code: Installed / Not installed [Install]
   - Authentication: Signed in / Not signed in [Sign in]
3. Both green? -> "Ready to chat!" with a [Go to Chat] button
4. Click Chat, type message, see "Thinking..." with dots
5. Response appears. Done.

**Everything else lives under an "Advanced" collapsible section.**

---

## Connect Tab Redesign

### Current Layout (too much, wrong order)
```
Server: Running on :13,579 (0 client(s))      <- noise, misleading count, comma in port
────────────────────────────────────────
Claude Code                                     <- critical but buried
  Claude Code v2.1.84 — authenticated
  Model: [Sonnet] [Opus] [Haiku]
────────────────────────────────────────
AI Clients (MCP)                                <- confusing terminology
  Claude Code    [Copy Config] [Write Config]   <- two ambiguous buttons
  Cursor         [Copy Config] [Write Config]
  VS Code        [Copy Config] [Write Config]
  Windsurf       [Copy Config] [Write Config]
────────────────────────────────────────
Prerequisites                                   <- useful but at the bottom
  Python (3.14.2)
  uv (0.11.1)
  PythonScriptPlugin
────────────────────────────────────────
Connected Clients                               <- always shows 0, misleading
  No clients connected
```

### Proposed Layout (clear, progressive disclosure)
```
SETUP
  Claude Code    Installed (v2.1.84)            [Refresh]
  Authentication Signed in
  Status:        Ready to chat!                 [Go to Chat]
────────────────────────────────────────
MODEL
  [Sonnet]  [Opus]  [Haiku]
   Fast      Best    Cheapest                   <- inline description under each
────────────────────────────────────────
CHAT SESSION
  Model: sonnet
  Session cost: $0.24
  Messages: 12
────────────────────────────────────────
[v Advanced]                                    <- collapsed by default

  TOOL PERMISSIONS
    ( ) Allow Always — Claude can run code without asking
    ( ) Chat Only    — Claude answers only, no tool access
    ( ) Ask Me       — Approve each tool use (coming soon)

  SERVER
    Port:            [13579]
    Auto-start:      [x]
    Auto-generate:   [x]
    Max Clients:     [2]
    Rate Limit:      [30] msg/s

  SYSTEM PROMPT
    [Multi-line text editor]
    [Save] [Reset to Default]

  PROJECT CONTEXT (CLAUDE.md)
    [Multi-line text editor]
    [Save] [Load from CLAUDE.md]

  USE IN OTHER EDITORS
    Claude Code    Configured          [Regenerate]
    Cursor         Not configured      [Setup]
    VS Code        Not configured      [Setup]
    Windsurf       Not configured      [Setup]

  PATHS (read-only, for troubleshooting)
    Project Root:  C:/UGW/game                    exists
    .mcp.json:     C:/UGW/game/.mcp.json          exists
    Bridge:        .../Scripts/bob_mcp_bridge.py   exists
    Prompt File:   .../Saved/BobBot/...            exists

  PREREQUISITES
    Python 3.14.2                                 OK
    uv 0.11.1                                     OK
    PythonScriptPlugin                            OK
```

### Key Changes
- **Setup checklist at top** — the only thing a new user needs
- **"Connected Clients" removed** — replaced with "Chat Session" showing useful info
- **Server status simplified** — no client count, no comma in port
- **MCP Clients section renamed** — "Use in Other Editors", single Setup/Regenerate button per client
- **All config hidden under Advanced** — collapsible, closed by default
- **Paths section** — makes invisible problems visible, for troubleshooting only
- **Permission mode** — safety control for tool access
- **CLAUDE.md editor** — project context, passed via --append-system-prompt-file

---

## Chat Tab Redesign

### Current Issues
| Issue | Impact |
|-------|--------|
| No visible thinking indicator in messages | User thinks it crashed for 10-30s |
| Server status header shows model but easy to miss | User doesn't know what model is active |
| No cumulative session cost | User can't track spend |
| No Stop button | Can't cancel long requests |
| Chat history lost on restart | Frustrating data loss |
| Error messages same color as system messages | Errors are invisible |
| No markdown/code block rendering | Code walls are unreadable |

### Proposed Layout
```
Model: Sonnet | Session: $0.24 | 12 messages        [Clear]
────────────────────────────────────────────────────────────

[You]  14:32:15
Can you make a Fresnel material in /Game/Testing?

[BobBot]  14:32:42
I created the material M_TestFresnel in /Game/Testing with a
Fresnel node connected to emissive.
$0.11 | 27.2s | 4 turns

[BobBot]  Thinking...

────────────────────────────────────────────────────────────
Ask BobBot anything... (Enter to send, Shift+Enter for newline)
                                              [Send] [Stop]
```

### Key Changes
- **Chat header bar** — model, session cost, message count, Clear button (always visible)
- **Thinking message** — shows as a real message in the chat with animated dots
- **Stop button** — visible while thinking, kills subprocess
- **Error messages in orange/red** — not gray
- **Larger font sizes** — sender 10pt, content 10pt, timestamps 9pt, cost 8pt

---

## Terminology Fixes

| Current | Problem | Replace With |
|---------|---------|-------------|
| AI Clients (MCP) | Nobody knows what MCP is | Use in Other Editors |
| Connected Clients | Misleading (always 0) | Chat Session |
| Server: Running on :13,579 | Technical, comma in port | Remove or simplify |
| Copy Config / Write Config | Two ambiguous buttons | Single "Setup" button |
| Prerequisites | Sounds like homework | Move under Advanced |
| Permission mode: bypassPermissions | Scary name | "Allow Always" / "Chat Only" |
| Bridge script | Internal implementation detail | Hide completely |

---

## Hardcoded Values to Expose in Settings

### High Priority
| Value | Current | Control |
|-------|---------|---------|
| Permission Mode | Always allow (dangerous) | Radio: Allow Always / Chat Only / Ask Me |
| System Prompt | Hardcoded paragraph | Multi-line text editor |
| Project Context (CLAUDE.md) | Not implemented | Multi-line text editor |
| Server Port | 13579 | Spinner |
| Chat Timeout | 300s | Spinner |

### Medium Priority
| Value | Current | Control |
|-------|---------|---------|
| Auto-start Server | true | Checkbox |
| Auto-generate .mcp.json | true | Checkbox |
| Max Clients | 2 | Spinner |
| Rate Limit | 30 msg/s | Spinner |

### Low Priority (leave hardcoded for now)
- UI colors, font sizes, animation speeds
- Buffer sizes, retry counts
- Bridge command structure
- Temp directory paths

---

## Sprint Plan

### Sprint 2 — Connect Tab Overhaul
1. Reorder: Setup checklist at top
2. Replace "Connected Clients" with "Chat Session" info
3. Fix server status (remove client count, fix port comma)
4. Collapse Advanced section (settings, other editors, paths, prerequisites)
5. Permission mode radio buttons (Allow Always / Chat Only)
6. Single Setup/Regenerate button per client (replace Copy/Write)
7. Check if config files exist and show status

### Sprint 3 — Chat Tab Polish
8. Thinking indicator as a chat message with animated dots
9. Stop button (kills subprocess)
10. Chat header bar (model, session cost, message count)
11. Error messages in red/orange
12. Chat history persistence

### Sprint 4 — Settings & Customization
13. System prompt editor
14. CLAUDE.md project context editor
15. Server settings UI (port, auto-start, max clients, rate limit)
16. Chat timeout setting
17. Paths debug section

### Sprint 5 — Nice to Have
18. Code block rendering (monospace background)
19. Message copy button
20. Slash commands (/clear, /model, /cost)
21. "Ask Me" permission mode with approval dialog
