# MCP Tool Reference

BobBot has 200 MCP tools organized by category. You don't call these directly. BobBot picks the right tool based on what you ask.

For the plugin's C++ and Python API, see [API.md](API.md). For Blueprint editing functions, see [BobBotLib.md](BobBotLib.md).

## Actors (6)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_selected_actors` | - | List all selected actors with labels, classes, and locations |
| `get_level_actors` | `class_filter?` | List all actors in the level, optionally filtered by class name |
| `spawn_actor` | `class_path, x, y, z, yaw?, pitch?, roll?` | Spawn an actor from a class path or Blueprint path |
| `delete_selected_actors` | - | Delete all selected actors |
| `get_actor_properties` | `actor_label` | Get an actor's location, rotation, scale, and components |
| `set_actor_property` | `actor_label, property_name, value` | Set a property on an actor (e.g., RelativeLocation, bHidden) |

## Assets (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `search_assets` | `path, name_filter?, type_filter?` | Search for assets by path and type |
| `get_asset_info` | `asset_path` | Get detailed info about an asset |
| `create_blueprint` | `name, parent_class, path` | Create a new Blueprint asset |
| `create_material` | `name, path` | Create a new Material asset |

## Asset Operations (6)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `rename_asset` | `old_path, new_path` | Rename an asset |
| `duplicate_asset` | `source_path, dest_path` | Duplicate an asset |
| `delete_asset` | `asset_path` | Delete an asset |
| `move_asset` | `source_path, dest_folder` | Move an asset to a different folder |
| `get_asset_references` | `asset_path` | Find all assets that reference this one |
| `get_asset_dependencies` | `asset_path` | Find all assets this one depends on |

## Materials (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `add_material_expression` | `material_path, expression_type, x, y` | Add a node to a material graph |
| `connect_material_to_property` | `material_path, expression_name, output_name, property_name` | Wire a node output to a material property |
| `get_material_expressions` | `material_path` | List all nodes in a material |

## Levels (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_current_level` | - | Get current level name and path |
| `open_level` | `level_path` | Open a level |
| `save_current_level` | - | Save the current level |

## Level Streaming (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `add_streaming_level` | `level_path` | Add a streaming sub-level |
| `remove_streaming_level` | `level_path` | Remove a streaming level |
| `get_streaming_levels` | - | List all streaming levels |

## Viewport (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `capture_viewport` | `filename?, width?, height?` | Take a screenshot of the viewport |
| `run_console_command` | `command` | Execute a UE console command |
| `get_output_log` | `lines?` | Read the last N lines of the output log |

## Context (2)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_project_info` | - | Project name, engine version, plugins, modules, content |
| `get_editor_state` | - | Selected actors, viewport info, content browser selection |

## Core (2)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `execute_unreal_python` | `code` | Run arbitrary Python in the editor (fallback for anything tools don't cover) |
| `ping_unreal` | - | Check if the editor is responsive |

## Editor Operations (8)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `select_actors` | `actor_labels` | Select actors by label (comma-separated) |
| `deselect_all` | - | Clear viewport selection |
| `undo` | - | Undo last action |
| `redo` | - | Redo last undone action |
| `focus_on_actor` | `actor_label` | Focus viewport camera on an actor |
| `set_actor_label` | `actor_label, new_label` | Rename an actor |
| `set_actor_folder` | `actor_label, folder_path` | Move actor to a World Outliner folder |
| `get_editor_selection` | - | Full selection info (actors + content browser) |

## Tags & Layers (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `set_actor_tags` | `actor_label, tags` | Set tags on an actor |
| `get_actors_by_tag` | `tag` | Find all actors with a specific tag |
| `set_actor_layer` | `actor_label, layer_name` | Set an actor's editor layer |
| `get_actor_tags` | `actor_label` | Get an actor's tags |

## Components (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `add_component_to_actor` | `actor_label, component_type, component_name` | Add a component to a placed actor |
| `remove_component` | `actor_label, component_name` | Remove a component |
| `get_component_properties` | `actor_label, component_name` | Inspect a component's properties |
| `set_component_property` | `actor_label, component_name, property_name, value` | Set a property on a component |

## World (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_world_settings` | - | Get gravity, kill Z, and other world settings |
| `set_world_setting` | `property_name, value` | Set a world setting |
| `get_game_mode` | - | Get the current game mode class and defaults |
| `set_game_mode` | `game_mode_path` | Set the level's game mode |

## Collision (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `set_collision_preset` | `actor_label, preset_name` | Set collision preset (BlockAll, OverlapAll, etc.) |
| `set_collision_enabled` | `actor_label, enabled` | Enable or disable collision |
| `get_collision_info` | `actor_label` | Get collision settings and channels |

## Physics (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `set_simulate_physics` | `actor_label, enabled` | Enable or disable physics simulation |
| `set_collision_channel` | `actor_label, channel` | Set collision channel (WorldStatic, Pawn, etc.) |
| `get_physics_info` | `actor_label` | Get mass, damping, and physics state |
| `set_physics_properties` | `actor_label, mass, linear_damping, angular_damping` | Set mass and damping |

## Lighting (5)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_light` | `light_type, x, y, z, intensity?, color?` | Create a light (Point, Spot, Directional, Rect, Sky) |
| `set_light_properties` | `actor_label, intensity?, color?, temperature?, attenuation_radius?` | Adjust light properties |
| `get_all_lights` | - | List all lights in the level |
| `set_lightmap_resolution` | `actor_label, resolution` | Set lightmap resolution on a mesh |
| `create_sky_atmosphere` | - | Create a full outdoor lighting setup (sky, sun, fog) |

## Camera (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_camera` | `x, y, z, yaw?, pitch?, fov?` | Create a camera actor |
| `set_camera_properties` | `actor_label, fov?, aperture?, focus_distance?` | Adjust camera settings |
| `get_active_viewport_camera` | - | Get current viewport camera position |
| `set_viewport_camera` | `x, y, z, yaw, pitch` | Move viewport camera |

## Texture & Mesh (5)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_static_mesh_info` | `mesh_path` | LODs, bounds, materials, triangle counts |
| `set_static_mesh_on_actor` | `actor_label, mesh_path` | Swap the mesh on a static mesh actor |
| `get_texture_info` | `texture_path` | Resolution, format, compression |
| `create_texture_from_file` | `file_path, name, dest_path` | Import an image as a texture asset |
| `set_material_texture_parameter` | `material_path, param_name, texture_path` | Set a texture parameter on a material |

## Import/Export (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `import_asset` | `file_path, destination_path` | Import a file (FBX, OBJ, PNG, WAV) |
| `export_asset` | `asset_path, file_path` | Export an asset to disk |
| `import_fbx` | `file_path, destination_path, import_animations?` | Import FBX with options |

## Post-Process (5)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_post_process_volume` | `x?, y?, z?, infinite_extent?` | Create a PostProcessVolume |
| `set_post_process_setting` | `actor_label, setting, value` | Set a post-process setting |
| `get_post_process_settings` | `actor_label` | List overridden settings |
| `set_color_grading` | `actor_label, saturation?, contrast?, gain?, offset?` | Adjust color grading |
| `get_rendering_stats` | - | Draw calls, mesh counts, light counts |

## Splines (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_spline_actor` | `points, closed?` | Create a spline actor from points |
| `get_spline_info` | `actor_label` | Inspect spline points and length |
| `add_spline_point` | `actor_label, x, y, z, index?` | Add a point to a spline |
| `set_spline_mesh` | `actor_label, mesh_path` | Repeat a mesh along a spline |

## Data Tables (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_data_table` | `name, struct_path, path` | Create a DataTable asset |
| `add_data_table_row` | `table_path, row_name, values_json` | Add or update a row |
| `get_data_table_rows` | `table_path` | List all rows |

## Skeletal Mesh (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_skeleton_info` | `skeleton_path` | Bone hierarchy and count |
| `get_skeletal_mesh_info` | `mesh_path` | LODs, materials, bone count |
| `create_socket` | `skeleton_path, bone_name, socket_name` | Create a socket on a skeleton |
| `attach_actor_to_socket` | `actor_label, parent_label, socket_name` | Attach an actor to a socket |

## Sequencer (5)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_sequence` | `name, path` | Create a LevelSequence |
| `get_sequence_info` | `sequence_path` | Tracks, bindings, length |
| `add_actor_to_sequence` | `sequence_path, actor_label` | Add an actor binding |
| `set_sequence_length` | `sequence_path, end_frame` | Set sequence duration |
| `play_sequence` | `sequence_path` | Play/preview the sequence |

## Animation (5)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_anim_blueprint` | `name, skeleton_path, path` | Create an AnimBlueprint |
| `create_anim_montage` | `name, animation_path, path` | Create an AnimMontage |
| `create_blend_space_1d` | `name, skeleton_path, path` | Create a 1D BlendSpace |
| `get_skeleton_animations` | `skeleton_path` | List all animations for a skeleton |
| `get_anim_blueprint_info` | `anim_bp_path` | Skeleton, variables, state machines |

## Blueprint Advanced (6)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_blueprint_function` | `blueprint_path, function_name, inputs?, outputs?` | Add a function to a Blueprint. inputs/outputs are comma-separated `name:type` pairs. |
| `create_blueprint_event` | `blueprint_path, event_name` | Add a custom event |
| `get_blueprint_functions` | `blueprint_path` | List all functions and event graphs |
| `get_blueprint_components` | `blueprint_path` | List all components |
| `set_blueprint_parent_class` | `blueprint_path, parent_class` | Reparent a Blueprint |
| `create_blueprint_interface` | `name, path, functions` | Create a Blueprint Interface |

## Enhanced Input (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_input_action` | `name, value_type, path` | Create an Input Action |
| `create_input_mapping_context` | `name, path` | Create an Input Mapping Context |
| `add_input_mapping` | `context_path, action_path, key_name` | Map a key to an action |
| `get_input_actions` | `path?` | List input assets |

## Audio (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_sound_cue` | `name, sound_wave_path, path` | Create a SoundCue |
| `get_audio_assets` | `path?` | List audio assets |
| `set_actor_audio` | `actor_label, sound_path` | Attach audio to an actor |

## Landscape (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_landscape_info` | - | Landscape dimensions, components, layers |
| `set_landscape_material` | `material_path` | Set the landscape material |
| `get_landscape_layers` | - | List paint layers |

## Foliage (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_foliage_types` | - | List registered foliage types |
| `add_foliage_type` | `static_mesh_path` | Register a mesh as paintable foliage |
| `get_foliage_stats` | - | Instance counts per type |

## Niagara/VFX (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_niagara_system` | `name, path` | Create a particle system |
| `get_niagara_info` | `system_path` | Emitters and exposed parameters |
| `set_niagara_parameter` | `system_path, param_name, value` | Set a user parameter |

## AI/Behavior (6)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_behavior_tree` | `name, path` | Create a Behavior Tree |
| `create_blackboard` | `name, path` | Create a Blackboard |
| `add_blackboard_key` | `blackboard_path, key_name, key_type` | Add a key to a Blackboard |
| `get_blackboard_keys` | `blackboard_path` | List all keys |
| `create_environment_query` | `name, path` | Create an EQS query |
| `get_ai_assets` | `path?` | List AI-related assets |

## PCG (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_pcg_graph` | `name, path` | Create a PCG graph |
| `get_pcg_graph_info` | `graph_path` | Inspect graph nodes |
| `execute_pcg_graph` | `actor_label` | Run PCG generation on an actor |
| `get_pcg_volumes` | - | List PCG volumes in the level |

## UMG/Widgets (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_widget_blueprint` | `name, parent_class, path` | Create a Widget Blueprint |
| `get_widget_tree` | `widget_path` | Inspect widget hierarchy |
| `create_widget_component` | `actor_label, widget_path` | Add a widget component to an actor |
| `get_all_widget_blueprints` | `path?` | List widget Blueprints |

## PIE Runtime (5)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `start_pie` | - | Start Play-In-Editor |
| `stop_pie` | - | Stop PIE |
| `is_pie_running` | - | Check if PIE is active |
| `get_pie_actors` | `class_filter?` | List actors in the game world |
| `execute_pie_console_command` | `command` | Run a console command during PIE |

## Source Control (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_source_control_status` | `asset_path` | Check if an asset is checked out |
| `check_out_asset` | `asset_path` | Check out an asset |
| `check_in_asset` | `asset_path, description` | Check in with a description |
| `revert_asset` | `asset_path` | Revert to last checked-in version |

## Build (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `build_lighting` | `quality?` | Build lighting for the level |
| `compile_blueprints` | - | Compile all Blueprints |
| `validate_assets` | `path?` | Run asset validation |
| `get_map_check_errors` | - | List map check errors and warnings |

## Movie Render (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_render_job` | `sequence_path, output_dir, format?, resolution?` | Create a render job |
| `get_render_queue_status` | - | Check render queue |
| `render_sequence_to_images` | `sequence_path, output_dir, format?` | Render a sequence to image files |

## Debug/Profiling (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_frame_stats` | - | FPS, frame time, draw calls |
| `get_memory_stats` | - | Memory usage breakdown |
| `get_gpu_stats` | - | GPU render time and utilization |
| `benchmark_scene` | `duration_seconds?` | Run a timed benchmark |

## System (1)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_bobbot_status` | - | Diagnostic info (backend, model, session, context) |

## Console Variables (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_cvar` | `name` | Get the current value of a console variable |
| `set_cvar` | `name, value` | Set a console variable |
| `list_cvars` | `pattern?` | List CVars matching a pattern |
| `reset_cvar` | `name` | Reset a CVar to its default value |

## View Modes (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `set_view_mode` | `mode` | Switch viewport (Lit, Unlit, Wireframe, DetailLighting, etc.) |
| `get_view_mode` | - | Get current viewport view mode |
| `set_show_flag` | `flag, enabled` | Toggle a show flag (Collision, Bounds, Grid, Navigation, etc.) |
| `get_show_flags` | - | List active show flags |

## Material Instances (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_material_instance` | `parent_path, name, dest_path?` | Create a Material Instance from a parent material |
| `set_material_instance_scalar` | `instance_path, param_name, value` | Set a scalar parameter |
| `set_material_instance_vector` | `instance_path, param_name, r, g, b, a?` | Set a vector/color parameter |
| `get_material_instance_params` | `instance_path` | List all parameters and current values |

## Content Browser (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `create_content_folder` | `path` | Create a folder in the Content Browser |
| `delete_content_folder` | `path` | Delete an empty folder |
| `find_unused_assets` | `path?` | Find assets with zero references |
| `get_asset_size_report` | `path?` | Disk size breakdown by asset type |

## LOD (4)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_lod_info` | `mesh_path` | LOD count, screen sizes, triangle counts |
| `set_lod_screen_size` | `mesh_path, lod_index, screen_size` | Set LOD distance threshold |
| `auto_generate_lods` | `mesh_path, num_lods?` | Auto-generate LODs |
| `get_nanite_status` | `mesh_path` | Check Nanite status and settings |

## Navigation (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `build_navigation` | - | Build the navigation mesh |
| `get_navmesh_info` | - | Nav bounds, agent radius/height, cell size |
| `set_navmesh_settings` | `agent_radius?, agent_height?, cell_size?` | Configure navmesh settings |

## Project Settings (3)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `get_project_setting` | `section, key, ini_file?` | Read a project config setting |
| `set_project_setting` | `section, key, value, ini_file?` | Write a project config setting |
| `get_engine_version` | - | Engine version, Python version, platform |

## Notifications (2)

| Tool | Parameters | What it does |
|------|-----------|-------------|
| `show_editor_notification` | `message, severity?, duration?` | Show a notification in the editor |
| `log_to_output` | `message, category?, verbosity?` | Write a categorized message to the Output Log |

## Notes

- Parameters with `?` are optional.
- Actor parameters accept the actor's display label (as shown in the World Outliner).
- Asset paths use the `/Game/Folder/Name` format.
- Class paths use `/Script/ModuleName.ClassName` for C++ classes or `/Game/Path/BP_Name` for Blueprints.
- For Blueprint editing beyond what MCP tools offer, BobBot uses [BobBotLib](BobBotLib.md) via `execute_unreal_python`.
