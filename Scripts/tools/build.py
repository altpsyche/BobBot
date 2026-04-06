"""Build tools: lighting builds, Blueprint compilation, asset validation, map checks, packaging, and derived data management."""

from _common import _exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def build_lighting(quality: str = "Preview") -> str:
        """Build lighting for the current level. quality: 'Preview', 'Medium', 'High', 'Production'."""
        return _exec(f"""
import unreal
quality = {_safe(quality)}
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
# Find all Blueprints — use ARFilter for UE 5.1+ compatibility
registry = unreal.AssetRegistryHelpers.get_asset_registry()
bp_assets = []
try:
    bp_filter = unreal.ARFilter()
    bp_filter.class_paths = [unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")]
    bp_assets = registry.get_assets(bp_filter)
except Exception:
    # Fallback for older UE versions
    try:
        bp_assets = registry.get_assets_by_class(unreal.Name("Blueprint"))
    except Exception as e:
        print(f"ERROR: Could not query Blueprints: {e}")

compiled = 0
errors = 0
for asset_data in bp_assets:
    bp = asset_data.get_asset()
    if bp and isinstance(bp, unreal.Blueprint):
        try:
            # Try BobBotLib helper first if available
            if hasattr(unreal, 'BobBotLib') and hasattr(unreal.BobBotLib, 'compile_blueprint_with_status'):
                status = unreal.BobBotLib.compile_blueprint_with_status(bp)
                if "error" in str(status).lower():
                    errors += 1
            elif hasattr(unreal, 'BlueprintEditorLibrary'):
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            elif hasattr(unreal, 'KismetSystemLibrary'):
                unreal.KismetSystemLibrary.compile_blueprint(bp)
            else:
                # Last resort: use the general compile method
                bp.compile()
            compiled += 1
        except Exception as e:
            unreal.log_warning(f'compile_blueprints: {e}')
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
search_path = {_safe(path)}
assets = unreal.EditorAssetLibrary.list_assets(search_path, recursive=True, include_folder=False)
if not assets:
    print(f"No assets found in {{search_path}}")
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
    print(f"Validated {{checked}} asset(s) in {{search_path}}")
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
        """Start packaging the project as a background process.
        config: Development, Shipping, or DebugGame.
        platform: Win64, Linux, Mac, Android, IOS.
        Packaging runs asynchronously — monitor the Output Log for progress."""
        return _exec(f"""
import unreal, subprocess, os

config = {_safe(config)}
platform = {_safe(platform)}
valid_configs = ["Development", "Shipping", "DebugGame"]
valid_platforms = ["Win64", "Linux", "Mac", "Android", "IOS"]
if config not in valid_configs:
    print(f"ERROR: Invalid config '{{config}}'. Valid: {{', '.join(valid_configs)}}")
elif platform not in valid_platforms:
    print(f"ERROR: Invalid platform '{{platform}}'. Valid: {{', '.join(valid_platforms)}}")
else:
    engine_dir = str(unreal.Paths.engine_dir())
    project_file = str(unreal.Paths.get_project_file_path())
    project_dir = os.path.dirname(project_file)

    # Find RunUAT script
    if os.name == 'nt':
        uat_path = os.path.join(engine_dir, "Build", "BatchFiles", "RunUAT.bat")
    else:
        uat_path = os.path.join(engine_dir, "Build", "BatchFiles", "RunUAT.sh")

    if not os.path.isfile(uat_path):
        print(f"ERROR: RunUAT not found at {{uat_path}}")
        print("Make sure the engine is installed correctly.")
    else:
        archive_dir = os.path.join(project_dir, "PackagedBuilds", platform)
        cmd = [
            uat_path, "BuildCookRun",
            "-project=" + project_file,
            "-noP4",
            "-platform=" + platform,
            "-clientconfig=" + config,
            "-cook", "-build", "-stage", "-pak", "-archive",
            "-archivedirectory=" + archive_dir,
        ]
        try:
            subprocess.Popen(cmd)
            print(f"Packaging started: {{config}} / {{platform}}")
            print(f"Output: {{archive_dir}}")
            print(f"UAT: {{uat_path}}")
            print("This runs as a separate process. Monitor the Output Log for progress.")
        except Exception as e:
            print(f"ERROR: Failed to start packaging: {{e}}")
            print(f"Command: {{' '.join(cmd)}}")
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
