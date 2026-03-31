---
paths:
  - "**/*Blueprint*"
  - "**/*BP_*"
  - "**/Blueprints/**"
---

# Blueprint Editing Reference

All Blueprint operations use `unreal.BobBotLib` which fills gaps in UE 5.4's Python API.

## Creating a Blueprint

```python
import unreal
factory = unreal.BlueprintFactory()
factory.set_editor_property("parent_class", unreal.Actor)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
bp = asset_tools.create_asset("BP_MyActor", "/Game/Blueprints",
                               unreal.Blueprint, factory)
```

Parent classes: Actor, Pawn, Character, GameModeBase, PlayerController, ActorComponent, SceneComponent

## Variables

```python
bp = unreal.load_asset("/Game/Blueprints/BP_MyActor")

unreal.BobBotLib.add_blueprint_variable(bp, "Health", "float", True)
unreal.BobBotLib.set_blueprint_variable_default(bp, "Health", "100.0")
unreal.BobBotLib.set_blueprint_variable_category(bp, "Health", "Stats")
unreal.BobBotLib.set_blueprint_variable_tooltip(bp, "Health", "Current health points")
unreal.BobBotLib.set_blueprint_variable_slider_range(bp, "Health", 0.0, 200.0)

names = unreal.BobBotLib.get_blueprint_variable_names(bp)
unreal.BobBotLib.remove_blueprint_variable(bp, "OldVar")
```

Supported types: float, double, int32, int64, bool, FString, FName, FText, byte, FVector, FRotator, FTransform, FLinearColor, FVector2D, and object references like "UTexture2D*" or "UStaticMesh*"

## Components

```python
comp_name = unreal.BobBotLib.add_component_to_blueprint(
    bp, unreal.StaticMeshComponent, "MeshComp")
```

## Functions and Events

```python
# Create a function graph (works on all UE 5.4+)
unreal.BobBotLib.add_function_graph(bp, "CalculateDamage")

# Create a custom event (works on all UE 5.4+)
unreal.BobBotLib.add_custom_event(bp, "OnDamaged")
```

## Graph nodes

```python
unreal.BobBotLib.add_function_call_node(bp, "SetActorLocation", unreal.Actor, 200, 0)
unreal.BobBotLib.add_set_mpc_scalar_node(bp, "/Game/MPC_Global", "Brightness", 400, 0)
unreal.BobBotLib.add_set_mpc_vector_node(bp, "/Game/MPC_Global", "SunColor", 400, 200)
```

## Compilation and CDO

```python
unreal.BobBotLib.compile_blueprint(bp)
status = unreal.BobBotLib.compile_blueprint_with_status(bp)

cdo = unreal.BobBotLib.get_blueprint_cdo(bp)
unreal.BobBotLib.set_cdo_property(bp, "MaxHealth", "100.0")
```

Always compile after making changes. Always save after compiling:
```python
unreal.BobBotLib.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset("/Game/Blueprints/BP_MyActor")
```
