# BobBot UX Audit

## Current State

BobBot has a working two-tab editor panel (Connect + Chat) that routes AI chat through Claude Code CLI with MCP tool access. The foundation works but the UX needs polish.

---

## Connect Tab

### What Works
- Server status indicator (green/yellow/red dot + text)
- Claude Code detection + auth status
- Model selector (Sonnet/Opus/Haiku buttons)
- Per-client MCP config (Copy/Write for Claude Code, Cursor, VS Code, Windsurf)
- Prerequisite checks (Python, uv, PythonScriptPlugin)

### Issues

| Priority | Issue | Fix |
|----------|-------|-----|
| **High** | Model buttons don't show which is selected | Highlight active button (different color/border) |
| **High** | No model descriptions | Add tooltips: "Sonnet: fast + balanced", "Opus: most capable", "Haiku: fastest + cheapest" |
| **High** | Claude detection runs once at panel open — no way to refresh | Add "Re-detect" or "Refresh" button |
| **Medium** | Color-only status dots (accessibility) | Add text labels: "Running", "Stopped", "Authenticated" alongside dots |
| **Medium** | No indication which MCP config files already exist | Show checkmark if file exists at target path |
| **Medium** | No preview of config before writing | Show generated JSON in a read-only text box or tooltip |
| **Medium** | Prerequisite text is small (font 9) | Increase to 10pt |
| **Low** | No install links for missing prerequisites | Add clickable URLs or "copy install command" |
| **Low** | No onboarding checklist for first-time users | Show a 1-2-3 setup flow when Claude not detected |
| **Low** | No way to configure port/host from the panel | Add Settings section or gear icon flyout |

---

## Chat Tab

### What Works
- Color-coded messages ([You] blue, [BobBot] green, [System] gray)
- Timestamps on each message
- Cost/duration/turns display below bot messages
- Send + Clear buttons
- Auto-scroll to bottom
- Session continuity via `--resume`

### Issues

| Priority | Issue | Fix |
|----------|-------|-----|
| **Critical** | **No "thinking" indicator** — user sees nothing for 5-30s after sending | Show "BobBot is thinking..." message with animated dots or spinner |
| **Critical** | **No Enter-to-send** — must click Send button with mouse | Bind Enter to send, Shift+Enter for newline |
| **High** | **Can spam Send while thinking** — no debounce | Disable Send button while `bAiThinking` is true |
| **High** | **No cancel/stop** — once sent, must wait for full response | Add "Stop" button that kills subprocess |
| **High** | No current model shown in Chat tab | Show model name in header or status line |
| **High** | Chat history lost on editor restart | Persist to `Saved/BobBot/chat_history.json`, reload on open |
| **Medium** | Timestamp font too small (8pt) | Increase to 9-10pt |
| **Medium** | Messages too cramped (8px padding) | Increase to 12px between messages |
| **Medium** | Session cost tracked but not displayed | Show cumulative cost in chat header or footer |
| **Medium** | No markdown rendering — code blocks are plain text | Render backtick blocks in monospace font at minimum |
| **Medium** | Error messages are gray (same as system) — easy to miss | Color errors red/orange |
| **Medium** | No message copy button | Add small clipboard icon on hover per message |
| **Low** | No code syntax highlighting | At least render ``` blocks with a darker background |
| **Low** | No message search/filter | Add search box for chat history |
| **Low** | No input persistence — draft lost on panel close | Save draft to temp file |
| **Low** | No /commands (e.g., /clear, /model, /cost) | Parse input for slash commands before sending |

---

## Thinking State (Critical Gap)

`bAiThinking` is tracked in code and `poll()` returns `thinking: true`, but **nothing is displayed to the user**. This is the single biggest UX failure.

**Proposed fix:**
1. When `bAiThinking` becomes true, add a temporary "[BobBot] Thinking..." message at the bottom of chat
2. Animate it (e.g., cycle dots: "Thinking.", "Thinking..", "Thinking...")
3. Replace it with the actual response when `poll()` returns a response
4. Disable the Send button while thinking
5. Show a "Stop" button that calls `_process.kill()` in Python

---

## Keyboard Shortcuts (Missing)

| Shortcut | Action | Status |
|----------|--------|--------|
| Enter | Send message | **Missing** |
| Shift+Enter | New line in input | Works (Slate default) |
| Ctrl+L | Clear chat | **Missing** |
| Escape | Close panel | **Missing** |
| Ctrl+C | Copy selected text | Works (OS default) |

---

## Visual Polish

### Current Font Sizes
| Element | Size | Recommended |
|---------|------|-------------|
| Tab buttons | Default | OK |
| Section headers | 11pt Bold | OK |
| Status text | Default | OK |
| Prereq text | 9pt | 10pt |
| Message content | 9pt Regular | 10pt |
| Sender labels | 9pt Bold | 10pt Bold |
| Timestamps | 8pt | 9pt |
| Cost line | 7pt | 8pt |
| Chat input | 10pt | OK |

### Spacing
| Element | Current | Recommended |
|---------|---------|-------------|
| Between messages | 8px top, 2px bottom | 4px top, 8px bottom |
| Message content padding | 4px left | 8px left |
| Between sections | Separator only | Separator + 4px extra |

---

## Implementation Priority

### Sprint 1 — Critical UX (must-have)
1. Thinking indicator ("BobBot is thinking..." with dots)
2. Disable Send button while thinking
3. Enter-to-send (Shift+Enter for newline)
4. Highlight active model button

### Sprint 2 — Important Polish
5. Chat history persistence across editor restarts
6. Session cost display
7. Error messages in red/orange (not gray)
8. Show current model in Chat tab header
9. Increase font sizes
10. "Stop" button to cancel thinking

### Sprint 3 — Nice to Have
11. Markdown-lite rendering (monospace for code blocks)
12. Message copy button
13. Refresh prerequisites button
14. Model tooltips with descriptions
15. MCP config file existence indicators
16. Slash commands (/clear, /model, /cost)
