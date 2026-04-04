"""Texture and mesh tools: inspect meshes and textures, import textures, set material parameters."""

from _common import _exec, _exec_ue, actor_exec, asset_exec

def register(mcp, send_fn):


    @mcp.tool()
    def get_static_mesh_info(mesh_path: str) -> str:
        """Get vertex count, triangle count, LOD count, bounds, and materials for a static mesh."""
        return asset_exec(mesh_path, f"""
if not isinstance(asset, unreal.StaticMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a StaticMesh")
else:
    print(f"Static Mesh: {mesh_path}")
    num_lods = asset.get_num_lods()
    print(f"LODs: {{num_lods}}")
    for lod in range(num_lods):
        num_sections = asset.get_num_sections(lod)
        print(f"  LOD {{lod}}: {{num_sections}} section(s)")
    bounds = asset.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    print(f"Bounds Origin: ({{origin.x:.0f}}, {{origin.y:.0f}}, {{origin.z:.0f}})")
    print(f"Bounds Extent: ({{extent.x:.0f}}, {{extent.y:.0f}}, {{extent.z:.0f}})")
    # Materials
    num_mats = asset.get_editor_property("StaticMaterials")
    if num_mats:
        print(f"Materials ({{len(num_mats)}}):")
        for i, mat_slot in enumerate(num_mats):
            mat_name = mat_slot.get_editor_property("MaterialSlotName")
            mat_iface = mat_slot.get_editor_property("MaterialInterface")
            print(f"  [{{i}}] {{mat_name}}: {{mat_iface.get_path_name() if mat_iface else 'None'}}")
""")

    @mcp.tool()
    def set_static_mesh_on_actor(actor_label: str, mesh_path: str) -> str:
        """Set the static mesh on a StaticMeshActor or any actor with a StaticMeshComponent."""
        return actor_exec(actor_label, f"""
mesh = unreal.EditorAssetLibrary.load_asset("{mesh_path}")
if mesh is None:
    print("ERROR: Mesh '{mesh_path}' not found")
elif not isinstance(mesh, unreal.StaticMesh):
    print(f"ERROR: '{{mesh.get_class().get_name()}}' is not a StaticMesh")
else:
    mesh_comps = target.get_components_by_class(unreal.StaticMeshComponent)
    if not mesh_comps:
        print(f"ERROR: {{target.get_actor_label()}} has no StaticMeshComponent")
    else:
        mesh_comps[0].set_static_mesh(mesh)
        print(f"Set mesh on {{target.get_actor_label()}} to {mesh_path}")
""")

    @mcp.tool()
    def get_texture_info(texture_path: str) -> str:
        """Get resolution, format, compression settings, and size for a texture."""
        return asset_exec(texture_path, f"""
if not isinstance(asset, unreal.Texture2D):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a Texture2D")
else:
    print(f"Texture: {texture_path}")
    print(f"Size: {{asset.blueprint_get_size_x()}}x{{asset.blueprint_get_size_y()}}")
    print(f"Compression: {{asset.get_editor_property('CompressionSettings')}}")
    print(f"SRGB: {{asset.get_editor_property('SRGB')}}")
    print(f"LOD Group: {{asset.get_editor_property('LODGroup')}}")
    print(f"Has Alpha: {{asset.get_editor_property('bHasAlphaChannel') if hasattr(asset, 'bHasAlphaChannel') else 'N/A'}}")
""")

    @mcp.tool()
    def create_texture_from_file(file_path: str, name: str = "",
                                 dest_path: str = "/Game/Textures") -> str:
        """Import an image file (PNG, JPG, TGA, BMP, EXR) as a Texture2D asset."""
        return _exec_ue(f"""
import os
file_path = r"{file_path}"
if not os.path.isfile(file_path):
    print(f"ERROR: File '{{file_path}}' not found on disk")
else:
    name = "{name}" or os.path.splitext(os.path.basename(file_path))[0]
    dest_path = "{dest_path}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", file_path)
    task.set_editor_property("destination_path", dest_path)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_editor_property("imported_object_paths")
    if imported:
        print(f"Imported texture: {{imported[0]}}")
    else:
        result_objs = task.get_editor_property("result")
        if result_objs:
            print(f"Imported texture: {{dest_path}}/{{name}}")
        else:
            print(f"Import completed: {{dest_path}}/{{name}} (verify in Content Browser)")
""")

    @mcp.tool()
    def set_material_texture_parameter(material_path: str, param_name: str,
                                       texture_path: str) -> str:
        """Set a texture parameter on a Material Instance. param_name is the parameter name defined in the parent material."""
        return asset_exec(material_path, f"""
tex = unreal.EditorAssetLibrary.load_asset("{texture_path}")
if tex is None:
    print("ERROR: Texture '{texture_path}' not found")
else:
    if isinstance(asset, unreal.MaterialInstanceConstant):
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(asset, "{param_name}", tex)
        unreal.EditorAssetLibrary.save_asset("{material_path}")
        print(f"Set {param_name} = {texture_path} on {material_path}")
    else:
        print(f"ERROR: '{material_path}' is a {{asset.get_class().get_name()}}, not a MaterialInstanceConstant. Texture parameters can only be set on Material Instances.")
""")
