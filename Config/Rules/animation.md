---
paths:
  - "**/*Anim*"
  - "**/Animations/**"
  - "**/*Montage*"
  - "**/*BlendSpace*"
---

# Animation Reference

## Creating animation assets

```python
import unreal
skel = unreal.EditorAssetLibrary.load_asset("/Game/Characters/SK_Mannequin")
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# AnimBlueprint
factory = unreal.AnimBlueprintFactory()
factory.set_editor_property("TargetSkeleton", skel)
factory.set_editor_property("ParentClass", unreal.AnimInstance)
abp = asset_tools.create_asset("ABP_Character", "/Game/Animations",
                                unreal.AnimBlueprint, factory)

# AnimMontage
anim = unreal.EditorAssetLibrary.load_asset("/Game/Animations/Anim_Attack")
factory = unreal.AnimMontageFactory()
factory.set_editor_property("SourceAnimation", anim)
montage = asset_tools.create_asset("AM_Attack", "/Game/Animations",
                                    unreal.AnimMontage, factory)

# BlendSpace1D
factory = unreal.BlendSpaceFactory1D()
factory.set_editor_property("TargetSkeleton", skel)
bs = asset_tools.create_asset("BS_Locomotion", "/Game/Animations",
                               unreal.BlendSpace1D, factory)
```

## Querying animations

```python
# Find all anims for a skeleton
registry = unreal.AssetRegistryHelpers.get_asset_registry()
all_anims = registry.get_assets_by_class(unreal.Name("AnimSequence"))
```

## AnimBlueprint properties

```python
abp.get_editor_property("TargetSkeleton")
abp.get_editor_property("ParentClass")
abp.get_editor_property("FunctionGraphs")
abp.get_editor_property("UbergraphPages")
```
