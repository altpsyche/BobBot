"""Blueprint graph reading: list nodes in a graph, inspect a single node in detail.

Both tools delegate to BobBotLib C++ helpers (DescribeBlueprintGraph,
DescribeBlueprintNode) so output is consistent across UE 5.4-5.6 and the
notoriously fragile UE Python EdGraphPin reflection is avoided entirely.
"""

from _common import _exec, _safe
from _registry import bob_tool


def register(mcp, send_fn):

    @mcp.tool()
    @bob_tool(category="Blueprint Graph", output_kind="large", default_timeout=60)
    def get_graph_nodes(blueprint_path: str, graph_name: str = "") -> str:
        """List every node in a Blueprint graph with name, title, position, pins,
        and connections. Pass graph_name='' (default) to dump all event + function
        graphs. Examples of graph_name: 'EventGraph', 'MyFunction'.

        Output is one line per node:
            <unique_name> | <title> | (x,y) | in: <pins> | out: <pins -> targets>

        Use this before modifying a Blueprint to understand existing logic. Capped
        at 80 nodes per call; call get_node_details for any node you need more on."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
graph_name = {_safe(graph_name)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint '" + bp_path + "' not found")
elif not isinstance(bp, unreal.Blueprint):
    print("ERROR: '" + bp_path + "' is not a Blueprint (" + bp.get_class().get_name() + ")")
else:
    print(unreal.BobBotLib.describe_blueprint_graph(bp, unreal.Name(graph_name)))
""")

    @mcp.tool()
    @bob_tool(category="Blueprint Graph", output_kind="large", default_timeout=60)
    def get_node_details(blueprint_path: str, node_name: str) -> str:
        """Inspect one Blueprint node in full: every pin with type, direction,
        default value, and every linked target node + pin.

        node_name accepts either the unique synthesized name (e.g. 'K2Node_CallFunction_3')
        or the editable title (e.g. 'Print String'). On title collisions returns an
        ambiguous-match error listing every candidate's unique name."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
node_name = {_safe(node_name)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint '" + bp_path + "' not found")
elif not isinstance(bp, unreal.Blueprint):
    print("ERROR: '" + bp_path + "' is not a Blueprint (" + bp.get_class().get_name() + ")")
else:
    print(unreal.BobBotLib.describe_blueprint_node(bp, node_name))
""")
