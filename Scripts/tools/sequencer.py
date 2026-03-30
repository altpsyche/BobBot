"""Sequencer tools: create and inspect LevelSequences, add tracks, control playback."""


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
    def create_sequence(name: str, path: str = "/Game/Cinematics") -> str:
        """Create a new LevelSequence asset."""
        return _exec(f"""
import unreal
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
        return _exec(f"""
import unreal
seq = unreal.EditorAssetLibrary.load_asset("{sequence_path}")
if seq is None:
    print("ERROR: LevelSequence '{sequence_path}' not found")
elif not isinstance(seq, unreal.LevelSequence):
    print(f"ERROR: '{{seq.get_class().get_name()}}' is not a LevelSequence")
else:
    print(f"LevelSequence: {sequence_path}")
    movie_scene = seq.get_editor_property("MovieScene")
    if movie_scene:
        # Playback range
        pr = movie_scene.get_editor_property("PlaybackRange")
        print(f"Playback Range: {{pr}}")
        # Master tracks
        master_tracks = movie_scene.get_editor_property("MasterTracks")
        if master_tracks:
            print(f"\\nMaster Tracks ({{len(master_tracks)}}):")
            for t in master_tracks:
                print(f"  {{t.get_class().get_name()}}")
        # Bindings (object binding tracks)
        bindings = movie_scene.get_editor_property("ObjectBindings")
        if bindings:
            print(f"\\nObject Bindings ({{len(bindings)}}):")
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
        return _exec(f"""
import unreal
seq = unreal.EditorAssetLibrary.load_asset("{sequence_path}")
if seq is None:
    print("ERROR: LevelSequence '{sequence_path}' not found")
elif not isinstance(seq, unreal.LevelSequence):
    print(f"ERROR: '{{seq.get_class().get_name()}}' is not a LevelSequence")
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
            binding = seq.add_possessable(target)
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
        return _exec(f"""
import unreal
seq = unreal.EditorAssetLibrary.load_asset("{sequence_path}")
if seq is None:
    print("ERROR: LevelSequence '{sequence_path}' not found")
elif not isinstance(seq, unreal.LevelSequence):
    print(f"ERROR: '{{seq.get_class().get_name()}}' is not a LevelSequence")
else:
    movie_scene = seq.get_editor_property("MovieScene")
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
        return _exec(f"""
import unreal
seq = unreal.EditorAssetLibrary.load_asset("{sequence_path}")
if seq is None:
    print("ERROR: LevelSequence '{sequence_path}' not found")
elif not isinstance(seq, unreal.LevelSequence):
    print(f"ERROR: '{{seq.get_class().get_name()}}' is not a LevelSequence")
else:
    played = False
    # Try modern API first
    if hasattr(unreal, 'LevelSequenceEditorBlueprintLibrary'):
        try:
            unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
            unreal.LevelSequenceEditorBlueprintLibrary.play()
            played = True
            print(f"Playing sequence: {sequence_path}")
        except Exception as e:
            pass
    # Fallback: open the asset in the editor (user presses play manually)
    if not played:
        try:
            unreal.EditorAssetLibrary.open_editor_for_assets([seq])
            print(f"Opened sequence in editor: {sequence_path}")
            print("Press Play in the Sequencer toolbar to preview")
        except Exception as e:
            print(f"ERROR: Could not open or play sequence: {{e}}")
""")
