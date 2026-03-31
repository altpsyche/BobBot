---
paths:
  - "**/*DataAsset*"
  - "**/Config/**"
---

# Data Assets

Data Assets are read-only asset containers for game configuration. They're loaded via asset references rather than file I/O.

## Working with Data Assets

```python
import unreal

# Load a data asset
da = unreal.EditorAssetLibrary.load_asset("/Game/Config/DA_GameSettings")

# Read properties
print(da.get_editor_property("SomeProperty"))

# Modify (if the property is editable)
da.set_editor_property("SomeProperty", new_value)
unreal.EditorAssetLibrary.save_asset("/Game/Config/DA_GameSettings")
```

## Data Assets vs Data Tables

| Feature | Data Asset | Data Table |
|---------|-----------|------------|
| Structure | Single object | Array of rows |
| Schema | C++ class | Struct |
| Best for | Singleton config | Collections (items, enemies) |
| Access | Direct property | Row lookup by name |

## Tips

- Data Assets are great for game-wide settings (difficulty curves, UI themes)
- They're blueprint-editable and show up in the Content Browser
- Use `search_assets` with type filter to find existing data assets
- Always save after modifying properties
