"""Material tools: add expressions, connect nodes, inspect material graphs."""

from _common import _exec, asset_exec

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
mat = unreal.EditorAssetLibrary.load_asset("{material_path}")
if mat is None:
    print("ERROR: Material '{material_path}' not found")
else:
    expr_class = getattr(unreal, "{expression_type}", None)
    if expr_class is None:
        print("ERROR: Expression type '{expression_type}' not found")
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
mat = unreal.EditorAssetLibrary.load_asset("{material_path}")
if mat is None:
    print("ERROR: Material '{material_path}' not found")
else:
    # Find the expression by name
    target_expr = None
    for expr in mat.get_editor_property("Expressions"):
        if expr.get_name() == "{expression_name}":
            target_expr = expr
            break
    if target_expr is None:
        print("ERROR: Expression '{expression_name}' not found in material")
        print("Available expressions:")
        for expr in mat.get_editor_property("Expressions"):
            print(f"  {{expr.get_name()}} ({{expr.get_class().get_name()}})")
    else:
        prop = getattr(unreal.MaterialProperty, "{property_name}", None)
        if prop is None:
            print("ERROR: Property '{property_name}' not recognized")
        else:
            result = unreal.MaterialEditingLibrary.connect_material_property(target_expr, "{output_name}", prop)
            if result:
                print(f"Connected {{target_expr.get_name()}} -> {property_name}")
            else:
                print("ERROR: Failed to connect")
""")

    @mcp.tool()
    def get_material_expressions(material_path: str) -> str:
        """List all expression nodes in a material with their types."""
        return _exec(f"""
import unreal
mat = unreal.EditorAssetLibrary.load_asset("{material_path}")
if mat is None:
    print("ERROR: Material '{material_path}' not found")
else:
    exprs = mat.get_editor_property("Expressions")
    if not exprs:
        print("Material has no expressions")
    else:
        print(f"Material: {material_path}")
        print(f"Expressions ({{len(exprs)}}):")
        for expr in exprs:
            print(f"  {{expr.get_name()}} ({{expr.get_class().get_name()}})")
""")


    @mcp.tool()
    def set_material_blend_mode(material_path: str, blend_mode: str) -> str:
        """Set the blend mode of a material. blend_mode: 'opaque', 'masked', 'translucent', 'additive', 'modulate', 'alpha_composite', 'alpha_holdout'."""
        safe_mode = blend_mode.strip().lower().replace(" ", "_")
        return asset_exec(material_path, f"""
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
    mode_str = "{safe_mode}"
    ue_mode = mode_map.get(mode_str)
    if ue_mode is None:
        print(f"ERROR: Unknown blend mode '{{mode_str}}'. Valid modes: {{', '.join(mode_map.keys())}}")
    else:
        asset.set_editor_property("BlendMode", ue_mode)
        # Recompile if it's a Material (not an instance)
        if isinstance(asset, unreal.Material):
            try:
                unreal.MaterialEditingLibrary.recompile_material(asset)
            except Exception as e:
                unreal.log_warning(f"Recompile warning: {{e}}")
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        print(f"Set blend mode to '{{mode_str}}' on {{asset.get_path_name()}}")
""")
