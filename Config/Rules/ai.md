---
paths:
  - "**/*BehaviorTree*"
  - "**/*Blackboard*"
  - "**/AI/**"
  - "**/*EQS*"
---

# AI System Reference

## Creating AI assets

```python
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Behavior Tree
factory = unreal.BehaviorTreeFactory()
bt = asset_tools.create_asset("BT_Enemy", "/Game/AI",
                               unreal.BehaviorTree, factory)

# Blackboard
factory = unreal.BlackboardDataFactory()
bb = asset_tools.create_asset("BB_Enemy", "/Game/AI",
                               unreal.BlackboardData, factory)

# Environment Query
factory = unreal.EnvQueryFactory()
eq = asset_tools.create_asset("EQS_FindCover", "/Game/AI",
                               unreal.EnvQuery, factory)
```

## Blackboard key types

- BlackboardKeyType_Object — UObject reference
- BlackboardKeyType_Class — UClass reference
- BlackboardKeyType_Float — float value
- BlackboardKeyType_Int — integer value
- BlackboardKeyType_Bool — boolean
- BlackboardKeyType_String — FString
- BlackboardKeyType_Name — FName
- BlackboardKeyType_Vector — FVector
- BlackboardKeyType_Rotator — FRotator
- BlackboardKeyType_Enum — enum value

## Reading blackboard data

```python
bb = unreal.EditorAssetLibrary.load_asset("/Game/AI/BB_Enemy")
keys = bb.get_editor_property("Keys")
for key in keys:
    name = key.get_editor_property("EntryName")
    key_type = key.get_editor_property("KeyType")
```
