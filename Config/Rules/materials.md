---
paths:
  - "**/*Material*"
  - "**/Materials/**"
---

# Material Editing Reference

## Creating a material

```python
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = asset_tools.create_asset("M_MyMaterial", "/Game/Materials",
                                unreal.Material, unreal.MaterialFactoryNew())
```

## Expression types

| Expression | Class | Properties |
|------------|-------|------------|
| Constant (float) | unreal.MaterialExpressionConstant | R |
| Color/Vector | unreal.MaterialExpressionConstant3Vector | Constant (LinearColor) |
| Texture Sample | unreal.MaterialExpressionTextureSample | Texture |
| Scalar Parameter | unreal.MaterialExpressionScalarParameter | ParameterName, DefaultValue |
| Vector Parameter | unreal.MaterialExpressionVectorParameter | ParameterName, DefaultValue |
| Multiply | unreal.MaterialExpressionMultiply | (connect inputs) |
| Add | unreal.MaterialExpressionAdd | (connect inputs) |
| Lerp | unreal.MaterialExpressionLinearInterpolate | (A, B, Alpha) |
| Fresnel | unreal.MaterialExpressionFresnel | Exponent, BaseReflectFraction |
| Texture Coordinate | unreal.MaterialExpressionTextureCoordinate | UTiling, VTiling |
| Panner | unreal.MaterialExpressionPanner | SpeedX, SpeedY |
| Time | unreal.MaterialExpressionTime | (none) |
| Power | unreal.MaterialExpressionPower | (Base, Exponent) |
| Clamp | unreal.MaterialExpressionClamp | (Input, Min, Max) |

## Adding and connecting nodes

```python
mel = unreal.MaterialEditingLibrary
color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
color.set_editor_property("Constant", unreal.LinearColor(r=0.8, g=0.2, b=0.1, a=1.0))
mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
```

## Material property enum values

- unreal.MaterialProperty.MP_BASE_COLOR
- unreal.MaterialProperty.MP_METALLIC
- unreal.MaterialProperty.MP_SPECULAR
- unreal.MaterialProperty.MP_ROUGHNESS
- unreal.MaterialProperty.MP_NORMAL
- unreal.MaterialProperty.MP_EMISSIVE_COLOR
- unreal.MaterialProperty.MP_OPACITY
- unreal.MaterialProperty.MP_OPACITY_MASK
- unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
- unreal.MaterialProperty.MP_AMBIENT_OCCLUSION

Always save after editing: `unreal.EditorAssetLibrary.save_asset(path)`
