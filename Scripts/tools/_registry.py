"""BobBot tool registry — single source of truth.

Every MCP tool is registered via the `bob_tool` decorator instead of the bare
`@mcp.tool()`. The decorator:

  - records `{name, category, output_kind, default_timeout, sensitive_args, doc, params}`
    into `_TOOL_REGISTRY`
  - applies `with_timeout(default_timeout)` automatically (per-call override
    via `tool_timeout` decorator or `with_timeout` ctxmgr still wins)
  - wraps the function so its return value (envelope dict, string, or None +
    captured stdout) is normalized into a JSON-serialized envelope, with
    oversized payloads spilled to disk
  - delegates to `@mcp.tool()` underneath for FastMCP discovery
  - records every call to the activity log with `{summary, spill_path,
    category, truncated, duration_s}`

Doc autogen, the in-editor InfoTab, and `list_tools` all read from
`_TOOL_REGISTRY`. Hand-maintained tool catalogs are gone.
"""

import functools
import inspect
import io
import json
import time
from contextlib import redirect_stdout

from _common import (
    envelope,
    serialize_envelope,
    with_timeout,
    _log_activity,
    _redact,
    _OUTPUT_BUDGETS,
    _DEFAULT_OUTPUT_KIND,
)

# name -> {category, output_kind, default_timeout, sensitive_args, doc, params, fn}
_TOOL_REGISTRY = {}


def _signature_param_names(fn):
    try:
        sig = inspect.signature(fn)
    except (TypeError, ValueError):
        return []
    return [p for p in sig.parameters.keys()]


def _redact_args_for_log(args, kwargs, param_names, sensitive_args):
    """Build a redacted single-line repr of (args, kwargs) for the activity log.
    Anything in `sensitive_args` is replaced with '<REDACTED>'; remaining
    string values are passed through `_redact` (regex pattern match)."""
    parts = []
    sensitive = set(sensitive_args or ())
    for i, v in enumerate(args):
        name = param_names[i] if i < len(param_names) else f"arg{i}"
        if name in sensitive:
            parts.append(f"{name}=<REDACTED>")
        else:
            parts.append(f"{name}={_redact(repr(v))[:80]}")
    for k, v in kwargs.items():
        if k in sensitive:
            parts.append(f"{k}=<REDACTED>")
        else:
            parts.append(f"{k}={_redact(repr(v))[:80]}")
    return "(" + ", ".join(parts) + ")"


def bob_tool(*, category, output_kind=_DEFAULT_OUTPUT_KIND, default_timeout=60, sensitive_args=()):
    """Decorator. Wraps an MCP tool function with metadata + envelope serialization.

    Usage in tool files:

        from _registry import bob_tool

        def register(mcp, send_fn):
            @mcp.tool()
            @bob_tool(category="Materials", output_kind="large", default_timeout=120)
            def get_material_complexity(material_path: str) -> str:
                ...

    Order matters: `@mcp.tool()` outside `@bob_tool` so FastMCP wraps the
    bob_tool-wrapped function (the one that returns serialized envelope JSON).
    """
    if output_kind not in _OUTPUT_BUDGETS:
        raise ValueError(f"output_kind must be one of {list(_OUTPUT_BUDGETS)} (got {output_kind!r})")

    def deco(fn):
        name = fn.__name__
        param_names = _signature_param_names(fn)
        # Strip leading whitespace per line for a clean docstring stored in the registry.
        doc = inspect.getdoc(fn) or ""

        @functools.wraps(fn)
        def wrapped(*args, **kwargs):
            started = time.time()
            captured = io.StringIO()
            tool_error = None
            ret = None
            try:
                with with_timeout(default_timeout):
                    with redirect_stdout(captured):
                        ret = fn(*args, **kwargs)
            except Exception as e:
                tool_error = "{}: {}".format(type(e).__name__, str(e))

            captured_text = captured.getvalue()
            duration = time.time() - started

            env = _normalize_envelope(ret, captured_text, name, tool_error)
            env = serialize_envelope(env, output_kind=output_kind, tool_name=name)

            # Activity log: tag with category + spill info; summary is already small.
            args_repr = _redact_args_for_log(args, kwargs, param_names, sensitive_args)
            _log_activity(
                name,
                args_repr,
                None,
                env.get("ok", True),
                duration,
                summary=env.get("summary"),
                spill_path=env.get("spill_path"),
                category=category,
                truncated=bool(env.get("meta", {}).get("truncated")),
            )
            # FastMCP serializes whatever we return as the tool result.
            # Emit the envelope as a JSON string so the chat layer can detect
            # and parse it (envelope keys are stable, easy to recognize).
            return json.dumps(env, ensure_ascii=False)

        wrapped.__bob_tool__ = True
        wrapped.__bob_tool_meta__ = {
            "name": name,
            "category": category,
            "output_kind": output_kind,
            "default_timeout": default_timeout,
            "sensitive_args": list(sensitive_args),
            "doc": doc,
            "params": param_names,
        }

        _TOOL_REGISTRY[name] = {
            "name": name,
            "category": category,
            "output_kind": output_kind,
            "default_timeout": default_timeout,
            "sensitive_args": list(sensitive_args),
            "doc": doc,
            "params": param_names,
            "fn": fn,
        }
        return wrapped

    return deco


def _normalize_envelope(ret, captured_text, tool_name, tool_error):
    """Coerce whatever the tool returned into an envelope dict."""
    if tool_error:
        summary = (captured_text or "").strip()
        if not summary:
            summary = tool_error
        else:
            summary = summary[:1500] + "\n\n[ERROR] " + tool_error
        return envelope(summary=summary, data=None, ok=False, error=tool_error, tool_name=tool_name)

    # Case A: tool returned an envelope already.
    if isinstance(ret, dict) and "ok" in ret and "summary" in ret:
        # Still merge captured stdout as supplementary info if non-empty.
        if captured_text and captured_text.strip():
            existing = ret.get("summary") or ""
            extra = captured_text.strip()
            ret["summary"] = (existing + ("\n" if existing else "") + extra)[:4000]
        ret.setdefault("data", None)
        ret.setdefault("spill_path", None)
        ret.setdefault("error", None)
        ret.setdefault("meta", {})
        ret["meta"].setdefault("tool", tool_name)
        return ret

    # Case B: tool returned a plain string (legacy `_exec` style). Use as summary.
    if isinstance(ret, str):
        body = (ret + ("\n" + captured_text if captured_text.strip() else "")).strip()
        return envelope(summary=body, data=None, tool_name=tool_name)

    # Case C: tool returned None — use captured stdout as summary.
    if ret is None:
        body = (captured_text or "").strip() or "(executed successfully, no output)"
        return envelope(summary=body, data=None, tool_name=tool_name)

    # Case D: tool returned something else (list, dict without envelope keys). Stash in data.
    summary_text = (captured_text or "").strip() or f"Returned {type(ret).__name__}"
    return envelope(summary=summary_text[:4000], data=ret, tool_name=tool_name)


def write_tool_manifest(path):
    """Write `_TOOL_REGISTRY` to `path` as JSON. Called by the bridge after
    `_register_all_tools()`. Non-fatal on failure."""
    try:
        manifest = []
        for name in sorted(_TOOL_REGISTRY):
            entry = dict(_TOOL_REGISTRY[name])
            entry.pop("fn", None)  # function ref not serializable
            manifest.append(entry)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(manifest, f, ensure_ascii=False, indent=2)
        return True
    except (OSError, TypeError, ValueError):
        return False


def get_tool_registry():
    """Read-only view of the registry, for `list_tools` and similar."""
    out = []
    for name in sorted(_TOOL_REGISTRY):
        entry = dict(_TOOL_REGISTRY[name])
        entry.pop("fn", None)
        out.append(entry)
    return out
