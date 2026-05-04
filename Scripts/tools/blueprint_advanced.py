"""Advanced Blueprint tools: functions, events, components, interfaces, and reparenting."""

from _common import _exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def create_blueprint_function(blueprint_path: str, function_name: str,
                                  inputs: str = "", outputs: str = "") -> str:
        """Add a custom function to a Blueprint. inputs/outputs are comma-separated 'name:type' pairs like 'Health:float,Name:FString'. Types: float, int32, bool, FString, FVector, etc."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
func_name = {_safe(function_name)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint " + bp_path + " not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    created = False
    # Try modern API first (UE 5.5+)
    if hasattr(unreal, 'BlueprintEditorLibrary'):
        try:
            graph = unreal.BlueprintEditorLibrary.create_function_graph(bp, func_name)
            if graph:
                created = True
        except Exception as e:
            unreal.log_warning(f'create_blueprint_function modern API: {{e}}')
    # Fall back to BobBotLib (works on all UE 5.4+)
    if not created:
        result = unreal.BobBotLib.add_function_graph(bp, func_name)
        if not result.startswith("ERROR"):
            created = True
        else:
            print(result)
    if created:
        unreal.BobBotLib.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset(bp_path)
        print(f"Created function '{{func_name}}' on {{bp_path}}")
""")

    @mcp.tool()
    def create_blueprint_event(blueprint_path: str, event_name: str) -> str:
        """Add a custom event to a Blueprint's event graph."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
event_name_local = {_safe(event_name)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint " + bp_path + " not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    created = False
    # Try modern API first (UE 5.5+)
    if hasattr(unreal, 'BlueprintEditorLibrary'):
        try:
            graph = unreal.BlueprintEditorLibrary.add_custom_event(bp, event_name_local)
            if graph:
                created = True
        except Exception as e:
            unreal.log_warning(f'create_blueprint_event modern API: {{e}}')
    # Fall back to BobBotLib (works on all UE 5.4+)
    if not created:
        result = unreal.BobBotLib.add_custom_event(bp, event_name_local)
        if not result.startswith("ERROR"):
            created = True
        else:
            print(result)
    if created:
        unreal.BobBotLib.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset(bp_path)
        print(f"Created custom event '{{event_name_local}}' on {{bp_path}}")
""")

    @mcp.tool()
    def get_blueprint_functions(blueprint_path: str) -> str:
        """List all functions in a Blueprint with their inputs and outputs."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint " + bp_path + " not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    print(f"Blueprint: {{bp_path}}")
    try:
        print(f"Parent: {{bp.get_editor_property('ParentClass').get_name()}}")
    except Exception:
        pass
    # UBlueprint::FunctionGraphs / UbergraphPages are protected on UE 5.x
    # Python reflection. Route through the typed C++ getters.
    func_graphs = unreal.BobBotLib.get_blueprint_function_graphs_arr(bp)
    if func_graphs:
        print()
        print(f"Functions ({{len(func_graphs)}}):")
        for fg in func_graphs:
            print(f"  {{fg.get_name()}}")
    else:
        print("\\nNo custom functions")
    uber_graphs = unreal.BobBotLib.get_blueprint_ubergraph_pages_arr(bp)
    if uber_graphs:
        print()
        print(f"Event Graphs: {{len(uber_graphs)}}")
""")

    @mcp.tool()
    def get_blueprint_components(blueprint_path: str) -> str:
        """List all components defined in a Blueprint's construction script."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint " + bp_path + " not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    print(f"Blueprint: {{bp_path}}")
    nodes = unreal.BobBotLib.get_blueprint_scs_nodes(bp)
    if nodes:
        print()
        print(f"Components ({{len(nodes)}}):")
        for node in nodes:
            try:
                comp_template = node.get_editor_property("ComponentTemplate")
            except Exception:
                comp_template = None
            if comp_template:
                print(f"  {{comp_template.get_name()}} ({{comp_template.get_class().get_name()}})")
    else:
        print("\\nNo components")
    # Variables for reference
    try:
        var_names = unreal.BobBotLib.get_blueprint_variable_names(bp)
        if var_names:
            print()
            print(f"Variables ({{len(var_names)}}):")
            for v in var_names:
                print(f"  {{v}}")
    except Exception as e:
        unreal.log_warning(f'get_blueprint_components: {{e}}')
""")

    @mcp.tool()
    def set_blueprint_parent_class(blueprint_path: str, parent_class: str) -> str:
        """Reparent a Blueprint to a different parent class. parent_class examples: 'Actor', 'Pawn', 'Character', 'GameModeBase'."""
        return _exec(f"""
import unreal
bp_path = {_safe(blueprint_path)}
parent = {_safe(parent_class)}
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
if bp is None:
    print("ERROR: Blueprint " + bp_path + " not found")
elif not isinstance(bp, unreal.Blueprint):
    print(f"ERROR: '{{bp.get_class().get_name()}}' is not a Blueprint")
else:
    parent_cls = getattr(unreal, parent, None)
    if parent_cls is None and parent.startswith("/"):
        parent_cls = unreal.load_class(None, parent)
    if parent_cls is None:
        print(f"ERROR: Parent class '{{parent}}' not found")
    else:
        old_parent = bp.get_editor_property("ParentClass").get_name()
        bp.set_editor_property("ParentClass", parent_cls)
        unreal.BobBotLib.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset(bp_path)
        print(f"Reparented {{bp_path}}: {{old_parent}} -> {{parent_cls.get_name()}}")
""")

    @mcp.tool()
    def create_blueprint_interface(name: str, path: str = "/Game/Blueprints",
                                   functions: str = "") -> str:
        """Create a Blueprint Interface. functions is comma-separated function names like 'OnDamaged,GetHealth'."""
        return _exec(f"""
import unreal
name_local = {_safe(name)}
path_local = {_safe(path)}
funcs_str = {_safe(functions)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.BlueprintFactory()
factory.set_editor_property("BlueprintType", unreal.BlueprintType.BPTYPE_INTERFACE)
bpi = asset_tools.create_asset(name_local, path_local, unreal.Blueprint, factory)
if bpi:
    if funcs_str:
        func_names = [f.strip() for f in funcs_str.split(",") if f.strip()]
        for fn in func_names:
            try:
                unreal.BlueprintEditorLibrary.create_function_graph(bpi, fn)
            except Exception as e:
                unreal.log_warning(f'create_blueprint_interface add function: {{e}}')
        unreal.BobBotLib.compile_blueprint(bpi)
    unreal.EditorAssetLibrary.save_asset(f"{{path_local}}/{{name_local}}")
    print(f"Created Blueprint Interface: {{path_local}}/{{name_local}}")
    if funcs_str:
        print(f"Functions: {{funcs_str}}")
else:
    print(f"ERROR: Failed to create Blueprint Interface '{{name_local}}'")
""")
