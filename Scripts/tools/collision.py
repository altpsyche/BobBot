"""Collision tools: manage collision presets, enabled state, and channels on actors."""

from _common import _exec, _exec_ue, actor_exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Collision", output_kind="small", default_timeout=60)
    def set_collision_preset(actor_label: str, preset_name: str) -> str:
        """Set collision preset on an actor. Common presets: 'NoCollision', 'BlockAll', 'OverlapAll', 'BlockAllDynamic', 'OverlapAllDynamic', 'Pawn', 'Spectator', 'CharacterMesh', 'PhysicsActor', 'Trigger'."""
        return actor_exec(actor_label, f"""
preset_name = {_safe(preset_name)}
comps = target.get_components_by_class(unreal.PrimitiveComponent)
if not comps:
    print(f"ERROR: {{target.get_actor_label()}} has no primitive components with collision")
else:
    for comp in comps:
        comp.set_collision_profile_name(preset_name)
    print(f"Set collision preset '{{preset_name}}' on {{len(comps)}} component(s) of {{target.get_actor_label()}}")
""")

    @mcp.tool()
    @bob_tool(category="Collision", output_kind="small", default_timeout=60)
    def set_collision_enabled(actor_label: str, enabled: bool = True) -> str:
        """Enable or disable collision on an actor."""
        return actor_exec(actor_label, f"""
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
    @bob_tool(category="Collision", output_kind="large", default_timeout=60)
    def get_collision_info(actor_label: str) -> str:
        """Get collision preset, enabled state, and object type for an actor."""
        return actor_exec(actor_label, f"""
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
