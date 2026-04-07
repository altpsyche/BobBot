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
        """List all expression nodes in a material with their types.

        UE 5.1+ moved the Material.Expressions array off the public reflection
        surface, so this walks the connected subgraph from each root material
        property using MaterialEditingLibrary instead of reading the array
        directly. Disconnected expressions are not visible to this walk."""
        return _exec(f"""
import unreal
mat_path = {_safe(material_path)}
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
if mat is None:
    print("ERROR: Material '" + mat_path + "' not found")
elif not isinstance(mat, unreal.Material):
    print("ERROR: '" + mat_path + "' is not a Material (" + mat.get_class().get_name() + ")")
else:
    mel = unreal.MaterialEditingLibrary
    # All possible material property roots. Some won't have a corresponding
    # enum value on every UE version — getattr-with-default keeps us robust.
    _PROP_NAMES = (
        "MP_BASE_COLOR", "MP_METALLIC", "MP_SPECULAR", "MP_ROUGHNESS",
        "MP_ANISOTROPY", "MP_EMISSIVE_COLOR", "MP_OPACITY", "MP_OPACITY_MASK",
        "MP_NORMAL", "MP_TANGENT", "MP_WORLD_POSITION_OFFSET",
        "MP_SUBSURFACE_COLOR", "MP_AMBIENT_OCCLUSION", "MP_REFRACTION",
    )
    discovered = {{}}
    queue = []
    for prop_name in _PROP_NAMES:
        prop = getattr(unreal.MaterialProperty, prop_name, None)
        if prop is None:
            continue
        root = mel.get_material_property_input_node(mat, prop)
        if root and root.get_name() not in discovered:
            discovered[root.get_name()] = root
            queue.append(root)
    while queue:
        expr = queue.pop()
        try:
            inputs = mel.get_inputs_for_material_expression(mat, expr)
        except Exception:
            inputs = []
        for inp in (inputs or []):
            if inp and inp.get_name() not in discovered:
                discovered[inp.get_name()] = inp
                queue.append(inp)

    print("Material: " + mat_path)
    print(f"Expressions ({{len(discovered)}}):")
    if not discovered:
        print("  (none — material has no expressions wired to root properties)")
    else:
        for expr in discovered.values():
            print(f"  {{expr.get_name()}} ({{expr.get_class().get_name()}})")
""")


    @mcp.tool()
    def get_material_graph(material_path: str) -> str:
        """Inspect a material's full expression graph: every node with class,
        editor position, parameter name and default value (for Scalar/Vector/Texture
        parameters), input connections, and which expressions feed which root
        material properties (BaseColor, Metallic, Roughness, Normal, EmissiveColor,
        Opacity, OpacityMask, AmbientOcclusion, Specular).

        Walks via MaterialEditingLibrary BFS from each root property — the
        Material.Expressions array is protected on UE 5.1+ and cannot be read
        directly. Orphan/disconnected expressions will not appear.

        Use this before modifying a material to understand existing wiring."""
        return _exec(f"""
import unreal
mat_path = {_safe(material_path)}
mat = unreal.EditorAssetLibrary.load_asset(mat_path)
if mat is None:
    print("ERROR: Material '" + mat_path + "' not found")
elif not isinstance(mat, unreal.Material):
    print("ERROR: '" + mat_path + "' is not a Material (" + mat.get_class().get_name() + ")")
else:
    mel = unreal.MaterialEditingLibrary

    # All material property roots. Some entries may not exist on older UE
    # versions; getattr-with-default skips them silently.
    _ROOTS = (
        ("BaseColor", "MP_BASE_COLOR"),
        ("Metallic", "MP_METALLIC"),
        ("Specular", "MP_SPECULAR"),
        ("Roughness", "MP_ROUGHNESS"),
        ("Anisotropy", "MP_ANISOTROPY"),
        ("EmissiveColor", "MP_EMISSIVE_COLOR"),
        ("Opacity", "MP_OPACITY"),
        ("OpacityMask", "MP_OPACITY_MASK"),
        ("Normal", "MP_NORMAL"),
        ("Tangent", "MP_TANGENT"),
        ("WorldPositionOffset", "MP_WORLD_POSITION_OFFSET"),
        ("SubsurfaceColor", "MP_SUBSURFACE_COLOR"),
        ("AmbientOcclusion", "MP_AMBIENT_OCCLUSION"),
        ("Refraction", "MP_REFRACTION"),
    )

    # Step 1: BFS from every root property to discover the connected subgraph.
    discovered = {{}}      # name -> expression
    root_wiring = []      # list[(label, expression, output_name)]
    queue = []
    for label, prop_name in _ROOTS:
        prop = getattr(unreal.MaterialProperty, prop_name, None)
        if prop is None:
            continue
        try:
            root_expr = mel.get_material_property_input_node(mat, prop)
        except Exception:
            root_expr = None
        if root_expr:
            try:
                out_name = mel.get_material_property_input_node_output_name(mat, prop)
            except Exception:
                out_name = ""
            root_wiring.append((label, root_expr, out_name))
            if root_expr.get_name() not in discovered:
                discovered[root_expr.get_name()] = root_expr
                queue.append(root_expr)
    while queue:
        expr = queue.pop()
        try:
            inputs = mel.get_inputs_for_material_expression(mat, expr) or []
        except Exception:
            inputs = []
        for inp in inputs:
            if inp and inp.get_name() not in discovered:
                discovered[inp.get_name()] = inp
                queue.append(inp)

    # Step 2: build a reverse map: target_name -> [(source_name, source_to_input_name), ...]
    # so when listing inputs we can show which source expression feeds them.
    inbound = {{}}  # target_expr_name -> list[source_expr_name]
    for src_expr in discovered.values():
        try:
            src_inputs = mel.get_inputs_for_material_expression(mat, src_expr) or []
        except Exception:
            src_inputs = []
        for src_input in src_inputs:
            if src_input:
                inbound.setdefault(src_input.get_name(), []).append(src_expr.get_name())

    print("Material: " + mat_path)
    print(f"Expressions ({{len(discovered)}}):")
    if not discovered:
        print("  (none — material has no expressions wired to root properties)")

    def _safe_pos(expr):
        try:
            x, y = mel.get_material_expression_node_position(expr)
            return (int(x or 0), int(y or 0))
        except Exception:
            return (0, 0)

    def _safe_input_names(expr):
        try:
            names = mel.get_material_expression_input_names(expr) or []
            return list(names)
        except Exception:
            return []

    for expr in discovered.values():
        cls = expr.get_class().get_name()
        x, y = _safe_pos(expr)
        line = f"  {{expr.get_name()}} | {{cls}} | ({{x}},{{y}})"

        # Parameter expressions: print parameter name + value via the
        # MaterialEditingLibrary lookups (which work even when reading
        # the underlying property struct doesn't).
        try:
            if isinstance(expr, unreal.MaterialExpressionScalarParameter):
                pname = expr.get_editor_property("parameter_name")
                pval = mel.get_material_default_scalar_parameter_value(mat, pname)
                line += f" | param '{{pname}}' = {{pval}}"
            elif isinstance(expr, unreal.MaterialExpressionVectorParameter):
                pname = expr.get_editor_property("parameter_name")
                v = mel.get_material_default_vector_parameter_value(mat, pname)
                line += f" | param '{{pname}}' = ({{v.r}},{{v.g}},{{v.b}},{{v.a}})"
            elif isinstance(expr, unreal.MaterialExpressionTextureSampleParameter2D):
                pname = expr.get_editor_property("parameter_name")
                tex = mel.get_material_default_texture_parameter_value(mat, pname)
                tex_name = tex.get_path_name() if tex else "(none)"
                line += f" | param '{{pname}}' = {{tex_name}}"
        except Exception:
            pass

        print(line)

        # Input pin names
        in_names = _safe_input_names(expr)
        if in_names:
            print(f"    inputs: {{', '.join(in_names)}}")

        # Connections feeding INTO this expression (computed from the
        # inbound map populated above).
        sources = inbound.get(expr.get_name(), [])
        if sources:
            print(f"    fed by: {{', '.join(sources)}}")

    # Root property connections
    print("Root properties:")
    if not root_wiring:
        print("  (no root properties wired)")
    else:
        for label, root_expr, out_name in root_wiring:
            suffix = f".{{out_name}}" if out_name else ""
            print(f"  {{label}} <- {{root_expr.get_name()}}{{suffix}}")
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
