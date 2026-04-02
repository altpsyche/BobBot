# Claude Agent SDK — Full Capabilities Reference for BobBot

BobBot currently uses ~5% of the SDK. This doc maps every available feature, what BobBot uses today, and what would bring it closer to the Claude Code experience.

## What BobBot Uses Today

- `query()` — one-shot per message (spawns new process each time)
- `ClaudeAgentOptions` — basic: model, system_prompt, mcp_servers, permission_mode, cli_path
- `fork_session()` — session branching
- Message types: AssistantMessage, UserMessage, ResultMessage, SystemMessage, TaskStarted/Progress/Notification

## Priority Features (Immediate Impact)

### 1. ClaudeSDKClient (Replaces query())
Persistent subprocess, multi-turn context, streaming.
```python
async with ClaudeSDKClient(options) as client:
    await client.query("message")
    async for msg in client.receive_response(): ...
```
**Methods:**
- `connect()` / `disconnect()` — lifecycle
- `query(prompt)` — send message
- `receive_response()` — stream until ResultMessage
- `receive_messages()` — stream indefinitely
- `interrupt()` — stop mid-response (cancel button)
- `set_model(model)` — switch model live
- `set_permission_mode(mode)` — change permissions live
- `get_context_usage()` — accurate token breakdown
- `get_mcp_status()` — server connection state
- `reconnect_mcp_server(name)` — retry failed server
- `toggle_mcp_server(name, enabled)` — enable/disable at runtime
- `stop_task(task_id)` — cancel running subagent task
- `rewind_files(user_message_id)` — restore files to prior state
- `get_server_info()` — discover capabilities

### 2. get_context_usage() — Accurate Context Data
```python
usage = await client.get_context_usage()
# Returns: {totalTokens, maxTokens, percentage, categories[], mcpTools[], memoryFiles[]}
```
Per-category breakdown (messages, tools, memory, agents). No more guessing.

### 3. interrupt() — Cancel Button
```python
await client.interrupt()  # Stops Claude mid-response
```
Maps to BobBot's Stop button. Currently kills the whole process — this is graceful.

### 4. set_model() — Live Model Switching
```python
await client.set_model("opus")  # No reconnect needed
```

## Session Management (Browse, Search, Tag)

All standalone functions (don't need ClaudeSDKClient):

| Function | Returns | Use Case |
|----------|---------|----------|
| `list_sessions(directory, limit, offset)` | `list[SDKSessionInfo]` | Browse history sidebar |
| `get_session_info(session_id)` | `SDKSessionInfo` | Show details without full load |
| `get_session_messages(session_id, limit, offset)` | `list[SessionMessage]` | Load transcript with pagination |
| `rename_session(session_id, title)` | — | Custom conversation titles |
| `tag_session(session_id, tag)` | — | Filter by workflow ("blueprints", "debug") |
| `delete_session(session_id)` | — | Remove conversations |
| `fork_session(session_id, title)` | `ForkSessionResult` | Branch from checkpoint |

`SDKSessionInfo` fields: `session_id`, `summary`, `last_modified`, `file_size`, `custom_title`, `first_prompt`, `git_branch`, `cwd`, `tag`, `created_at`

## Hooks (10 Event Types — Currently 0 Used)

| Hook | When | BobBot Use |
|------|------|-----------|
| `PreToolUse` | Before tool executes | Log tool calls, custom permission UI |
| `PostToolUse` | After tool succeeds | Log results, update UI |
| `PostToolUseFailure` | Tool error | Error recovery, retry logic |
| `UserPromptSubmit` | Before processing input | Inject context, expand prompts |
| `PreCompact` | Before context compaction | Custom summarization rules |
| `Notification` | System alert | Display in BobBot UI |
| `SubagentStart` | Sub-agent spawned | Track parallel tasks |
| `SubagentStop` | Sub-agent done | Aggregate results |
| `PermissionRequest` | Tool needs permission | Custom approval dialog |
| `Stop` | Session stopping | Cleanup, checkpoint |

Hook config:
```python
options = ClaudeAgentOptions(
    hooks={
        "PreToolUse": [HookMatcher(matcher=None, hooks=[callback_fn], timeout=60)]
    }
)
```

Hook callbacks receive typed input (tool_name, tool_input, session_id, etc.) and return decisions (continue, suppress, modify input).

## Thinking & Effort Control

```python
# Extended thinking with token budget
options = ClaudeAgentOptions(
    thinking=ThinkingConfigEnabled(type="enabled", budget_tokens=10000)
)

# Effort level (quick vs thorough)
options = ClaudeAgentOptions(effort="high")  # "low" | "medium" | "high" | "max"
```

BobBot UI: thinking toggle + effort slider in Connect tab.

## Permission System (Advanced)

Current: `permission_mode="bypassPermissions"` (trust everything)

Available modes: `default`, `acceptEdits`, `plan`, `dontAsk`, `bypassPermissions`

Per-tool custom logic:
```python
async def can_use_tool(tool_name, tool_input, context):
    if "critical" in tool_input.get("path", ""):
        return PermissionResultDeny(message="Cannot edit critical files")
    return PermissionResultAllow()

options = ClaudeAgentOptions(can_use_tool=can_use_tool)
```

Live switching: `await client.set_permission_mode("plan")`

## File Checkpointing & Undo

```python
options = ClaudeAgentOptions(enable_file_checkpointing=True)

# Later — revert files to state at message #5
await client.rewind_files(user_message_id="uuid-of-message-5")
```

## Custom Agents (Multi-Agent)

```python
options = ClaudeAgentOptions(
    agents={
        "blueprint-reviewer": AgentDefinition(
            description="Reviews Blueprint architecture",
            prompt="You are a Blueprint expert...",
            tools=["get_blueprint_functions", "get_blueprint_components"],
            model="haiku",
        )
    }
)
```

Claude can delegate to these agents. Tracked via SubagentStart/Stop hooks + TaskStarted/Progress/Notification messages.

## MCP Server Runtime Control

```python
status = await client.get_mcp_status()
# Returns: [{name, status: "connected"|"pending"|"failed", tools: [...]}]

await client.reconnect_mcp_server("unreal")
await client.toggle_mcp_server("unreal", enabled=False)
```

## In-Process MCP Tools (No Subprocess)

```python
from claude_agent_sdk import create_sdk_mcp_server, tool

@tool()
async def my_tool(param: str) -> str:
    """Runs in-process, no subprocess overhead."""
    return "result"

server = create_sdk_mcp_server("my-tools", "1.0", tools=[my_tool])
options = ClaudeAgentOptions(mcp_servers={"my-tools": server})
```

Could expose UE Python functions directly without TCP round-trip.

## Advanced Options (Not Yet Used)

| Option | Type | Purpose |
|--------|------|---------|
| `fallback_model` | str | Auto-fallback if primary unavailable |
| `max_turns` | int | Limit turns per conversation |
| `max_budget_usd` | float | Total cost limit |
| `task_budget` | TaskBudget | Token budget (Claude paces itself) |
| `output_format` | dict | Structured JSON output schema |
| `disallowed_tools` | list[str] | Explicitly block tools |
| `setting_sources` | list | Load user/project/local settings |
| `sandbox` | SandboxSettings | Network restrictions |
| `plugins` | list | Custom extensions |
| `betas` | list | Opt into beta features (1M context) |
| `add_dirs` | list | Additional directories for context |
| `extra_args` | dict | Arbitrary CLI flags |

## Feature Impact Matrix

| Feature | Impact | Complexity | Requires ClaudeSDKClient |
|---------|--------|-----------|-------------------------|
| Persistent client | Critical | Medium | Yes |
| Context monitor | High | Low | Yes |
| Interrupt/cancel | High | Low | Yes |
| Session browsing | High | Low | No (standalone) |
| Live model switch | Medium | Low | Yes |
| Hooks (tool logging) | Medium | Medium | No (options) |
| Extended thinking | Medium | Low | No (options) |
| Effort control | Medium | Low | No (options) |
| Plan mode | Medium | Low | Yes |
| File checkpointing | Medium | Medium | Yes |
| Custom agents | Medium | High | No (options) |
| MCP server control | Low | Low | Yes |
| In-process tools | Low | Medium | No (options) |
| Session tags | Low | Low | No (standalone) |
| Task budgets | Low | Low | No (options) |
