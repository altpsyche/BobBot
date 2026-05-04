"""Skeletal mesh tools: inspect skeletons and meshes, create sockets, attach actors."""

from _common import _exec, _exec_ue, actor_exec, asset_exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Skeletal", output_kind="large", default_timeout=60)
    def get_skeleton_info(skeleton_path: str) -> str:
        """Get bone hierarchy, socket list, and bone count for a Skeleton asset."""
        return asset_exec(skeleton_path, f"""
skeleton_path = {_safe(skeleton_path)}
if not isinstance(asset, unreal.Skeleton):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a Skeleton")
else:
    print(f"Skeleton: {{skeleton_path}}")
    # USkeleton::Sockets is protected — use the BobBotLib getter.
    sockets = unreal.BobBotLib.get_skeleton_sockets(asset)
    if sockets:
        print()
        print(f"Sockets ({{len(sockets)}}):")
        for s in sockets:
            bone = s.get_editor_property("BoneName")
            name = s.get_editor_property("SocketName")
            print(f"  {{name}} (on bone: {{bone}})")
    else:
        print("\\nNo sockets")
    # Note: full bone hierarchy requires SkeletalMesh
    print("\\n(Use get_skeletal_mesh_info for bone hierarchy from a SkeletalMesh)")
""")

    @mcp.tool()
    @bob_tool(category="Skeletal", output_kind="large", default_timeout=60)
    def get_skeletal_mesh_info(mesh_path: str) -> str:
        """Get vertex count, bone count, LOD count, materials, and physics asset for a SkeletalMesh."""
        return asset_exec(mesh_path, f"""
mesh_path = {_safe(mesh_path)}
if not isinstance(asset, unreal.SkeletalMesh):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a SkeletalMesh")
else:
    print(f"Skeletal Mesh: {{mesh_path}}")
    # LODs
    num_lods = asset.get_num_lod()
    print(f"LODs: {{num_lods}}")
    # Skeleton
    skel = asset.get_editor_property("Skeleton")
    if skel:
        print(f"Skeleton: {{skel.get_path_name()}}")
    # Physics asset
    phys = asset.get_editor_property("PhysicsAsset")
    if phys:
        print(f"Physics Asset: {{phys.get_path_name()}}")
    # Materials — USkeletalMesh::Materials is a TArray<FSkeletalMaterial> (struct
    # array, not object array). Show the count via the typed C++ getter; the
    # per-slot details require the editor's preview-mesh interaction or a
    # struct-array getter (deferred to a later sprint).
    mat_count = unreal.BobBotLib.get_skeletal_mesh_material_count(asset)
    print(f"Materials: {{mat_count}}")
    # Bounds
    bounds = asset.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    print(f"Bounds: origin=({{origin.x:.0f}}, {{origin.y:.0f}}, {{origin.z:.0f}}) extent=({{extent.x:.0f}}, {{extent.y:.0f}}, {{extent.z:.0f}})")
""")

    @mcp.tool()
    @bob_tool(category="Skeletal", output_kind="small", default_timeout=60)
    def create_socket(skeleton_path: str, bone_name: str, socket_name: str) -> str:
        """Create a socket on a bone in a Skeleton asset."""
        return asset_exec(skeleton_path, f"""
skeleton_path = {_safe(skeleton_path)}
bone_name = {_safe(bone_name)}
socket_name = {_safe(socket_name)}
if not isinstance(asset, unreal.Skeleton):
    print(f"ERROR: '{{asset.get_class().get_name()}}' is not a Skeleton")
else:
    socket = unreal.SkeletalMeshSocket()
    socket.set_editor_property("SocketName", socket_name)
    socket.set_editor_property("BoneName", bone_name)
    sockets = list(asset.get_editor_property("Sockets"))
    sockets.append(socket)
    asset.set_editor_property("Sockets", sockets)
    unreal.EditorAssetLibrary.save_asset(skeleton_path)
    print(f"Created socket '{{socket_name}}' on bone '{{bone_name}}' in {{skeleton_path}}")
""")

    @mcp.tool()
    @bob_tool(category="Skeletal", output_kind="small", default_timeout=60)
    def attach_actor_to_socket(actor_label: str, parent_label: str,
                               socket_name: str) -> str:
        """Attach an actor to a socket on another actor's skeletal mesh."""
        return _exec_ue(f"""
actor_label = {_safe(actor_label)}
parent_label = {_safe(parent_label)}
socket_name = {_safe(socket_name)}
actors = unreal.EditorLevelLibrary.get_all_level_actors()
child = None
parent = None
for a in actors:
    label = a.get_actor_label()
    if label == actor_label:
        child = a
    if label == parent_label:
        parent = a
    if child and parent:
        break
if child is None:
    print(f"ERROR: Actor '{{actor_label}}' not found")
elif parent is None:
    print(f"ERROR: Parent actor '{{parent_label}}' not found")
else:
    # Find a skeletal mesh component on the parent
    skel_comps = parent.get_components_by_class(unreal.SkeletalMeshComponent)
    if not skel_comps:
        print(f"ERROR: {{parent.get_actor_label()}} has no SkeletalMeshComponent")
    else:
        child.attach_to_actor(parent, socket_name,
                              unreal.AttachmentRule.SNAP_TO_TARGET,
                              unreal.AttachmentRule.SNAP_TO_TARGET,
                              unreal.AttachmentRule.KEEP_WORLD)
        print(f"Attached {{child.get_actor_label()}} to {{parent.get_actor_label()}} socket '{{socket_name}}'")
""")
