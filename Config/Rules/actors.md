---
paths:
  - "**/*.umap"
  - "**/Maps/**"
---

# Actor Manipulation Reference

## Spawning actors

```python
import unreal

# C++ class
actor_class = unreal.load_class(None, "/Script/Engine.PointLight")
loc = unreal.Vector(x=0, y=0, z=300)
rot = unreal.Rotator(pitch=0, yaw=0, roll=0)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, loc, rot)

# Blueprint class
bp = unreal.load_asset("/Game/Blueprints/BP_Enemy")
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp.generated_class(), loc, rot)
```

## Common C++ class paths

- /Script/Engine.PointLight
- /Script/Engine.SpotLight
- /Script/Engine.DirectionalLight
- /Script/Engine.StaticMeshActor
- /Script/Engine.CameraActor
- /Script/Engine.PlayerStart
- /Script/Engine.TriggerBox
- /Script/Engine.BlockingVolume
- /Script/Engine.ExponentialHeightFog
- /Script/Engine.SkyLight
- /Script/Engine.PostProcessVolume

## Modifying actors

```python
actor.set_actor_location(unreal.Vector(100, 200, 0), False, False)
actor.set_actor_rotation(unreal.Rotator(0, 45, 0), False)
actor.set_actor_scale3d(unreal.Vector(2, 2, 2))
actor.set_editor_property("Intensity", 5000.0)
```

## Setting materials on mesh actors

```python
mat = unreal.load_asset("/Game/Materials/M_MyMaterial")
actor.static_mesh_component.set_material(0, mat)

mesh = unreal.load_asset("/Game/Meshes/SM_Cube")
actor.static_mesh_component.set_static_mesh(mesh)
```

## Querying actors

```python
actors = unreal.EditorLevelLibrary.get_all_level_actors()
selected = unreal.EditorLevelLibrary.get_selected_level_actors()

for a in actors:
    label = a.get_actor_label()
    cls = a.get_class().get_name()
    loc = a.get_actor_location()
```

## Coordinate system

X forward, Y right, Z up. Centimeters. A character is ~180 units tall. Rotations in degrees: pitch around Y, yaw around Z, roll around X.
