---
paths:
  - "**/*Niagara*"
  - "**/VFX/**"
  - "**/Effects/**"
  - "**/Particles/**"
---

# Niagara VFX Reference

## Creating Niagara systems

```python
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.NiagaraSystemFactoryNew()
ns = asset_tools.create_asset("NS_Sparks", "/Game/VFX",
                               unreal.NiagaraSystem, factory)
```

## Inspecting systems

```python
ns = unreal.EditorAssetLibrary.load_asset("/Game/VFX/NS_Sparks")
emitter_handles = ns.get_editor_property("EmitterHandles")
for eh in emitter_handles:
    name = eh.get_editor_property("Name")
```

## Known limitations (UE 5.4)

Niagara has very limited Python API exposure:
- System and emitter creation works via factory
- Emitter handle listing works
- **Parameter read/write is unreliable** — the exposed parameters API varies by version
- Module/renderer configuration is not accessible via Python

For complex Niagara work, create the system via tool then configure in the Niagara Editor GUI.

## Spawning in level

```python
ns_class = unreal.load_class(None, "/Script/Niagara.NiagaraActor")
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(ns_class, loc)
nc = actor.get_components_by_class(unreal.NiagaraComponent)[0]
nc.set_editor_property("Asset", ns)
```
