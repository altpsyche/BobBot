---
paths:
  - "**/*Light*"
  - "**/Lighting/**"
---

# Lighting Reference

## Light class paths

- /Script/Engine.PointLight
- /Script/Engine.SpotLight
- /Script/Engine.DirectionalLight
- /Script/Engine.RectLight
- /Script/Engine.SkyLight

## Light properties

```python
light_comp = actor.get_components_by_class(unreal.LightComponentBase)[0]
light_comp.set_editor_property("Intensity", 5000.0)
light_comp.set_editor_property("LightColor", unreal.Color(r=255, g=200, b=150, a=255))
light_comp.set_editor_property("bUseTemperature", True)
light_comp.set_editor_property("Temperature", 6500.0)  # Kelvin
light_comp.set_editor_property("AttenuationRadius", 1000.0)
light_comp.set_editor_property("bAtmosphereSunLight", True)  # For directional lights
```

## Outdoor lighting setup

Standard outdoor scene needs 4 actors:
1. **DirectionalLight** — sun, intensity ~10, pitch=-50, bAtmosphereSunLight=True
2. **SkyLight** — ambient fill
3. **SkyAtmosphere** — sky rendering
4. **ExponentialHeightFog** — atmosphere, density ~0.02

Use `create_sky_atmosphere` tool to create all four at once.

## Lightmap resolution

Lower = faster builds, higher = better shadow quality. Typical values:
- Floors/walls: 64-128
- Small props: 16-32
- Large architecture: 128-256
- Production: 256-512
