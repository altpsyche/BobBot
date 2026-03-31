---
paths:
  - "**/*Widget*"
  - "**/UMG/**"
  - "**/UI/**"
---

# UMG / Widget Blueprints

## Creating widgets

Use `create_widget_blueprint(name, parent_class, path)` to create a new widget.
Common parent classes: `"/Script/UMG.UserWidget"`

## Inspecting widgets

- `get_widget_tree(widget_path)` — returns the hierarchy of a widget BP
- `get_all_widget_blueprints(path)` — lists all widget BPs under a path

## Adding widget components to actors

Use `create_widget_component(actor_label, widget_path)` to attach a widget to a 3D actor.

## Tips

- Widget Blueprints are UWidgetBlueprint assets, not regular Blueprints
- Use `"/Script/UMG.UserWidget"` as parent_class for standard screen widgets
- Widget components in 3D space need a widget class reference set after creation
