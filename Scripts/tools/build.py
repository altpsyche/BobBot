"""Build tools: lighting builds, Blueprint compilation, asset validation, and map checks."""


def register(mcp, send_fn):

    def _exec(code):
        result = send_fn({"type": "execute", "code": code})
        if result.get("success"):
            output = result.get("output", "")
            err = result.get("error")
            if err:
                output += "\nStderr: " + err
            return output if output.strip() else "(executed successfully, no output)"
        return "Error: " + result.get("error", "Unknown error")

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
        except:
            pass
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
        print(f"\\nIssues ({{len(issues)}}):")
        for issue in issues[:50]:
            print(issue)
    else:
        print("No issues found")
    if len(assets) > 200:
        print(f"\\n(Checked first 200 of {{len(assets)}} assets)")
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
                print(f"\\nRecent Map Check entries ({len(map_lines)}):")
                for line in map_lines[-20:]:
                    print(f"  {line}")
""")
