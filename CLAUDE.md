# BobBot — UE 5.4+ MCP Tool Reference

Prefer purpose-built tools over execute_unreal_python. They handle errors and return structured output. Fall back to execute_unreal_python when no tool covers the operation.

## Tools

**Actors:** get_selected_actors, get_level_actors (optional class_filter), spawn_actor (class_path, x, y, z, yaw, pitch, roll), delete_selected_actors, get_actor_properties (actor_label), set_actor_property (actor_label, property_name, value)

**Assets:** search_assets (path, name_filter, type_filter), get_asset_info (asset_path), create_blueprint (name, parent_class, path), create_material (name, path)

**Materials:** add_material_expression (material_path, expression_type, x, y), connect_material_to_property (material_path, expression_name, output_name, property_name), get_material_expressions (material_path)

**Levels:** get_current_level, open_level (level_path), save_current_level

**Viewport:** capture_viewport (filename, width, height), run_console_command (command), get_output_log (lines)

**Context:** get_project_info, get_editor_state

**Core:** execute_unreal_python (code), ping_unreal

## BobBotLib C++ API

Available via `unreal.BobBotLib` inside execute_unreal_python. Fills UE 5.4 Python API gaps for Blueprint editing.

**Variables:** add_blueprint_variable(bp, name, type, editable), remove_blueprint_variable(bp, name), set_blueprint_variable_default(bp, name, value), set_blueprint_variable_category(bp, name, category), set_blueprint_variable_tooltip(bp, name, tooltip), set_blueprint_variable_slider_range(bp, name, min, max), get_blueprint_variable_names(bp)

Variable types: float, double, int32, int64, bool, FString, FName, FText, byte, FVector, FRotator, FTransform, FLinearColor, FVector2D, UTexture2D*, UStaticMesh*

**Components:** add_component_to_blueprint(bp, component_class, name)

**Graph nodes:** add_function_call_node(bp, func_name, target_class, x, y), add_set_mpc_scalar_node(bp, mpc_path, param_name, x, y), add_set_mpc_vector_node(bp, mpc_path, param_name, x, y)

**Compilation:** compile_blueprint(bp), compile_blueprint_with_status(bp)

**CDO:** get_blueprint_cdo(bp), set_cdo_property(bp, property_name, value)

Always compile after modifying Blueprints. Always save assets after creating or modifying them.

## UE Essentials

Coordinate system: X forward, Y right, Z up. Units are centimeters. A character is ~180 units tall. Rotations in degrees (pitch=Y, yaw=Z, roll=X).

Asset paths: "/Game/Folder/AssetName" for project content. "/Script/Engine.ClassName" for C++ classes.

Material properties: MP_BaseColor, MP_Metallic, MP_Specular, MP_Roughness, MP_Normal, MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_WorldPositionOffset, MP_AmbientOcclusion

## Tips

- Call get_project_info() at conversation start to understand the project.
- Call get_editor_state() when the user says "this" or "the selected" thing.
- When a tool fails, use get_output_log(20) to see what went wrong.
- Variables persist across execute_unreal_python calls within a session.
