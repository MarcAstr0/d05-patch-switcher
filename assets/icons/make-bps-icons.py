#!/usr/bin/env python3
"""Writes the Boutique Patch Switcher icon sources (docs/design/DESIGN-2.md section 9).

  boutique-patch-switcher.svg          256 x 256 source: charcoal tile, a seven-segment window
                                       reading "1-1", three panel buttons in the three skins'
                                       face colours (D-05 grey with a slit LED, JUPITER-8 white
                                       and JUNO-106 teal with round LEDs), the middle LED lit
  boutique-patch-switcher-{16,24,32}.png  hand placed on whole pixels (Pillow)

Drawn for this project from the design tokens; no Roland logo, logotype or panel artwork.
The 48, 64, 128, 256, 512 and 1024 px PNGs, the .ico and the .icns are rendered from the SVG
with the commands in README.md.

Usage: python3 -I make-bps-icons.py [OUTDIR]   (default: this file's directory)
"""
import os
import sys

from PIL import Image

# Token values (tokens.json): d05 tile, LED readout of the JU-06A skins, faces of three skins.
C = dict(panel="#202326", band="#26292B", bandEdge="#3A3E41", bezelRim="#2E3134", bezel="#050606",
         ledBack="#0A0302", segOn="#FF3A2A", segGhost="#1C0806",
         d05Face="#3F4244", d05LedOff="#2A1A1A",
         jpFace="#CFCBC2", jpRim="#141414", jpLedOn="#FF3B30",
         juFace="#80A4A0", juRim="#2A2B2D", juLedOff="#4A2B28")

# Seven-segment geometry, DESIGN-2.md 4.2 (units of P; digit 8 x 15, segment 2, gap 0.25, slant 0.1).
SW, SH, T, E, SL = 8.0, 15.0, 2.0, 0.25, 0.1
H = T / 2


def hseg(yc):
    x0, x1 = H + E, SW - H - E
    return [(x0, yc), (x0 + H, yc - H), (x1 - H, yc - H), (x1, yc), (x1 - H, yc + H), (x0 + H, yc + H)]


def vseg(xc, y0, y1):
    return [(xc, y0), (xc + H, y0 + H), (xc + H, y1 - H), (xc, y1), (xc - H, y1 - H), (xc - H, y0 + H)]


SEGS = {"a": hseg(H), "g": hseg(SH / 2), "d": hseg(SH - H),
        "f": vseg(H, H + E, SH / 2 - E), "b": vseg(SW - H, H + E, SH / 2 - E),
        "e": vseg(H, SH / 2 + E, SH - H - E), "c": vseg(SW - H, SH / 2 + E, SH - H - E)}
LIT = ["bc", "g", "bc"]  # "1-1"


def svg():
    P = 3.4
    width = 2 * 11 * P + (SW + SL * SH) * P
    ox, oy = 44 + (168 - width) / 2, 60 + (72 - SH * P) / 2
    ghost, lit = [], []
    for k in range(3):
        for name, pts in SEGS.items():
            poly = " ".join(f"{ox + 11 * P * k + (x + SL * (SH - y)) * P:.2f},{oy + y * P:.2f}" for x, y in pts)
            (lit if name in LIT[k] else ghost).append(poly)
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           "<!-- Boutique Patch Switcher application icon. Geometry: docs/design/DESIGN-2.md section 9.",
           "     Written by make-bps-icons.py; drawn for this project, no trademarked artwork. -->",
           '<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">',
           '  <defs><clipPath id="tile"><rect x="8" y="8" width="240" height="240" rx="44" ry="44"/></clipPath></defs>',
           f'  <rect x="8" y="8" width="240" height="240" rx="44" ry="44" fill="{C["panel"]}"/>',
           '  <g clip-path="url(#tile)">',
           f'    <rect x="8" y="168" width="240" height="80" fill="{C["band"]}"/>',
           f'    <rect x="8" y="168" width="240" height="2" fill="{C["bandEdge"]}"/>',
           "  </g>",
           f'  <rect x="32" y="48" width="192" height="96" fill="{C["bezelRim"]}"/>',
           f'  <rect x="34" y="50" width="188" height="92" fill="{C["bezel"]}"/>',
           f'  <rect x="44" y="60" width="168" height="72" fill="{C["ledBack"]}"/>']
    out += [f'  <polygon fill="{C["segGhost"]}" points="{p}"/>' for p in ghost]
    out += [f'  <polygon fill="{C["segOn"]}" points="{p}"/>' for p in lit]
    out += [f'  <rect x="40" y="192" width="48" height="24" rx="3" ry="3" fill="{C["d05Face"]}"/>',
            f'  <rect x="56" y="201" width="16" height="6" fill="{C["d05LedOff"]}"/>',
            f'  <rect x="104" y="192" width="48" height="24" rx="3" ry="3" fill="{C["jpFace"]}"/>',
            f'  <circle cx="128" cy="204" r="7.5" fill="{C["jpRim"]}"/>',
            f'  <circle cx="128" cy="204" r="6" fill="{C["jpLedOn"]}"/>',
            f'  <rect x="168" y="192" width="48" height="24" rx="3" ry="3" fill="{C["juFace"]}"/>',
            f'  <circle cx="192" cy="204" r="7.5" fill="{C["juRim"]}"/>',
            f'  <circle cx="192" cy="204" r="6" fill="{C["juLedOff"]}"/>',
            "</svg>", ""]
    return "\n".join(out)


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


class Canvas:
    def __init__(self, n):
        self.n = n
        self.px = [[rgb(C["panel"])] * n for _ in range(n)]

    def rect(self, x, y, w, h, tok):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.px[yy][xx] = rgb(C[tok])

    def save(self, path, radius, ss=16):
        n = self.n
        im = Image.new("RGBA", (n, n))
        for y in range(n):
            for x in range(n):
                hit = 0
                for sy in range(ss):
                    for sx in range(ss):
                        px, py = x + (sx + .5) / ss, y + (sy + .5) / ss
                        cx, cy = min(max(px, radius), n - radius), min(max(py, radius), n - radius)
                        hit += (px - cx) ** 2 + (py - cy) ** 2 <= radius * radius
                im.putpixel((x, y), self.px[y][x] + (round(255 * hit / ss / ss),))
        im.save(path, optimize=True)


def small(n):
    """Whole-pixel layouts. Digits: "1" as a vertical bar, "-" as a horizontal bar."""
    c = Canvas(n)
    if n == 16:
        c.rect(1, 2, 14, 9, "bezelRim"); c.rect(2, 3, 12, 7, "ledBack")
        c.rect(4, 4, 1, 5, "segOn"); c.rect(6, 6, 4, 1, "segOn"); c.rect(11, 4, 1, 5, "segOn")
        c.rect(0, 12, 16, 1, "bandEdge"); c.rect(0, 13, 16, 3, "band")
        faces, fy, fw, fh, ly, radius = (1, 6, 11), 13, 4, 2, 13, 3
    elif n == 24:
        c.rect(2, 3, 20, 11, "bezelRim"); c.rect(3, 4, 18, 9, "ledBack")
        c.rect(6, 5, 1, 7, "segOn"); c.rect(9, 8, 6, 1, "segOn"); c.rect(17, 5, 1, 7, "segOn")
        c.rect(0, 16, 24, 1, "bandEdge"); c.rect(0, 17, 24, 7, "band")
        faces, fy, fw, fh, ly, radius = (4, 10, 16), 18, 4, 3, 19, 4
    else:
        c.rect(4, 5, 24, 13, "bezelRim"); c.rect(5, 6, 22, 11, "ledBack")
        c.rect(9, 7, 2, 9, "segOn"); c.rect(13, 11, 6, 1, "segOn"); c.rect(21, 7, 2, 9, "segOn")
        c.rect(0, 21, 32, 1, "bandEdge"); c.rect(0, 22, 32, 10, "band")
        faces, fy, fw, fh, ly, radius = (5, 13, 21), 24, 6, 3, 25, 6
    for x, face, led in zip(faces, ("d05Face", "jpFace", "juFace"), ("d05LedOff", "jpLedOn", "juLedOff")):
        c.rect(x, fy, fw, fh, face)
        c.rect(x + (fw - 2) // 2, ly, 2, 1, led)
    return c, radius


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(out, "boutique-patch-switcher.svg"), "w", encoding="utf-8", newline="\n") as f:
        f.write(svg())
    for n in (16, 24, 32):
        c, radius = small(n)
        c.save(os.path.join(out, "boutique-patch-switcher-%d.png" % n), radius)


if __name__ == "__main__":
    main()
