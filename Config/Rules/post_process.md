---
paths:
  - "**/*PostProcess*"
  - "**/PostProcess/**"
---

# Post-Process Volumes

## Creating volumes

Use `create_post_process_volume(x, y, z, infinite_extent)` to place a new volume.
Set `infinite_extent=True` for global effects (affects entire level).

## Modifying settings

- `set_post_process_setting(actor_label, setting, value)` — set any PP property by name
- `get_post_process_settings(actor_label)` — dump current settings

## Color grading

Use `set_color_grading(actor_label, saturation, contrast, gain, offset)` for quick color adjustments.
Values are float4 (RGBA): `[1.0, 1.0, 1.0, 1.0]` is neutral.

## Rendering stats

Use `get_rendering_stats()` to check frame time, draw calls, and triangle counts.

## Common settings

| Setting | Type | Notes |
|---------|------|-------|
| BloomIntensity | float | 0.0 = off |
| AutoExposureBias | float | EV adjustment |
| MotionBlurAmount | float | 0.0 = off |
| AmbientOcclusionIntensity | float | 0.0 = off |
| DepthOfFieldFstop | float | Lower = more blur |

## Tips

- Infinite extent volumes affect the whole level; bounded volumes blend by proximity
- Multiple volumes blend by priority (higher priority wins)
- Color grading values multiply — `[1,1,1,1]` is identity, not `[0,0,0,0]`
