"""AI tools: Behavior Trees, Blackboards, EQS queries, and AI asset management."""

from _common import _exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def create_behavior_tree(name: str, path: str = "/Game/AI") -> str:
        """Create a Behavior Tree asset."""
        return _exec(f"""
import unreal
name_local = {_safe(name)}
path_local = {_safe(path)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.BehaviorTreeFactory()
    bt = asset_tools.create_asset(name_local, path_local, unreal.BehaviorTree, factory)
    if bt:
        unreal.EditorAssetLibrary.save_asset(f"{{path_local}}/{{name_local}}")
        print(f"Created Behavior Tree: {{path_local}}/{{name_local}}")
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
name_local = {_safe(name)}
path_local = {_safe(path)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.BlackboardDataFactory()
    bb = asset_tools.create_asset(name_local, path_local, unreal.BlackboardData, factory)
    if bb:
        unreal.EditorAssetLibrary.save_asset(f"{{path_local}}/{{name_local}}")
        print(f"Created Blackboard: {{path_local}}/{{name_local}}")
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
blackboard_path_local = {_safe(blackboard_path)}
key_name_local = {_safe(key_name)}
key_type = {_safe(key_type)}
bb = unreal.EditorAssetLibrary.load_asset(blackboard_path_local)
if bb is None:
    print("ERROR: Blackboard " + blackboard_path_local + " not found")
elif not isinstance(bb, unreal.BlackboardData):
    print(f"ERROR: '{{bb.get_class().get_name()}}' is not a BlackboardData")
else:
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
                new_key.set_editor_property("EntryName", key_name_local)
                new_key.set_editor_property("KeyType", key_type_cls())
                keys.append(new_key)
                bb.set_editor_property("Keys", keys)
                unreal.EditorAssetLibrary.save_asset(blackboard_path_local)
                print(f"Added key '{{key_name_local}}' ({{key_type}}) to {{blackboard_path_local}}")
        except Exception as e:
            print(f"ERROR: Could not add key: {{e}}")
            print("Blackboard key manipulation has limited Python API. Try execute_unreal_python.")
""")

    @mcp.tool()
    def get_blackboard_keys(blackboard_path: str) -> str:
        """List all keys in a Blackboard with their types."""
        return _exec(f"""
import unreal
blackboard_path_local = {_safe(blackboard_path)}
bb = unreal.EditorAssetLibrary.load_asset(blackboard_path_local)
if bb is None:
    print("ERROR: Blackboard " + blackboard_path_local + " not found")
elif not isinstance(bb, unreal.BlackboardData):
    print(f"ERROR: '{{bb.get_class().get_name()}}' is not a BlackboardData")
else:
    print(f"Blackboard: {{blackboard_path_local}}")
    # UBlackboardData::Keys is protected; use BobBotLib typed getters.
    count = unreal.BobBotLib.get_blackboard_key_count(bb)
    if count > 0:
        print()
        print(f"Keys ({{count}}):")
        for i in range(count):
            name = unreal.BobBotLib.get_blackboard_key_name(bb, i)
            type_name = unreal.BobBotLib.get_blackboard_key_type(bb, i) or "Unknown"
            print(f"  {{name}} ({{type_name}})")
    else:
        print("\\nNo keys defined")
""")

    @mcp.tool()
    def create_environment_query(name: str, path: str = "/Game/AI") -> str:
        """Create an Environment Query System (EQS) query asset."""
        return _exec(f"""
import unreal
name_local = {_safe(name)}
path_local = {_safe(path)}
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    factory = unreal.EnvQueryFactory()
    eq = asset_tools.create_asset(name_local, path_local, unreal.EnvQuery, factory)
    if eq:
        unreal.EditorAssetLibrary.save_asset(f"{{path_local}}/{{name_local}}")
        print(f"Created EQS Query: {{path_local}}/{{name_local}}")
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
path_local = {_safe(path)}
assets = unreal.EditorAssetLibrary.list_assets(path_local, recursive=True, include_folder=False)
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
    print(f"AI Assets in {{path_local}} ({{total}}):")
    for category, paths in sorted(results.items()):
        print()
        print(f"  {{category}} ({{len(paths)}}):")
        for p in paths:
            print(f"    {{p}}")
else:
    print(f"No AI assets found in {{path_local}}")
""")
