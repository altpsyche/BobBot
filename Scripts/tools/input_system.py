"""Enhanced Input tools: create Input Actions, Mapping Contexts, and configure bindings."""

from _common import _exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Enhanced Input", output_kind="small", default_timeout=60)
    def create_input_action(name: str, value_type: str = "bool",
                            path: str = "/Game/Input") -> str:
        """Create an Enhanced Input Action asset. value_type: 'bool', 'float' (Axis1D), 'Vector2D' (Axis2D), 'Vector3D' (Axis3D)."""
        return _exec(f"""
import unreal
name = {_safe(name)}
path = {_safe(path)}
value_type = {_safe(value_type)}.lower()
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
# Create the InputAction asset
try:
    factory = unreal.InputActionFactory()
    ia = asset_tools.create_asset(name, path, unreal.InputAction, factory)
    if ia:
        # Set value type
        type_map = {{
            "bool": unreal.InputActionValueType.BOOLEAN,
            "float": unreal.InputActionValueType.AXIS1D,
            "axis1d": unreal.InputActionValueType.AXIS1D,
            "vector2d": unreal.InputActionValueType.AXIS2D,
            "axis2d": unreal.InputActionValueType.AXIS2D,
            "vector3d": unreal.InputActionValueType.AXIS3D,
            "axis3d": unreal.InputActionValueType.AXIS3D,
        }}
        vt = type_map.get(value_type, unreal.InputActionValueType.BOOLEAN)
        ia.set_editor_property("ValueType", vt)
        unreal.EditorAssetLibrary.save_asset(f"{{path}}/{{name}}")
        print(f"Created InputAction: {{path}}/{{name}} (type: {{value_type}})")
    else:
        print(f"ERROR: Failed to create InputAction")
except Exception as e:
    print(f"ERROR: {{e}}")
    print("InputAction creation may require the EnhancedInput plugin to be enabled")
""")

    @mcp.tool()
    @bob_tool(category="Enhanced Input", output_kind="small", default_timeout=60)
    def create_input_mapping_context(name: str,
                                     path: str = "/Game/Input") -> str:
        """Create an Input Mapping Context asset for Enhanced Input."""
        return _exec(f"""
import unreal
name = {_safe(name)}
path = {_safe(path)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.InputMappingContextFactory()
    imc = asset_tools.create_asset(name, path, unreal.InputMappingContext, factory)
    if imc:
        unreal.EditorAssetLibrary.save_asset(f"{{path}}/{{name}}")
        print(f"Created InputMappingContext: {{path}}/{{name}}")
    else:
        print(f"ERROR: Failed to create InputMappingContext")
except Exception as e:
    print(f"ERROR: {{e}}")
    print("InputMappingContext creation may require the EnhancedInput plugin to be enabled")
""")

    @mcp.tool()
    @bob_tool(category="Enhanced Input", output_kind="small", default_timeout=60)
    def add_input_mapping(context_path: str, action_path: str,
                          key_name: str) -> str:
        """Map a key to an action in a mapping context. key_name examples: 'W', 'SpaceBar', 'LeftMouseButton', 'Gamepad_FaceButton_Bottom', 'MouseX'."""
        return _exec(f"""
import unreal
context_path_local = {_safe(context_path)}
action_path_local = {_safe(action_path)}
key_name_local = {_safe(key_name)}
imc = unreal.EditorAssetLibrary.load_asset(context_path_local)
if imc is None:
    print("ERROR: InputMappingContext " + context_path_local + " not found")
else:
    ia = unreal.EditorAssetLibrary.load_asset(action_path_local)
    if ia is None:
        print("ERROR: InputAction " + action_path_local + " not found")
    else:
        try:
            key = unreal.Key(key_name=key_name_local)
            mapping = unreal.EnhancedActionKeyMapping()
            mapping.set_editor_property("Action", ia)
            mapping.set_editor_property("Key", key)
            mappings = list(imc.get_editor_property("Mappings"))
            mappings.append(mapping)
            imc.set_editor_property("Mappings", mappings)
            unreal.EditorAssetLibrary.save_asset(context_path_local)
            print(f"Mapped {{key_name_local}} -> {{ia.get_name()}} in {{imc.get_name()}}")
        except Exception as e:
            print(f"ERROR: Could not add mapping: {{e}}")
            print("Try using execute_unreal_python for manual mapping configuration")
""")

    @mcp.tool()
    @bob_tool(category="Enhanced Input", output_kind="large", default_timeout=60)
    def get_input_actions(path: str = "/Game/Input") -> str:
        """List all InputAction and InputMappingContext assets in a path."""
        return _exec(f"""
import unreal
path_local = {_safe(path)}
assets = unreal.EditorAssetLibrary.list_assets(path_local, recursive=True, include_folder=False)
actions = []
contexts = []
for asset_path in sorted(assets):
    data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    if data:
        cls = str(data.asset_class)
        if "InputAction" in cls:
            actions.append(asset_path)
        elif "InputMappingContext" in cls:
            contexts.append(asset_path)
if actions:
    print(f"Input Actions ({{len(actions)}}):")
    for a in actions:
        asset = unreal.EditorAssetLibrary.load_asset(a)
        vt = asset.get_editor_property("ValueType") if asset else "?"
        print(f"  {{a}} ({{vt}})")
if contexts:
    print()
    print(f"Input Mapping Contexts ({{len(contexts)}}):")
    for c in contexts:
        print(f"  {{c}}")
if not actions and not contexts:
    print(f"No input assets found in {{path_local}}")
""")

    @mcp.tool()
    @bob_tool(category="Enhanced Input", output_kind="large", default_timeout=60)
    def get_input_context_mappings(context_path: str) -> str:
        """List action -> key mappings on an InputMappingContext.
        UInputMappingContext::Mappings is protected; routed through BobBotLib."""
        return _exec(f"""
import unreal
context_path_local = {_safe(context_path)}
imc = unreal.EditorAssetLibrary.load_asset(context_path_local)
if imc is None:
    print("ERROR: InputMappingContext " + context_path_local + " not found")
elif not isinstance(imc, unreal.InputMappingContext):
    print(f"ERROR: '{{imc.get_class().get_name()}}' is not an InputMappingContext")
else:
    print(f"InputMappingContext: {{context_path_local}}")
    count = unreal.BobBotLib.get_input_context_mapping_count(imc)
    if count <= 0:
        print("\\nNo mappings")
    else:
        print()
        print(f"Mappings ({{count}}):")
        for i in range(count):
            action = unreal.BobBotLib.get_input_context_mapping_action(imc, i)
            key = unreal.BobBotLib.get_input_context_mapping_key(imc, i)
            print(f"  {{key}} -> {{action}}")
""")
