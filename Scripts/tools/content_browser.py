"""Content browser tools: create/delete folders, find unused assets, asset size reports."""

from _common import _exec_ue, _safe
from _registry import bob_tool


def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Content Browser", output_kind="small", default_timeout=60)
    def create_content_folder(path: str) -> str:
        """Create a folder in the Content Browser. path is a /Game/... style path, e.g. '/Game/Materials/Debug'."""
        clean_path = path.replace("\\", "/")
        return _exec_ue(f"""
folder_path = {_safe(clean_path)}
result = unreal.EditorAssetLibrary.make_directory(folder_path)
if result:
    print(f"Created folder: {{folder_path}}")
else:
    # It may already exist
    if unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        print(f"Folder already exists: {{folder_path}}")
    else:
        print(f"ERROR: Failed to create folder '{{folder_path}}'")
""")


    @mcp.tool()
    @bob_tool(category="Content Browser", output_kind="small", default_timeout=60)
    def delete_content_folder(path: str) -> str:
        """Delete an empty folder from the Content Browser. path is a /Game/... style path. The folder must be empty."""
        clean_path = path.replace("\\", "/")
        return _exec_ue(f"""
import os
folder_path = {_safe(clean_path)}

if not unreal.EditorAssetLibrary.does_directory_exist(folder_path):
    print(f"ERROR: Folder '{{folder_path}}' does not exist")
else:
    # Check if folder has assets
    assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True, include_folder=False)
    if assets:
        print(f"ERROR: Folder '{{folder_path}}' is not empty ({{len(assets)}} asset(s)). Delete or move assets first.")
    else:
        # Use EditorAssetLibrary or fall back to os.rmdir on the disk path
        project_content = str(unreal.Paths.project_content_dir())
        # Convert /Game/... to disk path
        rel = folder_path.replace("/Game/", "", 1).replace("/Game", "")
        disk_path = os.path.join(project_content, rel.replace("/", os.sep))
        if os.path.isdir(disk_path):
            try:
                os.rmdir(disk_path)
                print(f"Deleted folder: {{folder_path}}")
            except OSError as e:
                print(f"ERROR: Could not delete folder on disk: {{e}}")
        else:
            print(f"ERROR: Disk path not found for '{{folder_path}}'")
""")


    @mcp.tool()
    @bob_tool(category="Content Browser", output_kind="large", default_timeout=180)
    def find_unused_assets(path: str = "/Game") -> str:
        """Find assets with zero referencers (potentially unused). Scans up to 500 assets to prevent timeout.
        Returns asset paths that have no other assets referencing them."""
        clean_path = path.replace("\\", "/")
        return _exec_ue(f"""
search_path = {_safe(clean_path)}
registry = unreal.AssetRegistryHelpers.get_asset_registry()

asset_list = registry.get_assets_by_path(search_path, recursive=True)
if not asset_list:
    print(f"No assets found under '{{search_path}}'")
else:
    total = len(asset_list)
    limit = 500
    scanned = min(total, limit)
    unused = []

    for i, asset_data in enumerate(asset_list[:limit]):
        pkg_name = str(asset_data.package_name)
        referencers = registry.get_referencers(pkg_name)
        # Filter out self-references
        real_refs = [r for r in referencers if str(r) != pkg_name]
        if not real_refs:
            asset_name = str(asset_data.asset_name)
            asset_class = str(asset_data.asset_class_path.asset_name) if hasattr(asset_data, 'asset_class_path') else str(getattr(asset_data, 'asset_class', 'Unknown'))
            unused.append(f"  {{pkg_name}} ({{asset_class}})")

    if unused:
        print(f"Potentially unused assets ({{len(unused)}} of {{scanned}} scanned, {{total}} total):")
        for line in unused:
            print(line)
    else:
        print(f"No unused assets found (scanned {{scanned}} of {{total}} total)")
    if total > limit:
        print()
        print(f"Note: Only scanned first {{limit}} of {{total}} assets. Narrow the path for a complete scan.")
""")


    @mcp.tool()
    @bob_tool(category="Content Browser", output_kind="large", default_timeout=180)
    def get_asset_size_report(path: str = "/Game") -> str:
        """Get a size report of assets grouped by class. Shows total size per asset type and top largest assets.
        Scans up to 500 assets to prevent timeout."""
        clean_path = path.replace("\\", "/")
        return _exec_ue(f"""
import os

search_path = {_safe(clean_path)}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
asset_list = registry.get_assets_by_path(search_path, recursive=True)

if not asset_list:
    print(f"No assets found under '{{search_path}}'")
else:
    total_assets = len(asset_list)
    limit = 500
    project_content = str(unreal.Paths.project_content_dir())

    class_sizes = {{}}  # class_name -> total bytes
    class_counts = {{}}  # class_name -> count
    all_files = []  # (size, path, class)

    for asset_data in asset_list[:limit]:
        pkg_name = str(asset_data.package_name)
        asset_class = str(asset_data.asset_class_path.asset_name) if hasattr(asset_data, 'asset_class_path') else str(getattr(asset_data, 'asset_class', 'Unknown'))

        # Convert package path to disk path
        rel = pkg_name.replace("/Game/", "", 1).replace("/Game", "")
        disk_path = os.path.join(project_content, rel.replace("/", os.sep) + ".uasset")

        file_size = 0
        if os.path.isfile(disk_path):
            file_size = os.path.getsize(disk_path)
            # Also check .uexp (bulk data) alongside
            uexp_path = disk_path.replace(".uasset", ".uexp")
            if os.path.isfile(uexp_path):
                file_size += os.path.getsize(uexp_path)

        class_sizes[asset_class] = class_sizes.get(asset_class, 0) + file_size
        class_counts[asset_class] = class_counts.get(asset_class, 0) + 1
        all_files.append((file_size, pkg_name, asset_class))

    def fmt(b):
        if b >= 1024 * 1024:
            return f"{{b / (1024*1024):.1f}} MB"
        elif b >= 1024:
            return f"{{b / 1024:.1f}} KB"
        return f"{{b}} B"

    # Sort classes by total size descending
    sorted_classes = sorted(class_sizes.items(), key=lambda x: x[1], reverse=True)
    grand_total = sum(class_sizes.values())

    print(f"Asset Size Report for '{{search_path}}'")
    print(f"Scanned {{min(total_assets, limit)}} of {{total_assets}} assets")
    print(f"Total size: {{fmt(grand_total)}}")
    print()

    print("By asset type:")
    for cls, size in sorted_classes:
        count = class_counts[cls]
        print(f"  {{cls}}: {{fmt(size)}} ({{count}} asset(s))")

    # Top 10 largest individual assets
    all_files.sort(key=lambda x: x[0], reverse=True)
    top = all_files[:10]
    if top:
        print()
        print(f"Top {{len(top)}} largest assets:")
        for size, pkg, cls in top:
            print(f"  {{fmt(size)}} - {{pkg}} ({{cls}})")
""")
