"""Import/export tools: import files into Content Browser, export assets to disk."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def import_asset(file_path: str, destination_path: str = "/Game") -> str:
        """Import a file (FBX, OBJ, PNG, WAV, etc.) into the Content Browser. file_path is the absolute path on disk."""
        return _exec(f"""
import unreal, os
file_path = r"{file_path}"
if not os.path.isfile(file_path):
    print(f"ERROR: File '{{file_path}}' not found on disk")
else:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file_path)
    task.set_editor_property("destination_path", "{destination_path}")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_editor_property("imported_object_paths")
    if imported:
        print(f"Imported: {{imported[0]}}")
    else:
        name = os.path.splitext(os.path.basename(file_path))[0]
        print(f"Import completed: {destination_path}/{{name}} (verify in Content Browser)")
""")

    @mcp.tool()
    def export_asset(asset_path: str, file_path: str) -> str:
        """Export an asset to a file on disk. file_path should include the desired extension."""
        return _exec(f"""
import unreal, os
asset_path = "{asset_path}"
file_path = r"{file_path}"
if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    print(f"ERROR: Asset '{{asset_path}}' not found")
else:
    # Ensure output directory exists
    out_dir = os.path.dirname(file_path)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir, exist_ok=True)
    task = unreal.AssetExportTask()
    task.set_editor_property("object", unreal.EditorAssetLibrary.load_asset(asset_path))
    task.set_editor_property("filename", file_path)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    result = unreal.Exporter.run_asset_export_task(task)
    if result:
        print(f"Exported: {{asset_path}} -> {{file_path}}")
    else:
        print(f"ERROR: Export failed for '{{asset_path}}'")
""")

    @mcp.tool()
    def import_fbx(file_path: str, destination_path: str = "/Game/Meshes",
                   import_animations: bool = True) -> str:
        """Import an FBX file with mesh and animation options."""
        return _exec(f"""
import unreal, os
file_path = r"{file_path}"
if not os.path.isfile(file_path):
    print(f"ERROR: File '{{file_path}}' not found on disk")
elif not file_path.lower().endswith(".fbx"):
    print("ERROR: File must be an FBX file")
else:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file_path)
    task.set_editor_property("destination_path", "{destination_path}")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    # Configure FBX import options
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_animations", {import_animations})
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_editor_property("imported_object_paths")
    if imported:
        print(f"Imported FBX ({{len(imported)}} asset(s)):")
        for p in imported:
            print(f"  {{p}}")
    else:
        name = os.path.splitext(os.path.basename(file_path))[0]
        print(f"FBX import completed: {destination_path}/{{name}} (verify in Content Browser)")
""")
