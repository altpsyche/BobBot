"""Source control tools: check status, checkout, checkin, and revert assets."""

from _common import _exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Source Control", output_kind="large", default_timeout=60)
    def get_source_control_status(asset_path: str) -> str:
        """Check the source control status of an asset (checked out, up to date, etc.)."""
        return _exec(f"""
import unreal
path = {_safe(asset_path)}
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    try:
        sc = unreal.SourceControl
        if not sc.is_enabled():
            print("Source control is not enabled in this project")
        else:
            provider = sc.get_provider()
            print(f"Source control provider: {{provider.get_name() if provider else 'Unknown'}}")
            state = sc.query_file_state(path)
            print(f"Asset: {{path}}")
            print(f"Status: {{state}}")
    except Exception as e:
        print(f"ERROR: Source control query failed: {{e}}")
        print("Ensure source control is configured in Edit > Project Settings > Source Control")
""")

    @mcp.tool()
    @bob_tool(category="Source Control", output_kind="small", default_timeout=60)
    def check_out_asset(asset_path: str) -> str:
        """Check out an asset for editing in source control."""
        return _exec(f"""
import unreal
path = {_safe(asset_path)}
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    try:
        result = unreal.EditorAssetLibrary.checkout_asset(path)
        if result:
            print(f"Checked out: {{path}}")
        else:
            print(f"ERROR: Failed to check out '{{path}}'")
    except Exception as e:
        print(f"ERROR: Checkout failed: {{e}}")
""")

    @mcp.tool()
    @bob_tool(category="Source Control", output_kind="small", default_timeout=60)
    def check_in_asset(asset_path: str, description: str = "") -> str:
        """Check in an asset with a changelist description."""
        return _exec(f"""
import unreal
path = {_safe(asset_path)}
description = {_safe(description)}
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    try:
        # Save first
        unreal.EditorAssetLibrary.save_asset(path)
        result = unreal.EditorAssetLibrary.checkin_asset(path, description)
        if result:
            print(f"Checked in: {{path}}")
            if description:
                print(f"Description: {{description}}")
        else:
            print(f"ERROR: Failed to check in '{{path}}'")
    except Exception as e:
        print(f"ERROR: Checkin failed: {{e}}")
""")

    @mcp.tool()
    @bob_tool(category="Source Control", output_kind="small", default_timeout=60)
    def revert_asset(asset_path: str) -> str:
        """Revert an asset to the depot/repository version."""
        return _exec(f"""
import unreal
path = {_safe(asset_path)}
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    print(f"ERROR: Asset '{{path}}' not found")
else:
    try:
        result = unreal.EditorAssetLibrary.revert_asset(path)
        if result:
            print(f"Reverted: {{path}}")
        else:
            print(f"ERROR: Failed to revert '{{path}}'")
    except Exception as e:
        print(f"ERROR: Revert failed: {{e}}")
""")
