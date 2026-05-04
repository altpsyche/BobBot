"""Component tools: add, remove, inspect, and modify components on level actors."""

from _common import _exec, _exec_ue, actor_exec, _safe
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Components", output_kind="small", default_timeout=60)
    def add_component_to_actor(actor_label: str, component_type: str,
                               component_name: str = "") -> str:
        """Add a component to a level actor. component_type examples: 'StaticMeshComponent', 'PointLightComponent', 'AudioComponent', 'BoxCollisionComponent', 'SphereComponent'."""
        return actor_exec(actor_label, f"""
component_type = {_safe(component_type)}
component_name = {_safe(component_name)}
comp_class = getattr(unreal, component_type, None)
if comp_class is None:
    print(f"ERROR: Component type '{{component_type}}' not found. Use class names like 'StaticMeshComponent', 'PointLightComponent', etc.")
else:
    name = component_name or component_type
    comp = target.add_component_by_class(comp_class, False, unreal.Transform(), False)
    if comp:
        print(f"Added {{comp.get_class().get_name()}} to {{target.get_actor_label()}}")
    else:
        print(f"ERROR: Failed to add component. Try using execute_unreal_python for complex component setups.")
""")

    @mcp.tool()
    @bob_tool(category="Components", output_kind="small", default_timeout=60)
    def remove_component(actor_label: str, component_name: str) -> str:
        """Remove a component from a level actor by component name."""
        return actor_exec(actor_label, f"""
component_name = {_safe(component_name)}
comps = target.get_components_by_class(unreal.ActorComponent)
found = None
for c in comps:
    if c.get_name() == component_name:
        found = c
        break
if found is None:
    print(f"ERROR: Component '{{component_name}}' not found on {{target.get_actor_label()}}")
    print("Available components:")
    for c in comps:
        print(f"  {{c.get_name()}} ({{c.get_class().get_name()}})")
else:
    found.destroy_component(found)
    print(f"Removed {{component_name}} from {{target.get_actor_label()}}")
""")

    @mcp.tool()
    @bob_tool(category="Components", output_kind="large", default_timeout=60)
    def get_component_properties(actor_label: str, component_name: str) -> str:
        """Get properties of a specific component on an actor."""
        return actor_exec(actor_label, f"""
component_name = {_safe(component_name)}
comps = target.get_components_by_class(unreal.ActorComponent)
found = None
for c in comps:
    if c.get_name() == component_name:
        found = c
        break
if found is None:
    print(f"ERROR: Component '{{component_name}}' not found on {{target.get_actor_label()}}")
    print("Available components:")
    for c in comps:
        print(f"  {{c.get_name()}} ({{c.get_class().get_name()}})")
else:
    print(f"Component: {{found.get_name()}} ({{found.get_class().get_name()}})")
    print(f"Owner: {{target.get_actor_label()}}")
    if hasattr(found, 'get_relative_location'):
        loc = found.get_editor_property("RelativeLocation")
        print(f"Relative Location: ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
    if hasattr(found, 'is_visible'):
        print(f"Visible: {{found.is_visible()}}")
    if isinstance(found, unreal.StaticMeshComponent):
        mesh = found.get_editor_property("StaticMesh")
        print(f"Static Mesh: {{mesh.get_path_name() if mesh else 'None'}}")
        num_mats = found.get_num_materials()
        print(f"Materials: {{num_mats}}")
    if isinstance(found, unreal.LightComponentBase):
        print(f"Intensity: {{found.get_editor_property('Intensity')}}")
""")

    @mcp.tool()
    @bob_tool(category="Components", output_kind="small", default_timeout=60)
    def set_component_property(actor_label: str, component_name: str,
                               property_name: str, value: str) -> str:
        """Set a property on a component. Common properties: RelativeLocation, RelativeRotation, RelativeScale3D, Visibility, Intensity, StaticMesh."""
        return actor_exec(actor_label, f"""
component_name = {_safe(component_name)}
property_name = {_safe(property_name)}
value = {_safe(value)}
comps = target.get_components_by_class(unreal.ActorComponent)
found = None
for c in comps:
    if c.get_name() == component_name:
        found = c
        break
if found is None:
    print(f"ERROR: Component '{{component_name}}' not found on {{target.get_actor_label()}}")
else:
    try:
        found.set_editor_property(property_name, value)
        print(f"Set {{found.get_name()}}.{{property_name}} = {{value}}")
    except Exception as e:
        print(f"ERROR: Could not set '{{property_name}}': {{e}}")
""")
