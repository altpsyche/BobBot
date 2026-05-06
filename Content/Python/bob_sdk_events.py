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
APPROVAL_TIMEOUT = 120.0


async def _await_decision(tool_name, category, tool_input):
    """Queue an approval request for the UI and wait for the user's decision.

    Returns: 'allow', 'deny', or 'timeout'.
    Allocates a unique request id, registers a Future on the running loop,
    appends the approval_request stream event, and blocks until either
    set_permission_decision() resolves the future or the timeout fires.
    """
    request_id = bob_sdk_permissions._next_request_id()
    loop = asyncio.get_event_loop()
    future = loop.create_future()
    bob_sdk_permissions._register_pending(
        request_id, future, loop, tool_name, category)

    with _lock:
        _stream_events.append({
            "type": "approval_request",
            "id": request_id,
            "tool": tool_name,
            "category": category,
            "input": json.dumps(tool_input, indent=2) if isinstance(
                tool_input, dict) else str(tool_input),
        })

    import bob_sdk_config
    bob_sdk_config._log_sdk(
        "BobBot: approval[{}] waiting: {} ({})".format(request_id, tool_name, category))

    try:
        decision = await asyncio.wait_for(future, timeout=APPROVAL_TIMEOUT)
    except asyncio.TimeoutError:
        bob_sdk_permissions._pop_pending(request_id)
        bob_sdk_config._log_sdk(
            "BobBot: approval[{}] TIMEOUT: {}".format(request_id, tool_name))
        return "timeout"

    bob_sdk_config._log_sdk(
        "BobBot: approval[{}] got '{}' for: {}".format(request_id, decision, tool_name))
    return decision


async def _can_use_tool(tool_name, tool_input, context):
    """SDK can_use_tool callback. Auto-approve whitelisted categories.
    For everything else, show the approval UI and wait for user response.
    """
    import os
    from claude_agent_sdk import PermissionResultAllow, PermissionResultDeny

    # In Auto mode (bypassPermissions), allow everything.
    # The callback may still fire because it was registered at client creation
    # time and permission mode changes don't remove it.
    current_mode = os.environ.get("BOB_PERMISSION_MODE", "")
    if current_mode == "edit_automatically":
        return PermissionResultAllow()

    # In Plan mode, deny everything (read-only)
    if current_mode == "plan":
        return PermissionResultDeny(message="Plan mode: tool execution disabled")

    category = bob_sdk_permissions._classify_tool(tool_name)

    # Auto-approved categories pass through immediately
    if bob_sdk_permissions._is_auto_approved(category):
        return PermissionResultAllow()

    decision = await _await_decision(tool_name, category, tool_input)
    if decision == "allow":
        return PermissionResultAllow()
    if decision == "timeout":
        return PermissionResultDeny(message="Approval timed out")
    return PermissionResultDeny(message="User denied tool execution")


async def _on_permission_request(input_data, tool_use_id, context):
    """PermissionRequest hook — replaces Claude Code's terminal prompt for external MCP tools.
    Queues approval_request event for BobBot's Slate UI, blocks until user responds.
    """
    import os
    tool_name = input_data.get("tool_name", "")
    tool_input = input_data.get("tool_input", {})

    # In Auto mode, allow everything (don't show approval UI)
    current_mode = os.environ.get("BOB_PERMISSION_MODE", "")
    if current_mode == "edit_automatically":
        return {"hookSpecificOutput": {
            "hookEventName": "PermissionRequest",
            "decision": {"behavior": "allow"},
        }}

    category = bob_sdk_permissions._classify_tool(tool_name)

    # Auto-approved categories pass through without asking
    if bob_sdk_permissions._is_auto_approved(category):
        return {"hookSpecificOutput": {
            "hookEventName": "PermissionRequest",
            "decision": {"behavior": "allow"},
        }}

    decision = await _await_decision(tool_name, category, tool_input)
    if decision == "timeout":
        return {"hookSpecificOutput": {
            "hookEventName": "PermissionRequest",
            "decision": {"behavior": "deny", "message": "Approval timed out"},
        }}
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
    """PostToolUse hook — log tool results in chat UI.

    `tool_response` is now a JSON-serialized envelope (see _registry.bob_tool).
    Extract the small `summary` field for the chat-UI preview. Fall back to the
    raw string for legacy tools that haven't been wrapped yet — those get
    capped at 4 KB with an explicit truncation marker so a future silent slice
    can't reappear.
    """
    tool_name = input_data.get("tool_name", "")
    tool_response = input_data.get("tool_response", "")
    summary = _summarize_tool_response(tool_response)
    with _lock:
        _stream_events.append({
            "type": "hook_tool_result",
            "tool": tool_name,
            "output": summary,
        })
    return {"hookSpecificOutput": {"hookEventName": "PostToolUse"}}


def _summarize_tool_response(tool_response):
    """Extract a small chat-UI-renderable summary from a tool response.

    Handles three shapes:
      - JSON envelope: `{"ok":..,"summary":"..","data":..}` → return summary.
      - Plain string (legacy): truncate to 4 KB with explicit marker.
      - Anything else: stringify and truncate.
    """
    text = tool_response if isinstance(tool_response, str) else str(tool_response)
    try:
        parsed = json.loads(text)
        if isinstance(parsed, dict) and "ok" in parsed and "summary" in parsed:
            summary = str(parsed.get("summary") or "")
            spill = parsed.get("spill_path")
            if spill and parsed.get("meta", {}).get("truncated"):
                summary = summary + f"\n[full data spilled to {spill} — call read_overflow to fetch]"
            return summary
    except (ValueError, TypeError):
        pass
    LIMIT = 4096
    if len(text) <= LIMIT:
        return text
    return text[:LIMIT] + f"\n... ({len(text) - LIMIT:,} bytes truncated; legacy tool, no envelope)"


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
    StreamEvent = sdk["StreamEvent"]

    # Per-content-block text accumulator for partial-message streaming.
    # Reset on each text content_block_start so a fresh text block
    # streams from empty rather than appending to the previous block.
    # The C++ side replaces rather than appends, so we emit cumulative
    # text on each delta. The final AssistantMessage that arrives after
    # all stream events still re-emits the assembled text — that's a
    # harmless no-op replacement for the common single-block case and
    # corrects ordering for the rare text->tool->text case (the
    # AssistantMessage tool_use event resets StreamingBotMessageIndex,
    # so the second text becomes a new bot message).
    streaming_text_buffer = ""

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

            elif isinstance(message, StreamEvent):
                # Raw Anthropic API stream events: emitted per partial chunk
                # because we set include_partial_messages=True. We care about
                # text deltas; tool_use chunks are still processed at
                # AssistantMessage time (the input JSON arrives via
                # input_json_delta and isn't worth assembling twice).
                ev = message.event or {}
                etype = ev.get("type", "")
                if etype == "message_start":
                    streaming_text_buffer = ""
                elif etype == "content_block_start":
                    block = ev.get("content_block", {})
                    if block.get("type") == "text":
                        # New text block — start a fresh accumulator.
                        # Initial text on content_block_start is rare but
                        # possible (server-side concatenation).
                        streaming_text_buffer = block.get("text", "") or ""
                        if streaming_text_buffer:
                            with _lock:
                                _stream_events.append({
                                    "type": "text",
                                    "text": streaming_text_buffer,
                                })
                elif etype == "content_block_delta":
                    delta = ev.get("delta", {})
                    if delta.get("type") == "text_delta":
                        chunk = delta.get("text", "")
                        if chunk:
                            streaming_text_buffer += chunk
                            with _lock:
                                _stream_events.append({
                                    "type": "text",
                                    "text": streaming_text_buffer,
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
                        # Always re-emit assembled text. For single-block
                        # turns this is a harmless no-op replacement on the
                        # C++ side (same content). For multi-block turns
                        # (text -> tool_use -> text) the second text block
                        # streams over the first during partials, but the
                        # AssistantMessage replay restores correct ordering
                        # because the tool_use event between the two text
                        # emits resets StreamingBotMessageIndex.
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
