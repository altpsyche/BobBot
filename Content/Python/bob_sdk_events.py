"""
BobBot SDK — Stream event processing.

Holds the shared _stream_events list, _lock, _is_thinking flag,
the hook callbacks that append to _stream_events, and the core
_send_and_stream() async function.
"""

import json
import asyncio
import threading
import traceback

import bob_sdk_permissions


# --------------------------------------------------------------------------- #
# Shared state — accessed by multiple modules via import
# --------------------------------------------------------------------------- #
_lock = threading.Lock()
_is_thinking = False
_stream_events = []


# --------------------------------------------------------------------------- #
# Hook callbacks (append to _stream_events)
# --------------------------------------------------------------------------- #
async def _can_use_tool(tool_name, tool_input, context):
    """SDK can_use_tool callback — auto-approve categories, let others fall through.
    Runs before the permission system. Fast, no blocking.
    """
    from claude_agent_sdk import PermissionResultAllow
    category = bob_sdk_permissions._classify_tool(tool_name)
    if bob_sdk_permissions._is_auto_approved(category):
        return PermissionResultAllow()
    return None  # fall through to SDK permission system -> PermissionRequest hook


async def _on_permission_request(input_data, tool_use_id, context):
    """PermissionRequest hook — replaces Claude Code's terminal prompt.
    Queues approval_request event for BobBot's Slate UI, blocks until user responds.
    """
    tool_name = input_data.get("tool_name", "")
    tool_input = input_data.get("tool_input", {})
    category = bob_sdk_permissions._classify_tool(tool_name)

    # Queue for C++ approval widget
    request_id = id(input_data)
    bob_sdk_permissions._pending_permission = {
        "id": request_id, "tool": tool_name, "category": category}
    bob_sdk_permissions._permission_response = None
    with _lock:
        _stream_events.append({
            "type": "approval_request",
            "id": request_id,
            "tool": tool_name,
            "category": category,
            "input": json.dumps(tool_input, indent=2) if isinstance(
                tool_input, dict) else str(tool_input),
        })

    # Block until user responds via set_permission_decision()
    timeout = 120.0
    start = asyncio.get_event_loop().time()
    while bob_sdk_permissions._permission_response is None:
        if asyncio.get_event_loop().time() - start > timeout:
            bob_sdk_permissions._pending_permission = None
            return {"hookSpecificOutput": {
                "hookEventName": "PermissionRequest",
                "decision": {"behavior": "deny", "message": "Approval timed out"},
            }}
        await asyncio.sleep(0.1)

    decision = bob_sdk_permissions._permission_response
    bob_sdk_permissions._pending_permission = None
    bob_sdk_permissions._permission_response = None
    return {"hookSpecificOutput": {
        "hookEventName": "PermissionRequest",
        "decision": {"behavior": decision},
    }}


async def _on_pre_tool_use(input_data, tool_use_id, context):
    """PreToolUse hook — log tool calls to chat UI. No permission logic."""
    tool_name = input_data.get("tool_name", "")
    tool_input = input_data.get("tool_input", {})
    with _lock:
        _stream_events.append({
            "type": "hook_tool_start",
            "tool": tool_name,
            "input": json.dumps(tool_input, indent=2) if isinstance(tool_input, dict) else str(tool_input),
        })
    return {"hookSpecificOutput": {"hookEventName": "PreToolUse"}}


async def _on_post_tool_use(input_data, tool_use_id, context):
    """PostToolUse hook — log tool results in chat UI."""
    tool_name = input_data.get("tool_name", "")
    tool_response = input_data.get("tool_response", "")
    with _lock:
        _stream_events.append({
            "type": "hook_tool_result",
            "tool": tool_name,
            "output": str(tool_response)[:2000],
        })
    return {"hookSpecificOutput": {"hookEventName": "PostToolUse"}}


async def _on_post_tool_failure(input_data, tool_use_id, context):
    """PostToolUseFailure hook — log tool errors."""
    with _lock:
        _stream_events.append({
            "type": "hook_tool_error",
            "tool": input_data.get("tool_name", ""),
            "error": input_data.get("error", ""),
        })
    return {"hookSpecificOutput": {"hookEventName": "PostToolUseFailure"}}


async def _on_notification(input_data, tool_use_id, context):
    """Notification hook — display Claude's alerts in BobBot UI."""
    with _lock:
        _stream_events.append({
            "type": "notification",
            "message": input_data.get("message", ""),
            "title": input_data.get("title", ""),
        })
    return {"hookSpecificOutput": {"hookEventName": "Notification"}}


# --------------------------------------------------------------------------- #
# Core streaming loop
# --------------------------------------------------------------------------- #
async def _send_and_stream(user_message):
    """Send a message via persistent client and stream events into _stream_events."""
    global _is_thinking

    # Late imports to avoid circular dependency (events -> client -> events)
    import bob_sdk_client
    import bob_chat_sdk

    # get_status is needed by the internal MCP server's status tool
    _get_status_fn = bob_chat_sdk.get_status

    sdk = bob_sdk_client._get_sdk_types()
    AssistantMessage = sdk["AssistantMessage"]
    UserMessage = sdk["UserMessage"]
    ResultMessage = sdk["ResultMessage"]
    SystemMessage = sdk["SystemMessage"]
    TextBlock = sdk["TextBlock"]
    ToolUseBlock = sdk["ToolUseBlock"]
    ClaudeSDKError = sdk["ClaudeSDKError"]
    TaskStartedMessage = sdk["TaskStartedMessage"]
    TaskProgressMessage = sdk["TaskProgressMessage"]
    TaskNotificationMessage = sdk["TaskNotificationMessage"]

    with _lock:
        _is_thinking = True
        _stream_events.clear()

    try:
        await bob_sdk_client._ensure_client(get_status_fn=_get_status_fn)

        # Send message to living process
        await bob_sdk_client._client.query(user_message)

        # Stream response until ResultMessage
        async for message in bob_sdk_client._client.receive_response():

            # Subagent task messages (check BEFORE SystemMessage — subclasses)
            if isinstance(message, TaskStartedMessage):
                with _lock:
                    _stream_events.append({
                        "type": "subagent_start",
                        "task_id": message.task_id,
                        "description": message.description,
                        "task_type": getattr(message, "task_type", "") or "",
                    })

            elif isinstance(message, TaskProgressMessage):
                usage = message.usage or {}
                with _lock:
                    _stream_events.append({
                        "type": "subagent_progress",
                        "task_id": message.task_id,
                        "description": message.description,
                        "total_tokens": usage.get("total_tokens", 0) if isinstance(usage, dict) else 0,
                        "tool_uses": usage.get("tool_uses", 0) if isinstance(usage, dict) else 0,
                        "duration_ms": usage.get("duration_ms", 0) if isinstance(usage, dict) else 0,
                        "last_tool_name": getattr(message, "last_tool_name", "") or "",
                    })

            elif isinstance(message, TaskNotificationMessage):
                usage = message.usage or {}
                with _lock:
                    _stream_events.append({
                        "type": "subagent_complete",
                        "task_id": message.task_id,
                        "status": message.status,
                        "summary": getattr(message, "summary", "") or "",
                        "total_tokens": usage.get("total_tokens", 0) if isinstance(usage, dict) else 0,
                        "tool_uses": usage.get("tool_uses", 0) if isinstance(usage, dict) else 0,
                        "duration_ms": usage.get("duration_ms", 0) if isinstance(usage, dict) else 0,
                    })

            elif isinstance(message, SystemMessage):
                sid = message.data.get("session_id") if hasattr(message, "data") else None
                if sid:
                    with _lock:
                        bob_sdk_client._session_id = sid
                        bob_sdk_client._client_session_id = sid

            elif isinstance(message, AssistantMessage):
                for block in message.content:
                    if isinstance(block, TextBlock):
                        if block.text:
                            with _lock:
                                _stream_events.append({
                                    "type": "text", "text": block.text})
                    elif isinstance(block, ToolUseBlock):
                        tool_input = block.input
                        if isinstance(tool_input, dict):
                            try:
                                input_str = json.dumps(tool_input, indent=2)
                            except (TypeError, ValueError):
                                input_str = str(tool_input)
                        elif isinstance(tool_input, str):
                            try:
                                parsed = json.loads(tool_input)
                                input_str = json.dumps(parsed, indent=2)
                            except (json.JSONDecodeError, ValueError):
                                input_str = tool_input
                        else:
                            input_str = str(tool_input) if tool_input else "{}"
                        with _lock:
                            _stream_events.append({
                                "type": "tool_use",
                                "name": block.name,
                                "input": input_str,
                            })
                    else:
                        btype = getattr(block, "type", "")
                        if btype == "error":
                            error_msg = (getattr(block, "message", "")
                                         or getattr(block, "text", "")
                                         or str(block))
                            with _lock:
                                _stream_events.append({
                                    "type": "error",
                                    "message": "Assistant error: {}".format(
                                        error_msg),
                                })

            elif isinstance(message, UserMessage):
                for block in message.content:
                    btype = getattr(block, "type", "")
                    if btype == "tool_result":
                        output = getattr(block, "content", "")
                        if isinstance(output, list):
                            output = "\n".join(
                                b.get("text", "") for b in output
                                if isinstance(b, dict) and b.get("type") == "text"
                            )
                        with _lock:
                            _stream_events.append({
                                "type": "tool_result",
                                "output": str(output),
                            })

            elif isinstance(message, ResultMessage):
                cost = message.total_cost_usd or 0

                # Get accurate context usage from persistent client
                input_tokens = 0
                output_tokens = 0
                context_window = 0

                try:
                    ctx = await bob_sdk_client._client.get_context_usage()
                    # ContextUsageResponse is a TypedDict with camelCase keys
                    input_tokens = ctx.get("totalTokens", 0) or 0
                    context_window = ctx.get("maxTokens", 0) or 0
                    # output_tokens not in context_usage — get from ResultMessage
                    usage = message.usage or {}
                    if isinstance(usage, dict):
                        output_tokens = usage.get("output_tokens", 0) or 0
                except Exception:
                    # Fallback: derive from ResultMessage.usage
                    usage = message.usage or {}
                    model_usage = message.model_usage or {}
                    if isinstance(usage, dict):
                        input_tokens = (
                            (usage.get("input_tokens", 0) or 0)
                            + (usage.get("cache_read_input_tokens", 0) or 0)
                            + (usage.get("cache_creation_input_tokens", 0) or 0)
                        )
                        output_tokens = usage.get("output_tokens", 0) or 0
                    if isinstance(model_usage, dict):
                        for _model_name, model_data in model_usage.items():
                            if isinstance(model_data, dict):
                                cw = model_data.get("contextWindow", 0) or 0
                                if cw > 0:
                                    context_window = cw
                                    break

                with _lock:
                    if message.session_id:
                        bob_sdk_client._session_id = message.session_id
                        bob_sdk_client._client_session_id = message.session_id
                    bob_sdk_client._total_session_cost += cost
                    _stream_events.append({
                        "type": "complete",
                        "cost": cost,
                        "duration_ms": message.duration_ms or 0,
                        "num_turns": message.num_turns or 1,
                        "is_error": message.is_error,
                        "result_text": message.result or "",
                        "input_tokens": input_tokens,
                        "output_tokens": output_tokens,
                        "context_window": context_window,
                    })

    except ClaudeSDKError as e:
        # Mark client for reconnection if process died
        bob_sdk_client._check_client_health()
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "Claude SDK error: {}".format(str(e)),
            })
    except Exception:
        bob_sdk_client._check_client_health()
        with _lock:
            _stream_events.append({
                "type": "error",
                "message": "SDK stream error: {}".format(traceback.format_exc()),
            })
    finally:
        with _lock:
            _is_thinking = False
