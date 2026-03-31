---
paths:
  - "**/*Input*"
  - "**/Input/**"
---

# Enhanced Input Reference

## Creating input assets

```python
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# InputAction
factory = unreal.InputActionFactory()
ia = asset_tools.create_asset("IA_Jump", "/Game/Input",
                               unreal.InputAction, factory)
ia.set_editor_property("ValueType", unreal.InputActionValueType.BOOLEAN)

# InputMappingContext
factory = unreal.InputMappingContextFactory()
imc = asset_tools.create_asset("IMC_Default", "/Game/Input",
                                unreal.InputMappingContext, factory)
```

## Value types

```python
unreal.InputActionValueType.BOOLEAN   # Digital on/off
unreal.InputActionValueType.AXIS1D    # Single axis (float)
unreal.InputActionValueType.AXIS2D    # Two axes (Vector2D)
unreal.InputActionValueType.AXIS3D    # Three axes (Vector)
```

## Common key names

Keyboard: W, A, S, D, SpaceBar, LeftShift, LeftControl, E, Q, F, R, Tab, Escape, Enter
Mouse: LeftMouseButton, RightMouseButton, MiddleMouseButton, MouseX, MouseY, MouseScrollUp, MouseScrollDown
Gamepad: Gamepad_FaceButton_Bottom (A), Gamepad_FaceButton_Right (B), Gamepad_LeftTrigger, Gamepad_RightTrigger, Gamepad_LeftStick_X/Y
