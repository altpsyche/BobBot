# BobBot — UE 5.4+ MCP Tool Reference

Prefer purpose-built tools over execute_unreal_python. They handle errors and return structured output. Fall back to execute_unreal_python when no tool covers the operation.

## Tools (158)

**Actors:** get_selected_actors, get_level_actors (optional class_filter), spawn_actor (class_path, x, y, z, yaw, pitch, roll), delete_selected_actors, get_actor_properties (actor_label), set_actor_property (actor_label, property_name, value)

**Assets:** search_assets (path, name_filter, type_filter), get_asset_info (asset_path), create_blueprint (name, parent_class, path), create_material (name, path)

**Asset Operations:** rename_asset (old_path, new_path), duplicate_asset (source_path, dest_path), delete_asset (asset_path), move_asset (source_path, dest_folder), get_asset_references (asset_path), get_asset_dependencies (asset_path)

**Materials:** add_material_expression (material_path, expression_type, x, y), connect_material_to_property (material_path, expression_name, output_name, property_name), get_material_expressions (material_path)

**Levels:** get_current_level, open_level (level_path), save_current_level

**Level Streaming:** add_streaming_level (level_path), remove_streaming_level (level_path), get_streaming_levels

**Viewport:** capture_viewport (filename, width, height), run_console_command (command), get_output_log (lines)

**Context:** get_project_info, get_editor_state

**Core:** execute_unreal_python (code), ping_unreal

**Editor Operations:** select_actors (actor_labels), deselect_all, undo, redo, focus_on_actor (actor_label), set_actor_label (actor_label, new_label), set_actor_folder (actor_label, folder_path), get_editor_selection

**Tags & Layers:** set_actor_tags (actor_label, tags), get_actors_by_tag (tag), set_actor_layer (actor_label, layer_name), get_actor_tags (actor_label)

**Components:** add_component_to_actor (actor_label, component_type, component_name), remove_component (actor_label, component_name), get_component_properties (actor_label, component_name), set_component_property (actor_label, component_name, property_name, value)

**World:** get_world_settings, set_world_setting (property_name, value), get_game_mode, set_game_mode (game_mode_path)

**Collision:** set_collision_preset (actor_label, preset_name), set_collision_enabled (actor_label, enabled), get_collision_info (actor_label)

**Physics:** set_simulate_physics (actor_label, enabled), set_collision_channel (actor_label, channel), get_physics_info (actor_label), set_physics_properties (actor_label, mass, linear_damping, angular_damping)

**Lighting:** create_light (light_type, x, y, z, intensity, color), set_light_properties (actor_label, intensity, color, temperature, attenuation_radius), get_all_lights, set_lightmap_resolution (actor_label, resolution), create_sky_atmosphere

**Camera:** create_camera (x, y, z, yaw, pitch, fov), set_camera_properties (actor_label, fov, aperture, focus_distance), get_active_viewport_camera, set_viewport_camera (x, y, z, yaw, pitch)

**Texture & Mesh:** get_static_mesh_info (mesh_path), set_static_mesh_on_actor (actor_label, mesh_path), get_texture_info (texture_path), create_texture_from_file (file_path, name, dest_path), set_material_texture_parameter (material_path, param_name, texture_path)

**Import/Export:** import_asset (file_path, destination_path), export_asset (asset_path, file_path), import_fbx (file_path, destination_path, import_animations)

**Post-Process:** create_post_process_volume (x, y, z, infinite_extent), set_post_process_setting (actor_label, setting, value), get_post_process_settings (actor_label), set_color_grading (actor_label, saturation, contrast, gain, offset), get_rendering_stats

**Splines:** create_spline_actor (points, closed), get_spline_info (actor_label), add_spline_point (actor_label, x, y, z, index), set_spline_mesh (actor_label, mesh_path)

**Data Tables:** create_data_table (name, struct_path, path), add_data_table_row (table_path, row_name, values_json), get_data_table_rows (table_path)

**Skeletal Mesh:** get_skeleton_info (skeleton_path), get_skeletal_mesh_info (mesh_path), create_socket (skeleton_path, bone_name, socket_name), attach_actor_to_socket (actor_label, parent_label, socket_name)

**Sequencer:** create_sequence (name, path), get_sequence_info (sequence_path), add_actor_to_sequence (sequence_path, actor_label), set_sequence_length (sequence_path, end_frame), play_sequence (sequence_path)

**Animation:** create_anim_blueprint (name, skeleton_path, path), create_anim_montage (name, animation_path, path), create_blend_space_1d (name, skeleton_path, path), get_skeleton_animations (skeleton_path), get_anim_blueprint_info (anim_bp_path)

**Blueprint Advanced:** create_blueprint_function (blueprint_path, function_name, inputs, outputs), create_blueprint_event (blueprint_path, event_name), get_blueprint_functions (blueprint_path), get_blueprint_components (blueprint_path), set_blueprint_parent_class (blueprint_path, parent_class), create_blueprint_interface (name, path, functions)

**Enhanced Input:** create_input_action (name, value_type, path), create_input_mapping_context (name, path), add_input_mapping (context_path, action_path, key_name), get_input_actions (path)

**Audio:** create_sound_cue (name, sound_wave_path, path), get_audio_assets (path), set_actor_audio (actor_label, sound_path)

**Landscape:** get_landscape_info, set_landscape_material (material_path), get_landscape_layers

**Foliage:** get_foliage_types, add_foliage_type (static_mesh_path), get_foliage_stats

**Niagara/VFX:** create_niagara_system (name, path), get_niagara_info (system_path), set_niagara_parameter (system_path, param_name, value)

**AI/Behavior:** create_behavior_tree (name, path), create_blackboard (name, path), add_blackboard_key (blackboard_path, key_name, key_type), get_blackboard_keys (blackboard_path), create_environment_query (name, path), get_ai_assets (path)

**PCG:** create_pcg_graph (name, path), get_pcg_graph_info (graph_path), execute_pcg_graph (actor_label), get_pcg_volumes

**UMG/Widgets:** create_widget_blueprint (name, parent_class, path), get_widget_tree (widget_path), create_widget_component (actor_label, widget_path), get_all_widget_blueprints (path)

**PIE Runtime:** start_pie, stop_pie, is_pie_running, get_pie_actors (class_filter), execute_pie_console_command (command)

**Source Control:** get_source_control_status (asset_path), check_out_asset (asset_path), check_in_asset (asset_path, description), revert_asset (asset_path)

**Build:** build_lighting (quality), compile_blueprints, validate_assets (path), get_map_check_errors

**Movie Render:** create_render_job (sequence_path, output_dir, format, resolution), get_render_queue_status, render_sequence_to_images (sequence_path, output_dir, format)

**Debug/Profiling:** get_frame_stats, get_memory_stats, get_gpu_stats, benchmark_scene (duration_seconds)

## BobBotLib C++ API

Available via `unreal.BobBotLib` inside execute_unreal_python. Fills UE 5.4 Python API gaps for Blueprint editing.

**Variables:** add_blueprint_variable(bp, name, type, editable), remove_blueprint_variable(bp, name), set_blueprint_variable_default(bp, name, value), set_blueprint_variable_category(bp, name, category), set_blueprint_variable_tooltip(bp, name, tooltip), set_blueprint_variable_slider_range(bp, name, min, max), get_blueprint_variable_names(bp)

Variable types: float, double, int32, int64, bool, FString, FName, FText, byte, FVector, FRotator, FTransform, FLinearColor, FVector2D, UTexture2D*, UStaticMesh*

**Components:** add_component_to_blueprint(bp, component_class, name)

**Graph creation:** add_function_graph(bp, function_name), add_custom_event(bp, event_name)

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
- Tools use dual-path implementations: modern API where available, BobBotLib/Python fallback on older UE versions. All 158 tools work on UE 5.4+.
