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
"""

from _common import _exec, _safe
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
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=60)
    def get_actor_perf_signal(actor_label: str) -> str:
        """Per-actor perf-relevant facts: component count, tick group, mobility,
        casts shadow, lightmap-resolution overrides per StaticMeshComponent,
        material count per SMC, dynamic vs static lights on the actor.

        Reads only the actor's existing components — no asset loads."""
        return _exec(f"""
import unreal
label = {_safe(actor_label)}
target = None
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_actor_label() == label:
        target = a
        break
if target is None:
    print(f"ERROR: Actor '{{label}}' not found")
else:
    print(f"Actor: {{target.get_actor_label()}} ({{target.get_class().get_name()}})")
    try:
        print(f"  Tick group: {{target.primary_actor_tick.tick_group}}")
        print(f"  Tick interval: {{target.primary_actor_tick.tick_interval}}")
    except Exception:
        pass

    smcs = target.get_components_by_class(unreal.StaticMeshComponent)
    skel = target.get_components_by_class(unreal.SkeletalMeshComponent)
    lights = target.get_components_by_class(unreal.LightComponent)
    nia = target.get_components_by_class(unreal.NiagaraComponent)
    all_comps = target.get_components_by_class(unreal.ActorComponent)
    print(f"  Components: total={{len(all_comps)}} smc={{len(smcs)}} skm={{len(skel)}} lights={{len(lights)}} niagara={{len(nia)}}")

    for c in smcs:
        m = c.static_mesh
        mname = m.get_name() if m else "(none)"
        try:
            mob = c.mobility
        except Exception:
            mob = "?"
        try:
            shadow = c.cast_shadow
        except Exception:
            shadow = "?"
        try:
            override_lm = c.overridden_light_map_res
        except Exception:
            override_lm = -1
        try:
            mat_count = c.get_num_materials() if hasattr(c, 'get_num_materials') else (
                unreal.BobBotLib.get_static_mesh_material_count(m) if m else 0)
        except Exception:
            mat_count = -1
        cls = type(c).__name__
        try:
            inst = c.get_instance_count() if hasattr(c, 'get_instance_count') else 0
        except Exception:
            inst = 0
        print(f"  SMC[{{cls}}] {{mname}} | mob={{mob}} shadow={{shadow}} mats={{mat_count}} lightmap_override={{override_lm}} instances={{inst}}")

    if lights:
        print("  Lights:")
        for c in lights:
            try:
                mob = c.mobility
            except Exception:
                mob = "?"
            try:
                cs = c.cast_shadows
            except Exception:
                cs = "?"
            try:
                inten = c.intensity
            except Exception:
                inten = "?"
            print(f"    {{type(c).__name__}} | mob={{mob}} cast_shadows={{cs}} intensity={{inten}}")
""")

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=60)
    def get_lightmap_density_summary() -> str:
        """Roll up lightmap budget across all actors in the open level.
        Reports per-mobility totals (Static / Stationary / Movable) of
        lightmap pixel² (resolution × resolution per SMC), plus the top 20
        contributors. Helps spot single high-budget overrides drowning the
        lightmap pool."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    print(f"Map: {world.get_path_name()}")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    by_mobility = {"Static": 0, "Stationary": 0, "Movable": 0, "Unknown": 0}
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
                mob_str = str(c.mobility).split(".")[-1]
            except Exception:
                mob_str = "Unknown"
            by_mobility[mob_str] = by_mobility.get(mob_str, 0) + pixels
            contributors.append((pixels, res, a.get_actor_label(), m.get_name(), mob_str, override > 0))
    contributors.sort(reverse=True)
    print(f"SMCs scanned: {smc_count}")
    print(f"Total lightmap pixels: {total_pixels:,} (~{total_pixels/(1024*1024):.1f} M)")
    print(f"By mobility:")
    for k, v in by_mobility.items():
        if v:
            print(f"  {k}: {v:,} ({v/(1024*1024):.1f} M)")
    print(f"\\nTop 20 contributors:")
    for px, res, lbl, mn, mob, ovr in contributors[:20]:
        print(f"  {px:>10,} px (res {res}{'*' if ovr else ' '}) {mob:>10} | actor={lbl} | mesh={mn}")
""")

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=60)
    def get_texture_pool_status() -> str:
        """Texture streaming pool budget + utilization, plus dynamic resolution
        and a few other r.* texture cvars. Captures via console commands so
        the values are live."""
        return _exec("""
import unreal
out = []
def grab(cmd):
    try:
        unreal.SystemLibrary.execute_console_command(None, cmd)
    except Exception:
        pass

# Trigger the pool reports — output goes to UE log; we then ask for it.
unreal.SystemLibrary.execute_console_command(None, "Memreport -full")
# Read tunable cvars directly
for cv in [
    "r.Streaming.PoolSize",
    "r.Streaming.MaxTempMemoryAllowed",
    "r.Streaming.HLODStrategy",
    "r.MipMapLODBias",
    "r.Streaming.UseAllMips",
    "r.MaxAnisotropy",
]:
    try:
        v = unreal.SystemLibrary.get_console_variable_string_value(cv)
        out.append(f"  {cv} = {v}")
    except Exception:
        pass
print("Texture / streaming cvars:")
for line in out:
    print(line)
print()
print("(Texture pool live stats: run get_output_log(80) right after this — `Memreport` writes pool size + used + over-budget warnings to the UE log.)")
""")

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=60)
    def get_light_summary() -> str:
        """Roll up every light in the open level by mobility, type, and shadow
        casting. Flags movable shadow-casters (most expensive) and gives
        per-class counts."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
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
                mob_str = str(c.mobility).split(".")[-1]
            except Exception:
                mob_str = "Unknown"
            by_mobility[mob_str] = by_mobility.get(mob_str, 0) + 1
            try:
                cs = bool(c.cast_shadows)
            except Exception:
                cs = False
            if mob_str == "Movable" and cs:
                try:
                    inten = c.intensity
                except Exception:
                    inten = -1
                movable_shadow_casters.append((inten, a.get_actor_label(), cls))
    print(f"Total light components: {total}")
    print(f"By class:")
    for k, v in sorted(by_class.items(), key=lambda kv: -kv[1]):
        print(f"  {k}: {v}")
    print(f"By mobility:")
    for k, v in by_mobility.items():
        if v:
            print(f"  {k}: {v}")
    print(f"\\nMovable shadow-casters ({len(movable_shadow_casters)}) — typically the most expensive:")
    movable_shadow_casters.sort(reverse=True)
    for inten, lbl, cls in movable_shadow_casters[:30]:
        print(f"  {cls} intensity={inten} | {lbl}")
""")

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=60)
    def get_foliage_density_report() -> str:
        """Counts foliage and HISM/ISM instances grouped by mesh asset, total
        instance count, and unique-type count. Helps spot bloat (one mesh
        with 50k instances) vs diversity (1k unique types each at 50)."""
        return _exec("""
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    print("ERROR: No level open")
else:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    foliage_by_mesh = {}  # mesh_path -> total instances across HISMs/ISMs/foliage
    inst_actor_count = 0
    foliage_actor_count = 0

    try:
        FoliageISMC = unreal.FoliageInstancedStaticMeshComponent
    except AttributeError:
        FoliageISMC = None

    for a in actors:
        is_foliage = a.get_class().get_name() in ("InstancedFoliageActor",)
        if is_foliage:
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
            foliage_by_mesh[m.get_path_name()] = foliage_by_mesh.get(m.get_path_name(), 0) + max(n, 0)

    total_instances = sum(foliage_by_mesh.values())
    print(f"InstancedFoliageActors: {foliage_actor_count}")
    print(f"ISM/HISM components scanned: {inst_actor_count}")
    print(f"Unique meshes used as foliage/instances: {len(foliage_by_mesh)}")
    print(f"Total instances: {total_instances:,}")
    if foliage_by_mesh:
        ranked = sorted(foliage_by_mesh.items(), key=lambda kv: -kv[1])
        print(f"\\nTop 30 by instance count:")
        for path, n in ranked[:30]:
            print(f"  {n:>8,} | {path}")
""")
