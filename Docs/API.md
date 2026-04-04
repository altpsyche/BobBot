# Plugin API Reference

Developer reference for the BobBot plugin codebase. Covers C++ classes, Python modules, and how they connect.

For the 159 MCP tools BobBot exposes to AI, see [ToolReference.md](ToolReference.md). For the Blueprint editing library, see [BobBotLib.md](BobBotLib.md).

## C++ Classes

### FBobBotChatController

Chat business logic. Owns all state: history, polling, slash commands, approval flow, sessions. Widgets subscribe to its delegates.

```cpp
// Public API
void SendMessage(const FString& Message);
void ClearSession();
void StopChat();
void UndoLastMessage();
void ApproveExecution();
void DenyExecution();
void Tick(float DeltaTime);

// Multi-chat
void NewChat();
void SwitchChat(const FString& ChatId);
void DeleteChat(const FString& ChatId);
void ForkChat();
void RefreshChatIndex();

// Getters
const TArray<FBobBotChatMessage>& GetHistory() const;
bool IsThinking() const;
float GetSessionCost() const;
int32 GetMessageCount() const;
bool HasPendingApproval() const;
float GetContextPercent() const;

// Delegates (subscribe from widgets)
FOnChatMessageAdded    OnMessageAdded;      // New message in history
FOnChatHistoryCleared  OnHistoryCleared;     // Session cleared
FOnApprovalStateChanged OnApprovalStateChanged; // Tool needs approval
FOnThinkingStateChanged OnThinkingStateChanged; // AI started/stopped thinking
FOnMessageUpdated      OnMessageUpdated;     // Existing message changed (streaming, tool complete)
FOnChatListChanged     OnChatListChanged;    // Session list refreshed
```

Polling runs on Tick: `PollServerStatus()` delegates to three monitors (see below), `PollChatUpdates()` dispatches stream events to `HandleStreamEvent_*` methods.

Slash commands are registered in the constructor as `SlashCommands.Add(TEXT("/name"), lambda)`. The `/help` command auto-generates its output from the registered command keys. To add a new command, add a `SlashCommands.Add()` call in the constructor — it's automatically included in `/help`.

### FBobBotPythonBridge

Singleton for all C++ to Python communication. Wraps UE's `IPythonScriptPlugin`.

```cpp
static FBobBotPythonBridge& Get();

// Execute Python, get JSON back
TSharedPtr<FJsonObject> ExecPythonWithJsonResult(const FString& Script, const FString& TempFileName);

// Execute Python, no result needed
bool ExecPythonCommand(const FString& Script);

// Call a Python function and parse JSON result (handles script construction + escaping)
TSharedPtr<FJsonObject> CallPythonJson(const FString& Module, const FString& Function,
    const FString& PythonArgs = TEXT(""), const FString& TempFileName = TEXT(""));

// Escape a string for safe embedding in Python code
static FString PyStr(const FString& Value);

bool IsAvailable() const;   // True if PythonScriptPlugin is loaded
FString GetTempDir() const;  // Saved/BobBot/, creates if needed
```

`ExecPythonWithJsonResult` injects an `output_path` variable into the script. The script writes JSON to that path; the bridge reads, parses, and cleans up the file.

### FBobBotConfig

Singleton for all settings. Persisted to `Saved/Config/BobBot.ini`.

```cpp
static FBobBotConfig& Get();
void Load();
void Save();
void ApplyEnvironmentVars() const;  // Push settings to Python via env vars
```

Key fields: `Port`, `BridgePort`, `ChatModel`, `PermissionMode`, `MaxBudgetUsd`, `ThinkingMode`, `EffortLevel`, `AuthMode`, `ApiKey` (stored in OS keychain), auto-approve booleans.

See `BobBotConfig.h` for the full field list with defaults and comments.

### Status Monitors

Three classes extracted from `PollServerStatus()`, each with a single responsibility:

```cpp
class FBobBotServerMonitor {
    void Poll(FBobBotPythonBridge& Bridge);
    bool IsRunning() const;
    int32 GetClientCount() const;
};

class FBobBotBridgeMonitor {
    void SetMessageCallback(TFunction<void(bool bError, const FString& Msg)> Callback);
    void Poll(FBobBotPythonBridge& Bridge, float PollInterval);
    bool IsRunning() const;
    // Handles reconnection internally (3 retries, 5s cooldown)
};

class FBobBotSdkMonitor {
    void Poll(FBobBotPythonBridge& Bridge);
    bool IsDone() const;  // Stops after SDK found or 60 polls
};
```

### UBobBotLib

C++ Blueprint editing API exposed to Python. Full reference in [BobBotLib.md](BobBotLib.md).

### Widget Tabs

| Class | Tab | Key builder methods |
|-------|-----|-------------------|
| `SBobBotWelcomeTab` | Welcome | 6-step auto-setup, per-step retry |
| `SBobBotConnectTab` | Connect | `BuildSetupSection()`, `BuildAuthSection()`, `BuildBridgeStatusSection()`, 8 advanced builders |
| `SBobBotChatTab` | Chat | `BuildHeaderBar()`, `BuildInputArea()`, `BuildPermissionDropdown()`, `BuildModelDropdown()`, `BuildSettingsPopover()` |
| `SBobBotContextTab` | Context | System prompt editor, memory viewer |
| `SBobBotInfoTab` | Info | Tool list, slash commands, about |

### UI Helpers (BobBotConstants.h)

```cpp
namespace BobBot::UI {
    SectionHeading(Label);                      // Section title text
    KeyValueRow(Key, Value, Color);             // "Label:   Value" row
    Card(Content, Padding);                     // Dark overlay card
    Container(Content, Padding);                // Surface-level container
    Toolbar(Content, Padding);                  // Elevated toolbar
    AccentCard(Content, AccentColor, Padding);  // Left-bar accent + card
    ConfigCheckbox(&FBobBotConfig::bField, Label);       // Auto-save checkbox
    ConfigCheckboxSimple(&FBobBotConfig::bField, Label); // Auto-save, no env sync
}
```

### Theme (BobBotTheme.h)

All colors via `BobBot::Theme::` (Surface, Elevated, Overlay, TextPrimary, TextSecondary, BotAccent, etc.). All fonts via `Theme::FontBody()`, `FontHeading()`, `FontCode()`, etc.

## Python Modules

### Public API: bob_chat.py / bob_chat_sdk.py

`bob_chat.py` is a thin re-export wrapper. C++ calls `import bob_chat; bob_chat.function()`. The real implementation is `bob_chat_sdk.py`, which delegates to four sub-modules:

```python
# Core
send_message(text)          # Send user message, starts streaming
poll()                      # Returns {"events": [...], "thinking": bool}
cleanup()                   # Disconnect client, destroy event loop
clear_session()             # Clear conversation, next message starts fresh
is_thinking()               # True while AI is responding
interrupt()                 # Stop current response

# Config
set_model(name)             # "sonnet", "opus", "haiku" (live switch if connected)
get_model()                 # Current model name
get_status()                # Dict with claude_found, model, sdk, session info
get_session_cost()          # Running cost in USD
get_session_id()            # Current SDK session ID
set_session_id(sid)         # Switch to a different session

# Detection
detect_claude()             # Returns (found: bool, version: str)
check_auth()                # Returns (authenticated: bool, status: str)
check_sdk_available()       # Returns {"ok": bool, "ver": str}

# Sessions
list_saved_sessions(limit, offset)
rename_saved_session(session_id, title)
tag_saved_session(session_id, tag)
delete_saved_session(session_id)
fork_current_session()

# Permissions
set_permission_decision(decision)  # "allow" or "deny" — called by C++ approval UI
```

### Sub-modules

| Module | What it owns |
|--------|-------------|
| `bob_sdk_config.py` | System prompt, model state, project root, MCP server config, agent definitions |
| `bob_sdk_permissions.py` | Tool classification (5 categories), auto-approve logic, permission hooks |
| `bob_sdk_events.py` | `_stream_events` list, `_lock`, `_is_thinking`, hook callbacks, `_send_and_stream()` |
| `bob_sdk_client.py` | `ClaudeSDKClient` lifecycle, async event loop, connection/disconnection, SDK type cache |
| `bob_platform.py` | Platform abstraction (`IS_WINDOWS`/`IS_MAC`/`IS_LINUX`, venv paths, subprocess flags, process management) |
| `bob_bridge_launcher.py` | Venv creation, pip installs (`_run_pip_install`), bridge process lifecycle, health checks |
| `bob_mcp_server.py` | TCP server on game thread, tool classification, auto-approve, stdout capture |

### Tool helpers (_common.py)

```python
_exec(code)                     # Execute Python in UE, return stdout
_exec_ue(code)                  # Same but with `import unreal` prepended
actor_exec(label, code)         # Find actor by label, run code with `target` bound
asset_exec(path, code)          # Load asset by path, run code with `asset` bound
_error(msg)                     # Print standardized "ERROR: " message
```

## Config to Python flow

C++ settings reach Python through environment variables:

1. `FBobBotConfig::ApplyEnvironmentVars()` calls `FPlatformMisc::SetEnvironmentVar()` for each setting
2. `BobBotConstants::Scripts::EnvSync` runs on Python init to sync them into `os.environ` (uses `ctypes.windll.kernel32.GetEnvironmentVariableW` on Windows, direct `os.environ.get()` on macOS/Linux)
3. Python modules read them via `os.environ.get("BOB_*", default)`

### Environment variables

| Variable | Source field | Python consumer |
|----------|-------------|----------------|
| `BOB_MCP_PORT` | `Config.Port` | `bob_mcp_server.py` |
| `BOB_MCP_HOST` | `Config.Host` | `bob_mcp_server.py` |
| `BOB_MCP_BRIDGE_PORT` | `Config.BridgePort` | `bob_bridge_launcher.py` |
| `BOB_PROJECT_ROOT` | `FPaths::GetProjectFilePath()` | All modules (project path resolution) |
| `BOB_PERMISSION_MODE` | `Config.PermissionMode` | `bob_sdk_client.py` (SDK perm mapping) |
| `BOB_CHAT_TIMEOUT` | `Config.ChatTimeoutSeconds` | `bob_sdk_client.py` |
| `BOB_MAX_BUDGET` | `Config.MaxBudgetUsd` | `bob_sdk_client.py` |
| `BOB_THINKING_MODE` | `Config.ThinkingMode` | `bob_sdk_client.py` |
| `BOB_THINKING_BUDGET` | `Config.ThinkingBudget` | `bob_sdk_client.py` |
| `BOB_EFFORT` | `Config.EffortLevel` | `bob_sdk_client.py` |
| `BOB_AUTH_MODE` | `Config.AuthMode` | `bob_chat_sdk.py` |
| `BOB_API_KEY` | `Config.ApiKey` | `bob_chat_sdk.py` |
| `BOB_API_PROVIDER` | `Config.ApiProvider` | `bob_chat_sdk.py` |
| `BOB_AUTO_APPROVE_*` | `Config.bAutoApprove*` | `bob_sdk_permissions.py` |

To add a new config field: add the field to `FBobBotConfig`, add the env var write in `ApplyEnvironmentVars()`, add the env var key to the `EnvSync` script's key list in `BobBotConstants.h`, and read it in Python via `os.environ.get()`.

This avoids string interpolation of config values into Python source code.
