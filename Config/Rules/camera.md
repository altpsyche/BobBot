---
paths:
  - "**/*Camera*"
  - "**/*Cinematic*"
---

# Camera System

## Creating cameras

Use `create_camera(x, y, z, yaw, pitch, fov)` to spawn a CameraActor in the level.

## Modifying camera properties

Use `set_camera_properties(actor_label, fov, aperture, focus_distance)` for cinematic settings.
- `fov`: field of view in degrees (default 90)
- `aperture`: f-stop for depth of field (lower = more blur)
- `focus_distance`: distance to focus plane in cm

## Viewport camera

- `get_active_viewport_camera()` — get current viewport camera transform
- `set_viewport_camera(x, y, z, yaw, pitch)` — teleport the editor viewport

## Tips

- CameraActors can be added to Level Sequences for cinematic shots
- Viewport camera is separate from placed CameraActors
- FOV is horizontal by default in UE5
- Aperture/focus_distance only matter when depth of field is enabled
- Use `create_camera` + `add_actor_to_sequence` for cinematic workflows
