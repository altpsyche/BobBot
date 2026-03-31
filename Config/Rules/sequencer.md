---
paths:
  - "**/*Sequence*"
  - "**/*Cinematic*"
  - "**/Cinematics/**"
---

# Sequencer Reference

## Creating sequences

```python
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.LevelSequenceFactoryNew()
seq = asset_tools.create_asset("LS_Intro", "/Game/Cinematics",
                                unreal.LevelSequence, factory)
```

## LevelSequenceEditorBlueprintLibrary

```python
lib = unreal.LevelSequenceEditorBlueprintLibrary
lib.open_level_sequence(seq)
lib.play()
lib.pause()
lib.close_level_sequence()
```

## MovieScene properties

```python
movie_scene = seq.get_editor_property("MovieScene")
display_rate = movie_scene.get_editor_property("DisplayRate")  # FFrameRate
playback_range = movie_scene.get_editor_property("PlaybackRange")
master_tracks = movie_scene.get_editor_property("MasterTracks")
bindings = movie_scene.get_editor_property("ObjectBindings")
```

## Common track types

- MovieSceneFloatTrack — animate float properties
- MovieScene3DTransformTrack — animate transforms
- MovieSceneCameraCutTrack — camera cuts
- MovieSceneAudioTrack — audio playback
- MovieSceneEventTrack — trigger events
