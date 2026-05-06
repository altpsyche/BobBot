"""Texture audit — roll up textures referenced by in-use meshes.

Walks every material on every static mesh referenced by actors in the open
level. Follows the material expression graph for `MaterialExpressionTextureBase`
nodes. Aggregates by texture path: dimensions, format, estimated bytes, refs.
Flags textures over budget.

No asset loads beyond what's already resident — meshes referenced by the world
are loaded; their material chain and the textures inside it are the only set
we touch. Triggers no shader compiles.
"""

from _common import _exec, envelope
from _registry import bob_tool


def register(mcp, send_fn):

    @mcp.tool()
    @bob_tool(category="Perf Audit", output_kind="huge", default_timeout=180)
    def audit_textures(max_textures: int = 50,
                       max_dim_warn: int = 2048,
                       max_bytes_warn: int = 4 * 1024 * 1024):
        """Walk in-use materials, sum textures by dimensions + estimated bytes.

        Flags:
          OVERSIZED_DIM(WxH) : either side exceeds max_dim_warn
          HEAVY_BYTES(N)     : estimated bytes exceed max_bytes_warn

        Returns envelope with data = {textures: [...], totals: {...}}.
        """
        # The body runs in UE Python; we hand back a structured payload by
        # printing JSON and parsing on this side.
        code = f"""
import unreal, json

def _bytes_for(tex):
    try:
        w = tex.blueprint_get_size_x()
        h = tex.blueprint_get_size_y()
    except Exception:
        try:
            w = tex.get_editor_property('Source').get_editor_property('SizeX')
            h = tex.get_editor_property('Source').get_editor_property('SizeY')
        except Exception:
            return -1, 0, 0
    # Rough mip-pyramid byte estimate. Assume 4 bytes/px (BC3/RGBA8) — close
    # enough for relative ranking; precise byte counts need build-target info.
    px = 0
    cw, ch = w, h
    while cw >= 1 and ch >= 1:
        px += cw * ch
        if cw == 1 and ch == 1:
            break
        cw = max(1, cw // 2); ch = max(1, ch // 2)
    return int(px * 1.33), w, h  # 1.33x for mip overhead

world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()

mesh_set = set()
for a in actors:
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            mesh_set.add(c.static_mesh)

mat_set = set()
for m in mesh_set:
    try:
        for slot in m.static_materials:
            mi = slot.material_interface
            if mi:
                mat_set.add(mi)
    except Exception:
        pass

texture_refs = {{}}  # tex_path -> {{tex_obj, refs}}

def _walk_material(mat_iface):
    base = mat_iface
    try:
        while isinstance(base, unreal.MaterialInstance):
            base = base.parent
    except Exception:
        return
    if not isinstance(base, unreal.Material):
        return
    try:
        exprs = unreal.BobBotLib.get_material_expressions(base)
    except Exception:
        exprs = []
    for e in exprs:
        try:
            tex = e.get_editor_property('texture')
        except Exception:
            tex = None
        if tex is None:
            try:
                tex = e.get_editor_property('Texture')
            except Exception:
                tex = None
        if isinstance(tex, unreal.Texture):
            tp = tex.get_path_name()
            if tp not in texture_refs:
                texture_refs[tp] = {{'obj': tex, 'refs': 0}}
            texture_refs[tp]['refs'] += 1

for mi in mat_set:
    _walk_material(mi)

textures = []
total_bytes = 0
for path, info in texture_refs.items():
    b, w, h = _bytes_for(info['obj'])
    if b < 0:
        continue
    flags = []
    if w >= {max_dim_warn} or h >= {max_dim_warn}:
        flags.append('OVERSIZED_DIM({{}}x{{}})'.format(w, h))
    if b >= {max_bytes_warn}:
        flags.append('HEAVY_BYTES({{}})'.format(b))
    textures.append({{
        'path': path,
        'w': w,
        'h': h,
        'bytes_est': b,
        'refs': info['refs'],
        'flags': flags,
    }})
    total_bytes += b * info['refs']

textures.sort(key=lambda t: -t['bytes_est'])
flagged = [t for t in textures if t['flags']]

result = {{
    'data': {{
        'textures': textures[:{max_textures}],
        'totals': {{
            'unique_textures': len(textures),
            'flagged': len(flagged),
            'total_bytes_weighted': total_bytes,
            'meshes_walked': len(mesh_set),
            'materials_walked': len(mat_set),
        }},
    }},
    'summary': 'unique_textures={{}} flagged={{}} weighted_bytes={{:.1f}}MB top={{}}'.format(
        len(textures), len(flagged),
        total_bytes / (1024*1024),
        ', '.join(t['path'].split('/')[-1] for t in textures[:3])
    ),
}}
print('__BOBBOT_AUDIT_TEXTURES__' + json.dumps(result))
"""
        raw = _exec(code)
        marker = "__BOBBOT_AUDIT_TEXTURES__"
        idx = raw.find(marker)
        if idx < 0:
            return envelope(summary=raw[:1500], data=None, ok=False, error="audit produced no result")
        try:
            import json
            parsed = json.loads(raw[idx + len(marker):])
        except Exception as e:
            return envelope(summary=f"audit parse failed: {e}\n{raw[:500]}", data=None, ok=False, error=str(e))
        return envelope(summary=parsed.get("summary", ""), data=parsed.get("data"))
