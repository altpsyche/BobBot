---
paths:
  - "**/*.py"
---

# UE Python API Patterns

## Loading and saving assets

```python
import unreal

asset = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_Base")
exists = unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/M_Base")
unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_Base")
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
```

## Content Browser operations

```python
assets = unreal.EditorAssetLibrary.list_assets("/Game/Materials", recursive=True)
selected = unreal.EditorUtilityLibrary.get_selected_assets()
unreal.EditorAssetLibrary.rename_asset("/Game/Old", "/Game/New")
unreal.EditorAssetLibrary.duplicate_asset("/Game/Source", "/Game/Copy")
unreal.EditorAssetLibrary.delete_asset("/Game/ToDelete")
```

## Creating assets

```python
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Material
mat = asset_tools.create_asset("M_Name", "/Game/Materials",
                                unreal.Material, unreal.MaterialFactoryNew())

# Blueprint
factory = unreal.BlueprintFactory()
factory.set_editor_property("parent_class", unreal.Actor)
bp = asset_tools.create_asset("BP_Name", "/Game/Blueprints",
                               unreal.Blueprint, factory)
```

## Console commands

```python
world = unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "stat fps")
unreal.SystemLibrary.execute_console_command(world, "stat unit")
unreal.SystemLibrary.execute_console_command(world, "show collision")
```

## Project paths

```python
project_path = str(unreal.Paths.get_project_file_path())
content_dir = str(unreal.Paths.project_content_dir())
log_dir = str(unreal.Paths.project_log_dir())
```

## Asset path formats

- Project content: "/Game/Folder/AssetName"
- Engine content: "/Engine/Folder/AssetName"
- C++ class: "/Script/ModuleName.ClassName"
- Blueprint class for spawning: use the asset path directly
