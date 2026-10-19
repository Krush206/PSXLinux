#!/usr/bin/env python3
"""
obj2tmd.py — Convert a Wavefront OBJ file back into a PlayStation TMD.

Inverse of tmd2obj.py.  Reads vertices (v), texture coordinates (vt)
and faces (f).  Faces may be triangles or quads; anything else is
triangulated.  Faces that carry UVs become textured primitives (FT3 /
FT4), plain faces become flat-coloured primitives (F3 / F4) using the
optional colour sidecar, or a default mid-grey.

The output TMD uses one object with relative offsets (flag=0) and the
primitive modes GsSortObject4 understands.

Usage: obj2tmd.py FILE.obj [FILE2.obj ...]
"""

import struct
import sys


def parse_obj(path):
    verts = []      # (x, y, z)
    normals = []    # (x, y, z)
    uvs = []        # (u, v) normalised 0..1
    faces = []      # ([(v_idx, vt_idx_or_None), ...], colour_or_None)
    colour_next = None
    pad_next = 0

    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("v "):
                parts = line.split()
                verts.append(tuple(int(float(v)) for v in parts[1:4]))
            elif line.startswith("vn "):
                parts = line.split()
                normals.append(tuple(int(float(v)) for v in parts[1:4]))
            elif line.startswith("vt "):
                parts = line.split()
                uvs.append((float(parts[1]), float(parts[2])))
            elif line.startswith("f "):
                refs = []
                textured = False
                for tok in line.split()[1:]:
                    bits = tok.split("/")
                    vi = int(bits[0]) - 1
                    ti = int(bits[1]) - 1 if len(bits) > 1 and bits[1] else None
                    if ti is not None:
                        textured = True
                    refs.append((vi, ti))
                faces.append((refs, textured, colour_next, pad_next))
                colour_next = None
                pad_next = 0
            elif line.startswith("# pad"):
                pad_next = int(line.split()[2])
            elif line.startswith("# colour"):
                # sidecar emitted by tmd2obj.py: "# colour r g b"
                try:
                    colour_next = tuple(int(c) for c in
                                        line.split()[2:5])
                except ValueError:
                    colour_next = None
            elif line.startswith("# gouraud"):
                # sidecar emitted by tmd2obj.py for gouraud prims:
                # "# gouraud r g b r g b ..." — take the first colour
                # (obj2tmd emits flat primitives).
                try:
                    vals = [int(c) for c in line.split()[2:5]]
                    colour_next = tuple(vals)
                except ValueError:
                    colour_next = None
    return verts, normals, uvs, faces


def pack_uv(u, v):
    """OBJ normalised UV -> PSX texel-space u8, clamped."""
    return max(0, min(255, int(round(u * 256)))), max(0, min(255, int(round(v * 256))))


def build_tmd(verts, normals, uvs, faces):
    """Assemble the TMD binary."""
    prims = []
    for refs, textured, colour, pad in faces:
        colour = colour or (128, 128, 128)
        if textured:
            corner_uvs = []
            for _vi, ti in refs:
                u, v = pack_uv(*uvs[ti]) if ti is not None else (0, 0)
                corner_uvs.append((u, v))

        if len(refs) == 4:
            v0, v1, v2, v3 = (r[0] for r in refs)
            if textured:
                # FT4: u0v0+cba | u1v1+tsb | u2v2+pad | u3v3+pad |
                #      colour | v0v1 | v2v3 | pad     (9 words + hdr)
                body = b""
                body += struct.pack("<BBH", corner_uvs[0][0],
                                    corner_uvs[0][1], 0)
                body += struct.pack("<BBH", corner_uvs[1][0],
                                    corner_uvs[1][1], 0)
                body += struct.pack("<BBH", corner_uvs[2][0],
                                    corner_uvs[2][1], 0)
                body += struct.pack("<BBH", corner_uvs[3][0],
                                    corner_uvs[3][1], 0)
                body += struct.pack("<BBBB", colour[0], colour[1],
                                    colour[2], 0x2C)      # mode2 = mode
                body += struct.pack("<HH", v0, v1)
                body += struct.pack("<HH", v2, v3)
                body += struct.pack("<I", pad)
                olen = 9
                mode = 0x2C
            else:
                # F4: colour | v0v1 | v2v3 | pad (5 words + hdr)
                body = struct.pack("<BBBB", colour[0], colour[1],
                                   colour[2], 0x28)       # mode2 = mode
                body += struct.pack("<HH", v0, v1)
                body += struct.pack("<HH", v2, v3)
                body += struct.pack("<I", pad)
                olen = 5
                mode = 0x28
        else:
            # triangulate anything with more than 4 corners
            tris = []
            if len(refs) <= 3:
                tris = [tuple(r[0] for r in refs)]
            else:
                for i in range(1, len(refs) - 1):
                    tris.append((refs[0][0], refs[i][0], refs[i + 1][0]))
            for (v0, v1, v2) in tris:
                if textured:
                    body = b""
                    for k in range(3):
                        ti = refs[k][1] if k < len(refs) else None
                        u, v = pack_uv(*uvs[ti]) if ti is not None else (0, 0)
                        body += struct.pack("<BBH", u, v, 0)
                    body += struct.pack("<BBBB", colour[0], colour[1],
                                        colour[2], 0x24)  # mode2 = mode
                    body += struct.pack("<HH", v0, v1)
                    body += struct.pack("<HH", v2, pad)  # trailing pad
                    olen = 7
                    mode = 0x24
                else:
                    body = struct.pack("<BBBB", colour[0], colour[1],
                                       colour[2], 0x20)    # mode2 = mode
                    body += struct.pack("<HH", v0, v1)
                    body += struct.pack("<HH", v2, pad)    # trailing pad
                    olen = 4
                    mode = 0x20
                hdr = struct.pack("<BBBB", olen, olen - 1, 0, mode)
                prims.append(hdr + body)
            continue
        hdr = struct.pack("<BBBB", olen, olen - 1, 0, mode)
        prims.append(hdr + body)

    # ---- layout (section order matches the original TMDs:
    #      prims, then vertices, then normals) ----
    nobj = 1
    hdr_size = 12                      # id + flag + nobj
    obj_size = 28                      # one TMD_OBJECT
    prim_top = obj_size
    prim_data = b"".join(prims)
    # pad primitive data to a multiple of 4
    if len(prim_data) % 4:
        prim_data += b"\0" * (4 - len(prim_data) % 4)

    vert_top = prim_top + len(prim_data)
    vert_data = b"".join(struct.pack("<hhhH", x, y, z, 0)
                         for (x, y, z) in verts)

    normal_top = vert_top + len(vert_data)
    normal_data = b"".join(struct.pack("<hhhH", x, y, z, 0)
                           for (x, y, z) in normals)

    out = struct.pack("<III", 0x41, 0, nobj)
    out += struct.pack("<7I",
                       vert_top, len(verts),
                       normal_top, len(normals),
                       prim_top, len(prims), 5)
    out += prim_data
    out += vert_data
    out += normal_data
    return out


def convert(path):
    base = path.rsplit(".", 1)[0]
    tmd_path = base + ".tmd"
    verts, normals, uvs, faces = parse_obj(path)
    if not verts or not faces:
        raise ValueError("no geometry in file")
    data = build_tmd(verts, normals, uvs, faces)
    with open(tmd_path, "wb") as f:
        f.write(data)
    print("%-14s -> %-14s %d verts, %d normals, %d faces (%d bytes)"
          % (path.rsplit("/", 1)[-1], tmd_path.rsplit("/", 1)[-1],
             len(verts), len(normals), len(faces), len(data)))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    for p in sys.argv[1:]:
        try:
            convert(p)
        except Exception as e:
            print("%s: error: %s" % (p, e), file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
