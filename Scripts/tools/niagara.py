"""Niagara VFX tools: create particle systems, inspect emitters, set parameters."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_niagara_system(name: str, path: str = "/Game/VFX") -> str:
        """Create an empty Niagara particle system asset."""
        return _exec(f"""
import unreal
name = "{name}"
path = "{path}"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.NiagaraSystemFactoryNew()
    ns = asset_tools.create_asset(name, path, unreal.NiagaraSystem, factory)
    if ns:
        unreal.EditorAssetLibrary.save_asset(f"{{path}}/{{name}}")
        print(f"Created Niagara System: {{path}}/{{name}}")
    else:
        print(f"ERROR: Failed to create Niagara System")
except Exception as e:
    print(f"ERROR: {{e}}")
    print("Ensure the Niagara plugin is enabled in Edit > Plugins")
""")

    @mcp.tool()
    def get_niagara_info(system_path: str) -> str:
        """Get emitter count, parameter list, and basic info for a Niagara system."""
        return _exec(f"""
import unreal
ns = unreal.EditorAssetLibrary.load_asset("{system_path}")
if ns is None:
    print("ERROR: Niagara system '{system_path}' not found")
elif not isinstance(ns, unreal.NiagaraSystem):
    print(f"ERROR: '{{ns.get_class().get_name()}}' is not a NiagaraSystem")
else:
    print(f"Niagara System: {system_path}")
    # Get emitter handles
    try:
        emitter_handles = ns.get_editor_property("EmitterHandles")
        if emitter_handles:
            print()
            print(f"Emitters ({{len(emitter_handles)}}):")
            for eh in emitter_handles:
                name = eh.get_editor_property("Name")
                enabled = eh.get_editor_property("bIsEnabled") if hasattr(eh, "bIsEnabled") else True
                print(f"  {{name}} (enabled: {{enabled}})")
        else:
            print("\\nNo emitters")
    except Exception as e:
        print()
        print(f"Could not read emitters: {{e}}")
    # User parameters
    try:
        exposed_params = ns.get_editor_property("ExposedParameters")
        if exposed_params:
            print()
            print(f"Exposed Parameters:")
            # Parameters structure varies by version
            print(f"  (Use execute_unreal_python for detailed parameter inspection)")
    except Exception as e:
        unreal.log_warning(f'get_niagara_info: {{e}}')
""")

    @mcp.tool()
    def set_niagara_parameter(system_path: str, param_name: str,
                              value: str) -> str:
        """Set a user parameter on a Niagara system. Tries modern API first, falls back to property manipulation."""
        return _exec(f"""
import unreal
ns = unreal.EditorAssetLibrary.load_asset("{system_path}")
if ns is None:
    print("ERROR: Niagara system '{system_path}' not found")
elif not isinstance(ns, unreal.NiagaraSystem):
    print(f"ERROR: '{{ns.get_class().get_name()}}' is not a NiagaraSystem")
else:
    success = False
    val_str = "{value}"
    try:
        val = float(val_str)
    except ValueError:
        val = val_str

    # Try 1: Modern exposed parameter API (UE 5.5+)
    try:
        exposed = ns.get_editor_property("ExposedParameters")
        if exposed and hasattr(exposed, 'set_float_parameter') and isinstance(val, float):
            exposed.set_float_parameter("{param_name}", val)
            success = True
        elif exposed and hasattr(exposed, 'set_int_parameter') and isinstance(val, float) and val == int(val):
            exposed.set_int_parameter("{param_name}", int(val))
            success = True
    except Exception as e:
        unreal.log_warning(f'set_niagara_parameter modern API: {{e}}')

    # Try 2: Direct property manipulation fallback (UE 5.4)
    if not success:
        try:
            exposed = ns.get_editor_property("ExposedParameters")
            if exposed:
                # Try overriding via set_editor_property on the parameters store
                exposed.set_editor_property("{param_name}", val)
                success = True
        except Exception as e:
            unreal.log_warning(f'set_niagara_parameter direct property: {{e}}')

    # Try 3: Override parameter via system override
    if not success:
        try:
            overrides = ns.get_editor_property("SystemSpawnScript")
            if overrides:
                overrides.set_editor_property("{param_name}", val)
                success = True
        except Exception as e:
            unreal.log_warning(f'set_niagara_parameter system override: {{e}}')

    if success:
        unreal.EditorAssetLibrary.save_asset("{system_path}")
        print(f"Set {param_name} = {value} on {system_path}")
    else:
        print(f"ERROR: Could not set parameter '{param_name}' on this Niagara system.")
        print("Niagara parameter APIs vary across UE versions.")
        print("Workaround: Open the system in Niagara Editor and set the parameter manually,")
        print("or use execute_unreal_python to access the parameter structure directly.")
""")
