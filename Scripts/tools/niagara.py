"""Niagara VFX tools: create particle systems, inspect emitters, set parameters."""

from _common import _exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def create_niagara_system(name: str, path: str = "/Game/VFX") -> str:
        """Create an empty Niagara particle system asset."""
        return _exec(f"""
import unreal
name = {_safe(name)}
path = {_safe(path)}
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
system_path_local = {_safe(system_path)}
ns = unreal.EditorAssetLibrary.load_asset(system_path_local)
if ns is None:
    print("ERROR: Niagara system " + system_path_local + " not found")
elif not isinstance(ns, unreal.NiagaraSystem):
    print(f"ERROR: '{{ns.get_class().get_name()}}' is not a NiagaraSystem")
else:
    print(f"Niagara System: {{system_path_local}}")
    count = unreal.BobBotLib.get_niagara_emitter_count(ns)
    if count <= 0:
        print("\\nNo emitters")
    else:
        print()
        print(f"Emitters ({{count}}):")
        for i in range(count):
            ename = unreal.BobBotLib.get_niagara_emitter_name(ns, i)
            enabled = unreal.BobBotLib.get_niagara_emitter_enabled(ns, i)
            print(f"  {{ename}} (enabled: {{enabled}})")
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
system_path_local = {_safe(system_path)}
param_name_local = {_safe(param_name)}
val_str = {_safe(value)}
ns = unreal.EditorAssetLibrary.load_asset(system_path_local)
if ns is None:
    print("ERROR: Niagara system " + system_path_local + " not found")
elif not isinstance(ns, unreal.NiagaraSystem):
    print(f"ERROR: '{{ns.get_class().get_name()}}' is not a NiagaraSystem")
else:
    success = False
    try:
        val = float(val_str)
    except ValueError:
        val = val_str

    # Try 1: Modern exposed parameter API (UE 5.5+)
    try:
        exposed = ns.get_editor_property("ExposedParameters")
        if exposed and hasattr(exposed, 'set_float_parameter') and isinstance(val, float):
            exposed.set_float_parameter(param_name_local, val)
            success = True
        elif exposed and hasattr(exposed, 'set_int_parameter') and isinstance(val, float) and val == int(val):
            exposed.set_int_parameter(param_name_local, int(val))
            success = True
    except Exception as e:
        unreal.log_warning(f'set_niagara_parameter modern API: {{e}}')

    # Try 2: Direct property manipulation fallback (UE 5.4)
    if not success:
        try:
            exposed = ns.get_editor_property("ExposedParameters")
            if exposed:
                # Try overriding via set_editor_property on the parameters store
                exposed.set_editor_property(param_name_local, val)
                success = True
        except Exception as e:
            unreal.log_warning(f'set_niagara_parameter direct property: {{e}}')

    # Try 3: Override parameter via system override
    if not success:
        try:
            overrides = ns.get_editor_property("SystemSpawnScript")
            if overrides:
                overrides.set_editor_property(param_name_local, val)
                success = True
        except Exception as e:
            unreal.log_warning(f'set_niagara_parameter system override: {{e}}')

    if success:
        unreal.EditorAssetLibrary.save_asset(system_path_local)
        print(f"Set {{param_name_local}} = {{val_str}} on {{system_path_local}}")
    else:
        print(f"ERROR: Could not set parameter '{{param_name_local}}' on this Niagara system.")
        print("Niagara parameter APIs vary across UE versions.")
        print("Workaround: Open the system in Niagara Editor and set the parameter manually,")
        print("or use execute_unreal_python to access the parameter structure directly.")
""")

    @mcp.tool()
    def get_niagara_summary(system_path: str) -> str:
        """Compact perf summary for a Niagara system. Reports emitter count and
        flags VFX-heavy systems. Thresholds: HEAVY >5 enabled emitters, MASSIVE >15."""
        return _exec(f"""
import unreal
system_path_local = {_safe(system_path)}
ns = unreal.EditorAssetLibrary.load_asset(system_path_local)
if ns is None:
    print("ERROR: Niagara system " + system_path_local + " not found")
elif not isinstance(ns, unreal.NiagaraSystem):
    print(f"ERROR: '{{ns.get_class().get_name()}}' is not a NiagaraSystem")
else:
    count = unreal.BobBotLib.get_niagara_emitter_count(ns)
    enabled_count = 0
    for i in range(max(count, 0)):
        if unreal.BobBotLib.get_niagara_emitter_enabled(ns, i):
            enabled_count += 1
    flag = "OK"
    if enabled_count > 15:
        flag = "MASSIVE"
    elif enabled_count > 5:
        flag = "HEAVY"
    print(f"{{system_path_local}} | emitters={{count}} | enabled={{enabled_count}} | {{flag}}")
""")

