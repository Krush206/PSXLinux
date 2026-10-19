#!/usr/bin/env python3
"""
tim2png.py — Convert a PlayStation TIM image to PNG (open format).

TIM layout (little-endian):
    +00 u32  id     = 0x00000010
    +04 u32  flags  bits 0-1 = pmode (0=4bpp, 1=8bpp, 2=16bpp, 3=24bpp)
                    bit 3   = CLUT present (implied for pmode < 2)
    [pmode < 2 only]
    +08 u32  clen   CLUT block size, incl. its 12-byte header
    +0c RECT  crect (x, y, w, h — u16 each)
    +14 ..    CLUT data: w*h u16 entries, RGB555
    [always]
    u32  plen  pixel block size, incl. its 12-byte header
    RECT prect (x, y, w, h — w,h in 16-bit VRAM halfwords)
    ..   pixel data: (plen-12) bytes

Texel dimensions: 4bpp -> w*4 pixels, 8bpp -> w*2, 16bpp -> w.
Writes <name>.png plus a <name>.json sidecar with the VRAM placement
and pixel mode, so png2tim.py can rebuild a bit-identical TIM.

Usage: tim2png.py FILE.tim [FILE2.tim ...]
"""

import json
import struct
import sys
import zlib


# ---------------------------------------------------------------- PNG writer

def png_chunk(ctype, data):
    return (struct.pack(">I", len(data)) + ctype + data +
            struct.pack(">I", zlib.crc32(ctype + data) & 0xFFFFFFFF))


def write_png(path, width, height, rgba_rows):
    """rgba_rows: list of rows, each a list of (r, g, b, a) tuples.

    The alpha channel carries the PSX STP (semi-transparency) bit:
    a = 0 where STP is set, 255 otherwise — so pixels that were
    semi-transparent on the PSX show up as transparent in GIMP."""
    out = b"\x89PNG\r\n\x1a\n"
    out += png_chunk(b"IHDR", struct.pack(">IIBBBBB",
                                           width, height, 8, 6, 0, 0, 0))
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px)
                   for row in rgba_rows)
    out += png_chunk(b"IDAT", zlib.compress(raw, 9))
    out += png_chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


# ------------------------------------------------------------ RGB555 helpers

def rgb555_to_rgb888(v):
    r = v & 0x1F
    g = (v >> 5) & 0x1F
    b = (v >> 10) & 0x1F
    return (r * 255 // 31, g * 255 // 31, b * 255 // 31)


# ------------------------------------------------------------------ TIM read

class Tim:
    def __init__(self, data):
        if len(data) < 12:
            raise ValueError("file too short")
        (self.id, self.flags) = struct.unpack_from("<II", data, 0)
        if self.id != 0x10:
            raise ValueError("not a TIM file (id=0x%08x)" % self.id)
        self.pmode = self.flags & 3
        self.has_clut = self.pmode < 2

        off = 8
        self.crect = None
        self.clut = []
        if self.has_clut:
            clen, = struct.unpack_from("<I", data, off)
            cx, cy, cw, ch = struct.unpack_from("<HHHH", data, off + 4)
            self.crect = (cx, cy, cw, ch)
            n = cw * ch
            self.clut = [struct.unpack_from("<H", data, off + 12 + 2 * i)[0]
                         for i in range(n)]
            if len(self.clut) != n:
                raise ValueError("CLUT truncated")
            off += clen

        plen, = struct.unpack_from("<I", data, off)
        px, py, pw, ph = struct.unpack_from("<HHHH", data, off + 4)
        self.prect = (px, py, pw, ph)
        self.pixels_raw = data[off + 12: off + plen]
        if len(self.pixels_raw) < pw * ph * 2:
            raise ValueError("pixel data truncated (%d < %d)"
                             % (len(self.pixels_raw), pw * ph * 2))

        # texel dimensions
        if self.pmode == 0:
            self.tex_w = pw * 4
        elif self.pmode == 1:
            self.tex_w = pw * 2
        elif self.pmode == 2:
            self.tex_w = pw
        else:
            raise ValueError("24bpp TIMs not supported")
        self.tex_h = ph

    def rows(self):
        """Decode to a list of rows of (r, g, b, a) tuples.  Alpha
        carries the STP bit (0 = semi-transparent, 255 = opaque)."""
        out = []
        for y in range(self.tex_h):
            row = []
            for x in range(self.tex_w):
                if self.pmode == 2:            # direct RGB555
                    v = struct.unpack_from("<H", self.pixels_raw,
                                           (y * self.tex_w + x) * 2)[0]
                    stp = (v >> 15) & 1
                    row.append(rgb555_to_rgb888(v) + (0 if stp else 255,))
                else:                          # CLUT indexed
                    if self.pmode == 0:        # 4bpp: 4 texels per halfword
                        per_hw = 4
                    else:                       # 8bpp: 2 texels per halfword
                        per_hw = 2
                    bits = 16 // per_hw        # 4 bits (4bpp) / 8 bits (8bpp)
                    idx_in_line = (y * self.tex_w + x)
                    hw_index = idx_in_line // per_hw
                    halfword = struct.unpack_from("<H", self.pixels_raw,
                                                  hw_index * 2)[0]
                    shift = (idx_in_line % per_hw) * bits
                    ci = (halfword >> shift) & ((1 << bits) - 1)
                    cw = self.clut[ci]
                    stp = (cw >> 15) & 1
                    row.append(rgb555_to_rgb888(cw) + (0 if stp else 255,))
            out.append(row)
        return out

    def metadata(self):
        m = {"pmode": self.pmode,
             "prect": list(self.prect),
             "tex_w": self.tex_w,
             "tex_h": self.tex_h}
        if self.has_clut:
            m["crect"] = list(self.crect)
        return m


# ---------------------------------------------------------------------- main

def convert(path):
    with open(path, "rb") as f:
        data = f.read()
    tim = Tim(data)

    base = path.rsplit(".", 1)[0]
    png_path = base + ".png"
    json_path = base + ".json"

    write_png(png_path, tim.tex_w, tim.tex_h, tim.rows())
    with open(json_path, "w") as f:
        json.dump(tim.metadata(), f, indent=2)

    name = path.rsplit("/", 1)[-1]
    mode = {0: "4bpp", 1: "8bpp", 2: "16bpp", 3: "24bpp"}[tim.pmode]
    print("%-14s -> %-14s %s %dx%d texels @ VRAM (%d,%d)%s"
          % (name, png_path.rsplit("/", 1)[-1], mode,
             tim.tex_w, tim.tex_h, tim.prect[0], tim.prect[1],
             ("  CLUT %dx%d @ (%d,%d)" % (tim.crect[2], tim.crect[3],
                                          tim.crect[0], tim.crect[1]))
             if tim.has_clut else ""))


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
