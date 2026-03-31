---
paths:
  - "**/*DataTable*"
  - "**/Data/**"
---

# Data Tables

## Creating tables

Use `create_data_table(name, struct_path, path)` to create a new DataTable.
The `struct_path` is a C++ or Blueprint struct path.

## Managing rows

- `add_data_table_row(table_path, row_name, values_json)` — insert or update a row
- `get_data_table_rows(table_path)` — dump all rows as JSON

## Tips

- Row names must be unique within a table
- The struct_path must exactly match the table's row structure
- `values_json` keys must match the struct's property names (case-sensitive)
- Data Tables are commonly used for game design data (weapons, items, abilities)
- Always save after modifying: `unreal.EditorAssetLibrary.save_asset(path)`
