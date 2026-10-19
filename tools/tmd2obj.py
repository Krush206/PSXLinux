#!/usr/bin/env python3
"""
tmd2obj.py — Convert a PlayStation TMD 3D model to Wavefront OBJ (open format).

TMD layout (little-endian), matching the game's GsSortObject4 parser:
    +00 u32 id     = 0x00000041
    +04 u32 flag   (0 = offsets relative to object header)
    +08 u32 nobj
    +0C TMD_OBJECT[nobj] — 28 bytes each:
         vert_top u32, n_vert u32, normal_top u32, n_normal u32,
         primitive_top u32, n_primitive u32, scale u32
    Vertex = int16 x, y, z, pad (8 bytes).
    Primitive packets start with a u32:
         byte0 olen (packet length in u32 words), byte1 ilen,
         byte2 flag, byte3 mode (GPU command 0x20..0x3F)
    Supported modes (as in GsSortObject4):
         0x20 F3, 0x24 FT3, 0x28 F4, 0x2C FT4,
         0x30 G3, 0x34 GT3, 0x38 G4, 0x3C GT4

Writes <name>.obj (vertices, UVs, faces) and <name>.mtl (placeholder
material).  Quads are kept as 4-corner faces so Blender round-trips.

Usage: tmd2obj.py FILE.tmd [FILE2.tmd ...]
"""

import struct
import sys

# mode -> (name, textured, gouraud, corners)
MODES = {
    0x20: ("F3",  False, False, 3),
    0x24: ("FT3", True,  False, 3),
    0x28: ("F4",  False, False, 4),
    0x2C: ("FT4", True,  False, 4),
    0x30: ("G3",  False, True,  3),
    0x34: ("GT3", True,  True,  3),
    0x38: ("G4",  False, True,  4),
    0x3C: ("GT4", True,  True,  4),
}


def u16(buf, off):
    return struct.unpack_from("<H", buf, off)[0]


def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def s16(buf, off):
    return struct.unpack_from("<h", buf, off)[0]


class Tmd:
    def __init__(self, data):
        self.data = data
        self.id = u32(data, 0)
        if self.id != 0x41:
            raise ValueError("not a TMD file (id=0x%08x)" % self.id)
        self.flag = u32(data, 4)
        self.nobj = u32(data, 8)

    def object(self, i):
        """Return (obj_hdr_offset, vert_top, n_vert, normal_top, n_normal,
        prim_top, n_primitive, scale)."""
        hdr = 12 + i * 28
        (vert_top, n_vert, normal_top, n_normal,
         prim_top, n_prim, scale) = struct.unpack_from("<7I", self.data, hdr)
        return (hdr, vert_top, n_vert, normal_top, n_normal,
                prim_top, n_prim, scale)

    def vertex(self, obj_hdr, vert_top, i):
        """Vertex i of the object — absolute file offset."""
        off = obj_hdr + vert_top + i * 8
        return (s16(self.data, off), s16(self.data, off + 2),
                s16(self.data, off + 4))

    def normal(self, obj_hdr, normal_top, i):
        """Normal i of the object — absolute file offset."""
        off = obj_hdr + normal_top + i * 8
        return (s16(self.data, off), s16(self.data, off + 2),
                s16(self.data, off + 4))

    def primitives(self, obj_hdr, prim_top, n_prim):
        """Yield (mode, words[]) for each primitive packet."""
        off = obj_hdr + prim_top
        for _ in range(n_prim):
            hdr = u32(self.data, off)
            olen = hdr & 0xFF
            mode = (hdr >> 24) & 0xFF
            if olen < 1:
                break
            words = [u32(self.data, off + 4 * w) for w in range(1, olen)]
            yield (mode, words, hdr & 0xFF, (hdr >> 8) & 0xFF)
            off += 4 * olen


def face_data(mode, w):
    """Extract (vertex_indices, uvs, colours) from a packet's data words.

    Packet layouts (u32 word index 0 = the header word; here w[0] is
    the first word AFTER the header), from GsSortObject4:
      F3 : w0=colour,      w1=v0|v1<<16,          w2=v2
      FT3: w0=u0,v0,cba,   w1=u1,v1,tsb,  w2=u2,v2, w3=colour,
           w4=v0|v1<<16,   w5=v2
      F4 : w0=colour,      w1=v0|v1<<16,          w2=v2|v3<<16
      FT4: w0..w3=uvs,     w4=colour,     w5=v0|v1<<16, w6=v2|v3<<16
      G3 : w0..w2=colours, w3=v0|v1<<16,          w4=v2
      GT3: w0..w2=uvs,     w3..w5=colours, w6=v0|v1<<16,  w7=v2
      G4 : w0..w3=colours, w4=v0|v1<<16,          w5=v2|v3<<16
      GT4: w0..w3=uvs,     w4..w7=colours, w8=v0|v1<<16, w9=v2|v3<<16
    """
    name, textured, gouraud, corners = MODES[mode]

    if mode == 0x20:                                    # F3
        vs = [w[1] & 0xFFFF, w[1] >> 16, w[2] & 0xFFFF]
        uvs = [(0, 0)] * 3
        cols = [w[0] & 0xFFFFFF] * 3
    elif mode == 0x24:                                  # FT3
        vs = [w[4] & 0xFFFF, w[4] >> 16, w[5] & 0xFFFF]
        uvs = [(w[0] & 0xFF, (w[0] >> 8) & 0xFF),
               (w[1] & 0xFF, (w[1] >> 8) & 0xFF),
               (w[2] & 0xFF, (w[2] >> 8) & 0xFF)]
        cols = [w[3] & 0xFFFFFF] * 3
    elif mode == 0x28:                                  # F4
        vs = [w[1] & 0xFFFF, w[1] >> 16,
              w[2] & 0xFFFF, w[2] >> 16]
        uvs = [(0, 0)] * 4
        cols = [w[0] & 0xFFFFFF] * 4
    elif mode == 0x2C:                                  # FT4
        vs = [w[5] & 0xFFFF, w[5] >> 16,
              w[6] & 0xFFFF, w[6] >> 16]
        uvs = [(w[0] & 0xFF, (w[0] >> 8) & 0xFF),
               (w[1] & 0xFF, (w[1] >> 8) & 0xFF),
               (w[2] & 0xFF, (w[2] >> 8) & 0xFF),
               (w[3] & 0xFF, (w[3] >> 8) & 0xFF)]
        cols = [w[4] & 0xFFFFFF] * 4
    elif mode == 0x30:                                  # G3
        vs = [w[3] & 0xFFFF, w[3] >> 16, w[4] & 0xFFFF]
        uvs = [(0, 0)] * 3
        cols = [w[0] & 0xFFFFFF, w[1] & 0xFFFFFF, w[2] & 0xFFFFFF]
    elif mode == 0x34:                                  # GT3
        vs = [w[6] & 0xFFFF, w[6] >> 16, w[7] & 0xFFFF]
        uvs = [(w[0] & 0xFF, (w[0] >> 8) & 0xFF),
               (w[1] & 0xFF, (w[1] >> 8) & 0xFF),
               (w[2] & 0xFF, (w[2] >> 8) & 0xFF)]
        cols = [w[3] & 0xFFFFFF, w[4] & 0xFFFFFF, w[5] & 0xFFFFFF]
    elif mode == 0x38:                                  # G4
        vs = [w[4] & 0xFFFF, w[4] >> 16,
              w[5] & 0xFFFF, w[5] >> 16]
        uvs = [(0, 0)] * 4
        cols = [w[0] & 0xFFFFFF, w[1] & 0xFFFFFF,
                w[2] & 0xFFFFFF, w[3] & 0xFFFFFF]
    elif mode == 0x3C:                                  # GT4
        vs = [w[8] & 0xFFFF, w[8] >> 16,
              w[9] & 0xFFFF, w[9] >> 16]
        uvs = [(w[0] & 0xFF, (w[0] >> 8) & 0xFF),
               (w[1] & 0xFF, (w[1] >> 8) & 0xFF),
               (w[2] & 0xFF, (w[2] >> 8) & 0xFF),
               (w[3] & 0xFF, (w[3] >> 8) & 0xFF)]
        cols = [w[4] & 0xFFFFFF, w[5] & 0xFFFFFF,
                w[6] & 0xFFFFFF, w[7] & 0xFFFFFF]
    else:
        raise ValueError("unsupported mode 0x%02x" % mode)

    return vs, uvs, cols, textured, gouraud, corners


def convert(path):
    with open(path, "rb") as f:
        data = f.read()
    tmd = Tmd(data)

    base = path.rsplit(".", 1)[0]
    obj_path = base + ".obj"
    mtl_path = base + ".mtl"
    mat_name = base.rsplit("/", 1)[-1]

    lines = ["# Converted from %s by tmd2obj.py (PSXLinux open asset tools)"
             % path.rsplit("/", 1)[-1],
             "mtllib %s.mtl" % mat_name,
             "usemtl %s" % mat_name, ""]

    vt_count = 0
    prim_total = 0
    skipped = {}

    for obj_i in range(tmd.nobj):
        (hdr, vert_top, n_vert, normal_top, n_normal,
         prim_top, n_prim, _scale) = tmd.object(obj_i)
        lines.append("o object_%d" % obj_i)

        # vertices (OBJ is 1-based)
        for i in range(n_vert):
            x, y, z = tmd.vertex(hdr, vert_top, i)
            lines.append("v %d %d %d" % (x, y, z))

        # normals — carried so obj2tmd.py can rebuild a bit-identical
        # TMD.  The game's F3 path does not index them, but they are
        # part of the file.
        for i in range(n_normal):
            x, y, z = tmd.normal(hdr, normal_top, i)
            lines.append("vn %d %d %d" % (x, y, z))

        for mode, w, olen, ilen in tmd.primitives(hdr, prim_top, n_prim):
            if mode not in MODES:
                skipped[mode] = skipped.get(mode, 0) + 1
                continue
            vs, uvs, cols, textured, gouraud, corners = face_data(mode, w)
            prim_total += 1

            # Preserve the PSX face colour as an OBJ comment so
            # obj2tmd.py can rebuild an identical packet.  Flat
            # primitives carry one colour; gouraud carries one per
            # corner.
            def col_word(c):
                r, g, b = c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF
                return "%d %d %d" % (r, g, b)

            if gouraud:
                lines.append("# gouraud " +
                             " ".join(col_word(c) for c in cols))
            else:
                lines.append("# colour " + col_word(cols[0]))

            # The F3 packets carry a pad halfword in the last vertex
            # word; F4 carries a full pad word.  Preserve it so
            # obj2tmd can rebuild a bit-identical file.
            if mode == 0x20:
                lines.append("# pad %d" % ((w[2] >> 16) & 0xFFFF))
            elif mode == 0x28:
                lines.append("# pad %d" % (w[3] & 0xFFFFFFFF))

            # OBJ y-up vs PSX y-down: keep native coordinates, note it.
            refs = []
            for c in range(corners):
                vi = vs[c] + 1                 # 0-based -> 1-based
                if textured:
                    vt_count += 1
                    u, v = uvs[c]
                    # PSX texel UV; OBJ normalises to 0..1 within the
                    # texture — emit normalised assuming a 256x256 page.
                    lines.append("vt %.6f %.6f" % (u / 256.0, v / 256.0))
                    refs.append("%d/%d" % (vi, vt_count))
                else:
                    refs.append(str(vi))
            lines.append("f " + " ".join(refs))

    with open(obj_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    with open(mtl_path, "w") as f:
        f.write("newmtl %s\nKd 0.8 0.8 0.8\nKa 0.1 0.1 0.1\n"
                "illum 1\n" % mat_name)

    name = path.rsplit("/", 1)[-1]
    print("%-14s -> %-14s %d object(s), %d primitive(s)"
          % (name, obj_path.rsplit("/", 1)[-1], tmd.nobj, prim_total))
    if skipped:
        print("  skipped unsupported modes:",
              {hex(k): v for k, v in skipped.items()})


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
