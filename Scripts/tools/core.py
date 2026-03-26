"""
Core MCP tools: execute_unreal_python and ping_unreal.

To add new tools, create a new file in this directory with a
`register(mcp, send_fn)` function.
"""


def register(mcp, send_fn):
    """Register core tools with the MCP server."""

    @mcp.tool()
    def execute_unreal_python(code: str) -> str:
        """Execute Python code inside Unreal Engine 5 editor.

        Has access to the `unreal` module and the full UE Python API.
        Variables persist across calls (isolated per connection).

        Examples:
            # List all actors
            execute_unreal_python("for a in unreal.EditorLevelLibrary.get_all_level_actors(): print(a.get_actor_label())")

            # Create a material
            execute_unreal_python("mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Test', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())")

            # Add variable to a Blueprint (requires BobBot C++ plugin)
            execute_unreal_python("unreal.BobBotLib.add_blueprint_variable(bp, 'Health', 'float', True)")
        """
        result = send_fn({"type": "execute", "code": code})

        if result.get("success"):
            output = result.get("output", "")
            err = result.get("error")
            if err:
                output += "\nStderr: " + err
            return output if output.strip() else "(executed successfully, no output)"
        else:
            return "Error:\n" + result.get("error", "Unknown error")

    @mcp.tool()
    def ping_unreal() -> str:
        """Check if Unreal Engine editor is connected and responding."""
        result = send_fn({"type": "ping"})
        if result.get("success"):
            return "Connected to Unreal Engine editor"
        else:
            return "Not connected: " + result.get("error", "Unknown error")
