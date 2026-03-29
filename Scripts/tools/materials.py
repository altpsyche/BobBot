"""Material tools: add expressions, connect nodes, inspect material graphs."""


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
    for expr in mat.expressions:
        if expr.get_name() == "{expression_name}":
            target_expr = expr
            break
    if target_expr is None:
        print("ERROR: Expression '{expression_name}' not found in material")
        print("Available expressions:")
        for expr in mat.expressions:
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
    exprs = mat.expressions
    if not exprs:
        print("Material has no expressions")
    else:
        print(f"Material: {material_path}")
        print(f"Expressions ({{len(exprs)}}):")
        for expr in exprs:
            print(f"  {{expr.get_name()}} ({{expr.get_class().get_name()}})")
""")
