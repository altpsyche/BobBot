"""Meta tools: list_tools — runtime introspection of the tool registry.

The tool registry (`_TOOL_REGISTRY` in `_registry.py`) is the single source of
truth for tool metadata. This tool reads from it directly, bypassing FastMCP's
internal listing — that way it surfaces `category`, `output_kind`, and
`default_timeout` per tool, which `list_tools()` from FastMCP does not.
"""

from _registry import bob_tool, get_tool_registry
from _common import envelope


def register(mcp, send_fn):

    @mcp.tool()
    @bob_tool(category="Meta", output_kind="large", default_timeout=10)
    def list_tools(category: str = ""):
        """List registered MCP tools at runtime.

        Returns an envelope whose `data.tools` is a list of
        `{name, category, output_kind, default_timeout, params, doc}`.

        Args:
            category: Optional substring filter (case-insensitive) on tool name
                      OR on category. Empty returns the full catalog.
        """
        registry = get_tool_registry()
        needle = category.lower().strip() if category else ""
        if needle:
            registry = [
                t for t in registry
                if needle in t["name"].lower() or needle in t["category"].lower()
            ]

        if not registry:
            return envelope(
                summary=f"No tools matching '{category}'" if needle else "No tools registered",
                data={"tools": []},
            )

        # Compact summary: count + categories + first 10 names.
        cats = {}
        for t in registry:
            cats[t["category"]] = cats.get(t["category"], 0) + 1
        cats_line = ", ".join(f"{c}:{n}" for c, n in sorted(cats.items(), key=lambda kv: -kv[1]))
        sample = ", ".join(t["name"] for t in registry[:10])
        if len(registry) > 10:
            sample += f", ... (+{len(registry) - 10} more)"
        summary = f"{len(registry)} tools across {len(cats)} categories. {cats_line}. Sample: {sample}"

        return envelope(
            summary=summary,
            data={"tools": registry, "categories": cats},
        )
