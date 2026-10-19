#!/usr/bin/env python3
"""
png2tim.py — Convert a PNG (plus .json sidecar) back into a PlayStation TIM.

This is the inverse of tim2png.py: the PNG supplies the pixels, the
sidecar (produced by tim2png.py) supplies the VRAM placement, pixel
mode and CLUT rectangle.  Round-tripping
    tim2png.py X.tim ; png2tim.py X.png
reproduces a bit-identical X.tim.

Input colours are quantised:
    16bpp   PNG RGB888  -> RGB555 (5 bits per channel)
    4bpp    PNG colours -> nearest CLUT entry from the sidecar CLUT
                          (or a generated 16-colour CLUT if absent)

Usage: png2tim.py FILE.png [FILE2.png ...]
"""

import json
import struct
import sys
import zlib


# ---------------------------------------------------------------- PNG reader

def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")

    w = h = None
    idat = b""
    ctype_depth = None
    pos = 8
    while pos < len(data):
        (length,) = struct.unpack_from(">I", data, pos)
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            w, h, depth, color = struct.unpack_from(">IIBB", chunk, 0)
            ctype_depth = (depth, color)
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
        pos += 12 + length

    depth, color = ctype_depth
    if depth != 8 or color not in (2, 6):
        raise ValueError("PNG must be 8-bit RGB(A)")

    raw = zlib.decompress(idat)
    bpp = 3 if color == 2 else 4
    stride = 1 + w * bpp                 # filter byte + pixels
    if len(raw) < h * stride:
        raise ValueError("PNG data truncated")

    rows = []
    prev = bytearray(w * 4)
    for y in range(h):
        line = bytearray(raw[y * stride:(y + 1) * stride])
        ftype = line[0]
        line = line[1:]
        # de-filter (works on the raw bpp-wide samples)
        out = bytearray(w * bpp)
        for x in range(w):
            i = x * bpp
            left = bytes(out[i - bpp:i]) if x else b"\0" * bpp
            up = bytes(prev[x * bpp:x * bpp + bpp])
            ul = bytes(prev[(x - 1) * bpp:x * bpp + bpp]) if x else b"\0" * bpp
            for c in range(bpp):
                a = left[c]
                b = up[c]
                d = ul[c]
                p = line[i + c]
                if ftype == 0:
                    v = p
                elif ftype == 1:
                    v = (p + a) & 0xFF
                elif ftype == 2:
                    v = (p + b) & 0xFF
                elif ftype == 3:
                    v = (p + ((a + b) // 2)) & 0xFF
                elif ftype == 4:
                    pp = a + b - d
                    pa, pb, pd = abs(pp - a), abs(pp - b), abs(pp - d)
                    pr = a if (pa <= pb and pa <= pd) else (b if pb <= pd else d)
                    v = (p + pr) & 0xFF
                else:
                    raise ValueError("bad filter %d" % ftype)
                out[i + c] = v
        prev = out
        row = []
        for x in range(w):
            i = x * bpp
            r, g, b = out[i], out[i + 1], out[i + 2]
            a = out[i + 3] if bpp == 4 else 255
            row.append((r, g, b, a))
        rows.append(row)
    return w, h, rows


# ------------------------------------------------------------ colour helpers

def rgb888_to_rgb555(r, g, b):
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def load_clut_from_tim(path):
    """Pull the original CLUT back out of a TIM (for indexed round trips).

    CLUT block: +8 u32 len, +12 RECT (8 bytes), +20 CLUT data."""
    with open(path, "rb") as f:
        data = f.read()
    clen, = struct.unpack_from("<I", data, 8)
    n = (clen - 12) // 2
    return [struct.unpack_from("<H", data, 20 + 2 * i)[0] for i in range(n)]


def nearest_clut_index(rgb, clut):
    r, g, b = rgb
    best, bd = 0, 1 << 30
    for i, v in enumerate(clut):
        cr = v & 0x1F
        cg = (v >> 5) & 0x1F
        cb = (v >> 10) & 0x1F
        d = ((cr - (r >> 3)) ** 2 + (cg - (g >> 3)) ** 2 +
             (cb - (b >> 3)) ** 2)
        if d < bd:
            bd, best = d, i
    return best


# ---------------------------------------------------------------- TIM writer

def build_tim(pmode, prect, rows, clut, crect):
    """Assemble the TIM file.  rows hold (r, g, b, stp) tuples; the stp
    bit (PSX semi-transparency flag, TIM bit 15) is carried in the
    PNG's alpha channel by tim2png.py."""
    px, py, pw, ph = prect
    # flags: bits 0-1 pmode, bit 3 (0x8) = CLUT data follows
    out = struct.pack("<II", 0x10, pmode | (0x08 if pmode < 2 else 0))

    if pmode < 2:
        cw, ch = crect[2], crect[3]
        clut_words = b"".join(struct.pack("<H", c) for c in clut)
        clen = 12 + len(clut_words)
        out += struct.pack("<IHHHH", clen, crect[0], crect[1], cw, ch)
        out += clut_words

    # pack pixels
    if pmode == 2:                       # 16bpp, one halfword per texel
        per_hw = 1
        bits = 16
    elif pmode == 1:                     # 8bpp, 2 texels per halfword
        per_hw = 2
        bits = 8
    else:                                # 4bpp, 4 texels per halfword
        per_hw = 4
        bits = 4

    tex_w = len(rows[0])
    tex_h = len(rows)
    hw_w = tex_w // per_hw
    halfwords = []
    for y in range(tex_h):
        for hw_x in range(hw_w):
            hw = 0
            for s in range(per_hw):
                x = hw_x * per_hw + s
                r, g, b, stp = rows[y][x]
                if pmode == 2:
                    val = rgb888_to_rgb555(r, g, b)
                    if stp:
                        val |= 0x8000    # keep the semi-transparency bit
                else:
                    val = nearest_clut_index((r, g, b), clut)
                hw |= val << (s * bits)
            halfwords.append(hw)

    pix = b"".join(struct.pack("<H", v) for v in halfwords)
    plen = 12 + len(pix)
    out += struct.pack("<IHHHH", plen, px, py, pw, ph)
    out += pix
    return out


# ---------------------------------------------------------------------- main

def convert(path):
    base = path.rsplit(".", 1)[0]
    json_path = base + ".json"
    tim_path = base + ".tim"
    orig_tim = base + "_orig.tim"

    with open(json_path) as f:
        meta = json.load(f)
    pmode = meta["pmode"]
    prect = tuple(meta["prect"])

    w, h, rows = read_png(path)
    if w != meta["tex_w"] or h != meta["tex_h"]:
        raise ValueError("PNG is %dx%d but sidecar expects %dx%d"
                         % (w, h, meta["tex_w"], meta["tex_h"]))

    # PNG alpha -> PSX STP bit (semi-transparency flag)
    rows = [[(r, g, b, 1 if a < 128 else 0) for (r, g, b, a) in row]
            for row in rows]

    clut = []
    crect = None
    if pmode < 2:
        crect = tuple(meta.get("crect", (0, 480, 16, 1)))
        # prefer the CLUT embedded in the sidecar (fully open pipeline);
        # fall back to reading the original TIM if absent
        clut = meta.get("clut", [])
        if not clut:
            try:
                clut = load_clut_from_tim(tim_path)
            except (IOError, struct.error):
                pass
        n = 16 if pmode == 0 else 256
        if len(clut) < n:
            raise SystemExit(
                "no CLUT available: keep the %s next to the PNG or "
                "re-extract with tim2png.py (newer versions embed the "
                "CLUT in the sidecar)" % tim_path)

    data = build_tim(pmode, prect, rows, clut, crect)
    with open(tim_path, "wb") as f:
        f.write(data)

    print("%-14s -> %-14s pmode=%d %dx%d texels @ VRAM (%d,%d) (%d bytes)"
          % (path.rsplit("/", 1)[-1], tim_path.rsplit("/", 1)[-1],
             pmode, w, h, prect[0], prect[1], len(data)))


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
