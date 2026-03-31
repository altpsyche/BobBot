---
paths:
  - "**/*SourceControl*"
  - "**/*Perforce*"
  - "**/*Git*"
---

# Source Control

## Checking status

Use `get_source_control_status(asset_path)` before modifying assets to see if they're checked out.

## Checkout workflow

1. `check_out_asset(asset_path)` — locks the file for editing
2. Make modifications
3. `check_in_asset(asset_path, description)` — submits with a changelist description
4. `revert_asset(asset_path)` — discards changes and releases the lock

## Important

- Always check out before modifying assets in a source-controlled project
- `check_in_asset` requires a description string
- `revert_asset` discards ALL local changes to that asset
- Source control must be enabled in the editor (Edit > Project Settings > Source Control)

## Tips

- If check_out fails, the file may be locked by another user
- Binary assets (.uasset, .umap) require exclusive checkout in Perforce
- Use `get_source_control_status` to check before batch operations
