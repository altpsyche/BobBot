---
paths:
  - "**/*Physics*"
  - "**/*Collision*"
  - "**/*Phys*"
---

# Physics & Collision Reference

## Collision presets

Common presets for `set_collision_preset`: NoCollision, BlockAll, OverlapAll, BlockAllDynamic, OverlapAllDynamic, Pawn, Spectator, CharacterMesh, PhysicsActor, Trigger, Vehicle, UI

## Collision channels

```python
unreal.CollisionChannel.ECC_WORLD_STATIC
unreal.CollisionChannel.ECC_WORLD_DYNAMIC
unreal.CollisionChannel.ECC_PAWN
unreal.CollisionChannel.ECC_VISIBILITY
unreal.CollisionChannel.ECC_CAMERA
unreal.CollisionChannel.ECC_PHYSICS_BODY
unreal.CollisionChannel.ECC_VEHICLE
unreal.CollisionChannel.ECC_DESTRUCTIBLE
```

## Physics properties

```python
comp = actor.get_components_by_class(unreal.PrimitiveComponent)[0]
comp.set_simulate_physics(True)
comp.set_mass_override_in_kg(unreal.Name("None"), 50.0)
comp.set_editor_property("LinearDamping", 0.01)
comp.set_editor_property("AngularDamping", 0.0)
comp.set_editor_property("bEnableGravity", True)
comp.set_collision_profile_name("PhysicsActor")
comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
```

## Collision enabled types

```python
unreal.CollisionEnabled.NO_COLLISION
unreal.CollisionEnabled.QUERY_ONLY
unreal.CollisionEnabled.PHYSICS_ONLY
unreal.CollisionEnabled.QUERY_AND_PHYSICS
```
