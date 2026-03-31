---
paths:
  - "**/*Foliage*"
  - "**/*Vegetation*"
---

# Foliage System

## Querying foliage

- `get_foliage_types()` — lists all registered foliage types and their meshes
- `get_foliage_stats()` — instance counts, memory usage, LOD info

## Adding foliage types

Use `add_foliage_type(static_mesh_path)` to register a new mesh as paintable foliage.

## Tips

- Foliage is GPU-instanced — thousands of instances are cheap to render
- Each foliage type references a static mesh; changing the mesh updates all instances
- Use `get_foliage_stats()` to monitor instance counts before adding more
- Foliage painting is viewport-only; tools can register types but not paint
