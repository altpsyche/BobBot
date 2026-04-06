"""Material instance tools: create instances, set scalar/vector/texture parameters."""

from _common import _exec_ue, asset_exec, _safe


def register(mcp, send_fn):


    @mcp.tool()
    def create_material_instance(parent_path: str, name: str, dest_path: str = "/Game/Materials") -> str:
        """Create a new MaterialInstanceConstant from a parent material. parent_path: asset path of parent material (e.g. '/Game/Materials/M_Base'). name: name for the new instance. dest_path: destination folder."""
        return _exec_ue(f"""
parent_path = {_safe(parent_path)}
name = {_safe(name)}
dest_path = {_safe(dest_path)}

parent = unreal.EditorAssetLibrary.load_asset(parent_path)
if parent is None:
    print(f"ERROR: Parent material '{{parent_path}}' not found")
elif not isinstance(parent, (unreal.Material, unreal.MaterialInstanceConstant)):
    print(f"ERROR: '{{parent_path}}' is not a Material or MaterialInstance (got {{parent.get_class().get_name()}})")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    factory.set_editor_property("InitialParent", parent)
    new_asset = asset_tools.create_asset(name, dest_path, None, factory)
    if new_asset is None:
        print(f"ERROR: Failed to create material instance '{{name}}'")
    else:
        unreal.EditorAssetLibrary.save_loaded_asset(new_asset)
        full_path = dest_path.rstrip("/") + "/" + name
        print(f"Created MaterialInstanceConstant '{{name}}' at {{full_path}}")
        print(f"Parent: {{parent_path}}")
""")

    @mcp.tool()
    def set_material_instance_scalar(instance_path: str, param_name: str, value: float) -> str:
        """Set a scalar parameter on a MaterialInstanceConstant. instance_path: asset path of the material instance. param_name: parameter name. value: float value."""
        return asset_exec(instance_path, f"""
_param = {_safe(param_name)}
if not isinstance(asset, unreal.MaterialInstanceConstant):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a MaterialInstanceConstant")
else:
    result = unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(asset, _param, float({value}))
    if result:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        print(f"Set scalar parameter '{{_param}}' = {value} on {{asset.get_name()}}")
    else:
        print(f"ERROR: Failed to set scalar parameter '{{_param}}'. Does the parameter exist in the parent material?")
""")

    @mcp.tool()
    def set_material_instance_vector(instance_path: str, param_name: str,
                                      r: float, g: float, b: float, a: float = 1.0) -> str:
        """Set a vector (color) parameter on a MaterialInstanceConstant. instance_path: asset path. param_name: parameter name. r/g/b/a: color components (0.0-1.0)."""
        return asset_exec(instance_path, f"""
_param = {_safe(param_name)}
if not isinstance(asset, unreal.MaterialInstanceConstant):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a MaterialInstanceConstant")
else:
    color = unreal.LinearColor(r={r}, g={g}, b={b}, a={a})
    result = unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(asset, _param, color)
    if result:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        print(f"Set vector parameter '{{_param}}' = (R={r}, G={g}, B={b}, A={a}) on {{asset.get_name()}}")
    else:
        print(f"ERROR: Failed to set vector parameter '{{_param}}'. Does the parameter exist in the parent material?")
""")

    @mcp.tool()
    def get_material_instance_params(instance_path: str) -> str:
        """Get all parameter values (scalar, vector, texture) from a MaterialInstanceConstant."""
        return asset_exec(instance_path, """
if not isinstance(asset, unreal.MaterialInstanceConstant):
    print(f"ERROR: '{asset.get_class().get_name()}' is not a MaterialInstanceConstant")
else:
    print(f"Material Instance: {asset.get_name()}")
    parent = asset.get_editor_property("Parent")
    if parent:
        print(f"Parent: {parent.get_path_name()}")
    else:
        print("Parent: None")

    # Scalar parameters
    scalars = asset.get_editor_property("ScalarParameterValues")
    if scalars:
        print()
        print(f"Scalar Parameters ({len(scalars)}):")
        for sp in scalars:
            info = sp.get_editor_property("ParameterInfo")
            name = info.get_editor_property("Name") if info else "?"
            val = sp.get_editor_property("ParameterValue")
            print(f"  {name} = {val}")
    else:
        print("\\nScalar Parameters: (none)")

    # Vector parameters
    vectors = asset.get_editor_property("VectorParameterValues")
    if vectors:
        print()
        print(f"Vector Parameters ({len(vectors)}):")
        for vp in vectors:
            info = vp.get_editor_property("ParameterInfo")
            name = info.get_editor_property("Name") if info else "?"
            val = vp.get_editor_property("ParameterValue")
            print(f"  {name} = (R={val.r:.3f}, G={val.g:.3f}, B={val.b:.3f}, A={val.a:.3f})")
    else:
        print("\\nVector Parameters: (none)")

    # Texture parameters
    textures = asset.get_editor_property("TextureParameterValues")
    if textures:
        print()
        print(f"Texture Parameters ({len(textures)}):")
        for tp in textures:
            info = tp.get_editor_property("ParameterInfo")
            name = info.get_editor_property("Name") if info else "?"
            val = tp.get_editor_property("ParameterValue")
            tex_path = val.get_path_name() if val else "None"
            print(f"  {name} = {tex_path}")
    else:
        print("\\nTexture Parameters: (none)")
""")
