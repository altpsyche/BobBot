"""Advanced Blueprint tools: functions, events, components, interfaces, and reparenting."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_blueprint_function(blueprint_path: str, function_name: str,
                                  inputs: str = "", outputs: str = "") -> str:
        """Add a custom function to a Blueprint. inputs/outputs are comma-separated 'name:type' pairs like 'Health:float,Name:FString'. Types: float, int32, bool, FString, FVector, etc."""
        return _exec(f"""
import unreal
bp = unreal.EditorAssetLibrary.load_asset("{blueprint_path}")
if bp is None:
    print("ERROR: Blueprint '{blueprint_path}' not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    created = False
    # Try modern API first (UE 5.5+)
    if hasattr(unreal, 'BlueprintEditorLibrary'):
        try:
            graph = unreal.BlueprintEditorLibrary.create_function_graph(bp, "{function_name}")
            if graph:
                created = True
        except:
            pass
    # Fall back to BobBotLib (works on all UE 5.4+)
    if not created:
        result = unreal.BobBotLib.add_function_graph(bp, "{function_name}")
        if not result.startswith("ERROR"):
            created = True
        else:
            print(result)
    if created:
        unreal.BobBotLib.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset("{blueprint_path}")
        print(f"Created function '{function_name}' on {blueprint_path}")
""")

    @mcp.tool()
    def create_blueprint_event(blueprint_path: str, event_name: str) -> str:
        """Add a custom event to a Blueprint's event graph."""
        return _exec(f"""
import unreal
bp = unreal.EditorAssetLibrary.load_asset("{blueprint_path}")
if bp is None:
    print("ERROR: Blueprint '{blueprint_path}' not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    created = False
    # Try modern API first (UE 5.5+)
    if hasattr(unreal, 'BlueprintEditorLibrary'):
        try:
            graph = unreal.BlueprintEditorLibrary.add_custom_event(bp, "{event_name}")
            if graph:
                created = True
        except:
            pass
    # Fall back to BobBotLib (works on all UE 5.4+)
    if not created:
        result = unreal.BobBotLib.add_custom_event(bp, "{event_name}")
        if not result.startswith("ERROR"):
            created = True
        else:
            print(result)
    if created:
        unreal.BobBotLib.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset("{blueprint_path}")
        print(f"Created custom event '{event_name}' on {blueprint_path}")
""")

    @mcp.tool()
    def get_blueprint_functions(blueprint_path: str) -> str:
        """List all functions in a Blueprint with their inputs and outputs."""
        return _exec(f"""
import unreal
bp = unreal.EditorAssetLibrary.load_asset("{blueprint_path}")
if bp is None:
    print("ERROR: Blueprint '{blueprint_path}' not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    print(f"Blueprint: {blueprint_path}")
    print(f"Parent: {{bp.get_editor_property('ParentClass').get_name()}}")
    func_graphs = bp.get_editor_property("FunctionGraphs")
    if func_graphs:
        print(f"\\nFunctions ({{len(func_graphs)}}):")
        for fg in func_graphs:
            fname = fg.get_name()
            print(f"  {{fname}}")
    else:
        print("\\nNo custom functions")
    # Also show event graph info
    uber_graphs = bp.get_editor_property("UbergraphPages")
    if uber_graphs:
        print(f"\\nEvent Graphs: {{len(uber_graphs)}}")
""")

    @mcp.tool()
    def get_blueprint_components(blueprint_path: str) -> str:
        """List all components defined in a Blueprint's construction script."""
        return _exec(f"""
import unreal
bp = unreal.EditorAssetLibrary.load_asset("{blueprint_path}")
if bp is None:
    print("ERROR: Blueprint '{blueprint_path}' not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    print(f"Blueprint: {blueprint_path}")
    scs = bp.get_editor_property("SimpleConstructionScript")
    if scs:
        nodes = scs.get_all_nodes()
        if nodes:
            print(f"\\nComponents ({{len(nodes)}}):")
            for node in nodes:
                comp_template = node.get_editor_property("ComponentTemplate")
                if comp_template:
                    print(f"  {{comp_template.get_name()}} ({{comp_template.get_class().get_name()}})")
        else:
            print("\\nNo components")
    else:
        print("\\nNo SimpleConstructionScript")
    # Variables for reference
    try:
        var_names = unreal.BobBotLib.get_blueprint_variable_names(bp)
        if var_names:
            print(f"\\nVariables ({{len(var_names)}}):")
            for v in var_names:
                print(f"  {{v}}")
    except:
        pass
""")

    @mcp.tool()
    def set_blueprint_parent_class(blueprint_path: str, parent_class: str) -> str:
        """Reparent a Blueprint to a different parent class. parent_class examples: 'Actor', 'Pawn', 'Character', 'GameModeBase'."""
        return _exec(f"""
import unreal
bp = unreal.EditorAssetLibrary.load_asset("{blueprint_path}")
if bp is None:
    print("ERROR: Blueprint '{blueprint_path}' not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    parent = "{parent_class}"
    parent_cls = getattr(unreal, parent, None)
    if parent_cls is None and parent.startswith("/"):
        parent_cls = unreal.load_class(None, parent)
    if parent_cls is None:
        print(f"ERROR: Parent class '{{parent}}' not found")
    else:
        old_parent = bp.get_editor_property("ParentClass").get_name()
        bp.set_editor_property("ParentClass", parent_cls)
        unreal.BobBotLib.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset("{blueprint_path}")
        print(f"Reparented {blueprint_path}: {{old_parent}} -> {{parent_cls.get_name()}}")
""")

    @mcp.tool()
    def create_blueprint_interface(name: str, path: str = "/Game/Blueprints",
                                   functions: str = "") -> str:
        """Create a Blueprint Interface. functions is comma-separated function names like 'OnDamaged,GetHealth'."""
        return _exec(f"""
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.BlueprintFactory()
factory.set_editor_property("BlueprintType", unreal.BlueprintType.BPTYPE_INTERFACE)
bpi = asset_tools.create_asset("{name}", "{path}", unreal.Blueprint, factory)
if bpi:
    funcs_str = "{functions}"
    if funcs_str:
        func_names = [f.strip() for f in funcs_str.split(",") if f.strip()]
        for fn in func_names:
            try:
                unreal.BlueprintEditorLibrary.create_function_graph(bpi, fn)
            except:
                pass
        unreal.BobBotLib.compile_blueprint(bpi)
    unreal.EditorAssetLibrary.save_asset("{path}/{name}")
    print(f"Created Blueprint Interface: {path}/{name}")
    if funcs_str:
        print(f"Functions: {functions}")
else:
    print(f"ERROR: Failed to create Blueprint Interface '{name}'")
""")
