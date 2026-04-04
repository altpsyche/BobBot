"""Tags and layers: manage actor tags and editor layers."""

from _common import _exec, _exec_ue, actor_exec

def register(mcp, send_fn):


    @mcp.tool()
    def set_actor_tags(actor_label: str, tags: str) -> str:
        """Set tags on an actor. tags is a comma-separated list like 'Enemy,Boss,Phase1'."""
        return actor_exec(actor_label, f"""
tag_list = [t.strip() for t in "{tags}".split(",") if t.strip()]
target.tags = [unreal.Name(t) for t in tag_list]
print(f"Set {{len(tag_list)}} tag(s) on {{target.get_actor_label()}}: {{', '.join(tag_list)}}")
""")

    @mcp.tool()
    def get_actors_by_tag(tag: str) -> str:
        """Find all actors in the level with a specific tag."""
        return _exec_ue(f"""
actors = unreal.GameplayStatics.get_all_actors_with_tag(unreal.EditorLevelLibrary.get_editor_world(), "{tag}")
if actors:
    print(f"Actors with tag '{tag}' ({{len(actors)}}):")
    for a in actors:
        loc = a.get_actor_location()
        print(f"  {{a.get_actor_label()}} ({{a.get_class().get_name()}}) at ({{loc.x:.0f}}, {{loc.y:.0f}}, {{loc.z:.0f}})")
else:
    print(f"No actors found with tag '{tag}'")
""")

    @mcp.tool()
    def set_actor_layer(actor_label: str, layer_name: str) -> str:
        """Set the editor layer for an actor. Layers help organize actors in the editor."""
        return actor_exec(actor_label, f"""
layers = unreal.LayersSubsystem()
layers.add_actor_to_layer(target, "{layer_name}")
print(f"Added {{target.get_actor_label()}} to layer '{layer_name}'")
""")

    @mcp.tool()
    def get_actor_tags(actor_label: str) -> str:
        """Get all tags on an actor."""
        return actor_exec(actor_label, f"""
tags = target.tags
if tags:
    print(f"Tags on {{target.get_actor_label()}} ({{len(tags)}}):")
    for t in tags:
        print(f"  {{t}}")
else:
    print(f"{{target.get_actor_label()}} has no tags")
""")
