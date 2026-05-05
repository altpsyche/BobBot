"""Map perf audit toolkit.

Two roles:
- `audit_map_perf` — quick first-pass triage (one call → structured report).
  Use as triangulation input, not as the answer. Pair with the atomic
  getters below + stat captures + cvar dumps.
- atomic getters (`get_actor_perf_signal`, `get_lightmap_density_summary`,
  `get_texture_pool_status`, `get_light_summary`, `get_foliage_density_report`)
  — single-signal probes the agent composes freely.

Mobile-aware. No Nanite assumption. Splits StaticMeshComponent references
into SMC / ISM / HISM so non-instanced reuse can be flagged for HISM
conversion. All tools read only assets already resident in the open world
to avoid triggering shader compiles.

Every atomic getter returns an explicit envelope with structured `data`,
not a printed text blob. The `__BOBBOT_<TAG>__` marker convention lets
the UE-side body (run via _exec) emit JSON which the Python-side wrapper
parses into envelope.data.
"""

import json as _json

from _common import _exec, _safe, envelope


# --------------------------------------------------------------------------- #
# Internal helpers — parse the JSON marker emitted by UE-side tool bodies and
# build an envelope. Keeps the @bob_tool functions short.
# --------------------------------------------------------------------------- #

def _envelope_from_marker(raw, marker, summarize):
    """Find `marker<json>` in raw stdout, parse, hand off to `summarize(data)`
    which returns the human summary string. Returns an envelope."""
    if not isinstance(raw, str):
        return envelope(summary=str(raw)[:1500], data=None, ok=False, error="non-string raw")
    idx = raw.find(marker)
    if idx < 0:
        return envelope(summary=raw[:1500], data=None, ok=False, error=f"marker {marker} missing")
    try:
        data = _json.loads(raw[idx + len(marker):].strip())
    except (ValueError, TypeError) as e:
        return envelope(summary=raw[:1500], data=None, ok=False, error=f"parse: {e}")
    if isinstance(data, dict) and data.get("error"):
        return envelope(summary=data["error"], data=None, ok=False, error=data["error"])
    return envelope(summary=summarize(data), data=data)


def _summarize_lights(d):
    by_cls = d.get("by_class", {})
    by_mob = d.get("by_mobility", {})
    movable = d.get("movable_shadow_casters", []) or []
    cls_str = ", ".join(f"{k}={v}" for k, v in sorted(by_cls.items(), key=lambda kv: -kv[1])[:5])
    mob_str = ", ".join(f"{k}={v}" for k, v in by_mob.items())
    return f"total={d.get('total', 0)} | mobility: {mob_str} | top classes: {cls_str} | movable_shadow_casters={len(movable)}"


def _summarize_lightmap(d):
    by_mob = d.get("by_mobility", {})
    mob_mp = ", ".join(f"{k}={v/1_000_000:.1f}MP" for k, v in by_mob.items() if v)
    total_mp = d.get("total_pixels", 0) / 1_000_000
    top = d.get("top", [])
    return (f"smcs={d.get('smc_count', 0)} | total={total_mp:.1f}MP | mobility: {mob_mp} | "
            f"top contributor: {top[0]['actor']} ({top[0]['pixels']:,} px)" if top else
            f"smcs={d.get('smc_count', 0)} | total={total_mp:.1f}MP | mobility: {mob_mp}")


def _summarize_foliage(d):
    return (f"foliage_actors={d.get('foliage_actors', 0)} | "
            f"isms={d.get('inst_components', 0)} | "
            f"unique_meshes={d.get('unique_meshes', 0)} | "
            f"total_instances={d.get('total_instances', 0):,}")


def _summarize_actor_perf(d):
    if d.get("error"):
        return d["error"]
    comps = d.get("components", {})
    smcs = d.get("smcs", []) or []
    lights = d.get("lights", []) or []
    return (f"{d.get('actor', '?')} ({d.get('class', '?')}) | "
            f"comps total={comps.get('total', 0)} smc={comps.get('smc', 0)} "
            f"skm={comps.get('skm', 0)} lights={comps.get('lights', 0)} "
            f"niagara={comps.get('niagara', 0)} | smc_overrides={sum(1 for s in smcs if s.get('lightmap_override', 0) > 0)} | "
            f"shadow_lights={sum(1 for L in lights if L.get('cast_shadows'))}")


def _summarize_texture_pool(d):
    cvars = d.get("cvars", {})
    return "; ".join(f"{k}={v}" for k, v in cvars.items())
from _registry import bob_tool


def register(mcp, send_fn):

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=180)
    def audit_map_perf(
        max_meshes: int = 50,
        heavy_tris: int = 20000,
        high_tris_no_lod: int = 5000,
        instance_threshold: int = 50,
        high_lightmap: int = 256,
        many_materials: int = 4,
        complex_material_exprs: int = 50,
    ) -> str:
        """Walk the open level and report prioritized perf outliers.

        Mobile-aware: no Nanite assumption. Reports the top `max_meshes` mesh
        outliers by score (refs * tris), plus Niagara emitter counts and
        complex materials.

        Flags applied to each unique referenced mesh:
          HEAVY_TRIS        : LOD0 triangles > heavy_tris
          HIGH_TRIS_NO_LOD  : LOD0 tris > high_tris_no_lod and runtime LODs < 2
          HIGH_LIGHTMAP(N)  : lightmap resolution > high_lightmap
          MANY_MATERIALS(N) : material slots > many_materials
          SHOULD_INSTANCE(N): used as plain SMC (not HISM/ISM) >= instance_threshold times

        Niagara: HEAVY (>5 enabled emitters) / MASSIVE (>15).
        Materials: COMPLEX (>complex_material_exprs expressions) / OVERCOMPLEX (>3x that).

        Reads only assets already resident from the open world — no extra loads.
        """
        return _exec(f"""
import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    HEAVY_TRIS = {heavy_tris}
    HIGH_TRIS_NO_LOD = {high_tris_no_lod}
    INSTANCE_THRESHOLD = {instance_threshold}
    HIGH_LIGHTMAP = {high_lightmap}
    MANY_MATERIALS = {many_materials}
    COMPLEX_EXPRS = {complex_material_exprs}
    OVERCOMPLEX_EXPRS = COMPLEX_EXPRS * 3
    MAX_MESHES = {max_meshes}

    print(f"Map: {{world.get_path_name()}}")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    print(f"Actors: {{len(actors)}}")

    mesh_refs = {{}}
    niagara_refs = {{}}

    SMC = unreal.StaticMeshComponent
    ISMC = unreal.InstancedStaticMeshComponent
    try:
        HISMC = unreal.HierarchicalInstancedStaticMeshComponent
    except AttributeError:
        HISMC = ISMC
    NC = unreal.NiagaraComponent

    for a in actors:
        for c in a.get_components_by_class(SMC):
            m = c.static_mesh
            if not m:
                continue
            p = m.get_path_name()
            entry = mesh_refs.setdefault(p, {{'smc': 0, 'hism': 0, 'ism': 0, 'obj': m}})
            try:
                inst_count = c.get_instance_count() if hasattr(c, 'get_instance_count') else 1
            except Exception:
                inst_count = 1
            if isinstance(c, HISMC):
                entry['hism'] += max(inst_count, 1)
            elif isinstance(c, ISMC):
                entry['ism'] += max(inst_count, 1)
            else:
                entry['smc'] += 1
        for c in a.get_components_by_class(NC):
            ns = c.get_asset()
            if ns:
                p = ns.get_path_name()
                niagara_refs[p] = niagara_refs.get(p, 0) + 1

    print(f"Unique meshes referenced: {{len(mesh_refs)}}")
    print(f"Niagara systems referenced: {{len(niagara_refs)}}")

    lib = unreal.BobBotLib
    findings = []
    for path, info in mesh_refs.items():
        m = info['obj']
        runtime_lods = lib.get_static_mesh_runtime_lod_count(m)
        try:
            tris0 = lib.get_static_mesh_num_triangles(m, 0)
        except Exception:
            tris0 = -1
        mat_count = lib.get_static_mesh_material_count(m)
        lightmap = lib.get_static_mesh_lightmap_resolution(m)
        nanite = lib.get_static_mesh_nanite_enabled(m)
        flags = []
        if tris0 > HEAVY_TRIS:
            flags.append("HEAVY_TRIS")
        if tris0 > HIGH_TRIS_NO_LOD and runtime_lods < 2:
            flags.append("HIGH_TRIS_NO_LOD")
        if lightmap > HIGH_LIGHTMAP:
            flags.append(f"HIGH_LIGHTMAP({{lightmap}})")
        if mat_count > MANY_MATERIALS:
            flags.append(f"MANY_MATERIALS({{mat_count}})")
        if info['smc'] >= INSTANCE_THRESHOLD:
            flags.append(f"SHOULD_INSTANCE({{info['smc']}}x)")
        if flags:
            total_refs = info['smc'] + info['hism'] + info['ism']
            score = total_refs * max(tris0, 1)
            findings.append((score, path, info, runtime_lods, tris0, lightmap, mat_count, nanite, flags))

    findings.sort(reverse=True)
    print()
    print(f"=== Mesh outliers ({{len(findings)}} flagged, showing top {{min(MAX_MESHES, len(findings))}}) ===")
    print(f"{{'score':>10}} | {{'smc':>5}} {{'hism':>6}} {{'ism':>5}} | runtime/source LODs | tris0 | lightmap | mats | nanite | flags | path")
    for f in findings[:MAX_MESHES]:
        score, path, info, rl, tris0, lm, mc, nan, flags = f
        sl = lib.get_static_mesh_lod_count(info['obj'])
        print(f"{{score:>10}} | {{info['smc']:>5}} {{info['hism']:>6}} {{info['ism']:>5}} | {{rl}}/{{sl}} | {{tris0:>5}} | {{lm:>4}} | {{mc:>2}} | {{str(nan)[0]}} | {{','.join(flags):40}} | {{path}}")

    # Niagara
    print()
    print(f"=== Niagara outliers ===")
    ns_findings = []
    for path, refs in niagara_refs.items():
        ns = unreal.EditorAssetLibrary.find_asset_data(path)
        # Only inspect already-loaded systems (avoid a fresh load triggering compile)
        try:
            asset = ns.get_asset() if ns and ns.is_valid() else None
        except Exception:
            asset = None
        if not isinstance(asset, unreal.NiagaraSystem):
            continue
        ec = lib.get_niagara_emitter_count(asset)
        en = sum(1 for i in range(ec) if lib.get_niagara_emitter_enabled(asset, i))
        flag = "MASSIVE" if en > 15 else ("HEAVY" if en > 5 else "OK")
        ns_findings.append((refs * en, refs, ec, en, flag, path))
    ns_findings.sort(reverse=True)
    flagged = [x for x in ns_findings if x[4] != "OK"]
    print(f"flagged: {{len(flagged)}} (out of {{len(ns_findings)}})")
    for s, r, c, e, f, p in flagged[:20]:
        print(f"  refs={{r}} emitters={{c}} enabled={{e}} {{f}} | {{p}}")
    print(f"all niagara (top 5 by score):")
    for s, r, c, e, f, p in ns_findings[:5]:
        print(f"  refs={{r}} emitters={{c}} enabled={{e}} {{f}} | {{p}}")

    # Materials (only those already loaded via mesh static_materials chain)
    print()
    print(f"=== Material complexity (referenced root materials) ===")
    mat_findings = []
    seen_mats = set()
    for path, info in mesh_refs.items():
        m = info['obj']
        try:
            slots = m.static_materials
        except Exception:
            continue
        for slot in slots:
            mi = slot.material_interface
            if not mi:
                continue
            base = mi
            try:
                while isinstance(base, unreal.MaterialInstance):
                    base = base.parent
            except Exception:
                base = None
            if isinstance(base, unreal.Material):
                bp = base.get_path_name()
                if bp in seen_mats:
                    continue
                seen_mats.add(bp)
                exprs = lib.get_material_expressions(base)
                n = len(exprs)
                if n > OVERCOMPLEX_EXPRS:
                    mat_findings.append((n, "OVERCOMPLEX", bp))
                elif n > COMPLEX_EXPRS:
                    mat_findings.append((n, "COMPLEX", bp))
    mat_findings.sort(reverse=True)
    print(f"unique root materials: {{len(seen_mats)}}, flagged: {{len(mat_findings)}}")
    for n, f, p in mat_findings[:20]:
        print(f"  expressions={{n}} {{f}} | {{p}}")

    # Summary line
    print()
    print(f"=== Summary ===")
    print(f"  meshes_flagged: {{len(findings)}}")
    print(f"  niagara_flagged: {{len(flagged)}}")
    print(f"  materials_flagged: {{len(mat_findings)}}")
""")

    # ----------------------------------------------------------------- #
    # Atomic perf signals — composed by the agent, not the orchestrator.
    # ----------------------------------------------------------------- #

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="large", default_timeout=60)
    def get_actor_perf_signal(actor_label: str):
        """Per-actor perf-relevant facts: component count, tick group, mobility,
        casts shadow, lightmap-resolution overrides per StaticMeshComponent,
        material count per SMC, dynamic vs static lights on the actor.

        Reads only the actor's existing components — no asset loads. Returns
        structured `data` with components/smcs/lights breakdown."""
        raw = _exec(f"""
import unreal, json
label = {_safe(actor_label)}
target = None
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_actor_label() == label:
        target = a
        break
if target is None:
    print('__BOBBOT_ACTOR_PERF__' + json.dumps({{'error': "Actor '" + label + "' not found"}}))
else:
    smcs = target.get_components_by_class(unreal.StaticMeshComponent)
    skel = target.get_components_by_class(unreal.SkeletalMeshComponent)
    lights_cs = target.get_components_by_class(unreal.LightComponent)
    nia = target.get_components_by_class(unreal.NiagaraComponent)
    all_comps = target.get_components_by_class(unreal.ActorComponent)
    smc_data = []
    for c in smcs:
        m = c.static_mesh
        try: mob = str(c.mobility).split('.')[-1]
        except Exception: mob = 'Unknown'
        try: shadow = bool(c.cast_shadow)
        except Exception: shadow = False
        try: override_lm = int(c.overridden_light_map_res)
        except Exception: override_lm = -1
        try:
            mat_count = c.get_num_materials() if hasattr(c, 'get_num_materials') else (
                unreal.BobBotLib.get_static_mesh_material_count(m) if m else 0)
        except Exception:
            mat_count = -1
        try: inst = c.get_instance_count() if hasattr(c, 'get_instance_count') else 0
        except Exception: inst = 0
        smc_data.append({{
            'class': type(c).__name__,
            'mesh': m.get_name() if m else None,
            'mobility': mob, 'cast_shadow': shadow,
            'lightmap_override': override_lm, 'materials': mat_count,
            'instances': inst,
        }})
    light_data = []
    for c in lights_cs:
        try: mob = str(c.mobility).split('.')[-1]
        except Exception: mob = 'Unknown'
        try: cs = bool(c.cast_shadows)
        except Exception: cs = False
        try: inten = float(c.intensity)
        except Exception: inten = -1
        light_data.append({{
            'class': type(c).__name__, 'mobility': mob,
            'cast_shadows': cs, 'intensity': inten,
        }})
    try:
        tick_group = str(target.primary_actor_tick.tick_group).split('.')[-1]
        tick_interval = float(target.primary_actor_tick.tick_interval)
    except Exception:
        tick_group, tick_interval = 'Unknown', -1.0
    print('__BOBBOT_ACTOR_PERF__' + json.dumps({{
        'actor': target.get_actor_label(),
        'class': target.get_class().get_name(),
        'tick_group': tick_group,
        'tick_interval': tick_interval,
        'components': {{
            'total': len(all_comps), 'smc': len(smcs), 'skm': len(skel),
            'lights': len(lights_cs), 'niagara': len(nia),
        }},
        'smcs': smc_data,
        'lights': light_data,
    }}))
""")
        return _envelope_from_marker(raw, "__BOBBOT_ACTOR_PERF__", _summarize_actor_perf)

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="large", default_timeout=60)
    def get_lightmap_density_summary():
        """Roll up lightmap budget across all actors in the open level.
        Reports per-mobility totals (Static / Stationary / Movable) of
        lightmap pixel² (resolution × resolution per SMC), plus the top 20
        contributors. Returns structured `data` with totals + contributors list."""
        raw = _exec("""
import unreal, json
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print('__BOBBOT_LIGHTMAP_DENSITY__' + json.dumps({'error': 'no level open'}))
else:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    by_mobility = {'Static': 0, 'Stationary': 0, 'Movable': 0, 'Unknown': 0}
    contributors = []
    total_pixels = 0
    smc_count = 0
    for a in actors:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.static_mesh
            if not m:
                continue
            smc_count += 1
            try:
                override = c.overridden_light_map_res
            except Exception:
                override = 0
            base = unreal.BobBotLib.get_static_mesh_lightmap_resolution(m)
            res = override if override and override > 0 else base
            if res <= 0:
                continue
            pixels = res * res
            total_pixels += pixels
            try:
                mob_str = str(c.mobility).split('.')[-1]
            except Exception:
                mob_str = 'Unknown'
            by_mobility[mob_str] = by_mobility.get(mob_str, 0) + pixels
            contributors.append({
                'pixels': pixels, 'res': res, 'actor': a.get_actor_label(),
                'mesh': m.get_name(), 'mobility': mob_str, 'override': override > 0,
            })
    contributors.sort(key=lambda d: -d['pixels'])
    print('__BOBBOT_LIGHTMAP_DENSITY__' + json.dumps({
        'smc_count': smc_count,
        'total_pixels': total_pixels,
        'by_mobility': {k: v for k, v in by_mobility.items() if v},
        'top': contributors[:20],
    }))
""")
        return _envelope_from_marker(raw, "__BOBBOT_LIGHTMAP_DENSITY__", _summarize_lightmap)

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="small", default_timeout=30)
    def get_texture_pool_status():
        """Texture streaming pool budget + utilization, plus dynamic resolution
        and a few other r.* texture cvars. Returns structured `data.cvars` map.
        Live pool stats are written by `Memreport -full` to UE's output log;
        pair with `get_output_log` to read them."""
        raw = _exec("""
import unreal, json
unreal.SystemLibrary.execute_console_command(None, 'Memreport -full')
out = {}
for cv in ['r.Streaming.PoolSize', 'r.Streaming.MaxTempMemoryAllowed',
           'r.Streaming.HLODStrategy', 'r.MipMapLODBias',
           'r.Streaming.UseAllMips', 'r.MaxAnisotropy']:
    try:
        out[cv] = unreal.SystemLibrary.get_console_variable_string_value(cv)
    except Exception as e:
        out[cv] = '<unreadable: {}>'.format(e)
print('__BOBBOT_TEXTURE_POOL__' + json.dumps({
    'cvars': out,
    'note': 'Pool live stats logged via Memreport -full; call get_output_log(80) to read them.',
}))
""")
        return _envelope_from_marker(raw, "__BOBBOT_TEXTURE_POOL__", _summarize_texture_pool)

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="large", default_timeout=60)
    def get_light_summary():
        """Roll up every light in the open level by mobility, type, and shadow
        casting. Flags movable shadow-casters (most expensive) and gives
        per-class counts. Returns structured `data` with by_class, by_mobility,
        movable_shadow_casters list."""
        raw = _exec("""
import unreal, json
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print('__BOBBOT_LIGHT_SUMMARY__' + json.dumps({'error': 'no level open'}))
else:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    by_class = {}
    by_mobility = {"Static": 0, "Stationary": 0, "Movable": 0, "Unknown": 0}
    movable_shadow_casters = []
    total = 0
    for a in actors:
        for c in a.get_components_by_class(unreal.LightComponent):
            total += 1
            cls = type(c).__name__
            by_class[cls] = by_class.get(cls, 0) + 1
            try:
                mob_str = str(c.mobility).split('.')[-1]
            except Exception:
                mob_str = 'Unknown'
            by_mobility[mob_str] = by_mobility.get(mob_str, 0) + 1
            try:
                cs = bool(c.cast_shadows)
            except Exception:
                cs = False
            if mob_str == 'Movable' and cs:
                try:
                    inten = c.intensity
                except Exception:
                    inten = -1
                movable_shadow_casters.append({'intensity': inten, 'actor': a.get_actor_label(), 'class': cls})
    movable_shadow_casters.sort(key=lambda d: -d['intensity'])
    print('__BOBBOT_LIGHT_SUMMARY__' + json.dumps({
        'total': total,
        'by_class': by_class,
        'by_mobility': {k: v for k, v in by_mobility.items() if v},
        'movable_shadow_casters': movable_shadow_casters[:30],
    }))
""")
        return _envelope_from_marker(raw, "__BOBBOT_LIGHT_SUMMARY__", _summarize_lights)


    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="large", default_timeout=60)
    def get_foliage_density_report():
        """Counts foliage and HISM/ISM instances grouped by mesh asset, total
        instance count, and unique-type count. Helps spot bloat (one mesh
        with 50k instances) vs diversity (1k unique types each at 50).
        Returns structured `data` with totals + top 30 meshes by count."""
        raw = _exec("""
import unreal, json
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print('__BOBBOT_FOLIAGE_REPORT__' + json.dumps({'error': 'no level open'}))
else:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    foliage_by_mesh = {}
    inst_actor_count = 0
    foliage_actor_count = 0
    for a in actors:
        if a.get_class().get_name() == 'InstancedFoliageActor':
            foliage_actor_count += 1
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            inst_actor_count += 1
            m = c.static_mesh
            if not m:
                continue
            try:
                n = c.get_instance_count() if hasattr(c, 'get_instance_count') else 0
            except Exception:
                n = 0
            p = m.get_path_name()
            foliage_by_mesh[p] = foliage_by_mesh.get(p, 0) + max(n, 0)
    ranked = sorted(foliage_by_mesh.items(), key=lambda kv: -kv[1])
    top = [{'mesh': p, 'instances': n} for p, n in ranked[:30]]
    print('__BOBBOT_FOLIAGE_REPORT__' + json.dumps({
        'foliage_actors': foliage_actor_count,
        'inst_components': inst_actor_count,
        'unique_meshes': len(foliage_by_mesh),
        'total_instances': sum(foliage_by_mesh.values()),
        'top': top,
    }))
""")
        return _envelope_from_marker(raw, "__BOBBOT_FOLIAGE_REPORT__", _summarize_foliage)
