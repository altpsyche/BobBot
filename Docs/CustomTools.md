# Custom Tools

BobBot scans two directories for tools:

1. `Plugins/BobBot/Scripts/tools/` -- built-in tools (159 tools, don't edit these)
2. `<YourProject>/BobBot/tools/` -- your project-specific tools

Project tools live outside the plugin directory, so they survive plugin updates. BobBot discovers them automatically when the bridge connects.

## Creating a tool

Create a Python file in `<YourProject>/BobBot/tools/`:

```python
# BobBot/tools/my_tools.py

from _common import _exec_ue, actor_exec, asset_exec

def register(mcp, send_fn):

    @mcp.tool()
    def greet_world() -> str:
        """Print a greeting to the UE output log."""
        return _exec_ue("""
unreal.log("Hello from BobBot!")
print("Greeting sent to output log")
""")

    @mcp.tool()
    def paint_actor_red(actor_label: str) -> str:
        """Create a red material and apply it to an actor."""
        return actor_exec(actor_label, """
# target is already bound to the found actor
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_Red", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
if mat:
    # Set base color to red
    target.get_component_by_class(unreal.StaticMeshComponent).set_material(0, mat)
    print(f"Applied red material to {target.get_actor_label()}")
""")
```

Every tool file needs:
- A `register(mcp, send_fn)` function
- One or more `@mcp.tool()` decorated functions inside it
- A docstring on each tool (BobBot reads this to decide when to use the tool)

## Helpers

Import these from `_common` to avoid boilerplate:

| Helper | What it does |
|--------|-------------|
| `_exec_ue(code)` | Run Python code with `import unreal` already done |
| `actor_exec(label, code)` | Find an actor by label, run code with `target` bound to it. Prints error if not found. |
| `asset_exec(path, code)` | Load an asset by path, run code with `asset` bound to it. Prints error if not found. |
| `_exec(code)` | Raw execution, no imports. Use `_exec_ue` instead in most cases. |

Inside `actor_exec` and `asset_exec`, `unreal` is already imported. Use `target` (for actors) or `asset` (for assets) directly.

## Permission classification

BobBot auto-classifies your tools based on their function name:

| Prefix | Category | Requires approval in Ask mode? |
|--------|----------|-------------------------------|
| `get_*`, `search_*`, `is_*`, `list_*` | Read-only | No (auto-approved by default) |
| `capture_*`, camera tools | Viewport | No (auto-approved by default) |
| `spawn_*`, `create_*`, `add_*` | Create | Yes |
| `set_*`, `delete_*`, `remove_*` | Modify | Yes |
| `execute_*`, `run_*` | Code execution | Yes |

Name your tools with these prefixes so they get the right permission category automatically.

## Testing

Use `/test` in the chat to run smoke tests. Your custom tools aren't included in the built-in test suite, but you can test them by asking BobBot to use them in conversation.

## Tips

- Tools are picked up on the next bridge connection. If BobBot is already running, restart the bridge from Connect > Bridge > Restart.
- Keep tool docstrings descriptive. BobBot uses them to decide which tool fits the user's request.
- One file can contain multiple tools. Group related tools together.
- Print your results. BobBot reads stdout to know what happened.
