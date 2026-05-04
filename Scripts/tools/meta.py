"""Meta tools: list_tools — runtime introspection of the registered tool catalog.

Single source of truth for the agent. The static lists in CLAUDE.md, README.md,
Docs/ToolReference.md, and SBobBotInfoTab.cpp::GToolCategories[] are best-effort
docs for humans; this tool reports what is actually registered.
"""


def register(mcp, send_fn):

    @mcp.tool()
    def list_tools(category: str = "") -> str:
        """List registered MCP tools at runtime.

        Args:
            category: Optional substring filter on tool name (case-insensitive).
                      Empty returns the full catalog.

        Returns one tool per line: `name(params) — first line of docstring`.
        """
        try:
            entries = _collect_tools(mcp)
        except Exception as e:
            return f"ERROR: failed to enumerate tool registry: {e}"

        if not entries:
            return "ERROR: no tools registered"

        needle = category.lower().strip()
        if needle:
            entries = [e for e in entries if needle in e["name"].lower()]
            if not entries:
                return f"No tools matching '{category}'"

        entries.sort(key=lambda e: e["name"])
        lines = [f"Tools registered: {len(entries)}"]
        for e in entries:
            params = e["params"]
            doc = e["doc"]
            lines.append(f"  {e['name']}({params}) — {doc}" if doc else f"  {e['name']}({params})")
        return "\n".join(lines)


def _collect_tools(mcp):
    """Walk FastMCP internals to collect (name, params, doc) for every registered tool.

    FastMCP exposes its registry under different attributes across versions.
    Try each in order until one yields tools.
    """
    candidates = []

    # FastMCP >=1.x: _tool_manager._tools is a dict[name -> Tool]
    tm = getattr(mcp, "_tool_manager", None)
    if tm is not None:
        tools_attr = getattr(tm, "_tools", None)
        if isinstance(tools_attr, dict) and tools_attr:
            for name, tool in tools_attr.items():
                candidates.append(_describe_tool(name, tool))
            return candidates

        list_fn = getattr(tm, "list_tools", None)
        if callable(list_fn):
            try:
                items = list_fn()
            except TypeError:
                items = None
            if items:
                for tool in items:
                    name = getattr(tool, "name", None) or getattr(tool, "__name__", "?")
                    candidates.append(_describe_tool(name, tool))
                return candidates

    # Fallback: some FastMCP variants expose mcp.tools or mcp._tools
    for attr in ("tools", "_tools"):
        tools_attr = getattr(mcp, attr, None)
        if isinstance(tools_attr, dict) and tools_attr:
            for name, tool in tools_attr.items():
                candidates.append(_describe_tool(name, tool))
            return candidates

    return candidates


def _describe_tool(name, tool):
    """Pull a sensible (name, params, doc) tuple out of a Tool object."""
    fn = getattr(tool, "fn", None) or getattr(tool, "func", None) or getattr(tool, "function", None)

    params = ""
    parameters = getattr(tool, "parameters", None)
    if isinstance(parameters, dict):
        props = parameters.get("properties", {})
        if isinstance(props, dict):
            params = ", ".join(props.keys())

    if not params and fn is not None:
        try:
            import inspect
            sig = inspect.signature(fn)
            params = ", ".join(p for p in sig.parameters.keys())
        except (TypeError, ValueError):
            pass

    doc = getattr(tool, "description", None)
    if not doc and fn is not None:
        doc = (fn.__doc__ or "").strip()
    if doc:
        doc = doc.splitlines()[0].strip()
    else:
        doc = ""

    return {"name": name, "params": params, "doc": doc}
