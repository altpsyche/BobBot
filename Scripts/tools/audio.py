"""Audio tools: create SoundCues, list audio assets, set audio on actors."""


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
    def create_sound_cue(name: str, sound_wave_path: str,
                         path: str = "/Game/Audio") -> str:
        """Create a SoundCue from a SoundWave asset."""
        return _exec(f"""
import unreal
sw = unreal.EditorAssetLibrary.load_asset("{sound_wave_path}")
if sw is None:
    print("ERROR: SoundWave '{sound_wave_path}' not found")
elif not isinstance(sw, unreal.SoundWave):
    print(f"ERROR: '{{sw.get_class().get_name()}}' is not a SoundWave")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    try:
        factory = unreal.SoundCueFactoryNew()
        cue = asset_tools.create_asset("{name}", "{path}", unreal.SoundCue, factory)
        if cue:
            # Set the sound wave on the cue
            # SoundCue editing requires node graph manipulation
            unreal.EditorAssetLibrary.save_asset("{path}/{name}")
            print(f"Created SoundCue: {path}/{name}")
            print("Note: Connect the SoundWave node manually or via execute_unreal_python")
        else:
            print(f"ERROR: Failed to create SoundCue")
    except Exception as e:
        print(f"ERROR: {{e}}")
""")

    @mcp.tool()
    def get_audio_assets(path: str = "/Game/Audio") -> str:
        """List all sound assets (SoundWave, SoundCue, SoundAttenuation, etc.) in a path."""
        return _exec(f"""
import unreal
assets = unreal.EditorAssetLibrary.list_assets("{path}", recursive=True, include_folder=False)
audio_types = ["SoundWave", "SoundCue", "SoundAttenuation", "SoundConcurrency", "SoundMix", "SoundClass"]
results = []
for asset_path in sorted(assets):
    data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    if data:
        cls = str(data.asset_class)
        if any(at in cls for at in audio_types):
            results.append((asset_path, cls))
if results:
    print(f"Audio assets in {path} ({{len(results)}}):")
    for asset_path, cls in results:
        print(f"  {{asset_path}} ({{cls}})")
else:
    print(f"No audio assets found in {path}")
""")

    @mcp.tool()
    def set_actor_audio(actor_label: str, sound_path: str) -> str:
        """Set the sound on an actor's AudioComponent. Creates one if needed."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    sound = unreal.EditorAssetLibrary.load_asset("{sound_path}")
    if sound is None:
        print("ERROR: Sound asset '{sound_path}' not found")
    else:
        audio_comps = target.get_components_by_class(unreal.AudioComponent)
        if audio_comps:
            ac = audio_comps[0]
        else:
            ac = target.add_component_by_class(unreal.AudioComponent, False, unreal.Transform(), False)
        if ac:
            ac.set_editor_property("Sound", sound)
            print(f"Set sound on {{target.get_actor_label()}} to {sound_path}")
        else:
            print("ERROR: Could not find or create AudioComponent")
""")
