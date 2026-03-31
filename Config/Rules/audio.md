---
paths:
  - "**/*Audio*"
  - "**/*Sound*"
  - "**/*Music*"
---

# Audio

## Creating sound assets

Use `create_sound_cue(name, sound_wave_path, path)` to create a Sound Cue referencing a SoundWave.

## Querying audio

Use `get_audio_assets(path)` to list SoundWaves and SoundCues under a path.

## Attaching audio to actors

Use `set_actor_audio(actor_label, sound_path)` to add an AudioComponent playing the given sound.

## Tips

- SoundWaves are imported from .wav files via `import_asset()`
- SoundCues allow layering, randomization, and attenuation via a node graph
- Use `set_actor_audio` for spatial 3D audio on actors
- Attenuation settings control falloff distance and shape
