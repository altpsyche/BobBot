---
paths:
  - "**/*Spline*"
  - "**/Paths/**"
---

# Spline Actors

## Creating splines

Use `create_spline_actor(points, closed)` to spawn a spline actor.
- `points`: list of `[x, y, z]` coordinates
- `closed`: `true` for a loop, `false` for open

## Inspecting splines

Use `get_spline_info(actor_label)` to get point count, length, and tangent data.

## Modifying splines

- `add_spline_point(actor_label, x, y, z, index)` — insert a point at the given index
- `set_spline_mesh(actor_label, mesh_path)` — attach a mesh to extrude along the spline

## Tips

- Spline points are in world space (UE centimeters)
- Use `closed=True` for loops (fences, race tracks)
- `set_spline_mesh` extrudes a mesh along the spline path — great for roads, rails, pipes
- Splines can also drive AI movement paths and camera rails
