#!/usr/bin/env python3
"""Builds the T2.4 mockups from docs/design/DESIGN.md.

Usage (from anywhere):  python3 docs/design/mockup/build.py

Writes SVG sources into docs/design/mockup/ and renders them with Inkscape at 1x and 2x:

  docs/design/mockup-main.png            960 x 560   (and @2x)  search state
  docs/design/mockup-settings.png        520 x 436   (and @2x)  Settings dialog
  docs/design/mockup/main-disconnected.png           (and @2x)  second main frame
  docs/design/mockup/display-*.png                   (and @2x)  G2 comparison crops

Every number comes from DESIGN.md (section numbers in the comments). The display dots
are generated from the Appendix A glyph table, parsed out of DESIGN.md itself, so the
LCD is exactly what the builder will draw. Text is set by Inkscape with a private
fontconfig that sees only assets/fonts (the committed Inter 4.1 subset), and text
advances used for layout are measured from the same files with Pillow + libraqm
(kerning on), the way Qt's QFontMetrics::horizontalAdvance would measure them.

Needs: python3, Pillow built with raqm, inkscape 1.x. Does not launch any browser.
"""

import os
import re
import struct
import subprocess
import sys
import tempfile

from PIL import ImageFont, features

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DESIGN = os.path.join(ROOT, "docs", "design", "DESIGN.md")
PRESETS = os.path.join(ROOT, "data", "d05-presets.tsv")
FONTS = os.path.join(ROOT, "assets", "fonts")
FONT_FILE = {400: os.path.join(FONTS, "Inter-Regular.ttf"), 500: os.path.join(FONTS, "Inter-Medium.ttf")}

# ---------------------------------------------------------------- tokens (DESIGN 2)
T = dict(
    panel="#202326", band="#26292B", bandEdge="#3A3E41", well="#141617",
    wellEdgeDark="#0B0C0D", wellEdgeLight="#34383B", fieldBack="#0E0F10",
    fieldBorder="#3A3D40", fieldBorderHover="#4C5053", rowHover="#1B1D1F",
    selection="#33373B", selectionBarIdle="#6B6E70", matchBack="#2E4366",
    bezel="#050606", bezelRim="#2E3134", bezelRimTop="#45494D", lcdBack="#071104",
    lcdOn="#98CF1C", lcdGhost="#0F1D09",
    labelPrimary="#E2E0D4", labelSecondary="#A3A293", labelDim="#6E6D64",
    labelDisabled="#5F5F58", labelAccent="#86A6D4", ruleAccent="#6E8EBA",
    ruleSecondary="#8C8B7E", labelRed="#E8565C", arrow="#BDB9A0", starOff="#5C5E5A",
    tagBorder="#4A4C48",
    buttonSlot="#121415", buttonFace="#3F4244", buttonFaceHover="#4A4D50",
    buttonPressed="#2F3133", buttonHighlight="#56595C", buttonFaceLit="#4B3B3C",
    buttonDisabled="#2C2E30", ledOn="#F0383F", ledCore="#FF9C8C", ledOff="#2A1A1A",
    focusRing="#9DBBE8",
)

# ---------------------------------------------------------------- type roles (DESIGN 3.2)
ROLES = {  # weight, px size, letter spacing
    "panelLabel": (500, 10, 0.5), "listName": (400, 14, 0), "listAddress": (500, 13, 0),
    "listMeta": (500, 10, 0.5), "field": (400, 14, 0), "statusLabel": (500, 10, 0.5),
    "statusValue": (400, 11, 0), "body": (400, 13, 0), "note": (400, 12, 0),
    "emptyTitle": (500, 11, 1.0), "keycap": (500, 10, 0.5), "wordmark": (500, 15, 3.0),
    "wordmarkSub": (500, 10, 1.0),
}

assert features.check("raqm"), "Pillow needs libraqm for kerned measurements"
_pil = {}


def pil_font(weight, size):
    key = (weight, size)
    if key not in _pil:
        _pil[key] = ImageFont.truetype(FONT_FILE[weight], size, layout_engine=ImageFont.Layout.RAQM)
    return _pil[key]


def advance(s, role):
    """Qt-style horizontalAdvance: kerned advance plus letter spacing after every char."""
    w, size, ls = ROLES[role]
    if not s:
        return 0.0
    return pil_font(w, size).getlength(s) + ls * len(s)


def _hhea(path):
    data = open(path, "rb").read()
    num = struct.unpack(">H", data[4:6])[0]
    tables = {}
    for i in range(num):
        tag, _, off, ln = struct.unpack(">4sIII", data[12 + 16 * i: 28 + 16 * i])
        tables[tag.decode()] = off
    upm = struct.unpack(">H", data[tables["head"] + 18: tables["head"] + 20])[0]
    asc, desc = struct.unpack(">hh", data[tables["hhea"] + 4: tables["hhea"] + 8])
    return upm, asc, desc


METRICS = {w: _hhea(p) for w, p in FONT_FILE.items()}


def ascent_descent(role):
    """QFontMetrics ascent() and descent() (rounded), from the font's hhea table."""
    w, size, _ = ROLES[role]
    upm, asc, desc = METRICS[w]
    return round(asc * size / upm), round(-desc * size / upm)


def vcentre_baseline(top, h, role):
    """Baseline of text vertically centred (Qt::AlignVCenter) in a box of height h."""
    a, d = ascent_descent(role)
    return top + (h - (a + d)) // 2 + a


# ---------------------------------------------------------------- data
def glyph_table():
    text = open(DESIGN, encoding="utf-8").read()
    appendix = text[text.index("## Appendix A"):]
    glyphs = {}
    for m in re.finditer(r"^([0-9A-F]{2})  ((?:[0-9A-F]{2} ){6}[0-9A-F]{2})  ", appendix, re.M):
        glyphs[int(m.group(1), 16)] = [int(b, 16) for b in m.group(2).split()]
    assert len(glyphs) == 95 and min(glyphs) == 0x20 and max(glyphs) == 0x7E, len(glyphs)
    return glyphs


GLYPHS = glyph_table()


def presets():
    rows = []
    with open(PRESETS, encoding="utf-8") as f:
        next(f)
        for line in f:
            g, b, n, name, mode = line.rstrip("\n").split("\t")
            rows.append(dict(addr=f"{g}-{b}{n}", name=name, mode=mode, user=False))
    return rows


def catalogue():
    """All 896 addresses in catalogue order (SPEC: U1-11 .. U8-88, P1-11 .. P6-88)."""
    rows = []
    for g in range(1, 9):
        for b in range(1, 9):
            for n in range(1, 9):
                rows.append(dict(addr=f"U{g}-{b}{n}", name="", mode="", user=True))
    return rows + presets()


GROUP_LONG = {f"U{i}": f"USER {i}" for i in range(1, 9)}
GROUP_LONG.update({"P1": "PRESET 1: ORIGINAL D-50", "P2": "PRESET 2: NEW FOR D-05",
                   "P3": "PRESET 3: PN-D50-01", "P4": "PRESET 4: PN-D50-02",
                   "P5": "PRESET 5: PN-D50-03", "P6": "PRESET 6: PN-D50-04"})
MODE_WORD = {"W": "WHOLE", "D": "DUAL", "S": "SPLIT", "": ""}


def line1(addr, name, mode, key_mode_on_line2=False):
    """DESIGN 5.2: cols 1-5 address, 6 ':', 7-26 name, 36-40 key mode right aligned."""
    s = (addr + ":" + name[:20]).ljust(35)
    s += "" if key_mode_on_line2 else MODE_WORD[mode].rjust(5)
    return s.ljust(40)[:40]


def line2_group(group, mode, key_mode_on_line2=False):
    s = GROUP_LONG[group].ljust(35)
    s += MODE_WORD[mode].rjust(5) if key_mode_on_line2 else ""
    return s.ljust(40)[:40]


# ---------------------------------------------------------------- SVG helpers
def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.out, self.defs = w, h, [], []

    def add(self, s):
        self.out.append(s)

    def rect(self, x, y, w, h, fill, crisp=True, extra=""):
        if w <= 0 or h <= 0:
            return
        sr = ' shape-rendering="crispEdges"' if crisp else ""
        self.add(f'<rect x="{x:g}" y="{y:g}" width="{w:g}" height="{h:g}" fill="{fill}"{sr}{extra}/>')

    def rrect(self, x, y, w, h, r, fill):  # antialiased rounded rect (button corners)
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" ry="{r}" fill="{fill}"/>')

    def frame(self, x, y, w, h, t, top, left=None, bottom=None, right=None):
        """Border of thickness t drawn inside (x, y, w, h); colours per side."""
        left = left or top
        bottom = bottom or top
        right = right or bottom
        self.rect(x, y, w, t, top)
        self.rect(x, y + t, t, h - t, left)
        self.rect(x + t, y + h - t, w - t, t, bottom)
        self.rect(x + w - t, y + t, t, h - 2 * t, right)

    def text(self, x, baseline, s, role, fill):
        w, size, ls = ROLES[role]
        lsa = f' letter-spacing="{ls:g}"' if ls else ""
        self.add(f'<text x="{x:g}" y="{baseline:g}" font-family="Inter" font-weight="{w}" '
                 f'font-size="{size}px"{lsa} fill="{fill}" xml:space="preserve">{esc(s)}</text>')

    def text_centred(self, cx, baseline, s, role, fill):
        self.text(round(cx - advance(s, role) / 2), baseline, s, role, fill)

    def text_right(self, right, baseline, s, role, fill):
        self.text(round(right - advance(s, role)), baseline, s, role, fill)

    def poly(self, pts, fill, crisp=True):
        sr = ' shape-rendering="crispEdges"' if crisp else ""
        p = " ".join(f"{x:g},{y:g}" for x, y in pts)
        self.add(f'<polygon points="{p}" fill="{fill}"{sr}/>')

    def write(self, path):
        with open(path, "w", encoding="utf-8") as f:
            f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
                    f'viewBox="0 0 {self.w} {self.h}">\n')
            f.write("<!-- Generated by docs/design/mockup/build.py from docs/design/DESIGN.md. "
                    "Do not edit by hand. -->\n")
            if self.defs:
                f.write("<defs>" + "".join(self.defs) + "</defs>\n")
            f.write("\n".join(self.out))
            f.write("\n</svg>\n")


# ---------------------------------------------------------------- display (DESIGN 5)
BEZEL = {3: (767, 79), 4: (1010, 98), 5: (1253, 117)}


def draw_display(svg, x0, y0, l1, l2, p=3, ghost=True):
    bw, bh = BEZEL[p]
    svg.rect(x0, y0, bw, bh, T["bezel"])
    svg.rect(x0, y0, bw, 1, T["bezelRimTop"])
    svg.rect(x0, y0 + 1, 1, bh - 1, T["bezelRim"])
    svg.rect(x0 + bw - 1, y0 + 1, 1, bh - 1, T["bezelRim"])
    svg.rect(x0 + 1, y0 + bh - 1, bw - 2, 1, T["bezelRim"])
    fx, fy, fw, fh = x0 + 19, y0 + 11, 243 * p, 19 * p
    assert (fw + 38, fh + 22) == (bw, bh)
    svg.rect(fx, fy, fw, fh, T["lcdBack"])
    d = p - 1  # at 1x: gap g = 1, dot d = P - g; the SVG is exported at 2x for 2x (P 6, g 2, d 4)
    on, off = [], []
    for li, line in enumerate((l1, l2)):
        assert len(line) == 40, repr(line)
        for k, ch in enumerate(line):
            code = ord(ch) if 0x20 <= ord(ch) <= 0x7E else ord("?")
            rows = GLYPHS[code]
            cx, cy = fx + 2 * p + 6 * p * k, fy + 2 * p + 8 * p * li
            for r in range(7):
                for c in range(5):
                    lit = rows[r] & (0x10 >> c)
                    (on if lit else off).append((cx + c * p, cy + r * p))
    path = lambda pts: "".join(f"M{x},{y}h{d}v{d}h-{d}z" for x, y in pts)
    if ghost:
        svg.add(f'<path d="{path(off)}" fill="{T["lcdGhost"]}" shape-rendering="crispEdges"/>')
    svg.add(f'<path d="{path(on)}" fill="{T["lcdOn"]}" shape-rendering="crispEdges"/>')


# ---------------------------------------------------------------- panel parts (DESIGN 6)
def section_rule(svg, x, y, w, title, accent=True):
    tc = T["labelAccent"] if accent else T["labelSecondary"]
    rc = T["ruleAccent"] if accent else T["ruleSecondary"]
    adv = advance(title, "panelLabel")
    tl = x + round((w - adv) / 2)
    tr = tl + round(adv)
    svg.rect(x, y + 8, tl - 6 - x, 1, rc)
    svg.rect(tr + 6, y + 8, x + w - (tr + 6), 1, rc)
    svg.text(tl, y + 10, title, "panelLabel", tc)


def led(svg, x, y, w, h, lit, core=True):
    svg.rect(x, y, w, h, T["ledOn"] if lit else T["ledOff"])
    if lit and core:
        svg.rect(x + 2, y + 1, w - 4, max(1, h - 2), T["ledCore"])


def panel_button(svg, x, y, kind, lit=False, legend="", state="normal", wedge=None):
    """DESIGN 6.2. kind 'A' LED, 'B' legend+LED, 'C' plain, 'D' wedge."""
    pressed = state == "pressed"
    disabled = state == "disabled"
    if disabled:
        face = T["buttonDisabled"]
    elif pressed:
        face = T["buttonPressed"]
    elif state == "hover":
        face = T["buttonFaceHover"]
    else:
        face = T["buttonFaceLit"] if lit else T["buttonFace"]
    svg.rrect(x - 1, y - 1, 46, 26, 3, T["buttonSlot"])
    svg.rrect(x, y, 44, 24, 2, face)
    if not pressed and not disabled:
        svg.rect(x + 2, y + 1, 40, 1, T["buttonHighlight"])
    dy = 1 if pressed else 0
    on = lit and not disabled
    if kind == "A":
        led(svg, x + 15, y + 10 + dy, 14, 4, on)
    elif kind == "B":
        svg.text_centred(x + 22, y + 11 + dy, legend, "panelLabel",
                         T["labelDisabled"] if disabled else T["labelPrimary"])
        led(svg, x + 15, y + 16 + dy, 14, 3, on)
    elif kind == "D":
        pts = [(18, 12), (25, 8), (25, 16)] if wedge == "left" else [(26, 12), (19, 8), (19, 16)]
        svg.poly([(x + a, y + b + dy) for a, b in pts], T["labelDisabled"] if disabled else T["arrow"])
    if state == "focused":
        svg.frame(x - 4, y - 4, 52, 32, 2, T["focusRing"])


def wordmark(svg, x, y):
    """DESIGN 4.5: slot (x, y, 360, 28); baselines at y + 20."""
    svg.text(x, y + 20, "PATCH SWITCHER", "wordmark", T["labelAccent"])
    rx = x + round(advance("PATCH SWITCHER", "wordmark")) + 14
    svg.rect(rx, y + 9, 1, 12, T["ruleSecondary"])
    svg.text(rx + 15, y + 20, "FOR ROLAND D-05", "wordmarkSub", T["labelSecondary"])
    return rx + 15 + round(advance("FOR ROLAND D-05", "wordmarkSub")) - x


def star_points(cx, cy, ro=6.0, ri=2.6):
    import math
    pts = []
    for k in range(10):
        a = math.radians(-90 + 36 * k)
        r = ro if k % 2 == 0 else ri
        pts.append((round(cx + r * math.cos(a), 3), round(cy + r * math.sin(a), 3)))
    return pts


# ---------------------------------------------------------------- main window
W, H = 960, 560


def main_window(state, out_svg):
    """state: 'search' (the T2.4 deliverable) or 'disconnected' (second frame)."""
    p = 3
    bw, bh = BEZEL[p]
    B = 46 + bh + 14
    assert B == 139
    svg = Svg(W, H)
    svg.rect(0, 0, W, H, T["panel"])

    # nameplate: wordmark (no logo.png)
    wordmark(svg, 16, 10)

    # display
    cur = dict(addr="P1-11", name="Fantasia", mode="D", group="P1", bank=1, num=1)
    query = "brass"
    matches = []
    for r in presets():
        low = r["name"].lower()
        if query in low:
            ranges, i = [], low.find(query)
            while i >= 0:
                ranges.append((i, len(query)))
                i = low.find(query, i + len(query))
            matches.append(dict(r, ranges=ranges))
    if state == "search":
        n = len(matches)
        l2 = ("NO MATCHES" if n == 0 else "1 MATCH" if n == 1 else f"{n} MATCHES").ljust(40)
    else:
        l2 = "NO MIDI OUTPUT".ljust(40)
    draw_display(svg, 16, 46, line1(cur["addr"], cur["name"], cur["mode"]), l2, p)

    # band
    svg.rect(0, B, W, H - 22 - B, T["band"])
    svg.rect(0, B, W, 1, T["bandEdge"])

    # left column (4.3)
    col = lambda i: 16 + 50 * i
    section_rule(svg, 16, B + 12, 394, "PATCH GROUP")
    for i in range(8):
        g = f"U{i + 1}"
        panel_button(svg, col(i), B + 28, "B", lit=(g == cur["group"]), legend=g)
    for i in range(6):
        g = f"P{i + 1}"
        panel_button(svg, col(i), B + 56, "B", lit=(g == cur["group"]), legend=g)
    for title, ry, ny, by, curv in (("PATCH BANK", 92, 106, 119, cur["bank"]),
                                    ("PATCH NUMBER", 155, 169, 182, cur["num"])):
        section_rule(svg, 16, B + ry, 394, title)
        for i in range(8):
            svg.text_centred(col(i) + 22, B + ny + 9, str(i + 1), "panelLabel", T["labelAccent"])
            panel_button(svg, col(i), B + by, "A", lit=(i + 1 == curv))
    for i, (lab, wd) in enumerate((("PREV", "left"), ("NEXT", "right"))):
        svg.text_centred(col(i) + 22, B + 218 + 9, lab, "panelLabel", T["labelPrimary"])
        panel_button(svg, col(i), B + 231, "D", wedge=wd)

    # right column (4.3, 6.5)
    X, R = 434, W - 450
    controls = [(X + 0, "A", "ALL", T["labelPrimary"], True),
                (X + 72, "A", "FAVOURITES", T["labelPrimary"], False),
                (X + 150, "A", "RECENTS", T["labelPrimary"], False),
                (X + R - 124, "A", "AUTO SEND", T["labelRed"], False),
                (X + R - 44, "C", "SYSTEM", T["labelPrimary"], False)]
    for fx, kind, lab, colr, lit in controls:
        svg.text_centred(fx + 22, B + 15 + 9, lab, "panelLabel", colr)
        panel_button(svg, fx, B + 28, kind, lit=lit)

    # search field (7.1)
    sy = B + 64
    svg.rect(X, sy, R, 32, T["fieldBack"])
    fb = vcentre_baseline(sy + 2, 28, "field")  # border 1 + QLineEdit vertical margin 1
    tx = X + 13  # border 1 + padding 10 + QLineEdit horizontal margin 2 (focused: 2 + 9 + 2)
    if state == "search":
        svg.frame(X, sy, R, 32, 2, T["focusRing"])
        svg.text(tx, fb, query, "field", T["labelPrimary"])
        a, d = ascent_descent("field")
        cx = tx + round(advance(query, "field"))
        svg.rect(cx, fb - a, 1, a + d, T["labelPrimary"])  # caret
    else:
        svg.frame(X, sy, R, 32, 1, T["wellEdgeDark"], T["wellEdgeDark"], T["fieldBorder"], T["fieldBorder"])
        svg.text(tx, fb, "Search names or addresses: brass, P3-45", "field", T["labelDim"])

    # list well (7.2)
    wy = B + 104
    wh = H - 22 - B - 116
    svg.rect(X, wy, R, wh, T["well"])
    list_focused = state == "disconnected"
    if list_focused:
        svg.frame(X, wy, R, wh, 1, T["focusRing"])
    else:
        svg.frame(X, wy, R, wh, 1, T["wellEdgeDark"], T["wellEdgeDark"], T["wellEdgeLight"], T["wellEdgeLight"])
    ix, iy, iw, ih = X + 1, wy + 1, R - 2, wh - 2
    if state == "search":
        rows = matches
        first = 0
        highlighted = 0
        stars = {"P1-35"}
    else:
        rows = [dict(r, ranges=[]) for r in catalogue()]
        cur_i = next(i for i, r in enumerate(rows) if r["addr"] == cur["addr"])
        first = cur_i - 3  # three unnamed user slots above the current patch
        highlighted = cur_i
        stars = {"P1-12", "P1-16"}
    total = len(rows)
    need_scroll = total * 28 > ih
    Wl = iw - (10 if need_scroll else 0)
    svg.defs.append(f'<clipPath id="well"><rect x="{ix}" y="{iy}" width="{iw}" height="{ih}"/></clipPath>')
    svg.add('<g clip-path="url(#well)">')
    visible = (ih + 27) // 28
    for vi in range(visible):
        ri = first + vi
        if ri >= total:
            break
        r = rows[ri]
        ry = iy + vi * 28
        is_cur = r["addr"] == cur["addr"]
        if ri == highlighted:
            svg.rect(ix, ry, Wl, 28, T["selection"])
            svg.rect(ix, ry, 3, 28, T["focusRing"] if list_focused else T["selectionBarIdle"])
        if is_cur:
            led(svg, ix + 10, ry + 12, 10, 4, True)
        acol = T["labelPrimary"] if is_cur else T["labelSecondary"]
        for ci, ch in enumerate(r["addr"]):
            svg.text_centred(ix + 28 + 9 * ci + 4.5, ry + 19, ch, "listAddress", acol)
        name = r["name"] if r["name"] else r["addr"]
        for (st, ln) in r["ranges"]:
            hx = ix + 84 + round(advance(name[:st], "listName"))
            hw = round(advance(name[:st + ln], "listName")) - round(advance(name[:st], "listName"))
            svg.rect(hx, ry + 4, hw, 20, T["matchBack"])
        svg.text(ix + 84, ry + 19, name, "listName", T["labelPrimary"] if r["name"] else T["labelDim"])
        if r["mode"]:
            bx, by = ix + Wl - 52, ry + 6
            svg.frame(bx, by, 16, 16, 1, T["tagBorder"])
            svg.text_centred(bx + 8 + 0.25, vcentre_baseline(by, 16, "listMeta"), r["mode"], "listMeta",
                             T["labelSecondary"])
        pts = star_points(ix + Wl - 16, ry + 14)
        if r["addr"] in stars:
            svg.poly(pts, T["labelPrimary"], crisp=False)
        else:
            pp = " ".join(f"{a:g},{b:g}" for a, b in pts)
            svg.add(f'<polygon points="{pp}" fill="none" stroke="{T["starOff"]}" stroke-width="1"/>')
    svg.add("</g>")
    if need_scroll:  # 7.6
        sx = ix + iw - 10
        svg.rect(sx, iy, 10, ih, T["well"])
        hl = max(24, round(ih * ih / (total * 28)))
        maxv = total * 28 - ih
        hy = iy + round((ih - hl) * (first * 28) / maxv)
        svg.rect(sx + 2, hy, 6, hl, T["buttonFace"])

    # status bar (8.1)
    sy = H - 22
    svg.rect(0, sy, W, 22, T["panel"])
    svg.rect(0, sy, W, 1, T["bandEdge"])
    led(svg, 16, sy + 9, 8, 4, False, core=False)
    bl = vcentre_baseline(sy, 22, "statusLabel")
    assert bl == vcentre_baseline(sy, 22, "statusValue")
    x = 30

    def item(x, s, role, colour, gap_after):
        svg.text(x, bl, s, role, colour)
        return x + round(advance(s, role)) + gap_after

    x = item(x, "OUT", "statusLabel", T["labelSecondary"], 6)
    port = "Hapax MIDI 1"
    if state == "disconnected":
        x = item(x, port, "statusValue", T["labelPrimary"], 6)
        x = item(x, "NOT FOUND", "statusLabel", T["labelRed"], 16)
    else:
        x = item(x, port, "statusValue", T["labelPrimary"], 16)
    x = item(x, "CH", "statusLabel", T["labelSecondary"], 6)
    x = item(x, "1", "statusValue", T["labelPrimary"], 16)
    x = item(x, "IN", "statusLabel", T["labelSecondary"], 6)
    x = item(x, "None", "statusValue", T["labelDim"], 16)
    svg.write(out_svg)
    return dict(matches=len(matches), list_rows_visible=visible, Wl=Wl, well=(X, wy, R, wh))


# ---------------------------------------------------------------- display crops
def display_crop(out_svg, ghost=True, key_mode_on_line2=False):
    svg = Svg(767 + 32, 79 + 32)
    svg.rect(0, 0, svg.w, svg.h, T["panel"])
    l1 = line1("P1-11", "Fantasia", "D", key_mode_on_line2)
    l2 = line2_group("P1", "D", key_mode_on_line2)
    draw_display(svg, 16, 16, l1, l2, 3, ghost)
    svg.write(out_svg)


# ---------------------------------------------------------------- settings (DESIGN 10.1, 10.2)
def wrap(s, role, width):
    lines, cur = [], ""
    for word in s.split(" "):
        t = word if not cur else cur + " " + word
        if advance(t, role) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = word
    lines.append(cur)
    return lines


def settings(out_svg):
    note = ("The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also "
            "has a Bank.S switch, turn it ON too.")
    nlines = wrap(note, "note", 480)
    a, d = ascent_descent("note")
    note_h = len(nlines) * (a + d)
    btn_y = 318 + note_h + 20
    Hd = 20 + btn_y + 28 + 20
    svg = Svg(520, Hd)
    svg.rect(0, 0, 520, Hd, T["panel"])
    ox, oy = 20, 20

    def form_label(y, h, s):
        svg.text_right(ox + 100, vcentre_baseline(oy + y, h, "panelLabel"), s, "panelLabel", T["labelPrimary"])

    def combo(x, y, w, s, focused=False, dim=False):
        x, y = ox + x, oy + y
        svg.rect(x, y, w, 26, T["fieldBack"])
        if focused:
            svg.frame(x, y, w, 26, 2, T["focusRing"])
        else:
            svg.frame(x, y, w, 26, 1, T["fieldBorder"])
        svg.text(x + 1 + 8, vcentre_baseline(y, 26, "body"), s, "body", T["labelPrimary"])
        ax = x + w - 1 - 20 + 6
        ay = y + 11
        for i, wd in enumerate((8, 6, 4, 2)):  # arrow-down.png rows 8, 6, 4, 2 px
            svg.rect(ax + i, ay + i, wd, 1, T["arrow"])

    def button(x, y, w, h, s, default=False, disabled=False, red=False):
        x, y = ox + x, oy + y
        svg.rrect(x, y, w, h, 2, T["labelRed"] if default else T["buttonSlot"])
        svg.rrect(x + 1, y + 1, w - 2, h - 2, 1, T["buttonDisabled"] if disabled else T["buttonFace"])
        tc = T["labelDisabled"] if disabled else (T["labelRed"] if red else T["labelPrimary"])
        svg.text_centred(x + w / 2, vcentre_baseline(y, h, "panelLabel"), s, "panelLabel", tc)

    def checkbox(y, s, checked, disabled=False):
        x, y = ox + 112, oy + y
        iy = y + 7
        if disabled:
            svg.rect(x, iy, 16, 6, T["buttonDisabled"])
            svg.frame(x, iy, 16, 6, 1, T["fieldBorder"])
        elif checked:
            svg.rect(x, iy, 16, 6, T["ledOn"])
        else:
            svg.rect(x, iy, 16, 6, T["ledOff"])
            svg.frame(x, iy, 16, 6, 1, T["fieldBorderHover"])
        svg.text(x + 16 + 10, vcentre_baseline(y, 20, "body"), s, "body",
                 T["labelDisabled"] if disabled else T["labelPrimary"])

    section_rule(svg, ox, oy + 0, 480, "MIDI OUTPUT", accent=False)
    form_label(20, 26, "PORT")
    combo(112, 20, 260, "Hapax MIDI 1", focused=True)
    button(380, 20, 100, 26, "REFRESH")
    form_label(54, 26, "CHANNEL")
    sx, sy = ox + 112, oy + 54
    svg.rect(sx, sy, 72, 26, T["fieldBack"])
    svg.frame(sx, sy, 72, 26, 1, T["fieldBorder"])
    svg.text_centred(sx + 36, vcentre_baseline(sy, 26, "body"), "1", "body", T["labelPrimary"])
    led(svg, ox + 112, oy + 88 + 8, 10, 4, True, core=False)
    svg.text(ox + 130, vcentre_baseline(oy + 88, 20, "body"), "Connected to Hapax MIDI 1", "body",
             T["labelPrimary"])
    section_rule(svg, ox, oy + 124, 480, "MIDI INPUT", accent=False)
    form_label(144, 26, "PORT")
    combo(112, 144, 260, "None")
    checkbox(178, "Follow the panel", False, disabled=True)
    section_rule(svg, ox, oy + 214, 480, "BEHAVIOUR", accent=False)
    checkbox(234, "Auto-send on highlight", False)
    checkbox(262, "Resend the current patch on connect", True)
    section_rule(svg, ox, oy + 298, 480, "ON THE D-05", accent=False)
    for i, ln in enumerate(nlines):
        svg.text(ox, oy + 318 + i * (a + d) + a, ln, "note", T["labelSecondary"])
    button(0, btn_y, 100, 28, "TEST")
    button(272, btn_y, 100, 28, "CANCEL")
    button(380, btn_y, 100, 28, "OK", default=True)
    svg.write(out_svg)
    return dict(height=Hd, note_lines=nlines, button_row_y=btn_y)


# ---------------------------------------------------------------- render
def render(svg_path, png_path, width, fc_env):
    subprocess.run(["inkscape", "--export-type=png", f"--export-width={width}",
                    f"--export-filename={png_path}", svg_path],
                   check=True, env=fc_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    tmp = tempfile.mkdtemp(prefix="d05-mockup-fc-")
    conf = os.path.join(tmp, "fonts.conf")
    with open(conf, "w") as f:
        f.write('<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig>'
                f'<dir>{esc(FONTS)}</dir><cachedir>{esc(tmp)}/cache</cachedir></fontconfig>')
    env = dict(os.environ, FONTCONFIG_FILE=conf, HOME=tmp)
    design = os.path.join(ROOT, "docs", "design")
    jobs = []

    s = os.path.join(HERE, "main-search.svg")
    info = main_window("search", s)
    print("main search:", info)
    jobs.append((s, os.path.join(design, "mockup-main"), W))
    s = os.path.join(HERE, "main-disconnected.svg")
    main_window("disconnected", s)
    jobs.append((s, os.path.join(HERE, "main-disconnected"), W))
    s = os.path.join(HERE, "settings.svg")
    info = settings(s)
    print("settings:", info)
    jobs.append((s, os.path.join(design, "mockup-settings"), 520))
    for name, kw in (("display-ghost-on", dict(ghost=True)), ("display-ghost-off", dict(ghost=False)),
                     ("display-keymode-line1", dict()), ("display-keymode-line2", dict(key_mode_on_line2=True))):
        s = os.path.join(HERE, name + ".svg")
        display_crop(s, **kw)
        jobs.append((s, os.path.join(HERE, name), 767 + 32))
    for svg_path, base, w in jobs:
        render(svg_path, base + ".png", w, env)
        render(svg_path, base + "@2x.png", 2 * w, env)
        print("wrote", base + ".png", "and @2x")


if __name__ == "__main__":
    sys.exit(main())
