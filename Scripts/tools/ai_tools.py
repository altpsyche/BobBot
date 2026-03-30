"""AI tools: Behavior Trees, Blackboards, EQS queries, and AI asset management."""


def register(mcp, send_fn):

    def _exec(code):
        result = send_fn({"type": "execute", "code": code})
        if result.get("success"):
            output = result.get("output", "")
            err = result.get("error")
            if err:
                output += "\nStderr: " + err
            return output if output.strip() else "(executed successfully, no output)"
        return "Error: " + result.get("error", "Unknown error")

    @mcp.tool()
    def create_behavior_tree(name: str, path: str = "/Game/AI") -> str:
        """Create a Behavior Tree asset."""
        return _exec(f"""
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.BehaviorTreeFactory()
    bt = asset_tools.create_asset("{name}", "{path}", unreal.BehaviorTree, factory)
    if bt:
        unreal.EditorAssetLibrary.save_asset("{path}/{name}")
        print(f"Created Behavior Tree: {path}/{name}")
    else:
        print(f"ERROR: Failed to create Behavior Tree")
except Exception as e:
    print(f"ERROR: {{e}}")
    print("Ensure the AI Module is available")
""")

    @mcp.tool()
    def create_blackboard(name: str, path: str = "/Game/AI") -> str:
        """Create a Blackboard Data asset for use with Behavior Trees."""
        return _exec(f"""
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.BlackboardDataFactory()
    bb = asset_tools.create_asset("{name}", "{path}", unreal.BlackboardData, factory)
    if bb:
        unreal.EditorAssetLibrary.save_asset("{path}/{name}")
        print(f"Created Blackboard: {path}/{name}")
    else:
        print(f"ERROR: Failed to create Blackboard")
except Exception as e:
    print(f"ERROR: {{e}}")
""")

    @mcp.tool()
    def add_blackboard_key(blackboard_path: str, key_name: str,
                           key_type: str = "Object") -> str:
        """Add a key to a Blackboard. key_type: 'Object', 'Class', 'Enum', 'Float', 'Int', 'Bool', 'String', 'Name', 'Vector', 'Rotator'."""
        return _exec(f"""
import unreal
bb = unreal.EditorAssetLibrary.load_asset("{blackboard_path}")
if bb is None:
    print("ERROR: Blackboard '{blackboard_path}' not found")
elif not isinstance(bb, unreal.BlackboardData):
    print(f"ERROR: '{{bb.get_class().get_name()}}' is not a BlackboardData")
else:
    key_type = "{key_type}"
    type_map = {{
        "object": "BlackboardKeyType_Object",
        "class": "BlackboardKeyType_Class",
        "enum": "BlackboardKeyType_Enum",
        "float": "BlackboardKeyType_Float",
        "int": "BlackboardKeyType_Int",
        "bool": "BlackboardKeyType_Bool",
        "string": "BlackboardKeyType_String",
        "name": "BlackboardKeyType_Name",
        "vector": "BlackboardKeyType_Vector",
        "rotator": "BlackboardKeyType_Rotator",
    }}
    type_class_name = type_map.get(key_type.lower())
    if type_class_name is None:
        print(f"ERROR: Unknown key type '{{key_type}}'. Use: {{', '.join(type_map.keys())}}")
    else:
        try:
            key_type_cls = getattr(unreal, type_class_name, None)
            if key_type_cls is None:
                print(f"ERROR: Blackboard key type class '{{type_class_name}}' not found in this UE version.")
                print(f"Available types may vary. Try using execute_unreal_python to inspect available BlackboardKeyType classes.")
            else:
                keys = list(bb.get_editor_property("Keys"))
                new_key = unreal.BlackboardKeyData()
                new_key.set_editor_property("EntryName", "{key_name}")
                new_key.set_editor_property("KeyType", key_type_cls())
                keys.append(new_key)
                bb.set_editor_property("Keys", keys)
                unreal.EditorAssetLibrary.save_asset("{blackboard_path}")
                print(f"Added key '{key_name}' ({{key_type}}) to {blackboard_path}")
        except Exception as e:
            print(f"ERROR: Could not add key: {{e}}")
            print("Blackboard key manipulation has limited Python API. Try execute_unreal_python.")
""")

    @mcp.tool()
    def get_blackboard_keys(blackboard_path: str) -> str:
        """List all keys in a Blackboard with their types."""
        return _exec(f"""
import unreal
bb = unreal.EditorAssetLibrary.load_asset("{blackboard_path}")
if bb is None:
    print("ERROR: Blackboard '{blackboard_path}' not found")
elif not isinstance(bb, unreal.BlackboardData):
    print(f"ERROR: '{{bb.get_class().get_name()}}' is not a BlackboardData")
else:
    print(f"Blackboard: {blackboard_path}")
    keys = bb.get_editor_property("Keys")
    if keys:
        print(f"\\nKeys ({{len(keys)}}):")
        for key in keys:
            name = key.get_editor_property("EntryName")
            key_type = key.get_editor_property("KeyType")
            type_name = key_type.get_class().get_name() if key_type else "Unknown"
            print(f"  {{name}} ({{type_name}})")
    else:
        print("\\nNo keys defined")
    # Check parent blackboard
    parent = bb.get_editor_property("Parent")
    if parent:
        print(f"\\nParent Blackboard: {{parent.get_path_name()}}")
""")

    @mcp.tool()
    def create_environment_query(name: str, path: str = "/Game/AI") -> str:
        """Create an Environment Query System (EQS) query asset."""
        return _exec(f"""
import unreal
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.EnvQueryFactory()
    eq = asset_tools.create_asset("{name}", "{path}", unreal.EnvQuery, factory)
    if eq:
        unreal.EditorAssetLibrary.save_asset("{path}/{name}")
        print(f"Created EQS Query: {path}/{name}")
    else:
        print(f"ERROR: Failed to create EQS Query")
except Exception as e:
    print(f"ERROR: {{e}}")
    print("EQS may require the EnvironmentQueryEditor plugin")
""")

    @mcp.tool()
    def get_ai_assets(path: str = "/Game/AI") -> str:
        """List all AI assets (Behavior Trees, Blackboards, EQS) in a path."""
        return _exec(f"""
import unreal
assets = unreal.EditorAssetLibrary.list_assets("{path}", recursive=True, include_folder=False)
ai_types = ["BehaviorTree", "BlackboardData", "EnvQuery", "AIController"]
results = {{}}
for asset_path in sorted(assets):
    data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
    if data:
        cls = str(data.asset_class)
        for at in ai_types:
            if at in cls:
                if at not in results:
                    results[at] = []
                results[at].append(asset_path)
                break
if results:
    total = sum(len(v) for v in results.values())
    print(f"AI Assets in {path} ({{total}}):")
    for category, paths in sorted(results.items()):
        print(f"\\n  {{category}} ({{len(paths)}}):")
        for p in paths:
            print(f"    {{p}}")
else:
    print(f"No AI assets found in {path}")
""")
