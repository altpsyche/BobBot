"""Collision tools: manage collision presets, enabled state, and channels on actors."""


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
    def set_collision_preset(actor_label: str, preset_name: str) -> str:
        """Set collision preset on an actor. Common presets: 'NoCollision', 'BlockAll', 'OverlapAll', 'BlockAllDynamic', 'OverlapAllDynamic', 'Pawn', 'Spectator', 'CharacterMesh', 'PhysicsActor', 'Trigger'."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    comps = target.get_components_by_class(unreal.PrimitiveComponent)
    if not comps:
        print(f"ERROR: {{target.get_actor_label()}} has no primitive components with collision")
    else:
        for comp in comps:
            comp.set_collision_profile_name("{preset_name}")
        print(f"Set collision preset '{preset_name}' on {{len(comps)}} component(s) of {{target.get_actor_label()}}")
""")

    @mcp.tool()
    def set_collision_enabled(actor_label: str, enabled: bool = True) -> str:
        """Enable or disable collision on an actor."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    comps = target.get_components_by_class(unreal.PrimitiveComponent)
    if not comps:
        print(f"ERROR: {{target.get_actor_label()}} has no primitive components with collision")
    else:
        collision_type = unreal.CollisionEnabled.QUERY_AND_PHYSICS if {enabled} else unreal.CollisionEnabled.NO_COLLISION
        for comp in comps:
            comp.set_collision_enabled(collision_type)
        state = "enabled" if {enabled} else "disabled"
        print(f"Collision {{state}} on {{len(comps)}} component(s) of {{target.get_actor_label()}}")
""")

    @mcp.tool()
    def get_collision_info(actor_label: str) -> str:
        """Get collision preset, enabled state, and object type for an actor."""
        return _exec(f"""
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for a in actors:
    if a.get_actor_label() == "{actor_label}":
        target = a
        break
if target is None:
    print("ERROR: Actor '{actor_label}' not found")
else:
    comps = target.get_components_by_class(unreal.PrimitiveComponent)
    if not comps:
        print(f"{{target.get_actor_label()}} has no primitive components")
    else:
        print(f"Collision info for {{target.get_actor_label()}}:")
        for comp in comps:
            profile = comp.get_collision_profile_name()
            enabled = comp.get_collision_enabled()
            obj_type = comp.get_collision_object_type()
            print(f"  {{comp.get_name()}} ({{comp.get_class().get_name()}})")
            print(f"    Profile: {{profile}}")
            print(f"    Enabled: {{enabled}}")
            print(f"    Object Type: {{obj_type}}")
""")
