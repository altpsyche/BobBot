---
paths:
  - "**/*Landscape*"
  - "**/Terrain/**"
---

# Landscape Reference

## Finding landscapes

```python
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
landscapes = [a for a in actors if isinstance(a, unreal.Landscape)
              or isinstance(a, unreal.LandscapeProxy)]
```

## Landscape properties

```python
ls = landscapes[0]
mat = ls.get_editor_property("LandscapeMaterial")
ls.set_editor_property("LandscapeMaterial", new_material)

# Components
comps = ls.get_components_by_class(unreal.LandscapeComponent)

# Layers
layers = ls.get_editor_property("EditorLayerSettings")
for layer in layers:
    info = layer.get_editor_property("LayerInfoObj")
    name = info.get_editor_property("LayerName")
```

## Landscape material requirements

Landscape materials must use LandscapeLayerBlend or LandscapeLayerCoords nodes. Each layer needs a corresponding LandscapeLayerInfo asset.
