# MCP Tool Reference

<!-- AUTOGEN:TOOLS START -->

BobBot has 222 MCP tools organized by category. Auto-generated from the tool registry — edit `bob_tool(...)` decorators in `Scripts/tools/*.py` to change.

> **Live catalog:** call `list_tools` for the runtime-authoritative list.

## AI/Behavior (6)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_blackboard_key` | `blackboard_path, key_name, key_type` | small | 60s | Add a key to a Blackboard. |
| `create_behavior_tree` | `name, path` | small | 60s | Create a Behavior Tree asset. |
| `create_blackboard` | `name, path` | small | 60s | Create a Blackboard Data asset for use with Behavior Trees. |
| `create_environment_query` | `name, path` | small | 60s | Create an Environment Query System (EQS) query asset. |
| `get_ai_assets` | `path` | large | 60s | List all AI assets (Behavior Trees, Blackboards, EQS) in a path. |
| `get_blackboard_keys` | `blackboard_path` | large | 60s | List all keys in a Blackboard with their types. |

## Actors (6)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `delete_selected_actors` | `—` | small | 60s | Delete all currently selected actors in the viewport. |
| `get_actor_properties` | `actor_label` | large | 60s | Get all editable properties of an actor found by its label in the level. |
| `get_level_actors` | `class_filter` | large | 60s | Get all actors in the current level. |
| `get_selected_actors` | `—` | large | 60s | Get all currently selected actors in the viewport with their labels, classes, and locations. |
| `set_actor_property` | `actor_label, property_name, value` | small | 60s | Set a property on an actor by label. |
| `spawn_actor` | `class_path, x, y, z, yaw, pitch, roll` | small | 60s | Spawn an actor at a location. |

## Animation (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_anim_blueprint` | `name, skeleton_path, path` | small | 60s | Create an Animation Blueprint from a skeleton asset. |
| `create_anim_montage` | `name, animation_path, path` | small | 60s | Create an Animation Montage from an animation sequence. |
| `create_blend_space_1d` | `name, skeleton_path, path` | small | 60s | Create a 1D Blend Space for a skeleton. |
| `get_anim_blueprint_info` | `anim_bp_path` | large | 60s | Get info about an AnimBlueprint: skeleton, state machines, variables. |
| `get_skeleton_animations` | `skeleton_path` | large | 60s | List all animation sequences that use a given skeleton. |

## Asset Operations (6)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `delete_asset` | `asset_path` | small | 60s | Delete an asset from the Content Browser. |
| `duplicate_asset` | `source_path, dest_path` | small | 60s | Duplicate an asset. |
| `get_asset_dependencies` | `asset_path` | large | 60s | Get all assets that this asset depends on. |
| `get_asset_references` | `asset_path` | large | 60s | Get all assets that reference this asset (what depends on it). |
| `move_asset` | `source_path, dest_folder` | small | 60s | Move an asset to a different folder. |
| `rename_asset` | `old_path, new_path` | small | 60s | Rename or move an asset. |

## Assets (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_blueprint` | `name, parent_class, path` | large | 60s | Create a new Blueprint asset. |
| `create_material` | `name, path` | large | 60s | Create a new empty Material asset. |
| `get_asset_info` | `asset_path` | large | 60s | Get detailed info about an asset: class, package, and whether it exists on disk. |
| `search_assets` | `path, name_filter, type_filter, recursive` | large | 60s | Search for assets by path, name, and type. |

## Audio (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_sound_cue` | `name, sound_wave_path, path` | small | 60s | Create a SoundCue from a SoundWave asset. |
| `get_audio_assets` | `path` | large | 60s | List all sound assets (SoundWave, SoundCue, SoundAttenuation, etc.) in a path. |
| `set_actor_audio` | `actor_label, sound_path` | small | 60s | Set the sound on an actor's AudioComponent. |

## Blueprint Advanced (6)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_blueprint_event` | `blueprint_path, event_name` | large | 60s | Add a custom event to a Blueprint's event graph. |
| `create_blueprint_function` | `blueprint_path, function_name, inputs, outputs` | large | 60s | Add a custom function to a Blueprint. |
| `create_blueprint_interface` | `name, path, functions` | large | 60s | Create a Blueprint Interface. |
| `get_blueprint_components` | `blueprint_path` | large | 60s | List all components defined in a Blueprint's construction script. |
| `get_blueprint_functions` | `blueprint_path` | large | 60s | List all functions in a Blueprint with their inputs and outputs. |
| `set_blueprint_parent_class` | `blueprint_path, parent_class` | large | 60s | Reparent a Blueprint to a different parent class. |

## Blueprint Graph (2)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_graph_nodes` | `blueprint_path, graph_name` | large | 60s | List every node in a Blueprint graph with name, title, position, pins, and connections. |
| `get_node_details` | `blueprint_path, node_name` | large | 60s | Inspect one Blueprint node in full: every pin with type, direction, default value, and every linked target node + pin. |

## Build (7)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `build_lighting` | `quality` | small | 180s | Build lighting for the current level. |
| `clean_derived_data` | `—` | small | 60s | Delete the DerivedDataCache folder and flush the DDC. |
| `compile_blueprints` | `—` | small | 180s | Compile all dirty (modified) Blueprints in the project. |
| `get_build_errors` | `—` | large | 60s | Read the latest UE log file and extract lines containing 'Error' or 'Warning'. |
| `get_map_check_errors` | `—` | large | 60s | Run Map Check and return any errors or warnings. |
| `package_project` | `config, platform` | small | 180s | Start packaging the project as a background process. |
| `validate_assets` | `path` | small | 180s | Run asset validation on assets in a path. |

## Camera (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_camera` | `x, y, z, yaw, pitch, fov` | small | 60s | Create a CameraActor at a location with optional FOV. |
| `get_active_viewport_camera` | `—` | large | 60s | Get the editor viewport camera location, rotation, and FOV. |
| `set_camera_properties` | `actor_label, fov, aperture, focus_distance` | small | 60s | Set camera lens properties. |
| `set_viewport_camera` | `x, y, z, yaw, pitch` | small | 60s | Teleport the editor viewport camera to a position and rotation. |

## Collision (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_collision_info` | `actor_label` | large | 60s | Get collision preset, enabled state, and object type for an actor. |
| `set_collision_enabled` | `actor_label, enabled` | small | 60s | Enable or disable collision on an actor. |
| `set_collision_preset` | `actor_label, preset_name` | small | 60s | Set collision preset on an actor. |

## Components (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_component_to_actor` | `actor_label, component_type, component_name` | small | 60s | Add a component to a level actor. |
| `get_component_properties` | `actor_label, component_name` | large | 60s | Get properties of a specific component on an actor. |
| `remove_component` | `actor_label, component_name` | small | 60s | Remove a component from a level actor by component name. |
| `set_component_property` | `actor_label, component_name, property_name, value` | small | 60s | Set a property on a component. |

## Console Variables (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_cvar` | `name` | large | 60s | Get the current value of a console variable (CVar). |
| `list_cvars` | `pattern` | large | 60s | List console variables matching a pattern. |
| `reset_cvar` | `name` | small | 60s | Query a console variable's current value and help text. |
| `set_cvar` | `name, value` | small | 60s | Set a console variable (CVar) to a new value. |

## Content Browser (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_content_folder` | `path` | small | 60s | Create a folder in the Content Browser. |
| `delete_content_folder` | `path` | small | 60s | Delete an empty folder from the Content Browser. |
| `find_unused_assets` | `path` | large | 180s | Find assets with zero referencers (potentially unused). |
| `get_asset_size_report` | `path` | large | 180s | Get a size report of assets grouped by class. |

## Context (2)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_editor_state` | `—` | large | 60s | Get current editor state: selected actors, viewport info, and content browser selection. |
| `get_project_info` | `—` | large | 60s | Get project info: name, engine version, enabled plugins, source modules, and current level. |

## Core (2)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `execute_unreal_python` | `code` | large | 60s | Execute Python code inside Unreal Engine 5 editor. |
| `ping_unreal` | `—` | large | 60s | Check if Unreal Engine editor is connected and responding. |

## Data Tables (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_data_table_row` | `table_path, row_name, values_json` | small | 60s | Add or update a row in a DataTable. |
| `create_data_table` | `name, struct_path, path` | small | 60s | Create an empty DataTable with a row struct. |
| `get_data_table_rows` | `table_path` | large | 60s | List all rows in a DataTable with their values. |

## Debug/Profiling (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `benchmark_scene` | `duration_seconds` | large | 180s | Run a simple scene benchmark by enabling stats for a duration. |
| `get_frame_stats` | `—` | large | 60s | Get FPS, frame time, draw calls, and triangle count. |
| `get_gpu_stats` | `—` | large | 60s | Get GPU time, render target count, and shader complexity info. |
| `get_memory_stats` | `—` | large | 60s | Get memory usage breakdown. |

## Editor Operations (11)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `deselect_all` | `—` | small | 60s | Clear all viewport selection. |
| `focus_on_actor` | `actor_label` | small | 60s | Focus the viewport camera on an actor by label. |
| `get_editor_selection` | `—` | large | 60s | Get full selection info: selected actors in viewport and selected assets in Content Browser. |
| `list_plugins` | `—` | large | 60s | List all plugins referenced in the .uproject file and any additional plugins found in the Plugins/ directory. |
| `load_editor_bookmark` | `slot` | small | 60s | Jump the viewport camera to a previously saved bookmark (slot 0-9). |
| `redo` | `—` | small | 60s | Trigger editor redo. |
| `select_actors` | `actor_labels` | small | 60s | Select actors in the viewport by label. |
| `set_actor_folder` | `actor_label, folder_path` | small | 60s | Organize an actor into a World Outliner folder. |
| `set_actor_label` | `actor_label, new_label` | small | 60s | Rename an actor in the level. |
| `set_editor_bookmark` | `slot, x, y, z, yaw, pitch` | small | 60s | Save an editor viewport bookmark to a slot (0-9). |
| `undo` | `—` | small | 60s | Trigger editor undo. |

## Enhanced Input (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_input_mapping` | `context_path, action_path, key_name` | small | 60s | Map a key to an action in a mapping context. |
| `create_input_action` | `name, value_type, path` | small | 60s | Create an Enhanced Input Action asset. |
| `create_input_mapping_context` | `name, path` | small | 60s | Create an Input Mapping Context asset for Enhanced Input. |
| `get_input_actions` | `path` | large | 60s | List all InputAction and InputMappingContext assets in a path. |
| `get_input_context_mappings` | `context_path` | large | 60s | List action -> key mappings on an InputMappingContext. |

## Foliage (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_foliage_type` | `static_mesh_path` | small | 60s | Register a static mesh as a foliage type so it can be painted. |
| `get_foliage_stats` | `—` | large | 60s | Get total foliage instance count and breakdown by mesh type. |
| `get_foliage_types` | `—` | large | 60s | List all foliage types in the current level and their settings. |

## Import/Export (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `export_asset` | `asset_path, file_path` | small | 60s | Export an asset to a file on disk. |
| `import_asset` | `file_path, destination_path` | small | 60s | Import a file (FBX, OBJ, PNG, WAV, etc.) into the Content Browser. |
| `import_fbx` | `file_path, destination_path, import_animations` | small | 60s | Import an FBX file with mesh and animation options. |

## LOD (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `auto_generate_lods` | `mesh_path, num_lods` | large | 180s | Auto-generate LODs for a static mesh. |
| `get_lod_info` | `mesh_path` | large | 60s | Get LOD information for a static mesh: LOD count, triangle counts, and screen sizes. |
| `get_lod_summary` | `mesh_path` | large | 60s | Mobile-aware perf summary for a static mesh. |
| `get_nanite_status` | `mesh_path` | large | 60s | Check whether Nanite is enabled on a static mesh and display available Nanite settings. |
| `set_lod_screen_size` | `mesh_path, lod_index, screen_size` | large | 60s | Set the screen size threshold for a specific LOD on a static mesh. |

## Landscape (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_landscape_info` | `—` | large | 60s | Get landscape bounds, component count, layer names, and material. |
| `get_landscape_layers` | `—` | large | 60s | List all paint layers on the landscape and their info. |
| `set_landscape_material` | `material_path` | small | 60s | Assign a material to the landscape in the current level. |

## Level Streaming (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_streaming_level` | `level_path` | small | 60s | Add a sub-level as a streaming level to the current persistent level. |
| `get_streaming_levels` | `—` | large | 60s | List all streaming levels with their load status. |
| `remove_streaming_level` | `level_path` | small | 60s | Remove a streaming level from the current persistent level. |

## Levels (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_current_level` | `—` | large | 60s | Get info about the currently open level: name, path, and actor count. |
| `open_level` | `level_path` | small | 60s | Open a level by asset path (e.g. '/Game/Maps/MainMenu'). |
| `save_current_level` | `—` | small | 60s | Save the currently open level. |

## Lighting (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_light` | `light_type, x, y, z, intensity, color` | small | 60s | Create a light actor. |
| `create_sky_atmosphere` | `—` | small | 60s | Create a full outdoor lighting setup: DirectionalLight (sun), SkyLight, SkyAtmosphere, ExponentialHeightFog. |
| `get_all_lights` | `—` | large | 60s | List all light actors in the current level with type, intensity, and location. |
| `set_light_properties` | `actor_label, intensity, color, temperature, attenuation_radius` | small | 60s | Set properties on a light actor. |
| `set_lightmap_resolution` | `actor_label, resolution` | small | 60s | Set lightmap resolution on a static mesh actor. |

## Material Instances (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_material_instance` | `parent_path, name, dest_path` | small | 60s | Create a new MaterialInstanceConstant from a parent material. |
| `get_material_instance_params` | `instance_path` | large | 60s | Get all parameter values (scalar, vector, texture) from a MaterialInstanceConstant. |
| `set_material_instance_scalar` | `instance_path, param_name, value` | small | 60s | Set a scalar parameter on a MaterialInstanceConstant. |
| `set_material_instance_vector` | `instance_path, param_name, r, g, b, a` | small | 60s | Set a vector (color) parameter on a MaterialInstanceConstant. |

## Materials (6)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_material_expression` | `material_path, expression_type, x, y` | large | 60s | Add a material expression node to a material. |
| `connect_material_to_property` | `material_path, expression_name, output_name, property_name` | large | 60s | Connect a material expression output to a material property input. |
| `get_material_complexity` | `material_path` | large | 60s | Compact perf summary for a material. |
| `get_material_expressions` | `material_path` | large | 60s | List all expression nodes in a material with their types. |
| `get_material_graph` | `material_path` | large | 60s | Inspect a material's full expression graph: every node with class, editor position, parameter name and default value (for Scalar/Vector/Texture parameters), input connections, and which expressions feed which root material properties (BaseColor, Metallic, Roughness, Normal, EmissiveColor, Opacity, OpacityMask, AmbientOcclusion, Specular). |
| `set_material_blend_mode` | `material_path, blend_mode` | large | 60s | Set the blend mode of a material. |

## Meta (2)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `list_tools` | `category` | large | 10s | List registered MCP tools at runtime. |
| `read_overflow` | `spill_path, offset, max_bytes` | large | 10s | Read a slice of a spilled envelope file. |

## Movie Render (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_render_job` | `sequence_path, output_dir, format, resolution` | small | 60s | Create a Movie Render Queue job for a LevelSequence. |
| `get_render_queue_status` | `—` | large | 60s | Get the status of Movie Render Queue jobs. |
| `render_sequence_to_images` | `sequence_path, output_dir, format` | small | 180s | Render a LevelSequence to an image sequence. |

## Navigation (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `build_navigation` | `—` | small | 180s | Rebuild the navigation mesh for the current level. |
| `get_navmesh_info` | `—` | large | 60s | Get navigation mesh information: NavMeshBoundsVolumes, RecastNavMesh settings (AgentRadius, AgentHeight, CellSize). |
| `set_navmesh_settings` | `agent_radius, agent_height, cell_size` | small | 60s | Set properties on the RecastNavMesh actor. |

## Niagara/VFX (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_niagara_system` | `name, path` | small | 60s | Create an empty Niagara particle system asset. |
| `get_niagara_info` | `system_path` | large | 60s | Get emitter count, parameter list, and basic info for a Niagara system. |
| `get_niagara_summary` | `system_path` | large | 60s | Compact perf summary for a Niagara system. |
| `set_niagara_parameter` | `system_path, param_name, value` | small | 60s | Set a user parameter on a Niagara system. |

## Notifications (2)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `log_to_output` | `message, category, verbosity` | small | 60s | Write a message to the UE Output Log with a category prefix. |
| `show_editor_notification` | `message, severity, duration` | small | 60s | Show a notification in the UE editor output log. |

## PCG (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_pcg_graph` | `name, path` | small | 60s | Create a PCG graph asset. |
| `execute_pcg_graph` | `actor_label` | small | 180s | Execute/regenerate a PCG graph on an actor that has a PCGComponent. |
| `get_pcg_graph_info` | `graph_path` | large | 60s | Get nodes, settings, and basic info for a PCG graph asset. |
| `get_pcg_volumes` | `—` | large | 60s | List all actors with PCG components in the current level. |

## PIE Runtime (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `execute_pie_console_command` | `command` | small | 60s | Execute a console command in the game world during PIE. |
| `get_pie_actors` | `class_filter` | large | 60s | Get actors in the game world during PIE. |
| `is_pie_running` | `—` | small | 60s | Check if a Play-In-Editor session is currently active. |
| `start_pie` | `—` | small | 60s | Start Play-In-Editor session. |
| `stop_pie` | `—` | small | 60s | Stop the current Play-In-Editor session. |

## Perf Audit (7)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `audit_map_perf` | `max_meshes, heavy_tris, high_tris_no_lod, instance_threshold, high_lightmap, many_materials, complex_material_exprs` | huge | 180s | Walk the open level and report prioritized perf outliers. |
| `audit_textures` | `max_textures, max_dim_warn, max_bytes_warn` | huge | 180s | Walk in-use materials, sum textures by dimensions + estimated bytes. |
| `get_actor_perf_signal` | `actor_label` | large | 60s | Per-actor perf-relevant facts: component count, tick group, mobility, casts shadow, lightmap-resolution overrides per StaticMeshComponent, material count per SMC, dynamic vs static lights on the actor. |
| `get_foliage_density_report` | `—` | large | 60s | Counts foliage and HISM/ISM instances grouped by mesh asset, total instance count, and unique-type count. |
| `get_light_summary` | `—` | large | 60s | Roll up every light in the open level by mobility, type, and shadow casting. |
| `get_lightmap_density_summary` | `—` | large | 60s | Roll up lightmap budget across all actors in the open level. |
| `get_texture_pool_status` | `—` | small | 30s | Texture streaming pool budget + utilization, plus dynamic resolution and a few other r.* texture cvars. |

## Physics (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_physics_info` | `actor_label` | large | 60s | Get mass, damping, and physics simulation state for an actor. |
| `set_collision_channel` | `actor_label, channel` | small | 60s | Set the object collision channel. |
| `set_physics_properties` | `actor_label, mass, linear_damping, angular_damping` | small | 60s | Set physics body properties. |
| `set_simulate_physics` | `actor_label, enabled` | small | 60s | Enable or disable physics simulation on an actor. |

## Post-Process (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_post_process_volume` | `x, y, z, infinite_extent` | small | 60s | Create a PostProcessVolume. |
| `get_post_process_settings` | `actor_label` | large | 60s | Get all overridden post-process settings on a PostProcessVolume. |
| `get_rendering_stats` | `—` | large | 60s | Get rendering statistics: draw calls, triangles, and shader complexity from the editor. |
| `set_color_grading` | `actor_label, saturation, contrast, gain, offset` | small | 60s | Set color grading on a PostProcessVolume. |
| `set_post_process_setting` | `actor_label, setting, value` | small | 60s | Set a post-process setting on a PostProcessVolume. |

## Project Settings (3)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_engine_version` | `—` | large | 60s | Get the Unreal Engine version, Python version, and OS platform info. |
| `get_project_setting` | `section, key, ini_file` | large | 60s | Read a project setting from an INI config file. |
| `set_project_setting` | `section, key, value, ini_file` | small | 60s | Write a project setting to an INI config file. |

## Sequencer (9)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_actor_to_sequence` | `sequence_path, actor_label` | large | 60s | Add an actor binding track to a LevelSequence. |
| `add_camera_cut` | `sequence_path, camera_label, start_frame` | large | 60s | Add a CameraCutTrack to a LevelSequence bound to a CameraActor. |
| `add_transform_key` | `sequence_path, actor_label, frame, x, y, z` | large | 60s | Add a transform (location) key to an actor's track in a LevelSequence at the given frame. |
| `create_sequence` | `name, path` | large | 60s | Create a new LevelSequence asset. |
| `get_sequence_info` | `sequence_path` | large | 60s | List all tracks, bindings, and playback range of a LevelSequence. |
| `get_sequence_tracks` | `sequence_path` | large | 60s | Get all tracks in a LevelSequence: master tracks, bindings, track names, types, and section counts. |
| `play_sequence` | `sequence_path` | large | 60s | Play/preview a LevelSequence in the editor. |
| `set_playback_range` | `sequence_path, start_frame, end_frame` | large | 60s | Set the playback range (start and end frames) of a LevelSequence. |
| `set_sequence_length` | `sequence_path, end_frame` | large | 60s | Set the playback range end frame of a LevelSequence. |

## Skeletal (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `attach_actor_to_socket` | `actor_label, parent_label, socket_name` | small | 60s | Attach an actor to a socket on another actor's skeletal mesh. |
| `create_socket` | `skeleton_path, bone_name, socket_name` | small | 60s | Create a socket on a bone in a Skeleton asset. |
| `get_skeletal_mesh_info` | `mesh_path` | large | 60s | Get vertex count, bone count, LOD count, materials, and physics asset for a SkeletalMesh. |
| `get_skeleton_info` | `skeleton_path` | large | 60s | Get bone hierarchy, socket list, and bone count for a Skeleton asset. |

## Source Control (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `check_in_asset` | `asset_path, description` | small | 60s | Check in an asset with a changelist description. |
| `check_out_asset` | `asset_path` | small | 60s | Check out an asset for editing in source control. |
| `get_source_control_status` | `asset_path` | large | 60s | Check the source control status of an asset (checked out, up to date, etc.). |
| `revert_asset` | `asset_path` | small | 60s | Revert an asset to the depot/repository version. |

## Splines (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `add_spline_point` | `actor_label, x, y, z, index` | small | 60s | Add a point to a spline. |
| `create_spline_actor` | `points, closed` | small | 60s | Create a spline actor with given points. |
| `get_spline_info` | `actor_label` | large | 60s | Get point count, length, and closed state of a spline on an actor. |
| `set_spline_mesh` | `actor_label, mesh_path` | small | 60s | Set a mesh that follows the spline by creating SplineMeshComponents along its length. |

## System (1)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_bobbot_status` | `—` | large | 60s | Get BobBot system status: backend type, bridge health, session info, MCP config, and environment. |

## Tags & Layers (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_actor_tags` | `actor_label` | large | 60s | Get all tags on an actor. |
| `get_actors_by_tag` | `tag` | large | 60s | Find all actors in the level with a specific tag. |
| `set_actor_layer` | `actor_label, layer_name` | small | 60s | Set the editor layer for an actor. |
| `set_actor_tags` | `actor_label, tags` | small | 60s | Set tags on an actor. |

## Texture & Mesh (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_texture_from_file` | `file_path, name, dest_path` | small | 60s | Import an image file (PNG, JPG, TGA, BMP, EXR) as a Texture2D asset. |
| `get_static_mesh_info` | `mesh_path` | large | 60s | Get vertex count, triangle count, LOD count, bounds, and materials for a static mesh. |
| `get_texture_info` | `texture_path` | large | 60s | Get resolution, format, compression settings, and size for a texture. |
| `set_material_texture_parameter` | `material_path, param_name, texture_path` | small | 60s | Set a texture parameter on a Material Instance. |
| `set_static_mesh_on_actor` | `actor_label, mesh_path` | small | 60s | Set the static mesh on a StaticMeshActor or any actor with a StaticMeshComponent. |

## Trace (6)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `delete_trace` | `paths` | small | 10s | Delete one or more `.utrace` files from the trace dir. |
| `list_traces` | `dir` | small | 10s | List `.utrace` files in the trace directory. |
| `open_trace_in_insights` | `path, foreground` | small | 10s | Launch UnrealInsights with the given `.utrace` loaded. |
| `start_trace` | `channels, file_name` | small | 30s | Start a `Trace.Start` recording into the trace dir. |
| `stop_trace` | `—` | small | 30s | Stop the current `Trace.Start` recording. |
| `summarize_trace` | `path` | large | 180s | Header-only summary of a `.utrace` file (trace-1 scope). |

## UMG/Widgets (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `create_widget_blueprint` | `name, parent_class, path` | large | 60s | Create a Widget Blueprint. |
| `create_widget_component` | `actor_label, widget_path` | large | 60s | Add a WidgetComponent to an actor that displays a Widget Blueprint. |
| `get_all_widget_blueprints` | `path` | large | 60s | List all Widget Blueprints in a path. |
| `get_widget_tree` | `widget_path` | large | 60s | List the widget hierarchy of a Widget Blueprint. |

## View Modes (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_show_flags` | `—` | large | 60s | Get the current show flags state. |
| `get_view_mode` | `—` | large | 60s | Get the current viewport rendering mode. |
| `set_show_flag` | `flag` | small | 60s | Toggle a show flag in the viewport. |
| `set_view_mode` | `mode` | small | 60s | Set the viewport rendering mode. |

## Viewport (5)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `capture_viewport` | `filename, width, height` | small | 60s | Take a screenshot of the current viewport. |
| `get_output_log` | `lines` | large | 60s | Get the last N lines from the UE output log. |
| `run_console_command` | `command` | small | 60s | Execute a UE console command. |
| `set_viewport_resolution` | `width, height` | small | 60s | Set the editor viewport resolution. |
| `toggle_realtime_rendering` | `—` | small | 60s | Toggle realtime rendering in the active viewport. |

## World (4)

| Tool | Parameters | Output | Timeout | What it does |
|------|-----------|--------|---------|-------------|
| `get_game_mode` | `—` | large | 60s | Get the current game mode class and its properties. |
| `get_world_settings` | `—` | large | 60s | Get world settings: gravity, game mode, kill Z, time dilation, and more. |
| `set_game_mode` | `game_mode_path` | small | 60s | Set the default game mode for the current level. |
| `set_world_setting` | `property_name, value` | small | 60s | Set a world setting by property name. |

<!-- AUTOGEN:TOOLS END -->

For the plugin's C++ and Python API, see [API.md](API.md). For Blueprint editing functions, see [BobBotLib.md](BobBotLib.md).
