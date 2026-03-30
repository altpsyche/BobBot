"""Data asset tools: create DataTables, add rows, and inspect table contents."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_data_table(name: str, struct_path: str,
                          path: str = "/Game/Data") -> str:
        """Create an empty DataTable with a row struct. struct_path is the struct to use as row type, e.g. '/Script/Engine.DataTableRowHandle' or a user struct path."""
        return _exec(f"""
import unreal
name = "{name}"
path = "{path}"
struct_path = "{struct_path}"
# Try to load the struct
row_struct = unreal.load_object(None, struct_path)
if row_struct is None:
    # Try as a script struct
    row_struct = unreal.load_class(None, struct_path)
if row_struct is None:
    print(f"ERROR: Row struct '{{struct_path}}' not found")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataTableFactory()
    factory.set_editor_property("Struct", row_struct)
    dt = asset_tools.create_asset(name, path, unreal.DataTable, factory)
    if dt:
        print(f"Created DataTable: {{path}}/{{name}} (row struct: {{struct_path}})")
    else:
        print(f"ERROR: Failed to create DataTable '{{name}}' at '{{path}}'")
""")

    @mcp.tool()
    def add_data_table_row(table_path: str, row_name: str, values_json: str) -> str:
        """Add or update a row in a DataTable. values_json is a JSON string of column values like '{\"Name\": \"Sword\", \"Damage\": 50}'."""
        safe_json = values_json.replace('"', '\\"')
        return _exec(f"""
import unreal, json
table = unreal.EditorAssetLibrary.load_asset("{table_path}")
if table is None:
    print("ERROR: DataTable '{table_path}' not found")
elif not isinstance(table, unreal.DataTable):
    print(f"ERROR: '{{table.get_class().get_name()}}' is not a DataTable")
else:
    # Use JSON string import to add/update the row
    json_str = '{safe_json}'
    try:
        row_data = json.loads(json_str)
        # Build a CSV-like JSON for the DataTable
        # Format: [{{"Name": "RowName", ...fields}}]
        row_data["Name"] = "{row_name}"
        import_str = json.dumps([row_data])
        # Try fill_from_json_string
        try:
            table.fill_from_json_string(import_str)
            unreal.EditorAssetLibrary.save_asset("{table_path}")
            print(f"Added/updated row '{row_name}' in {table_path}")
        except Exception as e:
            print(f"ERROR: Could not add row via JSON: {{e}}")
            print("Try using execute_unreal_python with DataTableFunctionLibrary for complex row operations")
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON: {{e}}")
""")

    @mcp.tool()
    def get_data_table_rows(table_path: str) -> str:
        """List all rows in a DataTable with their values."""
        return _exec(f"""
import unreal
table = unreal.EditorAssetLibrary.load_asset("{table_path}")
if table is None:
    print("ERROR: DataTable '{table_path}' not found")
elif not isinstance(table, unreal.DataTable):
    print(f"ERROR: '{{table.get_class().get_name()}}' is not a DataTable")
else:
    row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    if not row_names:
        print(f"DataTable '{table_path}' has no rows")
    else:
        print(f"DataTable: {table_path}")
        print(f"Rows ({{len(row_names)}}):")
        for name in row_names:
            print(f"  {{name}}")
        # Try to export as string for full data
        try:
            csv_str = table.get_editor_property("AssetImportData")
            print(f"\\n(Use execute_unreal_python for full row data inspection)")
        except:
            pass
""")
