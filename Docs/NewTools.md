# BobBot New Tools Plan

## Overview

BobBot currently has 23 MCP tools. CLAUDIUS has 130+ across 19 categories. This document plans 135 new tools bringing BobBot to 158, surpassing CLAUDIUS and covering every major area of UE game development.

All tools follow the same pattern: pure Python wrappers using `send_fn({"type": "execute", "code": ...})`. No C++ changes needed.

## Existing tools (23)

- **core.py** (2): execute_unreal_python, ping_unreal
- **actors.py** (6): get_selected_actors, get_level_actors, spawn_actor, delete_selected_actors, get_actor_properties, set_actor_property
- **assets.py** (4): search_assets, get_asset_info, create_blueprint, create_material
- **materials.py** (3): add_material_expression, connect_material_to_property, get_material_expressions
- **levels.py** (3): get_current_level, open_level, save_current_level
- **viewport.py** (3): capture_viewport, run_console_command, get_output_log
- **context.py** (2): get_project_info, get_editor_state

## New tools (135 across 22 files)

### asset_ops.py (6 tools)

| Tool | Description |
|------|-------------|
| `rename_asset(old_path, new_path)` | Rename/move an asset |
| `duplicate_asset(source_path, dest_path)` | Duplicate an asset |
| `delete_asset(asset_path)` | Delete an asset from Content Browser |
| `move_asset(source_path, dest_folder)` | Move asset to a different folder |
| `get_asset_references(asset_path)` | What references this asset |
| `get_asset_dependencies(asset_path)` | What this asset depends on |

### world.py (4 tools)

| Tool | Description |
|------|-------------|
| `get_world_settings()` | Returns gravity, game mode class, kill z, time dilation |
| `set_world_setting(property_name, value)` | Set a world setting by name |
| `get_game_mode()` | Returns current game mode class and properties |
| `set_game_mode(game_mode_path)` | Set default game mode for current level |

### sequencer.py (5 tools)

| Tool | Description |
|------|-------------|
| `create_sequence(name, path)` | Create a new LevelSequence asset |
| `get_sequence_info(sequence_path)` | List all tracks and their bound actors |
| `add_actor_to_sequence(sequence_path, actor_label)` | Add an actor binding track |
| `set_sequence_length(sequence_path, end_frame)` | Set playback range |
| `play_sequence(sequence_path)` | Play/preview a sequence in the editor |

### collision.py (3 tools)

| Tool | Description |
|------|-------------|
| `set_collision_preset(actor_label, preset_name)` | Set collision preset on an actor |
| `set_collision_enabled(actor_label, enabled)` | Enable or disable collision |
| `get_collision_info(actor_label)` | Get collision preset, enabled state, object type |

### import_export.py (3 tools)

| Tool | Description |
|------|-------------|
| `import_asset(file_path, destination_path)` | Import a file (FBX, OBJ, PNG, WAV, etc.) into Content Browser |
| `export_asset(asset_path, file_path)` | Export an asset to disk |
| `import_fbx(file_path, destination_path, import_animations)` | Import FBX with mesh and animation options |

### animation.py (5 tools)

| Tool | Description |
|------|-------------|
| `create_anim_blueprint(name, skeleton_path, path)` | Create an Animation Blueprint from a skeleton |
| `create_anim_montage(name, animation_path, path)` | Create an Animation Montage from an animation sequence |
| `create_blend_space_1d(name, skeleton_path, path)` | Create a 1D Blend Space |
| `get_skeleton_animations(skeleton_path)` | List all animation sequences for a skeleton |
| `get_anim_blueprint_info(anim_bp_path)` | Get info about an AnimBP: skeleton, state machines, variables |

### input_system.py (4 tools)

| Tool | Description |
|------|-------------|
| `create_input_action(name, value_type, path)` | Create an Enhanced Input Action (bool, float, Vector2D, Vector3D) |
| `create_input_mapping_context(name, path)` | Create an Input Mapping Context |
| `add_input_mapping(context_path, action_path, key_name)` | Map a key to an action in a mapping context |
| `get_input_actions(path)` | List all InputAction assets in a path |

### niagara.py (3 tools)

| Tool | Description |
|------|-------------|
| `create_niagara_system(name, path)` | Create an empty Niagara particle system |
| `get_niagara_info(system_path)` | Get emitter count and parameter list |
| `set_niagara_parameter(system_path, param_name, value)` | Set a user parameter on a Niagara system |

### landscape.py (3 tools)

| Tool | Description |
|------|-------------|
| `get_landscape_info()` | Get landscape bounds, component count, layer names, material |
| `set_landscape_material(material_path)` | Assign a material to the landscape |
| `get_landscape_layers()` | List all paint layers and their weights |

### audio.py (3 tools)

| Tool | Description |
|------|-------------|
| `create_sound_cue(name, sound_wave_path, path)` | Create a SoundCue from a SoundWave |
| `get_audio_assets(path)` | List all sound assets in a path |
| `set_actor_audio(actor_label, sound_path)` | Set audio component sound on an actor |

### data_assets.py (3 tools)

| Tool | Description |
|------|-------------|
| `create_data_table(name, struct_path, path)` | Create an empty DataTable with a row struct |
| `add_data_table_row(table_path, row_name, values_json)` | Add or update a row in a DataTable |
| `get_data_table_rows(table_path)` | List all rows in a DataTable with their values |

### physics.py (4 tools)

| Tool | Description |
|------|-------------|
| `set_simulate_physics(actor_label, enabled)` | Enable or disable physics simulation |
| `set_collision_channel(actor_label, channel)` | Set object collision channel |
| `get_physics_info(actor_label)` | Get mass, damping, simulate physics state |
| `set_physics_properties(actor_label, mass, linear_damping, angular_damping)` | Set physics body properties |

### editor_ops.py (8 tools)

| Tool | Description |
|------|-------------|
| `select_actors(actor_labels)` | Select actors in viewport by label (comma-separated) |
| `deselect_all()` | Clear viewport selection |
| `undo()` | Trigger editor undo |
| `redo()` | Trigger editor redo |
| `focus_on_actor(actor_label)` | Focus viewport camera on an actor |
| `set_actor_label(actor_label, new_label)` | Rename an actor in the level |
| `set_actor_folder(actor_label, folder_path)` | Organize actor into World Outliner folder |
| `get_editor_selection()` | Get full selection info |

### blueprint_advanced.py (6 tools)

| Tool | Description |
|------|-------------|
| `create_blueprint_function(blueprint_path, function_name, inputs, outputs)` | Add a custom function to a Blueprint |
| `create_blueprint_event(blueprint_path, event_name)` | Add a custom event to event graph |
| `get_blueprint_functions(blueprint_path)` | List all functions with inputs/outputs |
| `get_blueprint_components(blueprint_path)` | List all components in construction script |
| `set_blueprint_parent_class(blueprint_path, parent_class)` | Reparent a Blueprint |
| `create_blueprint_interface(name, path, functions)` | Create a Blueprint Interface |

### ai_tools.py (6 tools)

| Tool | Description |
|------|-------------|
| `create_behavior_tree(name, path)` | Create a Behavior Tree asset |
| `create_blackboard(name, path)` | Create a Blackboard Data asset |
| `add_blackboard_key(blackboard_path, key_name, key_type)` | Add a key to a Blackboard |
| `get_blackboard_keys(blackboard_path)` | List all keys in a Blackboard |
| `create_environment_query(name, path)` | Create an EQS query asset |
| `get_ai_assets(path)` | List all AI assets |

### pcg.py (4 tools)

| Tool | Description |
|------|-------------|
| `create_pcg_graph(name, path)` | Create a PCG graph asset |
| `get_pcg_graph_info(graph_path)` | Get nodes, edges, and settings |
| `execute_pcg_graph(actor_label)` | Execute a PCG graph on an actor |
| `get_pcg_volumes()` | List all PCG volume actors in current level |

### umg_widgets.py (4 tools)

| Tool | Description |
|------|-------------|
| `create_widget_blueprint(name, parent_class, path)` | Create a Widget Blueprint |
| `get_widget_tree(widget_path)` | List the widget hierarchy |
| `create_widget_component(actor_label, widget_path)` | Add Widget Component to an actor |
| `get_all_widget_blueprints(path)` | List all Widget Blueprints |

### pie.py (5 tools)

| Tool | Description |
|------|-------------|
| `start_pie()` | Start Play-In-Editor |
| `stop_pie()` | Stop Play-In-Editor |
| `is_pie_running()` | Check if PIE is active |
| `get_pie_actors(class_filter)` | Get actors in game world during PIE |
| `execute_pie_console_command(command)` | Execute console command in game world |

### source_control.py (4 tools)

| Tool | Description |
|------|-------------|
| `get_source_control_status(asset_path)` | Check if asset is checked out, etc. |
| `check_out_asset(asset_path)` | Check out an asset for editing |
| `check_in_asset(asset_path, description)` | Check in with changelist description |
| `revert_asset(asset_path)` | Revert to depot version |

### build.py (4 tools)

| Tool | Description |
|------|-------------|
| `build_lighting(quality)` | Build lighting (Preview/Medium/High/Production) |
| `compile_blueprints()` | Compile all dirty Blueprints |
| `validate_assets(path)` | Run asset validation |
| `get_map_check_errors()` | Run Map Check and return errors |

### texture_mesh.py (5 tools)

| Tool | Description |
|------|-------------|
| `get_static_mesh_info(mesh_path)` | Get vertex/triangle/LOD count, bounds, materials |
| `set_static_mesh_on_actor(actor_label, mesh_path)` | Set mesh on a StaticMeshActor |
| `get_texture_info(texture_path)` | Get resolution, format, compression, size |
| `create_texture_from_file(file_path, name, path)` | Import image as Texture2D |
| `set_material_texture_parameter(material_path, param_name, texture_path)` | Set texture parameter on material instance |

### foliage.py (3 tools)

| Tool | Description |
|------|-------------|
| `get_foliage_types()` | List all foliage types and their settings |
| `add_foliage_type(static_mesh_path)` | Register a static mesh as foliage type |
| `get_foliage_stats()` | Get total instance count and breakdown |

### post_process.py (5 tools)

| Tool | Description |
|------|-------------|
| `create_post_process_volume(x, y, z, infinite_extent)` | Create a PostProcessVolume |
| `set_post_process_setting(actor_label, setting, value)` | Set a post-process setting |
| `get_post_process_settings(actor_label)` | Get all overridden settings |
| `set_color_grading(actor_label, saturation, contrast, gain, offset)` | Set color grading |
| `get_rendering_stats()` | Get draw calls, triangles, shader complexity |

### camera.py (4 tools)

| Tool | Description |
|------|-------------|
| `create_camera(x, y, z, yaw, pitch, fov)` | Create a CameraActor |
| `set_camera_properties(actor_label, fov, aperture, focus_distance)` | Set camera lens properties |
| `get_active_viewport_camera()` | Get editor viewport camera location/rotation/FOV |
| `set_viewport_camera(x, y, z, yaw, pitch)` | Teleport editor viewport camera |

### skeletal.py (4 tools)

| Tool | Description |
|------|-------------|
| `get_skeleton_info(skeleton_path)` | Get bone hierarchy, sockets, bone count |
| `get_skeletal_mesh_info(mesh_path)` | Get vertex/bone/LOD count, materials, physics asset |
| `create_socket(skeleton_path, bone_name, socket_name)` | Create a socket on a bone |
| `attach_actor_to_socket(actor_label, parent_label, socket_name)` | Attach actor to socket |

### level_streaming.py (3 tools)

| Tool | Description |
|------|-------------|
| `add_streaming_level(level_path)` | Add a sub-level as streaming level |
| `remove_streaming_level(level_path)` | Remove a streaming level |
| `get_streaming_levels()` | List all streaming levels with load status |

### splines.py (4 tools)

| Tool | Description |
|------|-------------|
| `create_spline_actor(points, closed)` | Create a spline actor with given points |
| `get_spline_info(actor_label)` | Get point count, length, closed state |
| `add_spline_point(actor_label, x, y, z, index)` | Add a point to a spline |
| `set_spline_mesh(actor_label, mesh_path)` | Set mesh that follows the spline |

### lighting.py (5 tools)

| Tool | Description |
|------|-------------|
| `create_light(light_type, x, y, z, intensity, color)` | Create a light actor |
| `set_light_properties(actor_label, intensity, color, temperature, attenuation_radius)` | Set light properties |
| `get_all_lights()` | List all lights with type, intensity, location |
| `set_lightmap_resolution(actor_label, resolution)` | Set lightmap resolution on mesh |
| `create_sky_atmosphere()` | Create full outdoor lighting setup |

### components.py (4 tools)

| Tool | Description |
|------|-------------|
| `add_component_to_actor(actor_label, component_type, component_name)` | Add a component to a level actor |
| `remove_component(actor_label, component_name)` | Remove a component |
| `get_component_properties(actor_label, component_name)` | Get component properties |
| `set_component_property(actor_label, component_name, property_name, value)` | Set component property |

### tags_layers.py (4 tools)

| Tool | Description |
|------|-------------|
| `set_actor_tags(actor_label, tags)` | Set tags on an actor (comma-separated) |
| `get_actors_by_tag(tag)` | Find all actors with a specific tag |
| `set_actor_layer(actor_label, layer_name)` | Set editor layer for an actor |
| `get_actor_tags(actor_label)` | Get all tags on an actor |

### movie_render.py (3 tools)

| Tool | Description |
|------|-------------|
| `create_render_job(sequence_path, output_dir, format, resolution)` | Create a Movie Render Queue job |
| `get_render_queue_status()` | Get status of render queue jobs |
| `render_sequence_to_images(sequence_path, output_dir, format)` | Render sequence to image sequence |

### debug_profiling.py (4 tools)

| Tool | Description |
|------|-------------|
| `get_frame_stats()` | Get FPS, frame time, draw calls, triangle count |
| `get_memory_stats()` | Get memory usage breakdown |
| `get_gpu_stats()` | Get GPU time, render target count, shader complexity |
| `benchmark_scene(duration_seconds)` | Run benchmark and capture averages |

## Test harness

`_test_tools.py` registers a `run_bobbot_tests` MCP tool that tests every tool against a live editor. Each test creates, verifies, and cleans up. Returns pass/fail summary.

## Totals

| Category | File | Count |
|----------|------|-------|
| Existing | core, actors, assets, materials, levels, viewport, context | 23 |
| Asset Operations | asset_ops.py | 6 |
| World | world.py | 4 |
| Sequencer | sequencer.py | 5 |
| Collision | collision.py | 3 |
| Import/Export | import_export.py | 3 |
| Animation | animation.py | 5 |
| Enhanced Input | input_system.py | 4 |
| Niagara/VFX | niagara.py | 3 |
| Landscape | landscape.py | 3 |
| Audio | audio.py | 3 |
| Data Tables | data_assets.py | 3 |
| Physics | physics.py | 4 |
| Editor Ops | editor_ops.py | 8 |
| Blueprint Advanced | blueprint_advanced.py | 6 |
| AI/Behavior | ai_tools.py | 6 |
| PCG | pcg.py | 4 |
| UMG/Widgets | umg_widgets.py | 4 |
| PIE Runtime | pie.py | 5 |
| Source Control | source_control.py | 4 |
| Build | build.py | 4 |
| Texture/Mesh | texture_mesh.py | 5 |
| Foliage | foliage.py | 3 |
| Post-Process | post_process.py | 5 |
| Camera | camera.py | 4 |
| Skeletal Mesh | skeletal.py | 4 |
| Level Streaming | level_streaming.py | 3 |
| Splines | splines.py | 4 |
| Lighting | lighting.py | 5 |
| Components | components.py | 4 |
| Tags/Layers | tags_layers.py | 4 |
| Movie Render | movie_render.py | 3 |
| Debug/Profiling | debug_profiling.py | 4 |
| Test Harness | _test_tools.py | 1 |
| **Total** | | **158** |
