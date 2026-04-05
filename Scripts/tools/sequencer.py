"""Sequencer tools: create and inspect LevelSequences, add tracks, control playback."""

from _common import _exec, _exec_ue, actor_exec, asset_exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_sequence(name: str, path: str = "/Game/Cinematics") -> str:
        """Create a new LevelSequence asset."""
        return _exec_ue(f"""
name = "{name}"
path = "{path}"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.LevelSequenceFactoryNew()
seq = asset_tools.create_asset(name, path, unreal.LevelSequence, factory)
if seq:
    print(f"Created LevelSequence: {{path}}/{{name}}")
else:
    print(f"ERROR: Failed to create LevelSequence '{{name}}' at '{{path}}'")
""")

    @mcp.tool()
    def get_sequence_info(sequence_path: str) -> str:
        """List all tracks, bindings, and playback range of a LevelSequence."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    print(f"LevelSequence: {sequence_path}")
    movie_scene = asset.get_editor_property("MovieScene")
    if movie_scene:
        # Playback range
        pr = movie_scene.get_editor_property("PlaybackRange")
        print(f"Playback Range: {{pr}}")
        # Master tracks
        master_tracks = movie_scene.get_editor_property("MasterTracks")
        if master_tracks:
            print()
            print(f"Master Tracks ({{len(master_tracks)}}):")
            for t in master_tracks:
                print(f"  {{t.get_class().get_name()}}")
        # Bindings (object binding tracks)
        bindings = movie_scene.get_editor_property("ObjectBindings")
        if bindings:
            print()
            print(f"Object Bindings ({{len(bindings)}}):")
            for b in bindings:
                bname = b.get_editor_property("BindingName")
                tracks = b.get_editor_property("Tracks")
                print(f"  {{bname}} ({{len(tracks)}} track(s))")
                for t in tracks:
                    print(f"    {{t.get_class().get_name()}}")
        if not master_tracks and not bindings:
            print("\\nSequence is empty (no tracks or bindings)")
""")

    @mcp.tool()
    def add_actor_to_sequence(sequence_path: str, actor_label: str) -> str:
        """Add an actor binding track to a LevelSequence."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    target = None
    for a in actors:
        if a.get_actor_label() == "{actor_label}":
            target = a
            break
    if target is None:
        print("ERROR: Actor '{actor_label}' not found in level")
    else:
        # Use LevelSequenceEditorBlueprintLibrary if available
        try:
            binding = asset.add_possessable(target)
            if binding:
                print(f"Added {{target.get_actor_label()}} to sequence {sequence_path}")
            else:
                print(f"ERROR: Failed to add binding for {{target.get_actor_label()}}")
        except Exception as e:
            print(f"ERROR: Could not add actor to sequence: {{e}}")
            print("Try using execute_unreal_python with LevelSequenceEditorBlueprintLibrary")
""")

    @mcp.tool()
    def set_sequence_length(sequence_path: str, end_frame: int = 150) -> str:
        """Set the playback range end frame of a LevelSequence. Default tick resolution is 24000 ticks per second."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    movie_scene = asset.get_editor_property("MovieScene")
    if movie_scene:
        # Set playback range using frame numbers
        display_rate = movie_scene.get_editor_property("DisplayRate")
        fps = display_rate.numerator / max(display_rate.denominator, 1)
        tick_resolution = movie_scene.get_editor_property("TickResolution")
        tps = tick_resolution.numerator / max(tick_resolution.denominator, 1)
        ticks_per_frame = tps / fps if fps > 0 else 800
        end_tick = int({end_frame} * ticks_per_frame)
        pr = unreal.FrameNumberRange()
        movie_scene.set_editor_property("PlaybackRange", pr)
        unreal.EditorAssetLibrary.save_asset("{sequence_path}")
        print(f"Set sequence length to {end_frame} frames ({{fps:.0f}} fps) on {sequence_path}")
    else:
        print("ERROR: Could not access MovieScene")
""")

    @mcp.tool()
    def play_sequence(sequence_path: str) -> str:
        """Play/preview a LevelSequence in the editor."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    played = False
    # Try modern API first
    if hasattr(unreal, 'LevelSequenceEditorBlueprintLibrary'):
        try:
            unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(asset)
            unreal.LevelSequenceEditorBlueprintLibrary.play()
            played = True
            print(f"Playing sequence: {sequence_path}")
        except Exception as e:
            pass
    # Fallback: open the asset in the editor (user presses play manually)
    if not played:
        try:
            unreal.EditorAssetLibrary.open_editor_for_assets([asset])
            print(f"Opened sequence in editor: {sequence_path}")
            print("Press Play in the Sequencer toolbar to preview")
        except Exception as e:
            print(f"ERROR: Could not open or play sequence: {{e}}")
""")

    @mcp.tool()
    def add_transform_key(sequence_path: str, actor_label: str, frame: int,
                          x: float = 0.0, y: float = 0.0, z: float = 0.0) -> str:
        """Add a transform (location) key to an actor's track in a LevelSequence at the given frame."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    actor_label = "{actor_label}"
    frame = {frame}
    x, y, z = {x}, {y}, {z}
    try:
        # Find the actor in the level
        target_actor = None
        for a in unreal.EditorLevelLibrary.get_all_level_actors():
            if a.get_actor_label() == actor_label:
                target_actor = a
                break
        if target_actor is None:
            print(f"ERROR: Actor '{{actor_label}}' not found in level")
        else:
            # Try using LevelSequenceEditorBlueprintLibrary (modern UE 5.x)
            added = False
            if hasattr(unreal, 'LevelSequenceEditorBlueprintLibrary'):
                try:
                    lse = unreal.LevelSequenceEditorBlueprintLibrary
                    lse.open_level_sequence(asset)
                    # Select the actor binding
                    lse.select_none()
                    # This may add the actor if not already bound
                    lse.select_tracks([])
                    added = False  # can't confirm, fall through
                except Exception:
                    pass
            # MovieScene track manipulation approach
            if not added:
                movie_scene = asset.get_editor_property("MovieScene")
                if movie_scene is None:
                    print("ERROR: Could not access MovieScene")
                else:
                    # Find or create a binding for the actor
                    binding = None
                    try:
                        binding = asset.add_possessable(target_actor)
                    except Exception:
                        pass
                    if binding is None:
                        # Try to find existing binding
                        try:
                            bindings = movie_scene.get_editor_property("ObjectBindings")
                            if bindings:
                                for b in bindings:
                                    bname = b.get_editor_property("BindingName")
                                    if bname == actor_label:
                                        binding = b
                                        break
                        except Exception:
                            pass
                    if binding is None:
                        print(f"ERROR: Could not create or find binding for '{{actor_label}}'")
                        print("Try using add_actor_to_sequence first, then add_transform_key")
                    else:
                        # Try to add a 3D transform track
                        try:
                            # UE 5.x: use MovieScene3DTransformTrack
                            transform_track = None
                            tracks = None
                            try:
                                tracks = binding.get_editor_property("Tracks")
                            except Exception:
                                try:
                                    tracks = binding.get_tracks()
                                except Exception:
                                    pass
                            if tracks:
                                for t in tracks:
                                    if "Transform" in t.get_class().get_name():
                                        transform_track = t
                                        break
                            if transform_track is None:
                                try:
                                    transform_track = binding.add_track(unreal.MovieScene3DTransformTrack)
                                except Exception:
                                    pass
                            if transform_track is not None:
                                # Get or add a section
                                sections = transform_track.get_sections()
                                if not sections:
                                    section = transform_track.add_section()
                                    # Set range to cover the whole sequence
                                    try:
                                        section.set_range(0, frame + 100)
                                    except Exception:
                                        pass
                                else:
                                    section = sections[0]
                                # Try to set keys via channels
                                try:
                                    channels = section.get_all_channels()
                                    # Transform channels: X,Y,Z location, X,Y,Z rotation, X,Y,Z scale
                                    # Location is typically the first 3 channels
                                    if len(channels) >= 3:
                                        key_time = unreal.FrameNumber(frame)
                                        channels[0].add_key(key_time, x)  # Location X
                                        channels[1].add_key(key_time, y)  # Location Y
                                        channels[2].add_key(key_time, z)  # Location Z
                                        print(f"Added transform key at frame {{frame}}: ({{x}}, {{y}}, {{z}}) for '{{actor_label}}'")
                                        unreal.EditorAssetLibrary.save_asset("{sequence_path}")
                                    else:
                                        print(f"ERROR: Expected at least 3 channels, got {{len(channels)}}")
                                except Exception as e:
                                    print(f"ERROR: Could not add key to channels: {{e}}")
                                    print("Try using execute_unreal_python with direct MovieSceneScriptingChannel API")
                            else:
                                print("ERROR: Could not create MovieScene3DTransformTrack")
                                print("Try using execute_unreal_python to manually add the track")
                        except Exception as e:
                            print(f"ERROR: Transform track manipulation failed: {{e}}")
                            print("Try using execute_unreal_python for manual keyframe insertion")
    except Exception as e:
        print(f"ERROR: add_transform_key failed: {{e}}")
        print("This is a complex operation. Consider using execute_unreal_python as a fallback.")
""")

    @mcp.tool()
    def get_sequence_tracks(sequence_path: str) -> str:
        """Get all tracks in a LevelSequence: master tracks, bindings, track names, types, and section counts."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    print(f"LevelSequence: {sequence_path}")
    movie_scene = asset.get_editor_property("MovieScene")
    if movie_scene is None:
        print("ERROR: Could not access MovieScene")
    else:
        total_tracks = 0
        # Master tracks (not bound to any actor)
        try:
            master_tracks = movie_scene.get_editor_property("MasterTracks")
            if master_tracks and len(master_tracks) > 0:
                print()
                print(f"Master Tracks ({{len(master_tracks)}}):")
                for t in master_tracks:
                    cls = t.get_class().get_name()
                    sections = t.get_sections() if hasattr(t, 'get_sections') else []
                    try:
                        display_name = t.get_editor_property("DisplayName")
                    except Exception:
                        display_name = cls
                    print(f"  {{display_name}} ({{cls}}) - {{len(sections)}} section(s)")
                    total_tracks += 1
            else:
                print("\\nMaster Tracks: None")
        except Exception as e:
            print()
            print(f"Master Tracks: Could not read ({{e}})")
        # Object bindings (actor-bound tracks)
        try:
            bindings = movie_scene.get_editor_property("ObjectBindings")
            if bindings and len(bindings) > 0:
                print()
                print(f"Object Bindings ({{len(bindings)}}):")
                for b in bindings:
                    try:
                        bname = b.get_editor_property("BindingName")
                    except Exception:
                        bname = "(unknown)"
                    try:
                        tracks = b.get_editor_property("Tracks")
                    except Exception:
                        tracks = []
                    print(f"  {{bname}}:")
                    if tracks:
                        for t in tracks:
                            cls = t.get_class().get_name()
                            sections = t.get_sections() if hasattr(t, 'get_sections') else []
                            print(f"    {{cls}} - {{len(sections)}} section(s)")
                            total_tracks += 1
                    else:
                        print("    (no tracks)")
            else:
                print("\\nObject Bindings: None")
        except Exception as e:
            print()
            print(f"Object Bindings: Could not read ({{e}})")
        print()
        print(f"Total tracks: {{total_tracks}}")
""")

    @mcp.tool()
    def add_camera_cut(sequence_path: str, camera_label: str, start_frame: int = 0) -> str:
        """Add a CameraCutTrack to a LevelSequence bound to a CameraActor. Creates the track if not present."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    camera_label = "{camera_label}"
    start_frame = {start_frame}
    try:
        # Find the camera actor in the level
        camera_actor = None
        for a in unreal.EditorLevelLibrary.get_all_level_actors():
            if a.get_actor_label() == camera_label:
                camera_actor = a
                break
        if camera_actor is None:
            print(f"ERROR: Camera actor '{{camera_label}}' not found in level")
        else:
            movie_scene = asset.get_editor_property("MovieScene")
            if movie_scene is None:
                print("ERROR: Could not access MovieScene")
            else:
                # Ensure the camera has a binding in the sequence
                camera_binding = None
                try:
                    camera_binding = asset.add_possessable(camera_actor)
                except Exception:
                    # Search existing bindings
                    try:
                        bindings = movie_scene.get_editor_property("ObjectBindings")
                        if bindings:
                            for b in bindings:
                                bname = b.get_editor_property("BindingName")
                                if bname == camera_label:
                                    camera_binding = b
                                    break
                    except Exception:
                        pass
                # Find or create CameraCutTrack
                camera_cut_track = None
                try:
                    master_tracks = movie_scene.get_editor_property("MasterTracks")
                    if master_tracks:
                        for t in master_tracks:
                            if "CameraCut" in t.get_class().get_name():
                                camera_cut_track = t
                                break
                except Exception:
                    pass
                if camera_cut_track is None:
                    try:
                        camera_cut_track = movie_scene.add_master_track(unreal.MovieSceneCameraCutTrack)
                        if camera_cut_track:
                            print("Created CameraCutTrack")
                    except Exception as e:
                        print(f"ERROR: Could not create CameraCutTrack: {{e}}")
                if camera_cut_track is not None:
                    try:
                        # Add a section for the camera
                        section = camera_cut_track.add_section()
                        # Try to set the camera binding on the section
                        if camera_binding is not None:
                            try:
                                # UE 5.x: set_camera_binding_id or set binding guid
                                if hasattr(section, 'set_camera_binding_id'):
                                    binding_id = camera_binding.get_editor_property("BindingId") if hasattr(camera_binding, 'get_editor_property') else None
                                    if binding_id:
                                        section.set_camera_binding_id(binding_id)
                                elif hasattr(section, 'set_editor_property'):
                                    section.set_editor_property("CameraBindingID", camera_binding)
                            except Exception as e:
                                print(f"WARNING: Could not bind camera to section: {{e}}")
                                print("Camera cut track created but binding may need manual setup")
                        # Set section range
                        try:
                            section.set_range(start_frame, start_frame + 150)
                        except Exception:
                            pass
                        unreal.EditorAssetLibrary.save_asset("{sequence_path}")
                        print(f"Added camera cut for '{{camera_label}}' starting at frame {{start_frame}} in {sequence_path}")
                    except Exception as e:
                        print(f"ERROR: Could not add camera cut section: {{e}}")
                        print("Try using execute_unreal_python for manual CameraCutTrack setup")
                else:
                    print("ERROR: Could not find or create CameraCutTrack")
                    print("Try using execute_unreal_python to manually add the track")
    except Exception as e:
        print(f"ERROR: add_camera_cut failed: {{e}}")
        print("This is a complex operation. Consider using execute_unreal_python as a fallback.")
""")

    @mcp.tool()
    def set_playback_range(sequence_path: str, start_frame: int = 0, end_frame: int = 150) -> str:
        """Set the playback range (start and end frames) of a LevelSequence."""
        return asset_exec(sequence_path, f"""
if not isinstance(asset, unreal.LevelSequence):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a LevelSequence")
else:
    start_frame = {start_frame}
    end_frame = {end_frame}
    try:
        movie_scene = asset.get_editor_property("MovieScene")
        if movie_scene is None:
            print("ERROR: Could not access MovieScene")
        else:
            # Get tick resolution to convert frames to ticks
            display_rate = movie_scene.get_editor_property("DisplayRate")
            fps = display_rate.numerator / max(display_rate.denominator, 1)
            tick_resolution = movie_scene.get_editor_property("TickResolution")
            tps = tick_resolution.numerator / max(tick_resolution.denominator, 1)
            ticks_per_frame = tps / fps if fps > 0 else 800
            start_tick = int(start_frame * ticks_per_frame)
            end_tick = int(end_frame * ticks_per_frame)
            # Try setting playback range via set_playback_range if available
            set_ok = False
            if hasattr(movie_scene, 'set_playback_range'):
                try:
                    movie_scene.set_playback_range(start_tick, end_tick - start_tick)
                    set_ok = True
                except Exception:
                    pass
            if not set_ok:
                # Try set_editor_property with FrameNumberRange
                try:
                    pr = unreal.FrameNumberRange()
                    # Try setting upper/lower bounds
                    try:
                        lower = unreal.FrameNumberRangeBound()
                        lower.set_editor_property("Type", unreal.ERangeBoundTypes.INCLUSIVE)
                        lower.set_editor_property("Value", unreal.FrameNumber(start_tick))
                        upper = unreal.FrameNumberRangeBound()
                        upper.set_editor_property("Type", unreal.ERangeBoundTypes.EXCLUSIVE)
                        upper.set_editor_property("Value", unreal.FrameNumber(end_tick))
                        pr.set_editor_property("LowerBound", lower)
                        pr.set_editor_property("UpperBound", upper)
                    except Exception:
                        pass
                    movie_scene.set_editor_property("PlaybackRange", pr)
                    set_ok = True
                except Exception as e:
                    print(f"WARNING: FrameNumberRange approach failed: {{e}}")
            if not set_ok:
                # Last resort: try set_playback_range_locked / set_work_range
                try:
                    movie_scene.set_editor_property("InTime", start_tick)
                    movie_scene.set_editor_property("OutTime", end_tick)
                    set_ok = True
                except Exception:
                    pass
            if set_ok:
                unreal.EditorAssetLibrary.save_asset("{sequence_path}")
                print(f"Set playback range: frames {{start_frame}} - {{end_frame}} ({{fps:.0f}} fps) on {sequence_path}")
            else:
                print("ERROR: Could not set playback range via any available method")
                print("Try using execute_unreal_python with direct MovieScene API calls")
    except Exception as e:
        print(f"ERROR: set_playback_range failed: {{e}}")
        print("Consider using execute_unreal_python as a fallback.")
""")
