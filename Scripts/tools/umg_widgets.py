"""UMG Widget tools: create Widget Blueprints, inspect hierarchies, add widget components."""

from _common import _exec_ue, actor_exec, asset_exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def create_widget_blueprint(name: str, parent_class: str = "UserWidget",
                                path: str = "/Game/UI") -> str:
        """Create a Widget Blueprint. parent_class: 'UserWidget', 'WidgetBlueprint', etc."""
        return _exec_ue(f"""
name = {_safe(name)}
path = {_safe(path)}
parent = {_safe(parent_class)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.WidgetBlueprintFactory()
    parent_cls = getattr(unreal, parent, None)
    if parent_cls:
        factory.set_editor_property("ParentClass", parent_cls)
    wb = asset_tools.create_asset(name, path, unreal.WidgetBlueprint, factory)
    if wb:
        unreal.EditorAssetLibrary.save_asset(f"{{path}}/{{name}}")
        print(f"Created Widget Blueprint: {{path}}/{{name}} (parent: {{parent}})")
    else:
        print(f"ERROR: Failed to create Widget Blueprint")
except Exception as e:
    print(f"ERROR: {{e}}")
""")

    @mcp.tool()
    def get_widget_tree(widget_path: str) -> str:
        """List the widget hierarchy of a Widget Blueprint."""
        return asset_exec(widget_path, f"""
widget_path = {_safe(widget_path)}
if not isinstance(asset, unreal.WidgetBlueprint):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a WidgetBlueprint")
else:
    print(f"Widget Blueprint: {{widget_path}}")
    tree = asset.get_editor_property("WidgetTree")
    if tree:
        root = tree.get_editor_property("RootWidget")
        if root:
            def print_widget(widget, indent=0):
                prefix = "  " * indent
                print(f"{{prefix}}{{widget.get_name()}} ({{widget.get_class().get_name()}})")
                # Try to get children
                if hasattr(widget, 'get_all_children'):
                    children = widget.get_all_children()
                    for child in children:
                        print_widget(child, indent + 1)
            print("\\nWidget Tree:")
            print_widget(root)
        else:
            print("\\nWidget tree has no root widget")
    else:
        print("\\nNo widget tree")
""")

    @mcp.tool()
    def create_widget_component(actor_label: str, widget_path: str) -> str:
        """Add a WidgetComponent to an actor that displays a Widget Blueprint."""
        return actor_exec(actor_label, f"""
widget_path = {_safe(widget_path)}
wb = unreal.EditorAssetLibrary.load_asset(widget_path)
if wb is None:
    print(f"ERROR: Widget Blueprint '{{widget_path}}' not found")
else:
    wc = target.add_component_by_class(unreal.WidgetComponent, False, unreal.Transform(), False)
    if wc:
        wc.set_editor_property("WidgetClass", wb.generated_class())
        print(f"Added WidgetComponent to {{target.get_actor_label()}} displaying {{widget_path}}")
    else:
        print("ERROR: Failed to add WidgetComponent")
""")

    @mcp.tool()
    def get_all_widget_blueprints(path: str = "/Game/UI") -> str:
        """List all Widget Blueprints in a path."""
        return _exec_ue(f"""
path = {_safe(path)}
assets = unreal.EditorAssetLibrary.list_assets(path, recursive=True, include_folder=False)
widgets = []
for asset_path in sorted(assets):
    data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    if data:
        cls = str(data.asset_class)
        if "WidgetBlueprint" in cls:
            widgets.append(asset_path)
if widgets:
    print(f"Widget Blueprints in {{path}} ({{len(widgets)}}):")
    for w in widgets:
        print(f"  {{w}}")
else:
    print(f"No Widget Blueprints found in {{path}}")
""")
