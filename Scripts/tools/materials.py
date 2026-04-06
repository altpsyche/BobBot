"""Material tools: add expressions, connect nodes, inspect material graphs."""

from _common import _exec, asset_exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def add_material_expression(material_path: str, expression_type: str,
                                x: int = 0, y: int = 0) -> str:
        """Add a material expression node to a material. expression_type examples:
        'MaterialExpressionTextureSample', 'MaterialExpressionConstant3Vector',
        'MaterialExpressionFresnel', 'MaterialExpressionMultiply',
        'MaterialExpressionLerp', 'MaterialExpressionScalarParameter',
        'MaterialExpressionVectorParameter', 'MaterialExpressionTextureCoordinate'."""
        return _exec(f"""
import unreal
mat_path = {_safe(material_path)}
expr_type_name = {_safe(expression_type)}
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
if mat is None:
    print("ERROR: Material '" + mat_path + "' not found")
else:
    expr_class = getattr(unreal, expr_type_name, None)
    if expr_class is None:
        print("ERROR: Expression type '" + expr_type_name + "' not found")
    else:
        expr = unreal.MaterialEditingLibrary.create_material_expression(mat, expr_class, {x}, {y})
        if expr:
            print(f"Added {{expr.get_name()}} ({{expr.get_class().get_name()}}) at ({x}, {y})")
        else:
            print("ERROR: Failed to create expression")
""")

    @mcp.tool()
    def connect_material_to_property(material_path: str, expression_name: str,
                                     output_name: str, property_name: str) -> str:
        """Connect a material expression output to a material property input.
        property_name examples: 'MP_BaseColor', 'MP_Metallic', 'MP_Roughness',
        'MP_Normal', 'MP_EmissiveColor', 'MP_Opacity', 'MP_OpacityMask'.
        output_name is usually '' (empty string) for the default output."""
        return _exec(f"""
import unreal
mat_path = {_safe(material_path)}
expr_name = {_safe(expression_name)}
out_name = {_safe(output_name)}
prop_name = {_safe(property_name)}
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
if mat is None:
    print("ERROR: Material '" + mat_path + "' not found")
else:
    # Find the expression by name
    target_expr = None
    for expr in mat.get_editor_property("Expressions"):
        if expr.get_name() == expr_name:
            target_expr = expr
            break
    if target_expr is None:
        print("ERROR: Expression '" + expr_name + "' not found in material")
        print("Available expressions:")
        for expr in mat.get_editor_property("Expressions"):
            print(f"  {{expr.get_name()}} ({{expr.get_class().get_name()}})")
    else:
        prop = getattr(unreal.MaterialProperty, prop_name, None)
        if prop is None:
            print("ERROR: Property '" + prop_name + "' not recognized")
        else:
            result = unreal.MaterialEditingLibrary.connect_material_property(target_expr, out_name, prop)
            if result:
                print(f"Connected {{target_expr.get_name()}} -> " + prop_name)
            else:
                print("ERROR: Failed to connect")
""")

    @mcp.tool()
    def get_material_expressions(material_path: str) -> str:
        """List all expression nodes in a material with their types."""
        return _exec(f"""
import unreal
mat_path = {_safe(material_path)}
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
if mat is None:
    print("ERROR: Material '" + mat_path + "' not found")
else:
    exprs = mat.get_editor_property("Expressions")
    if not exprs:
        print("Material has no expressions")
    else:
        print("Material: " + mat_path)
        print(f"Expressions ({{len(exprs)}}):  ")
        for expr in exprs:
            print(f"  {{expr.get_name()}} ({{expr.get_class().get_name()}})")
""")


    @mcp.tool()
    def set_material_blend_mode(material_path: str, blend_mode: str) -> str:
        """Set the blend mode of a material. blend_mode: 'opaque', 'masked', 'translucent', 'additive', 'modulate', 'alpha_composite', 'alpha_holdout'."""
        safe_mode = blend_mode.strip().lower().replace(" ", "_")
        return asset_exec(material_path, f"""
_mode_str = {_safe(safe_mode)}
if not isinstance(asset, unreal.Material) and not isinstance(asset, unreal.MaterialInstance):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a Material")
else:
    mode_map = {{
        "opaque": unreal.BlendMode.BLEND_OPAQUE,
        "masked": unreal.BlendMode.BLEND_MASKED,
        "translucent": unreal.BlendMode.BLEND_TRANSLUCENT,
        "additive": unreal.BlendMode.BLEND_ADDITIVE,
        "modulate": unreal.BlendMode.BLEND_MODULATE,
        "alpha_composite": unreal.BlendMode.BLEND_ALPHA_COMPOSITE,
        "alpha_holdout": unreal.BlendMode.BLEND_ALPHA_HOLDOUT,
    }}
    ue_mode = mode_map.get(_mode_str)
    if ue_mode is None:
        print(f"ERROR: Unknown blend mode '{{_mode_str}}'. Valid modes: {{', '.join(mode_map.keys())}}")
    else:
        asset.set_editor_property("BlendMode", ue_mode)
        # Recompile if it's a Material (not an instance)
        if isinstance(asset, unreal.Material):
            try:
                unreal.MaterialEditingLibrary.recompile_material(asset)
            except Exception as e:
                unreal.log_warning(f"Recompile warning: {{e}}")
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        print(f"Set blend mode to '{{_mode_str}}' on {{asset.get_path_name()}}")
""")
