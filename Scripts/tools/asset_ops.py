"""Asset operations: rename, duplicate, delete, move assets and query references."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def rename_asset(old_path: str, new_path: str) -> str:
        """Rename or move an asset. old_path and new_path are full asset paths like '/Game/Materials/M_Old'."""
        return _exec(f"""
import unreal
old_path = "{old_path}"
new_path = "{new_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(old_path):
    print(f"ERROR: Asset '{{old_path}}' not found")
else:
    if unreal.EditorAssetLibrary.rename_asset(old_path, new_path):
        print(f"Renamed: {{old_path}} -> {{new_path}}")
    else:
        print(f"ERROR: Failed to rename '{{old_path}}' to '{{new_path}}'")
""")

    @mcp.tool()
    def duplicate_asset(source_path: str, dest_path: str) -> str:
        """Duplicate an asset. source_path is the original, dest_path is the new copy path."""
        return _exec(f"""
import unreal
source = "{source_path}"
dest = "{dest_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(source):
    print(f"ERROR: Asset '{{source}}' not found")
else:
    result = unreal.EditorAssetLibrary.duplicate_asset(source, dest)
    if result:
        print(f"Duplicated: {{source}} -> {{dest}}")
    else:
        print(f"ERROR: Failed to duplicate '{{source}}' to '{{dest}}'")
""")

    @mcp.tool()
    def delete_asset(asset_path: str) -> str:
        """Delete an asset from the Content Browser."""
        return _exec(f"""
import unreal
path = "{asset_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    if unreal.EditorAssetLibrary.delete_asset(path):
        print(f"Deleted: {{path}}")
    else:
        print(f"ERROR: Failed to delete '{{path}}'")
""")

    @mcp.tool()
    def move_asset(source_path: str, dest_folder: str) -> str:
        """Move an asset to a different folder. dest_folder is like '/Game/NewFolder'."""
        return _exec(f"""
import unreal, posixpath
source = "{source_path}"
dest_folder = "{dest_folder}"
if not unreal.EditorAssetLibrary.does_asset_exist(source):
    print(f"ERROR: Asset '{{source}}' not found")
else:
    asset_name = source.rsplit("/", 1)[-1]
    dest = dest_folder.rstrip("/") + "/" + asset_name
    if unreal.EditorAssetLibrary.rename_asset(source, dest):
        print(f"Moved: {{source}} -> {{dest}}")
    else:
        print(f"ERROR: Failed to move '{{source}}' to '{{dest}}'")
""")

    @mcp.tool()
    def get_asset_references(asset_path: str) -> str:
        """Get all assets that reference this asset (what depends on it)."""
        return _exec(f"""
import unreal
path = "{asset_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    # Get package name from asset path
    package_name = path.rsplit(".", 1)[0] if "." in path else path
    deps = registry.get_referencers(package_name)
    if deps:
        print(f"Assets referencing {{path}} ({{len(deps)}}):")
        for d in sorted(deps):
            print(f"  {{d}}")
    else:
        print(f"No assets reference {{path}}")
""")

    @mcp.tool()
    def get_asset_dependencies(asset_path: str) -> str:
        """Get all assets that this asset depends on."""
        return _exec(f"""
import unreal
path = "{asset_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    package_name = path.rsplit(".", 1)[0] if "." in path else path
    deps = registry.get_dependencies(package_name)
    if deps:
        print(f"Dependencies of {{path}} ({{len(deps)}}):")
        for d in sorted(deps):
            print(f"  {{d}}")
    else:
        print(f"{{path}} has no dependencies")
""")
