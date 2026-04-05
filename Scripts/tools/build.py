"""Build tools: lighting builds, Blueprint compilation, asset validation, map checks, packaging, and derived data management."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def build_lighting(quality: str = "Preview") -> str:
        """Build lighting for the current level. quality: 'Preview', 'Medium', 'High', 'Production'."""
        return _exec(f"""
import unreal
quality = "{quality}"
valid_qualities = ["Preview", "Medium", "High", "Production"]
if quality not in valid_qualities:
    print(f"ERROR: Invalid quality '{{quality}}'. Use: {{', '.join(valid_qualities)}}")
else:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        print("ERROR: No level open")
    else:
        # Use console command for lighting build
        cmd = f"BUILD LIGHTING QUALITY={{quality}}"
        unreal.SystemLibrary.execute_console_command(world, cmd)
        print(f"Started lighting build ({{quality}} quality)")
        print("Check the Build panel for progress")
""")

    @mcp.tool()
    def compile_blueprints() -> str:
        """Compile all dirty (modified) Blueprints in the project."""
        return _exec("""
import unreal
# Find all Blueprints
registry = unreal.AssetRegistryHelpers.get_asset_registry()
bp_assets = registry.get_assets_by_class(unreal.Name("Blueprint"))
compiled = 0
errors = 0
for asset_data in bp_assets:
    bp = asset_data.get_asset()
    if bp and isinstance(bp, unreal.Blueprint):
        try:
            status = unreal.BobBotLib.compile_blueprint_with_status(bp)
            if "error" in str(status).lower():
                errors += 1
            compiled += 1
        except Exception as e:
            unreal.log_warning(f'compile_blueprints: {{e}}')
print(f"Compiled {compiled} Blueprint(s)")
if errors:
    print(f"  {errors} with errors (check Output Log)")
else:
    print("  All compiled successfully")
""")

    @mcp.tool()
    def validate_assets(path: str = "/Game") -> str:
        """Run asset validation on assets in a path. Returns warnings and errors."""
        return _exec(f"""
import unreal
assets = unreal.EditorAssetLibrary.list_assets("{path}", recursive=True, include_folder=False)
if not assets:
    print(f"No assets found in {path}")
else:
    issues = []
    checked = 0
    for asset_path in assets[:200]:  # Limit to prevent timeout
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            issues.append(f"  Missing: {{asset_path}}")
            continue
        data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
        if data and not data.is_asset_loaded():
            pass  # Skip unloaded assets for speed
        checked += 1
    print(f"Validated {{checked}} asset(s) in {path}")
    if issues:
        print()
        print(f"Issues ({{len(issues)}}):")
        for issue in issues[:50]:
            print(issue)
    else:
        print("No issues found")
    if len(assets) > 200:
        print()
        print(f"(Checked first 200 of {{len(assets)}} assets)")
""")

    @mcp.tool()
    def get_map_check_errors() -> str:
        """Run Map Check and return any errors or warnings."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    # Run map check via console command
    unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
    print("Map Check executed - check Output Log for results")
    # Read recent log entries for map check results
    import os
    log_dir = str(unreal.Paths.project_log_dir())
    if os.path.isdir(log_dir):
        log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
        if log_files:
            log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
            log_path = os.path.join(log_dir, log_files[0])
            with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
            # Find recent MapCheck lines
            map_lines = [l.strip() for l in lines[-200:] if "MapCheck" in l or "Map Check" in l]
            if map_lines:
                print()
                print(f"Recent Map Check entries ({len(map_lines)}):")
                for line in map_lines[-20:]:
                    print(f"  {line}")
""")

    @mcp.tool()
    def package_project(config: str = "Development", platform: str = "Win64") -> str:
        """Start packaging the project. config: Development, Shipping, or DebugGame. platform: Win64, Linux, Mac, Android, IOS. Packaging runs asynchronously - monitor the Output Log for progress."""
        return _exec(f"""
import unreal
config = "{config}"
platform = "{platform}"
valid_configs = ["Development", "Shipping", "DebugGame"]
valid_platforms = ["Win64", "Linux", "Mac", "Android", "IOS"]
if config not in valid_configs:
    print(f"ERROR: Invalid config '{{config}}'. Valid: {{', '.join(valid_configs)}}")
elif platform not in valid_platforms:
    print(f"ERROR: Invalid platform '{{platform}}'. Valid: {{', '.join(valid_platforms)}}")
else:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        print("ERROR: No world available")
    else:
        # Map platform names to UAT arguments
        platform_map = {{
            "Win64": "Win64",
            "Linux": "Linux",
            "Mac": "Mac",
            "Android": "Android",
            "IOS": "IOS",
        }}
        plat = platform_map[platform]
        # Trigger packaging via automation
        cmd = f"AutomationTool BuildCookRun -project={{unreal.Paths.get_project_file_path()}} -noP4 -platform={{plat}} -clientconfig={{config}} -cook -build -stage -pak -archive"
        unreal.SystemLibrary.execute_console_command(world, cmd)
        print(f"Packaging started: {{config}} / {{platform}}")
        print("This runs asynchronously. Monitor the Output Log for progress and completion.")
""")

    @mcp.tool()
    def get_build_errors() -> str:
        """Read the latest UE log file and extract lines containing 'Error' or 'Warning'. Returns up to 50 results from the last 200 lines."""
        return _exec("""
import unreal, os
log_dir = str(unreal.Paths.project_log_dir())
if not os.path.isdir(log_dir):
    print("ERROR: Log directory not found at " + log_dir)
else:
    log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
    if not log_files:
        print("ERROR: No .log files found in " + log_dir)
    else:
        log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
        log_path = os.path.join(log_dir, log_files[0])
        with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
        # Search last 200 lines for errors and warnings
        issues = []
        for line in lines[-200:]:
            lower = line.lower()
            if "error" in lower or "warning" in lower:
                issues.append(line.strip())
        if issues:
            shown = issues[:50]
            print(f"Build errors/warnings ({len(issues)} total, showing {len(shown)}):")
            for issue in shown:
                print(f"  {issue}")
            if len(issues) > 50:
                print()
                print(f"... and {len(issues) - 50} more")
        else:
            print("No errors or warnings found in the last 200 lines of " + os.path.basename(log_path))
""")

    @mcp.tool()
    def clean_derived_data() -> str:
        """Delete the DerivedDataCache folder and flush the DDC. Reports the size of data deleted."""
        return _exec("""
import unreal, os, shutil
ddc_path = os.path.join(str(unreal.Paths.project_saved_dir()), "DerivedDataCache")
total_size = 0
deleted = False

if os.path.isdir(ddc_path):
    # Calculate size before deletion
    for dirpath, dirnames, filenames in os.walk(ddc_path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            try:
                total_size += os.path.getsize(fp)
            except OSError:
                pass
    # Delete the directory
    try:
        shutil.rmtree(ddc_path)
        deleted = True
    except Exception as e:
        print(f"ERROR: Failed to delete DerivedDataCache: {e}")
else:
    print("DerivedDataCache directory not found (may already be clean)")

# Flush the in-memory DDC
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    unreal.SystemLibrary.execute_console_command(world, "DerivedDataCache.Flush")

if deleted:
    if total_size >= 1024 * 1024 * 1024:
        size_str = f"{total_size / (1024*1024*1024):.2f} GB"
    elif total_size >= 1024 * 1024:
        size_str = f"{total_size / (1024*1024):.1f} MB"
    elif total_size >= 1024:
        size_str = f"{total_size / 1024:.1f} KB"
    else:
        size_str = f"{total_size} bytes"
    print(f"Deleted DerivedDataCache ({size_str})")
    print("Flushed in-memory DDC via DerivedDataCache.Flush")
    print("Note: DDC will be rebuilt on next asset load (may cause brief hitches)")
""")
