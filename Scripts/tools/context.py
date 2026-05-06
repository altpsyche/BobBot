"""Project context tools: gather project info and editor state for Claude."""

from _common import _exec_ue
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Context", output_kind="large", default_timeout=60)
    def get_project_info() -> str:
        """Get project info: name, engine version, enabled plugins, source modules, and current level. Call this at the start of a conversation to understand the project."""
        return _exec_ue("""
import os

# Project basics
project_path = str(unreal.Paths.get_project_file_path())
project_name = os.path.splitext(os.path.basename(project_path))[0]
engine_ver = unreal.SystemLibrary.get_engine_version()
print(f"Project: {project_name}")
print(f"Engine: {engine_ver}")
print(f"Path: {os.path.dirname(project_path)}")

# Current level
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    print()
    print(f"Current level: {world.get_path_name()}")
    print(f"Actors in level: {len(actors)}")

# Source modules (check Source/ directory)
source_dir = os.path.join(os.path.dirname(project_path), "Source")
if os.path.isdir(source_dir):
    modules = [d for d in os.listdir(source_dir) if os.path.isdir(os.path.join(source_dir, d))]
    if modules:
        print()
        print(f"Source modules ({len(modules)}):")
        for m in sorted(modules):
            print(f"  {m}")

# Plugins (check Plugins/ directory)
plugins_dir = os.path.join(os.path.dirname(project_path), "Plugins")
if os.path.isdir(plugins_dir):
    plugins = [d for d in os.listdir(plugins_dir) if os.path.isdir(os.path.join(plugins_dir, d))]
    if plugins:
        print()
        print(f"Project plugins ({len(plugins)}):")
        for p in sorted(plugins):
            print(f"  {p}")

# Content overview
content_dir = str(unreal.Paths.project_content_dir())
if os.path.isdir(content_dir):
    top_dirs = [d for d in os.listdir(content_dir) if os.path.isdir(os.path.join(content_dir, d))]
    if top_dirs:
        print()
        print(f"Content folders ({len(top_dirs)}):")
        for d in sorted(top_dirs)[:20]:
            print(f"  /Game/{d}/")
""")

    @mcp.tool()
    @bob_tool(category="Context", output_kind="large", default_timeout=60)
    def get_editor_state() -> str:
        """Get current editor state: selected actors, viewport info, and content browser selection. Useful for understanding what the user is looking at right now."""
        return _exec_ue("""
# Selected actors in viewport
selected = unreal.EditorLevelLibrary.get_selected_level_actors()
if selected:
    print(f"Selected actors ({len(selected)}):")
    for a in selected:
        loc = a.get_actor_location()
        print(f"  {a.get_actor_label()} ({a.get_class().get_name()}) at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
else:
    print("No actors selected in viewport")

# Selected assets in Content Browser
try:
    sel_assets = unreal.EditorUtilityLibrary.get_selected_assets()
    if sel_assets:
        print()
        print(f"Selected assets ({len(sel_assets)}):")
        for a in sel_assets:
            print(f"  {a.get_path_name()} ({a.get_class().get_name()})")
except Exception as e:
    unreal.log_warning(f'get_editor_state selected assets: {{e}}')

# Current level
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    print()
    print(f"Current level: {world.get_path_name()}")
""")
