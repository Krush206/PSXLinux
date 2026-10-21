#!/usr/bin/env python3
"""
gltron_data.py - Convert GLTron assets to PSXLinux-embeddable C arrays.

Outputs (into /opt/PSXLinux/gltron/):
  psx_model_data.c - the Blender Wavefront light cycle model (t-u-low.obj),
                     processed exactly like loadModel() does: rescaled to
                     CYCLE_HEIGHT, normals inverted (flags=1), split into
                     MeshParts by material.
  psx_tex_data.c   - SGI textures converted to PSX 15bpp VRAM halfwords:
                       floor 64x64 (as-is)
                       wall  64x64 (downsampled from 256x64)
                       crash 64x64 (downsampled from 256x256, STP from alpha)
                       font  64x256 (xenotron glyphs 32x32 -> 8x16, 128 cells)
"""
import struct, sys, os

SRC = '/opt/gltron-old'
OUT = '/opt/PSXLinux/gltron'
CYCLE_HEIGHT = 8.0
MODEL_FACESIZE = 4

# ---------------------------------------------------------------- SGI loader
def load_sgi(fn):
    d = open(os.path.join(SRC, fn), 'rb').read()
    magic, storage, bpc = struct.unpack('>HBB', d[0:4])
    xsize, ysize, zsize = struct.unpack('>HHH', d[6:12])
    assert magic == 474 and storage == 0 and bpc == 1, f"{fn}: unsupported SGI"
    planes = []
    off = 512
    for z in range(zsize):
        planes.append(d[off:off + xsize * ysize])
        off += xsize * ysize
    return xsize, ysize, zsize, planes

def rgb15(r, g, b, stp=0):
    r = max(0, min(255, int(r)))
    g = max(0, min(255, int(g)))
    b = max(0, min(255, int(b)))
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3) | ((stp & 1) << 15)

def peak_sample(planes, xs, ys, x0, y0, w, h):
    """Take the per-channel MAXIMUM over the w*h block.  Sparse line
    art (the wall's thin magenta strokes, the crash streaks) must not
    be averaged: a 4:1 box downsample dilutes a one-pixel line to a
    quarter of its brightness, which turned tex_wall 99% black."""
    R, G, B, A = planes
    r = g = b = a = 0
    for j in range(h):
        base = (y0 + j) * xs
        for i in range(w):
            p = base + x0 + i
            if R[p] > r: r = R[p]
            if G[p] > g: g = G[p]
            if B[p] > b: b = B[p]
            if A[p] > a: a = A[p]
    return r, g, b, a

def box_sample(planes, xs, ys, x0, y0, w, h):
    """Average RGBA over the w*h block at (x0,y0)."""
    R, G, B, A = planes
    n = w * h
    r = g = b = a = 0
    for j in range(h):
        base = (y0 + j) * xs
        for i in range(w):
            p = base + x0 + i
            r += R[p]; g += G[p]; b += B[p]; a += A[p]
    return r/n, g/n, b/n, a/n

# ---------------------------------------------------------------- OBJ loader
def load_obj():
    verts, norms, faces = [], [], []   # faces: (mat, [(vi, ni), ...])
    mats = {}                          # name -> index
    mat_list = []                      # [{name, ka, kd, ks}]
    cur_mat = 0
    mtllib = None
    for line in open(os.path.join(SRC, 't-u-low.obj')):
        t = line.split()
        if not t:
            continue
        if t[0] == 'mtllib':
            mtllib = t[1]
        elif t[0] == 'v':
            verts.append([float(x) for x in t[1:4]])
        elif t[0] == 'vn':
            norms.append([float(x) for x in t[1:4]])
        elif t[0] == 'usemtl':
            name = t[1]
            if name not in mats:
                mats[name] = len(mat_list)
                mat_list.append({'name': name})
            cur_mat = mats[name]
        elif t[0] == 'f':
            idx = []
            for v in t[1:]:
                a, _, c = v.partition('//')
                if not c:
                    a, _, c = v.partition('/')   # v/vt or v only
                idx.append((int(a), int(c) if c else 0))
            faces.append((cur_mat, idx))
    # material library
    cur = None
    for line in open(os.path.join(SRC, mtllib)):
        t = line.split()
        if not t:
            continue
        if t[0] == 'newmtl':
            cur = t[1]
        elif t[0] == 'Ka' and cur in mats:
            mat_list[mats[cur]]['ka'] = [float(x) for x in t[1:4]] + [1.0]
        elif t[0] == 'Kd' and cur in mats:
            mat_list[mats[cur]]['kd'] = [float(x) for x in t[1:4]] + [1.0]
        elif t[0] == 'Ks' and cur in mats:
            mat_list[mats[cur]]['ks'] = [float(x) for x in t[1:4]] + [1.0]
    return verts, norms, faces, mat_list

def rescale(verts, size):
    """Port of rescaleVertices() from model.c"""
    n = len(verts)
    xs = [v[0] for v in verts]; ys = [v[1] for v in verts]; zs = [v[2] for v in verts]
    x, y, z = min(xs), min(ys), min(zs)
    for v in verts:
        v[0] -= x; v[1] -= y; v[2] -= z
    xmax, ymax, zmax = max(v[0] for v in verts), max(v[1] for v in verts), max(v[2] for v in verts)
    m = max(xmax, ymax, zmax)
    for v in verts:
        v[0] = v[0]/m*size; v[1] = v[1]/m*size; v[2] = v[2]/m*size
    return [xmax/m*size, ymax/m*size, zmax/m*size]

def fltfmt(f):
    """Emit a float constant that GCC 2.91 accepts."""
    return "%.5ff" % f

def lod_cluster(verts, norms, faces, step):
    """Vertex-clustering LOD: snap vertices to a coarse grid, weld
    coincident ones, average their normals, and drop faces that
    degenerate.  Unlike area cutoffs this keeps the mesh
    watertight - no holes - at the cost of a chunkier silhouette,
    which suits the PSX-era look.  Returns new (verts, norms, faces).

    The per-face vertex list already carries (vi, ni) pairs; the
    clustered face keeps the ORIGINAL vertex order so the winding
    (and thus the lighting sidedness) is preserved.
    """
    def key(p):
        return (round(p[0] / step), round(p[1] / step), round(p[2] / step))

    cluster = {}                      # grid key -> new vertex index
    new_verts = []
    # accumulated normals per new vertex
    new_norm_acc = []
    remap = {}                        # old vertex index -> new index

    def weld(old_vi):
        if old_vi in remap:
            return remap[old_vi]
        p = verts[old_vi - 1]          # OBJ indices are 1-based
        k = key(p)
        if k not in cluster:
            cluster[k] = len(new_verts)
            new_verts.append(list(p))
            new_norm_acc.append([0.0, 0.0, 0.0])
        ni = cluster[k]
        remap[old_vi] = ni
        return ni

    new_faces = []
    for mat, idx in faces:
        seq = []
        for vi, nri in idx:
            ni = weld(vi)
            if nri:
                n = norms[nri - 1]
                a = new_norm_acc[ni]
                a[0] += n[0]; a[1] += n[1]; a[2] += n[2]
            if not seq or seq[-1] != ni:      # collapse runs
                seq.append(ni)
        if len(seq) > 1 and seq[0] == seq[-1]:
            seq.pop()
        if len(seq) >= 3:
            new_faces.append((mat, seq))

    # unit-ize the averaged normals (the converter inverts them
    # later, exactly like the original pipeline)
    new_norms = []
    for a in new_norm_acc:
        l = (a[0]*a[0] + a[1]*a[1] + a[2]*a[2]) ** 0.5
        if l < 1e-12:
            l = 1.0
        new_norms.append([a[0]/l, a[1]/l, a[2]/l])
    return new_verts, new_norms, new_faces

# LOD grid step in rescaled units (model is CYCLE_HEIGHT tall).
# 1.5 welds the 184-face mesh down to ~76 faces while keeping all
# four material parts (body, front wheel, metal, window) distinct;
# from chase-cam distance the silhouette is unchanged and the
# chunkier shading suits the PSX-era look.
LOD_STEP = 1.5

def gen_model():
    verts, norms, faces, mat_list = load_obj()
    bbox = rescale(verts, CYCLE_HEIGHT)
    n_orig = len(faces)
    # LOD: cluster-weld the mesh (see lod_cluster).  Faces that
    # welded into polygons larger than MODEL_FACESIZE are split
    # back into triangles.
    verts, norms, faces = lod_cluster(verts, norms, faces, LOD_STEP)
    split = []
    for mat, seq in faces:
        if len(seq) > MODEL_FACESIZE:
            for k in range(1, len(seq) - 1):
                split.append((mat, [seq[0], seq[k], seq[k+1]]))
        else:
            split.append((mat, seq))
    faces = [(mat, [(vi, vi) for vi in seq]) for mat, seq in split]
    print(f"model: {n_orig} faces -> LOD {len(faces)} faces "
          f"({len(verts)} welded verts), {len(mat_list)} materials, bbox={bbox}")
    out = []
    w = out.append
    w("/*\n * psx_model_data.c - GLTron light cycle model (Blender Wavefront\n"
      " * t-u-low.obj + tron.mtl), preprocessed offline exactly like\n"
      " * loadModel() in model.c: rescaled to CYCLE_HEIGHT=%s,\n"
      " * normals inverted, split into MeshParts per material.\n"
      " * Generated by tools/gltron_data.py - do not edit.\n */\n\n" % CYCLE_HEIGHT)
    w('#include "psxdata.h"\n\n')
    # materials
    for i, m in enumerate(mat_list):
        w(f"static char mat_name_{i}[] = \"{m['name']}\";\n")
    w("\nconst Material model_materials[] = {\n")
    for i, m in enumerate(mat_list):
        def c3(k):
            return ", ".join(fltfmt(v) for v in m[k])
        w(f"  {{ {{ {c3('ka')} }}, {{ {c3('kd')} }}, {{ {c3('ks')} }}, mat_name_{i} }},\n")
    w("};\n")
    w(f"const int model_nmaterials = {len(mat_list)};\n\n")
    w(f"const float model_bbox[3] = {{ {fltfmt(bbox[0])}, {fltfmt(bbox[1])}, {fltfmt(bbox[2])} }};\n\n")
    # meshparts: faces grouped per material, in original order within material
    w(f"const int model_nfaces_total = {len(faces)};\n\n")
    for mi, m in enumerate(mat_list):
        mf = [f for f in faces if f[0] == mi]
        w(f"/* material {mi} '{m['name']}': {len(mf)} faces */\n")
        w(f"const int model_fs_{mi}[{len(mf)}] = {{ ")
        w(", ".join(str(len(f[1])) for f in mf))
        w(" };\n")
        w(f"const float model_v_{mi}[{len(mf)}][{MODEL_FACESIZE}][3] = {{\n")
        for _, idx in mf:
            w("  { ")
            for k in range(MODEL_FACESIZE):
                if k < len(idx):
                    v = verts[idx[k][0]-1]
                    w("{ %s, %s, %s }, " % (fltfmt(v[0]), fltfmt(v[1]), fltfmt(v[2])))
                else:
                    w("{ 0, 0, 0 }, ")
            w("},\n")
        w("};\n")
        w(f"const float model_n_{mi}[{len(mf)}][{MODEL_FACESIZE}][3] = {{\n")
        for _, idx in mf:
            w("  { ")
            for k in range(MODEL_FACESIZE):
                if k < len(idx):
                    nv = norms[idx[k][1]-1]
                    # flags&1 == 1 in loadModel() call -> inverted normals
                    w("{ %s, %s, %s }, " % (fltfmt(-nv[0]), fltfmt(-nv[1]), fltfmt(-nv[2])))
                else:
                    w("{ 0, 0, 0 }, ")
            w("},\n")
        w("};\n")
        w(f"const int model_nf_{mi} = {len(mf)};\n\n")
    open(os.path.join(OUT, 'psx_model_data.c'), 'w').write("".join(out))
    print(f"wrote {OUT}/psx_model_data.c ({len(''.join(out))} bytes)")

def gen_textures():
    out = []
    w = out.append
    w("/*\n * psx_tex_data.c - GLTron textures as PSX 15bpp VRAM halfwords.\n"
      " * floor/wall/crash: 64x64 texels; font: 64x256 (8x16 glyphs,\n"
      " * 8 columns x 16 rows = 128 cells, cell index = char - 31).\n"
      " * Generated by tools/gltron_data.py - do not edit.\n */\n\n")
    w('#include "psxdata.h"\n\n')

    def emit_u16(name, data, per_line=8):
        w(f"const unsigned short {name}[{len(data)}] = {{\n")
        for i in range(0, len(data), per_line):
            w("  " + ", ".join("0x%04x" % v for v in data[i:i+per_line]) + ",\n")
        w("};\n\n")

    # floor 64x64, opaque
    xs, ys, zs, p = load_sgi('gltron_floor.sgi')
    assert (xs, ys) == (64, 64)
    R, G, B, A = p
    tex = []
    for j in range(64):
        for i in range(64):
            o = j*64 + i
            tex.append(rgb15(R[o], G[o], B[o]))
    emit_u16('tex_floor', tex)
    print(f"floor: {xs}x{ys} -> {len(tex)} halfwords")

    # wall 256x64 -> 64x64 (x 4:1 peak: keep the thin line work at
    # full brightness - a box average dilutes it to black)
    xs, ys, zs, p = load_sgi('gltron_wall.sgi')
    assert (xs, ys) == (256, 64)
    tex = []
    for j in range(64):
        for i in range(64):
            r, g, b, a = peak_sample(p, xs, ys, i*4, j, 4, 1)
            tex.append(rgb15(r, g, b))
    emit_u16('tex_wall', tex)
    print(f"wall: {xs}x{ys} -> 64x64")

    # crash 256x256 -> 64x64, STP from alpha (semi-transparent where
    # visible); peak-sampled so the streaks keep their brightness
    xs, ys, zs, p = load_sgi('gltron_crash.sgi')
    assert (xs, ys) == (256, 256)
    tex = []
    for j in range(64):
        for i in range(64):
            r, g, b, a = peak_sample(p, xs, ys, i*4, j*4, 4, 4)
            stp = 1 if a >= 16 else 0
            tex.append(rgb15(r, g, b, stp))
    emit_u16('tex_crash', tex)
    print(f"crash: {xs}x{ys} -> 64x64")

    # font: 2 textures 256x256, cells 32x32, index = char-31, cell index
    # within texture = index % 64, texture = index / 64.  Downsample
    # 32x32 -> 8x16 (4x2 box), pack into 64x256 sheet (8 cols of 8px,
    # 16 rows of 16px).
    ft = [load_sgi('xenotron.0.sgi'), load_sgi('xenotron.1.sgi')]
    assert all(t[0] == 256 and t[1] == 256 for t in ft)
    # 64x256 sheet of zeros
    sheet = [[0]*(64) for _ in range(256)]
    for index in range(1, 95):          # chars 32..126 (index = ch-31)
        ti = index // 64
        within = index % 64
        col, row = within % 8, within // 8
        x0, y0 = col*32, row*32
        _, _, _, p = ft[ti]
        sy = (index // 8) * 16          # sheet row
        sx = (index % 8) * 8           # sheet col
        for j in range(16):
            for i in range(8):
                r, g, b, a = box_sample(p, 256, 256, x0 + i*4, y0 + j*2, 4, 2)
                # glyph luminance = alpha (RGB is white).  Map to
                # modulation range: 16 == full vertex color.
                lum = int(a * 16 / 255)
                v = (lum << 10) | (lum << 5) | lum
                if lum > 0:
                    v |= 0x8000         # STP: semi-transparent glyph pixels
                sheet[sy + j][sx + i] = v
    flat = [sheet[j][i] for j in range(256) for i in range(64)]
    emit_u16('tex_font', flat)
    print(f"font: 2x 256x256 -> 64x256 sheet ({len(flat)} halfwords)")

    open(os.path.join(OUT, 'psx_tex_data.c'), 'w').write("".join(out))
    print(f"wrote {OUT}/psx_tex_data.c ({len(''.join(out))} bytes)")

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    gen_model()
    gen_textures()
