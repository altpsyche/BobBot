# BobBot v1.5 — Implementation Prompt

Read these docs before starting:
1. `Plugins/BobBot/Docs/BobBotReleaseRoadmap.md` — the full roadmap with 5 phases, 19 items
2. Memory at `~/.claude/projects/c--UGW-game/memory/project_bobbot_v3.md` — current architecture

## Context

BobBot is an AI chat plugin for Unreal Engine 5.4+. It has 159 MCP tools, a Claude Agent SDK backend (persistent ClaudeSDKClient process), a redesigned Chat UI with theme system (BobBotTheme.h), SVG icons, selectable text, and dark containers. The plugin is at `c:\UGW\game\Plugins\BobBot\`.

The SDK refactor (v4.0-dev) is complete. All 5 SDK phases are done. The UI redesign is done (theme system, message component, input toolbar with dropdowns, SVG icons, container helpers). BobBot branding is done (no "Claude" in AI personality, plain language system prompt).

Now we need to ship v1.5 — the first distributable version. The roadmap has 5 phases with 19 items total.

## What to implement

Work through the roadmap phases in order. Each phase should be completed and tested before moving to the next.

### Phase 1: Release Blockers (P0)
1. **Error handling** — Replace 60+ bare `except:` in `Scripts/tools/` with `except Exception as e:` + `unreal.log_warning()`. Also fix broad catches in `bob_bridge_launcher.py`, `bob_mcp_server.py`.
2. **License** — Add `LICENSE.md` (MIT). Add attribution for claude-agent-sdk, mcp, pywin32.
3. **Plugin descriptor** — Update `BobBot.uplugin`: VersionName "1.5-beta", populate DocsURL/SupportURL.
4. **Stale docs** — Archive pre-refactor docs (SDKClientRewrite.md, SDKCapabilities.md). Update README for current architecture.
5. **Memory dir mismatch** — Fix `GetMemoryDir()` in `SBobBotContextTab.cpp` to hash from `ProjectSavedDir()/BobBot` instead of `ProjectDir()`.

### Phase 2: Robustness (P1)
1. **FTUE hardening** — Better error messages when SDK install fails. Per-step retry buttons in Welcome tab.
2. **API key security** — Move API key storage from plaintext INI to Windows Credential Manager.
3. **Test harness** — Add `/test` slash command. Formalize `_test_tools.py` into proper smoke tests.
4. **Reconnection** — Auto-detect bridge death, attempt restart, show "Reconnecting..." in chat.

### Phase 3: UX Polish (P1-P2)
1. **Quickstart chips** — Show suggestion buttons below welcome message ("Set up a basic level", "Create a material").
2. **Streaming text** — Wire partial messages to display text as it arrives, not after completion.
3. **Image preview** — Embed viewport captures inline in chat using SImage.
4. **Drag feedback** — Show asset thumbnails instead of raw paths when dropping assets.

### Phase 4: Power Features (P2-P3)
1. **Project memory** — Wire SDK auto-memory properly. Ensure BobBot's memory namespace is isolated.
2. **Blueprint scripting** — Expand BobBotLib: `connect_pins`, `add_branch_node`, `add_variable_get/set_node`, `add_cast_node`.
3. **Live preview** — Auto-capture viewport after actor modifications, include in tool result.
4. **Context suggestions** — Add system prompt line: "When the user refers to 'this' or 'the selected', call get_editor_selection()."

### Phase 5: Distribution (P3-P4)
1. **Marketplace packaging** — Standalone plugin, install script, test on fresh projects.
2. **Multi-platform** — Abstract Windows-specific code (pywin32, CREATE_NO_WINDOW, taskkill).
3. **Version management** — Add CHANGELOG.md, semver, migration logic.

## Key files
- `Content/Python/bob_chat_sdk.py` — SDK backend (persistent client, hooks, sessions)
- `Content/Python/bob_chat.py` — thin re-export wrapper
- `Content/Python/bob_mcp_server.py` — TCP server inside UE
- `Content/Python/bob_bridge_launcher.py` — HTTP bridge launcher + venv management
- `Scripts/tools/` — 39 tool files (error handling cleanup target)
- `Source/BobBot/Public/BobBotTheme.h` — theme system (colors, fonts, spacing)
- `Source/BobBot/Public/BobBotConstants.h` — UI helpers (Card, Container, Toolbar, AccentCard)
- `Source/BobBot/Public/BobBotConfig.h` — config fields
- `Source/BobBot/Private/Widgets/SBobBotChatTab.cpp` — chat UI
- `Source/BobBot/Private/Widgets/SBobBotConnectTab.cpp` — settings UI
- `Source/BobBot/Private/Widgets/SBobBotContextTab.cpp` — context/memory UI
- `Source/BobBot/Private/Widgets/SBobBotWelcomeTab.cpp` — FTUE wizard
- `Source/BobBot/Private/BobBotChatController.cpp` — chat business logic
- `Source/BobBot/Private/BobBotStyle.cpp` — SVG icons + rich text styles
- `Source/BobBot/Private/BobBotLib.cpp` — C++ Blueprint editing API
- `CLAUDE.md` — tool reference (159 tools)
- `Saved/BobBot/agents.json` — custom agent definitions

## Constraints
- UE's Python is single-threaded game thread — async runs in background thread
- C++ polls Python via `bob_chat.poll()` every 50ms when thinking
- Theme system: all colors via `BobBot::Theme::`, all fonts via `Theme::Font*()`, containers via `UI::Card/Container/Toolbar/AccentCard`
- SVG icons in `Resources/` registered in `BobBotStyle.cpp`
- System prompt is short identity + personality — CLAUDE.md discovered by SDK via `add_dirs`
- Permission modes: Plan / AskBeforeEdits / EditAutomatically — mapped to SDK's plan / acceptEdits / bypassPermissions
- BobBot branding: AI assistant = "BobBot", CLI tool = "Claude Code"

Start with Phase 1. Plan each item, implement, test, then move to the next phase.
