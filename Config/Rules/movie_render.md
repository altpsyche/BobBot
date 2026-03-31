---
paths:
  - "**/*MovieRender*"
  - "**/*Render*"
  - "**/Cinematics/**"
---

# Movie Render Queue

## Creating render jobs

Use `create_render_job(sequence_path, output_dir, format, resolution)` to queue a render.
- `format`: "png", "jpg", "exr", or "avi"
- `resolution`: "1920x1080", "3840x2160", etc.

## Quick image sequence export

Use `render_sequence_to_images(sequence_path, output_dir, format)` for simple image exports.

## Checking status

Use `get_render_queue_status()` to see pending/active/completed jobs.

## Tips

- Ensure the Level Sequence exists and has content before rendering
- Output directory is created automatically if it doesn't exist
- EXR format preserves HDR data for compositing workflows
- Higher resolutions significantly increase render time
- Use `play_sequence(sequence_path)` to preview before committing to a render
