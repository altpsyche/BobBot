"""Lighting tools: create lights, modify properties, and set up outdoor lighting."""

from _common import _exec, _exec_ue, actor_exec, _safe, autocaptured
from _registry import bob_tool

def register(mcp, send_fn):


    @mcp.tool()
    @bob_tool(category="Lighting", output_kind="small", default_timeout=60)
    @autocaptured
    def create_light(light_type: str, x: float = 0.0, y: float = 0.0, z: float = 300.0,
                     intensity: float = 5000.0, color: str = ""):
        """Create a light actor. light_type: 'Point', 'Spot', 'Directional', 'Rect', 'Sky'. color is optional R,G,B like '255,200,150'."""
        return _exec_ue(f"""
light_type = {_safe(light_type)}
color_str = {_safe(color)}
type_map = {{
    "point": "/Script/Engine.PointLight",
    "spot": "/Script/Engine.SpotLight",
    "directional": "/Script/Engine.DirectionalLight",
    "rect": "/Script/Engine.RectLight",
    "sky": "/Script/Engine.SkyLight",
}}
lt = light_type.lower()
class_path = type_map.get(lt)
if class_path is None:
    print(f"ERROR: Unknown light type '{{light_type}}'. Use: Point, Spot, Directional, Rect, Sky")
else:
    actor_class = unreal.load_class(None, class_path)
    loc = unreal.Vector(x={x}, y={y}, z={z})
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, loc)
    if actor:
        # Set intensity on the light component
        light_comp = actor.get_components_by_class(unreal.LightComponentBase)
        if light_comp:
            light_comp[0].set_editor_property("Intensity", {intensity})
            if color_str:
                parts = [float(c.strip()) for c in color_str.split(",")]
                if len(parts) >= 3:
                    light_comp[0].set_editor_property("LightColor", unreal.Color(r=int(parts[0]), g=int(parts[1]), b=int(parts[2]), a=255))
        print(f"Created {{actor.get_actor_label()}} ({{lt}}) at ({x}, {y}, {z}) intensity={intensity}")
    else:
        print("ERROR: Failed to spawn light")
""")

    @mcp.tool()
    @bob_tool(category="Lighting", output_kind="small", default_timeout=60)
    @autocaptured
    def set_light_properties(actor_label: str, intensity: float = -1.0,
                             color: str = "", temperature: float = -1.0,
                             attenuation_radius: float = -1.0):
        """Set properties on a light actor. Pass -1 for numeric values to leave unchanged. color as 'R,G,B' like '255,200,150'."""
        return actor_exec(actor_label, f"""
color_str = {_safe(color)}
light_comps = target.get_components_by_class(unreal.LightComponentBase)
if not light_comps:
    print(f"ERROR: {{target.get_actor_label()}} has no light component")
else:
    lc = light_comps[0]
    changes = []
    if {intensity} >= 0:
        lc.set_editor_property("Intensity", {intensity})
        changes.append(f"intensity={intensity}")
    if color_str:
        parts = [float(c.strip()) for c in color_str.split(",")]
        if len(parts) >= 3:
            lc.set_editor_property("LightColor", unreal.Color(r=int(parts[0]), g=int(parts[1]), b=int(parts[2]), a=255))
            changes.append(f"color={{color_str}}")
    if {temperature} >= 0:
        lc.set_editor_property("bUseTemperature", True)
        lc.set_editor_property("Temperature", {temperature})
        changes.append(f"temperature={temperature}")
    if {attenuation_radius} >= 0:
        lc.set_editor_property("AttenuationRadius", {attenuation_radius})
        changes.append(f"attenuation_radius={attenuation_radius}")
    if changes:
        print(f"Updated {{target.get_actor_label()}}: {{', '.join(changes)}}")
    else:
        print("No properties changed")
""")

    @mcp.tool()
    @bob_tool(category="Lighting", output_kind="large", default_timeout=60)
    def get_all_lights() -> str:
        """List all light actors in the current level with type, intensity, and location."""
        return _exec_ue("""
actors = unreal.EditorLevelLibrary.get_all_level_actors()
light_classes = (unreal.PointLight, unreal.SpotLight, unreal.DirectionalLight, unreal.RectLight, unreal.SkyLight)
lights = []
for a in actors:
    if isinstance(a, light_classes):
        lights.append(a)
if not lights:
    print("No lights in current level")
else:
    print(f"Lights ({len(lights)}):")
    for light in lights:
        loc = light.get_actor_location()
        light_comps = light.get_components_by_class(unreal.LightComponentBase)
        intensity = light_comps[0].get_editor_property("Intensity") if light_comps else "N/A"
        print(f"  {light.get_actor_label()} ({light.get_class().get_name()}) intensity={intensity} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})")
""")

    @mcp.tool()
    @bob_tool(category="Lighting", output_kind="small", default_timeout=60)
    def set_lightmap_resolution(actor_label: str, resolution: int = 64) -> str:
        """Set lightmap resolution on a static mesh actor. Lower = faster builds, higher = better quality. Typical: 32-256."""
        return actor_exec(actor_label, f"""
mesh_comps = target.get_components_by_class(unreal.StaticMeshComponent)
if not mesh_comps:
    print(f"ERROR: {{target.get_actor_label()}} has no static mesh component")
else:
    for comp in mesh_comps:
        comp.set_editor_property("bOverrideLightMapRes", True)
        comp.set_editor_property("OverriddenLightMapRes", {resolution})
    print(f"Set lightmap resolution to {resolution} on {{target.get_actor_label()}}")
""")

    @mcp.tool()
    @bob_tool(category="Lighting", output_kind="small", default_timeout=60)
    def create_sky_atmosphere() -> str:
        """Create a full outdoor lighting setup: DirectionalLight (sun), SkyLight, SkyAtmosphere, ExponentialHeightFog."""
        return _exec_ue("""
created = []

# Directional Light (sun)
sun_class = unreal.load_class(None, "/Script/Engine.DirectionalLight")
sun = unreal.EditorLevelLibrary.spawn_actor_from_class(sun_class, unreal.Vector(0, 0, 0), unreal.Rotator(pitch=-50, yaw=-45, roll=0))
if sun:
    sun.set_actor_label("Sun")
    light_comps = sun.get_components_by_class(unreal.LightComponentBase)
    if light_comps:
        light_comps[0].set_editor_property("Intensity", 10.0)
        light_comps[0].set_editor_property("bAtmosphereSunLight", True)
    created.append("DirectionalLight (Sun)")

# Sky Light
sky_class = unreal.load_class(None, "/Script/Engine.SkyLight")
sky = unreal.EditorLevelLibrary.spawn_actor_from_class(sky_class, unreal.Vector(0, 0, 0))
if sky:
    sky.set_actor_label("SkyLight")
    created.append("SkyLight")

# Sky Atmosphere
atmo_class = unreal.load_class(None, "/Script/Engine.SkyAtmosphere")
atmo = unreal.EditorLevelLibrary.spawn_actor_from_class(atmo_class, unreal.Vector(0, 0, 0))
if atmo:
    atmo.set_actor_label("SkyAtmosphere")
    created.append("SkyAtmosphere")

# Exponential Height Fog
fog_class = unreal.load_class(None, "/Script/Engine.ExponentialHeightFog")
fog = unreal.EditorLevelLibrary.spawn_actor_from_class(fog_class, unreal.Vector(0, 0, 0))
if fog:
    fog.set_actor_label("HeightFog")
    fog_comps = fog.get_components_by_class(unreal.ExponentialHeightFogComponent)
    if fog_comps:
        fog_comps[0].set_editor_property("FogDensity", 0.02)
    created.append("ExponentialHeightFog")

if created:
    print(f"Created outdoor lighting setup ({len(created)} actors):")
    for name in created:
        print(f"  {name}")
else:
    print("ERROR: Failed to create lighting setup")
""")
