"""Animation tools: create AnimBPs, Montages, BlendSpaces, and inspect animation assets."""

from _common import _exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_anim_blueprint(name: str, skeleton_path: str,
                              path: str = "/Game/Animations") -> str:
        """Create an Animation Blueprint from a skeleton asset."""
        return _exec(f"""
import unreal
skel = unreal.EditorAssetLibrary.load_asset("{skeleton_path}")
if skel is None:
    print("ERROR: Skeleton '{skeleton_path}' not found")
elif not isinstance(skel, unreal.Skeleton):
    print(f"ERROR: '{{skel.get_class().get_name()}}' is not a Skeleton")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property("TargetSkeleton", skel)
    factory.set_editor_property("ParentClass", unreal.AnimInstance)
    abp = asset_tools.create_asset("{name}", "{path}", unreal.AnimBlueprint, factory)
    if abp:
        print(f"Created AnimBlueprint: {path}/{name} (skeleton: {skeleton_path})")
    else:
        print(f"ERROR: Failed to create AnimBlueprint '{name}'")
""")

    @mcp.tool()
    def create_anim_montage(name: str, animation_path: str,
                            path: str = "/Game/Animations") -> str:
        """Create an Animation Montage from an animation sequence."""
        return _exec(f"""
import unreal
anim = unreal.EditorAssetLibrary.load_asset("{animation_path}")
if anim is None:
    print("ERROR: Animation '{animation_path}' not found")
elif not isinstance(anim, unreal.AnimSequence):
    print(f"ERROR: '{{anim.get_class().get_name()}}' is not an AnimSequence")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("SourceAnimation", anim)
    montage = asset_tools.create_asset("{name}", "{path}", unreal.AnimMontage, factory)
    if montage:
        print(f"Created AnimMontage: {path}/{name} (from: {animation_path})")
    else:
        print(f"ERROR: Failed to create AnimMontage '{name}'")
""")

    @mcp.tool()
    def create_blend_space_1d(name: str, skeleton_path: str,
                              path: str = "/Game/Animations") -> str:
        """Create a 1D Blend Space for a skeleton."""
        return _exec(f"""
import unreal
skel = unreal.EditorAssetLibrary.load_asset("{skeleton_path}")
if skel is None:
    print("ERROR: Skeleton '{skeleton_path}' not found")
elif not isinstance(skel, unreal.Skeleton):
    print(f"ERROR: '{{skel.get_class().get_name()}}' is not a Skeleton")
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlendSpaceFactory1D()
    factory.set_editor_property("TargetSkeleton", skel)
    bs = asset_tools.create_asset("{name}", "{path}", unreal.BlendSpace1D, factory)
    if bs:
        print(f"Created BlendSpace1D: {path}/{name} (skeleton: {skeleton_path})")
    else:
        print(f"ERROR: Failed to create BlendSpace1D '{name}'")
""")

    @mcp.tool()
    def get_skeleton_animations(skeleton_path: str) -> str:
        """List all animation sequences that use a given skeleton."""
        return _exec(f"""
import unreal
skel = unreal.EditorAssetLibrary.load_asset("{skeleton_path}")
if skel is None:
    print("ERROR: Skeleton '{skeleton_path}' not found")
elif not isinstance(skel, unreal.Skeleton):
    print(f"ERROR: '{{skel.get_class().get_name()}}' is not a Skeleton")
else:
    # Search for AnimSequences that reference this skeleton
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    all_anims = registry.get_assets_by_class(unreal.Name("AnimSequence"))
    matching = []
    for asset_data in all_anims:
        asset = asset_data.get_asset()
        if asset:
            try:
                asset_skel = asset.get_editor_property("Skeleton")
                if asset_skel and asset_skel.get_path_name() == skel.get_path_name():
                    matching.append(asset)
            except:
                pass
    if matching:
        print(f"Animations for {skeleton_path} ({{len(matching)}}):")
        for a in matching:
            length = a.get_editor_property("SequenceLength")
            print(f"  {{a.get_path_name()}} ({{length:.2f}}s)")
    else:
        print(f"No AnimSequences found for skeleton {skeleton_path}")
""")

    @mcp.tool()
    def get_anim_blueprint_info(anim_bp_path: str) -> str:
        """Get info about an AnimBlueprint: skeleton, state machines, variables."""
        return _exec(f"""
import unreal
abp = unreal.EditorAssetLibrary.load_asset("{anim_bp_path}")
if abp is None:
    print("ERROR: AnimBlueprint '{anim_bp_path}' not found")
elif not isinstance(abp, unreal.AnimBlueprint):
    print(f"ERROR: '{{abp.get_class().get_name()}}' is not an AnimBlueprint")
else:
    print(f"AnimBlueprint: {anim_bp_path}")
    skel = abp.get_editor_property("TargetSkeleton")
    if skel:
        print(f"Target Skeleton: {{skel.get_path_name()}}")
    parent = abp.get_editor_property("ParentClass")
    if parent:
        print(f"Parent Class: {{parent.get_name()}}")
    # Variables
    try:
        var_names = unreal.BobBotLib.get_blueprint_variable_names(abp)
        if var_names:
            print(f"\\nVariables ({{len(var_names)}}):")
            for v in var_names:
                print(f"  {{v}}")
    except:
        pass
    # Animation groups/graphs info
    anim_graphs = abp.get_editor_property("AnimationGraphs") if hasattr(abp, "AnimationGraphs") else None
    if anim_graphs:
        print(f"\\nAnimation Graphs: {{len(anim_graphs)}}")
""")
