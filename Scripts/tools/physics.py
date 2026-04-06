"""Physics tools: enable/disable physics, set mass, damping, and collision channels."""

from _common import _exec, _exec_ue, actor_exec, _safe

def register(mcp, send_fn):


    @mcp.tool()
    def set_simulate_physics(actor_label: str, enabled: bool = True) -> str:
        """Enable or disable physics simulation on an actor."""
        return actor_exec(actor_label, f"""
comps = target.get_components_by_class(unreal.PrimitiveComponent)
if not comps:
    print(f"ERROR: {{target.get_actor_label()}} has no primitive components")
else:
    for comp in comps:
        comp.set_simulate_physics({enabled})
    state = "enabled" if {enabled} else "disabled"
    print(f"Physics simulation {{state}} on {{target.get_actor_label()}}")
""")

    @mcp.tool()
    def set_collision_channel(actor_label: str, channel: str) -> str:
        """Set the object collision channel. channel examples: 'ECC_WorldStatic', 'ECC_WorldDynamic', 'ECC_Pawn', 'ECC_Visibility', 'ECC_Camera', 'ECC_PhysicsBody', 'ECC_Vehicle', 'ECC_Destructible'."""
        return actor_exec(actor_label, f"""
channel_name = {_safe(channel)}
channel = getattr(unreal.CollisionChannel, channel_name, None)
if channel is None:
    print(f"ERROR: Unknown collision channel '{{channel_name}}'. Use ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn, etc.")
else:
    comps = target.get_components_by_class(unreal.PrimitiveComponent)
    if not comps:
        print(f"ERROR: {{target.get_actor_label()}} has no primitive components")
    else:
        for comp in comps:
            comp.set_collision_object_type(channel)
        print(f"Set collision channel to {{channel_name}} on {{target.get_actor_label()}}")
""")

    @mcp.tool()
    def get_physics_info(actor_label: str) -> str:
        """Get mass, damping, and physics simulation state for an actor."""
        return actor_exec(actor_label, f"""
comps = target.get_components_by_class(unreal.PrimitiveComponent)
if not comps:
    print(f"{{target.get_actor_label()}} has no primitive components")
else:
    print(f"Physics info for {{target.get_actor_label()}}:")
    for comp in comps:
        print(f"  {{comp.get_name()}} ({{comp.get_class().get_name()}})")
        print(f"    Simulate Physics: {{comp.is_simulating_physics()}}")
        print(f"    Mass (kg): {{comp.get_mass()}}")
        print(f"    Linear Damping: {{comp.get_editor_property('LinearDamping')}}")
        print(f"    Angular Damping: {{comp.get_editor_property('AngularDamping')}}")
        print(f"    Enable Gravity: {{comp.get_editor_property('bEnableGravity')}}")
""")

    @mcp.tool()
    def set_physics_properties(actor_label: str, mass: float = -1.0,
                               linear_damping: float = -1.0,
                               angular_damping: float = -1.0) -> str:
        """Set physics body properties. Pass -1 to leave a property unchanged."""
        return actor_exec(actor_label, f"""
comps = target.get_components_by_class(unreal.PrimitiveComponent)
if not comps:
    print(f"ERROR: {{target.get_actor_label()}} has no primitive components")
else:
    changes = []
    for comp in comps:
        if {mass} >= 0:
            comp.set_mass_override_in_kg(unreal.Name("None"), {mass})
            changes.append(f"mass={mass}")
        if {linear_damping} >= 0:
            comp.set_editor_property("LinearDamping", {linear_damping})
            changes.append(f"linear_damping={linear_damping}")
        if {angular_damping} >= 0:
            comp.set_editor_property("AngularDamping", {angular_damping})
            changes.append(f"angular_damping={angular_damping}")
    if changes:
        print(f"Updated physics on {{target.get_actor_label()}}: {{', '.join(changes)}}")
    else:
        print("No properties changed (all values were -1)")
""")
