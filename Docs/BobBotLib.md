# BobBotLib Reference

`UBobBotLib` is a C++ library exposed to Python via `unreal.BobBotLib`. It fills gaps in UE 5.4's Python API for Blueprint editing — variables, graph nodes, pin wiring, compilation. All functions are static.

BobBot's AI uses these functions internally. You can also call them from [custom tools](CustomTools.md) or `execute_unreal_python`.

For the 159 MCP tools that BobBot exposes to AI (actors, assets, materials, etc.), see [ToolReference.md](ToolReference.md).

## Variables

### AddBlueprintVariable

```python
unreal.BobBotLib.add_blueprint_variable(blueprint, var_name, var_type, instance_editable=True)
```

Add a member variable to a Blueprint. Returns `True` on success.

**var_type** accepts: `float`, `double`, `int32`, `int64`, `bool`, `FString`, `FName`, `FText`, `byte`, `FVector`, `FRotator`, `FTransform`, `FLinearColor`, `FVector2D`, `UTexture2D*`, `UStaticMesh*`

### RemoveBlueprintVariable

```python
unreal.BobBotLib.remove_blueprint_variable(blueprint, var_name)
```

Remove a variable from a Blueprint. Returns `True` on success.

### SetBlueprintVariableDefault

```python
unreal.BobBotLib.set_blueprint_variable_default(blueprint, var_name, default_value)
```

Set a variable's default value as a string. Examples: `"100.0"` for float, `"True"` for bool, `"(X=1,Y=0,Z=0)"` for FVector.

### SetBlueprintVariableCategory

```python
unreal.BobBotLib.set_blueprint_variable_category(blueprint, var_name, category)
```

Organize a variable under a category heading in the Details panel. Example: `"Health|Stats"`.

### SetBlueprintVariableTooltip

```python
unreal.BobBotLib.set_blueprint_variable_tooltip(blueprint, var_name, tooltip)
```

Set the hover tooltip for a variable.

### SetBlueprintVariableSliderRange

```python
unreal.BobBotLib.set_blueprint_variable_slider_range(blueprint, var_name, min, max)
```

Set the slider range for a numeric variable in the Details panel.

### GetBlueprintVariableNames

```python
names = unreal.BobBotLib.get_blueprint_variable_names(blueprint)
```

Returns an array of all variable names in the Blueprint.

## Components

### AddComponentToBlueprint

```python
name = unreal.BobBotLib.add_component_to_blueprint(blueprint, component_class, component_name)
```

Add a component to a Blueprint's construction script. Returns the template name on success.

```python
bp = unreal.EditorAssetLibrary.load_asset("/Game/BP_MyActor")
unreal.BobBotLib.add_component_to_blueprint(bp, unreal.PointLightComponent, "MyLight")
unreal.BobBotLib.compile_blueprint(bp)
```

## Graph creation

### AddFunctionGraph

```python
result = unreal.BobBotLib.add_function_graph(blueprint, function_name)
```

Create a new function graph. Returns the function name on success, or an error string starting with `"ERROR:"`.

### AddCustomEvent

```python
result = unreal.BobBotLib.add_custom_event(blueprint, event_name)
```

Create a custom event node in the event graph. Returns the event name on success.

## Graph nodes

### AddFunctionCallNode

```python
result = unreal.BobBotLib.add_function_call_node(blueprint, function_name, target_class, x=0, y=0)
```

Add a function call node. `target_class` is the UClass that owns the function.

### AddSetMPCScalarNode / AddSetMPCVectorNode

```python
result = unreal.BobBotLib.add_set_mpc_scalar_node(blueprint, mpc_path, param_name, x=0, y=0)
result = unreal.BobBotLib.add_set_mpc_vector_node(blueprint, mpc_path, param_name, x=0, y=0)
```

Add Material Parameter Collection setter nodes.

### AddBranchNode

```python
result = unreal.BobBotLib.add_branch_node(blueprint, x=0, y=0)
```

Add an if/else (Branch) node. Returns a description with pin names: `Execute`, `Condition`, `Then`, `Else`.

### AddVariableGetNode / AddVariableSetNode

```python
result = unreal.BobBotLib.add_variable_get_node(blueprint, var_name, x=0, y=0)
result = unreal.BobBotLib.add_variable_set_node(blueprint, var_name, x=0, y=0)
```

Add getter/setter nodes for a Blueprint member variable. The variable must already exist.

### AddCastNode

```python
result = unreal.BobBotLib.add_cast_node(blueprint, target_class, x=0, y=0)
```

Add a dynamic cast node for the given class.

## Node wiring

### ConnectPins

```python
result = unreal.BobBotLib.connect_pins(blueprint, source_node, source_pin, target_node, target_pin)
```

Connect two pins by node name and pin name. Returns `"OK: connected ..."` on success or `"ERROR: ..."` on failure.

Node names are matched against both the node's title and internal name. Pin names must match exactly (e.g., `"ReturnValue"`, `"Execute"`, `"Then"`).

## Compilation

### CompileBlueprint

```python
success = unreal.BobBotLib.compile_blueprint(blueprint)
```

Compile a Blueprint. Returns `True` if compilation succeeded.

### CompileBlueprintWithStatus

```python
status = unreal.BobBotLib.compile_blueprint_with_status(blueprint)
```

Compile and return a status string describing the result.

**Always compile after modifying a Blueprint. Always save after compiling.**

```python
unreal.BobBotLib.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset("/Game/BP_MyActor")
```

## Utility

### GetBlueprintCDO

```python
cdo = unreal.BobBotLib.get_blueprint_cdo(blueprint)
```

Get the Class Default Object. Useful for reading/writing default property values.

### SetCDOProperty

```python
success = unreal.BobBotLib.set_cdo_property(blueprint, property_name, value)
```

Set a property on the CDO by name and string value.

### ClearTypeCache

```python
unreal.BobBotLib.clear_type_cache()
```

Clear the internal type resolution cache. Call after hot reload if variable types aren't resolving correctly.
