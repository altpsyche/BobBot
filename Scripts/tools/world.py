"""World settings: inspect and modify world properties, gravity, game mode."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def get_world_settings() -> str:
        """Get world settings: gravity, game mode, kill Z, time dilation, and more."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    ws = world.get_world_settings()
    print("World Settings:")
    print(f"  Global Gravity Z: {ws.get_editor_property('GlobalGravityZ')}")
    print(f"  World Gravity Z: {ws.get_editor_property('WorldGravityZ') if hasattr(ws, 'WorldGravityZ') else 'default'}")
    print(f"  Kill Z: {ws.get_editor_property('KillZ')}")
    gm = ws.get_editor_property("DefaultGameMode")
    print(f"  Default Game Mode: {gm.get_name() if gm else 'None'}")
    print(f"  Global Time Dilation: {ws.get_editor_property('TimeDilation') if hasattr(ws, 'TimeDilation') else 'N/A'}")
    print(f"  Enable World Bounds Checks: {ws.get_editor_property('bEnableWorldBoundsChecks')}")
    print(f"  World to Meters: {ws.get_editor_property('WorldToMeters')}")
""")

    @mcp.tool()
    def set_world_setting(property_name: str, value: str) -> str:
        """Set a world setting by property name. Examples: 'GlobalGravityZ', 'KillZ', 'WorldToMeters', 'bEnableWorldBoundsChecks'."""
        return _exec(f"""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    ws = world.get_world_settings()
    try:
        # Try numeric first
        try:
            val = float("{value}")
            if val == int(val):
                val = int(val)
        except ValueError:
            val = "{value}"
            if val.lower() == "true":
                val = True
            elif val.lower() == "false":
                val = False
        ws.set_editor_property("{property_name}", val)
        print(f"Set {property_name} = {{val}}")
    except Exception as e:
        print(f"ERROR: Could not set '{property_name}': {{e}}")
""")

    @mcp.tool()
    def get_game_mode() -> str:
        """Get the current game mode class and its properties."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    ws = world.get_world_settings()
    gm = ws.get_editor_property("DefaultGameMode")
    if gm is None:
        print("No default game mode set for this level")
        print("Using project default game mode")
    else:
        print(f"Default Game Mode: {gm.get_name()}")
        print(f"Path: {gm.get_path_name()}")
        # Try to get CDO for properties
        try:
            cdo = unreal.get_default_object(gm)
            if cdo:
                print(f"Default Pawn Class: {cdo.get_editor_property('DefaultPawnClass')}")
                print(f"Player Controller Class: {cdo.get_editor_property('PlayerControllerClass')}")
                print(f"HUD Class: {cdo.get_editor_property('HUDClass')}")
        except Exception as e:
            unreal.log_warning(f'get_game_mode CDO: {{e}}')
""")

    @mcp.tool()
    def set_game_mode(game_mode_path: str) -> str:
        """Set the default game mode for the current level. Use a class path like '/Script/Engine.GameModeBase' or a Blueprint path like '/Game/Blueprints/BP_GameMode'."""
        return _exec(f"""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("No level open")
else:
    path = "{game_mode_path}"
    gm_class = unreal.load_class(None, path) if path.startswith("/") else getattr(unreal, path, None)
    if gm_class is None:
        # Try loading as Blueprint
        bp = unreal.load_asset(path)
        if bp and hasattr(bp, 'generated_class'):
            gm_class = bp.generated_class()
    if gm_class is None:
        print(f"ERROR: Game mode class '{{path}}' not found")
    else:
        ws = world.get_world_settings()
        ws.set_editor_property("DefaultGameMode", gm_class)
        print(f"Set game mode to: {{gm_class.get_name()}}")
""")
