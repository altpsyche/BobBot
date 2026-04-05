"""Asset tools: search, inspect, and create assets in the Content Browser."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def search_assets(path: str = "/Game", name_filter: str = "",
                      type_filter: str = "", recursive: bool = True) -> str:
        """Search for assets by path, name, and type. Returns asset paths and classes.
        Examples: search_assets("/Game/Materials"), search_assets(name_filter="Player"), search_assets(type_filter="Blueprint")."""
        return _exec(f"""
import unreal
assets = unreal.EditorAssetLibrary.list_assets("{path}", recursive={recursive}, include_folder=False)
name_filter = "{name_filter}".lower()
type_filter = "{type_filter}".lower()
count = 0
for asset_path in sorted(assets):
    if name_filter and name_filter not in asset_path.lower():
        continue
    data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    cls = str(data.asset_class) if data else "Unknown"
    if type_filter and type_filter not in cls.lower():
        continue
    print(f"{{asset_path}}  ({{cls}})")
    count += 1
    if count >= 100:
        print("... (truncated at 100 results)")
        break
print()
print(f"Found {{count}} asset(s)")
""")

    @mcp.tool()
    def get_asset_info(asset_path: str) -> str:
        """Get detailed info about an asset: class, package, and whether it exists on disk."""
        return _exec(f"""
import unreal
asset_path = "{asset_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    print(f"Asset '{{asset_path}}' not found")
else:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    print(f"Path: {{asset_path}}")
    print(f"Class: {{asset.get_class().get_name()}}")
    print(f"Package: {{data.package_name}}")
    if hasattr(asset, 'get_num_expressions'):
        print(f"Material expressions: {{asset.get_num_expressions()}}")
""")

    @mcp.tool()
    def create_blueprint(name: str, parent_class: str = "Actor",
                         path: str = "/Game") -> str:
        """Create a new Blueprint asset. parent_class examples: 'Actor', 'Pawn', 'Character', 'GameModeBase', 'PlayerController'."""
        return _exec(f"""
import unreal
name = "{name}"
path = "{path}"
parent = "{parent_class}"
# Try getattr(unreal, Name) for C++ classes, then load_class for full paths
parent_class = getattr(unreal, parent, None)
if parent_class is None and parent.startswith("/"):
    parent_class = unreal.load_class(None, parent)
if parent_class is None:
    print(f"ERROR: Parent class '{{parent}}' not found. Use class names like 'Actor', 'Pawn', 'Character' or full paths like '/Script/Engine.Actor'")
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = asset_tools.create_asset(name, path, unreal.Blueprint, factory)
    if bp:
        full_path = f"{{path}}/{{name}}"
        print(f"Created Blueprint: {{full_path}} (parent: {{parent_class.get_name()}})")
    else:
        print(f"ERROR: Failed to create Blueprint '{{name}}' at '{{path}}'")
""")

    @mcp.tool()
    def create_material(name: str, path: str = "/Game/Materials") -> str:
        """Create a new empty Material asset."""
        return _exec(f"""
import unreal
name = "{name}"
path = "{path}"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = asset_tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
if mat:
    full_path = f"{{path}}/{{name}}"
    print(f"Created Material: {{full_path}}")
else:
    print(f"ERROR: Failed to create Material '{{name}}' at '{{path}}'")
""")
