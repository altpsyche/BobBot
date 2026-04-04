# BobBot Release Roadmap

## Current State (pre-release)

BobBot is a working UE5 AI plugin with 159 MCP tools, full Claude Agent SDK integration (90% usage), a redesigned Chat UI with theme system and SVG icons, and a natural language system prompt. Core functionality works: persistent conversations, tool execution, session management, thinking/effort controls.

Not yet release-ready. Critical gaps: error handling, licensing, test coverage, docs freshness.

**Target: v1.5 release after completing all 5 phases.**

---

## Phase 1: Release Blockers (must fix before any distribution)

### 1.1 Error Handling Cleanup
60+ bare `except:` clauses across tool scripts silently swallow errors. Users can't debug when tools fail.

**Fix**: Replace all bare `except:` with `except Exception as e:` + `unreal.log_warning()`. Every tool error should produce a visible message.

**Files**: All 39 files in `Scripts/tools/`, `Content/Python/bob_bridge_launcher.py`, `Content/Python/bob_mcp_server.py`

### 1.2 License and Attribution
No LICENSE file exists. Can't distribute without one.

**Fix**: Add `LICENSE.md` (MIT or Apache 2.0). Add attribution section for claude-agent-sdk, mcp, pywin32. Add copyright headers to source files.

### 1.3 Plugin Descriptor
Empty DocsURL, SupportURL in `BobBot.uplugin`. Version still "1.0".

**Fix**: Set VersionName to "4.0-beta", populate URLs, set proper category.

### 1.4 Stale Documentation
`Docs/` folder has pre-refactor planning docs (SDKClientRewrite.md, SDKCapabilities.md, NextChatPrompt.md). README may reference old subprocess architecture.

**Fix**: Remove or archive stale docs. Update README to reflect current SDK-only architecture, new permission modes, theme system, system prompt approach.

### 1.5 Memory Directory Mismatch
Context tab shows wrong memory. C++ `GetMemoryDir()` hashes from project root (`C:\UGW\game` → `c--UGW-game/memory`) but the SDK uses `cwd=Saved/BobBot` so actual memory is at `C--UGW-game-Saved-BobBot/memory`. UI displays stale VS Code Claude memories instead of BobBot's own.

**Root cause**: `GetMemoryDir()` in `SBobBotContextTab.cpp` (line 288) hashes from `FPaths::ProjectDir()`. Should hash from `ProjectDir/Saved/BobBot` to match the SDK's `cwd`.

**Fix**: Update `GetMemoryDir()` to compute hash from `FPaths::ProjectSavedDir() / "BobBot"` instead of `FPaths::ProjectDir()`. This aligns the C++ UI with where the SDK actually stores memory, so the Context tab shows BobBot's own memories, not cross-contamination from other Claude Code sessions.

---

## Phase 2: Robustness (quality for daily use)

### 2.1 FTUE Hardening
Welcome tab doesn't handle SDK installation failure gracefully. If step 4 (Install Agent SDK) fails, step 6 (Verify SDK) shows confusing errors.

**Fix**: If SDK install fails, show clear message with manual fix instructions. Add a "Retry step" button per step instead of retrying from scratch.

### 2.2 API Key Security
API keys stored in plaintext INI file (BobBotConfig.h TODO).

**Fix**: Use platform keychain (Windows Credential Manager via `FWindowsPlatformMisc::GetStoredValue`). Fall back to INI if keychain unavailable.

### 2.3 Test Harness
Only one ad-hoc test file (`_test_tools.py`). No automated testing.

**Fix**: Formalize test suite. Add `/test` slash command that runs smoke tests and reports results in chat. Test each tool category with basic operations. Add C++ unit tests for BobBotLib.

### 2.4 Reconnection Resilience
If the bridge dies mid-conversation, BobBot shows "Cannot connect to 127.0.0.1:13579" and the user has to restart the editor.

**Fix**: Auto-detect bridge death and attempt restart. Show "Reconnecting..." in chat instead of a raw error. Add bridge health monitoring to the MCP status indicator.

---

## Phase 3: User Experience Polish

### 3.1 Quickstart / First Conversation
New users open BobBot and don't know what to say. The empty chat with "BobBot ready. Type a message and press Enter." is cold.

**Fix**: Show 3-4 suggestion chips below the welcome message: "Set up a basic level", "Show me what you can do", "Create a material". Clicking one sends the message.

### 3.2 Streaming Text Display
Bot messages appear all at once after the response completes. No character-by-character streaming.

**Fix**: Wire `include_partial_messages=True` (already set) to append text chunks to the chat in real-time instead of waiting for the complete event.

### 3.3 Image/Viewport Preview in Chat
BobBot can capture viewports but the image isn't shown in chat. Users have to manually find the file.

**Fix**: When `capture_viewport` runs, embed the captured image inline in the chat using `SImage` with the captured file as a `FSlateImageBrush`.

### 3.4 Drag and Drop Feedback
The asset drop target works but there's no visual indicator of what was dropped. Just the path string appears in the input.

**Fix**: Show the asset thumbnail next to the path, or convert to a friendly name like "SM_Cube (Static Mesh)" instead of the raw path.

---

## Phase 4: Power Features

### 4.1 Project Memory
BobBot forgets project conventions across sessions. Users repeat "always use metric units" or "materials go in /Game/Materials".

**Fix**: Wire SDK auto-memory. After each conversation, BobBot summarizes learned conventions and stores them in `~/.claude/projects/{hash}/memory/`. These are loaded via `add_dirs` on next session automatically.

### 4.2 Blueprint Visual Scripting
BobBotLib has basic Blueprint editing (variables, functions, events) but can't wire nodes or create complete gameplay systems.

**Fix**: Expand BobBotLib with:
- `connect_pins(bp, source_node, source_pin, target_node, target_pin)` — wire nodes
- `add_branch_node(bp, x, y)` — flow control
- `add_variable_get_node(bp, var_name, x, y)` / `add_variable_set_node(bp, var_name, x, y)`
- `add_cast_node(bp, target_class, x, y)`

This unlocks "make a health system Blueprint" as a single prompt.

### 4.3 Live Preview Loop
BobBot can't see the result of its actions. It spawns a cube but doesn't know if it clipped through the floor.

**Fix**: After any actor modification, auto-capture a viewport screenshot and include it in the tool result. BobBot can then self-correct: "I see the cube is underground, let me move it up."

### 4.4 Context-Aware Suggestions
BobBot doesn't know what the user is looking at. If they have an actor selected and type "change the color", BobBot doesn't know which actor.

**Fix**: Add one line to system prompt: "When the user refers to 'this', 'the selected', or an actor without naming it, call get_editor_selection() to find out what they mean." BobBot already has `get_editor_selection()` as an MCP tool. Claude calls it only when the message is ambiguous. Zero extra tokens on clear requests, one tool call on ambiguous ones. No code change needed beyond the prompt line.

---

## Phase 5: Distribution

### 5.1 Marketplace Packaging
Package as a standalone plugin that works by dropping into any UE5.4+ project.

**Fix**: 
- Create install script that handles Claude Code detection and `claude login`
- Remove dependency on project-specific paths
- Test on fresh UE5.4, 5.5 projects
- Write Marketplace description, screenshots, feature list

### 5.2 Multi-Platform
Currently Windows-only (pywin32, CREATE_NO_WINDOW, taskkill).

**Fix**: Abstract platform-specific code. Test on macOS (UE5 has Mac editor). Linux is lower priority but share the same Python backend.

### 5.3 Version Management
No versioning system. No changelog. No upgrade path.

**Fix**: Add CHANGELOG.md. Version each release (semver). Add migration logic in Welcome tab for breaking config changes between versions.

---

## Priority Matrix

| Item | Impact | Effort | Priority |
|------|--------|--------|----------|
| 1.1 Error handling | High | Low | P0 |
| 1.2 License | High | Low | P0 |
| 1.3 Plugin descriptor | Medium | Trivial | P0 |
| 1.4 Stale docs | Medium | Low | P0 |
| 1.5 Memory dir mismatch | High | Trivial | P0 |
| 2.1 FTUE hardening | Medium | Medium | P1 |
| 2.4 Reconnection | High | Medium | P1 |
| 3.1 Quickstart chips | High | Low | P1 |
| 3.2 Streaming text | High | Medium | P1 |
| 2.2 API key security | Medium | Medium | P2 |
| 2.3 Test harness | Medium | High | P2 |
| 3.3 Image preview | Medium | Medium | P2 |
| 3.4 Drag feedback | Low | Low | P2 |
| 4.1 Project memory | High | Medium | P2 |
| 4.4 Context suggestions | High | Low | P2 |
| 4.2 Blueprint scripting | Very High | Very High | P3 |
| 4.3 Live preview | Medium | Medium | P3 |
| 5.1 Marketplace | High | High | P3 |
| 5.2 Multi-platform | Medium | High | P4 |
| 5.3 Versioning | Medium | Low | P4 |

---

## v1.5 Release Checklist

After completing all 5 phases, tag as v1.5. This is the first distributable version.

**Phase 1 — Release Blockers:**
- Error handling cleanup (60+ bare excepts)
- LICENSE.md + attribution
- Plugin descriptor (version, URLs)
- Stale docs removed/updated
- Memory directory mismatch fixed

**Phase 2 — Robustness:**
- FTUE hardening (clear error paths)
- API key keychain storage
- Test harness (/test slash command)
- Reconnection resilience (auto-restart bridge)

**Phase 3 — UX Polish:**
- Quickstart suggestion chips
- Streaming text display
- Image/viewport preview in chat
- Drag and drop asset feedback

**Phase 4 — Power Features:**
- Project memory (SDK auto-memory wired properly)
- Blueprint visual scripting (node wiring via BobBotLib)
- Live preview loop (auto viewport capture)
- Context-aware suggestions (system prompt line)

**Phase 5 — Distribution:**
- Marketplace packaging (standalone plugin)
- Multi-platform (macOS support)
- Version management (CHANGELOG.md, semver, migration)

**Release steps:**
1. Complete all 5 phases
2. Update `BobBot.uplugin` VersionName to "1.5"
3. Add CHANGELOG.md with full v1.5 entry
4. Final manual test pass (FTUE, chat, tools, permissions, sessions, memory)
5. Test on fresh UE5.4 project
6. Tag commit as v1.5
