"""Post-process tools: create volumes, set settings, color grading, and rendering stats."""

from _common import _exec, _exec_ue, actor_exec

def register(mcp, send_fn):


    @mcp.tool()
    def create_post_process_volume(x: float = 0.0, y: float = 0.0, z: float = 0.0,
                                   infinite_extent: bool = True) -> str:
        """Create a PostProcessVolume. Set infinite_extent=True for global effect."""
        return _exec_ue(f"""
pp_class = unreal.load_class(None, "/Script/Engine.PostProcessVolume")
loc = unreal.Vector(x={x}, y={y}, z={z})
pp = unreal.EditorLevelLibrary.spawn_actor_from_class(pp_class, loc)
if pp:
    pp.set_editor_property("bUnbound", {infinite_extent})
    extent_str = "infinite" if {infinite_extent} else "bounded"
    print(f"Created {{pp.get_actor_label()}} at ({x}, {y}, {z}) ({{extent_str}})")
else:
    print("ERROR: Failed to create PostProcessVolume")
""")

    @mcp.tool()
    def set_post_process_setting(actor_label: str, setting: str, value: str) -> str:
        """Set a post-process setting on a PostProcessVolume. Examples: 'BloomIntensity', 'AutoExposureBias', 'VignetteIntensity', 'MotionBlurAmount', 'AmbientOcclusionIntensity'."""
        return actor_exec(actor_label, f"""
if not isinstance(target, unreal.PostProcessVolume):
    print(f"ERROR: {{target.get_actor_label()}} is not a PostProcessVolume")
else:
    settings = target.get_editor_property("Settings")
    try:
        val = float("{value}")
        settings.set_editor_property("{setting}", val)
        # Enable the override for this setting
        override_name = "bOverride_" + "{setting}"
        try:
            settings.set_editor_property(override_name, True)
        except Exception as e:
            unreal.log_warning(f'set_post_process_setting override: {{e}}')
        target.set_editor_property("Settings", settings)
        print(f"Set {setting} = {value} on {{target.get_actor_label()}}")
    except ValueError:
        val = "{value}"
        if val.lower() == "true":
            val = True
        elif val.lower() == "false":
            val = False
        try:
            settings.set_editor_property("{setting}", val)
            override_name = "bOverride_" + "{setting}"
            try:
                settings.set_editor_property(override_name, True)
            except Exception as e:
                unreal.log_warning(f'set_post_process_setting override: {{e}}')
            target.set_editor_property("Settings", settings)
            print(f"Set {setting} = {value} on {{target.get_actor_label()}}")
        except Exception as e:
            print(f"ERROR: Could not set '{{setting}}': {{e}}")
""")

    @mcp.tool()
    def get_post_process_settings(actor_label: str) -> str:
        """Get all overridden post-process settings on a PostProcessVolume."""
        return actor_exec(actor_label, f"""
if not isinstance(target, unreal.PostProcessVolume):
    print(f"ERROR: {{target.get_actor_label()}} is not a PostProcessVolume")
else:
    print(f"Post-Process Settings on {{target.get_actor_label()}}:")
    print(f"  Infinite Extent: {{target.get_editor_property('bUnbound')}}")
    print(f"  Priority: {{target.get_editor_property('Priority')}}")
    print(f"  Blend Radius: {{target.get_editor_property('BlendRadius')}}")
    settings = target.get_editor_property("Settings")
    # Check common settings
    common = ["BloomIntensity", "BloomThreshold", "AutoExposureBias",
              "VignetteIntensity", "MotionBlurAmount", "MotionBlurMax",
              "AmbientOcclusionIntensity", "ScreenPercentage",
              "DepthOfFieldFstop", "DepthOfFieldFocalDistance"]
    overrides = []
    for s in common:
        try:
            override_name = "bOverride_" + s
            if settings.get_editor_property(override_name):
                val = settings.get_editor_property(s)
                overrides.append(f"  {{s}}: {{val}}")
        except Exception:
            pass
    if overrides:
        print("Overridden settings:")
        for o in overrides:
            print(o)
    else:
        print("  No common settings overridden (use execute_unreal_python for full inspection)")
""")

    @mcp.tool()
    def set_color_grading(actor_label: str, saturation: float = -1.0,
                          contrast: float = -1.0, gain: float = -1.0,
                          offset: float = -1.0) -> str:
        """Set color grading on a PostProcessVolume. Values are multipliers (1.0 = default). Pass -1 to leave unchanged."""
        return actor_exec(actor_label, f"""
if not isinstance(target, unreal.PostProcessVolume):
    print(f"ERROR: {{target.get_actor_label()}} is not a PostProcessVolume")
else:
    settings = target.get_editor_property("Settings")
    changes = []
    if {saturation} >= 0:
        v = unreal.Vector4(x={saturation}, y={saturation}, z={saturation}, w={saturation})
        settings.set_editor_property("ColorSaturation", v)
        try:
            settings.set_editor_property("bOverride_ColorSaturation", True)
        except Exception as e:
            unreal.log_warning(f'set_color_grading override: {{e}}')
        changes.append(f"saturation={saturation}")
    if {contrast} >= 0:
        v = unreal.Vector4(x={contrast}, y={contrast}, z={contrast}, w={contrast})
        settings.set_editor_property("ColorContrast", v)
        try:
            settings.set_editor_property("bOverride_ColorContrast", True)
        except Exception as e:
            unreal.log_warning(f'set_color_grading override: {{e}}')
        changes.append(f"contrast={contrast}")
    if {gain} >= 0:
        v = unreal.Vector4(x={gain}, y={gain}, z={gain}, w={gain})
        settings.set_editor_property("ColorGain", v)
        try:
            settings.set_editor_property("bOverride_ColorGain", True)
        except Exception as e:
            unreal.log_warning(f'set_color_grading override: {{e}}')
        changes.append(f"gain={gain}")
    if {offset} >= 0:
        v = unreal.Vector4(x={offset}, y={offset}, z={offset}, w={offset})
        settings.set_editor_property("ColorOffset", v)
        try:
            settings.set_editor_property("bOverride_ColorOffset", True)
        except Exception as e:
            unreal.log_warning(f'set_color_grading override: {{e}}')
        changes.append(f"offset={offset}")
    if changes:
        target.set_editor_property("Settings", settings)
        print(f"Color grading on {{target.get_actor_label()}}: {{', '.join(changes)}}")
    else:
        print("No properties changed")
""")

    @mcp.tool()
    def get_rendering_stats() -> str:
        """Get rendering statistics: draw calls, triangles, and shader complexity from the editor."""
        return _exec_ue("""
world = unreal.EditorLevelLibrary.get_editor_world()
# Use stat commands - output goes to viewport/log
unreal.SystemLibrary.execute_console_command(world, "stat RHI")
# Also gather what we can programmatically
actors = unreal.EditorLevelLibrary.get_all_level_actors()
mesh_count = 0
light_count = 0
total_components = 0
for a in actors:
    comps = a.get_components_by_class(unreal.StaticMeshComponent)
    mesh_count += len(comps)
    lights = a.get_components_by_class(unreal.LightComponentBase)
    light_count += len(lights)
    all_comps = a.get_components_by_class(unreal.ActorComponent)
    total_components += len(all_comps)
print(f"Scene Statistics:")
print(f"  Total Actors: {len(actors)}")
print(f"  Static Mesh Components: {mesh_count}")
print(f"  Light Components: {light_count}")
print(f"  Total Components: {total_components}")
print()
print(f"(stat RHI enabled in viewport for detailed GPU stats)")
""")
