#!/usr/bin/env python3
"""Builds the version 2 mockups (T7.4) from docs/design/DESIGN-2.md and tokens-2.json.

Usage (from anywhere):  python3 docs/design/mockup/build2.py

Writes SVG sources into docs/design/mockup/ and renders them with Inkscape at 1x and 2x:

  docs/design/mockup-jp08.png              960 x 560   JP-08 skin, list focused, a renamed slot
  docs/design/mockup-ju06a-60.png          960 x 560   JU-06A on 60, search "pad" across both engines
  docs/design/mockup-ju06a-106.png         960 x 560   JU-06A on 106, armed for 60:3-6, prompt showing
  docs/design/mockup-devices.png           960 x 560   D-05 skin, switcher with three devices, JU-06A missing
  docs/design/mockup-settings-devices.png  760 x H     Settings, device list, JU-06A page
  docs/design/mockup/led-states.png                    LED display slot in every state (sheet)
  docs/design/mockup/engine-states.png                 engine control states (sheet)
  docs/design/mockup/jp08-grid-up.png                  owner alternative: JP-08 grid moved up 80 px

Reuses the version 1 helpers in build.py (Svg, text measuring with Pillow + raqm, the LCD
drawing for the D-05) and does not modify that file, whose T and ROLES tables are checked
by gen-tokens.py against tokens.json version 1. Colours come from tokens-2.json resolved per
skin, so the mockups are exactly the token values the builder gets. Needs what build.py
needs: python3, Pillow with raqm, inkscape 1.x. Does not launch any browser.
"""

import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True  # no __pycache__ next to the sources
import build as B  # noqa: E402  (version 1 helpers)

ROOT = B.ROOT
TOK = json.load(open(os.path.join(ROOT, "docs", "design", "tokens-2.json"), encoding="utf-8"))
DATA = os.path.join(ROOT, "data")

# New type roles (DESIGN-2 3) registered with the version 1 measuring helpers.
for name, r in TOK["base"]["type"].items():
    B.ROLES.setdefault(name, (r["weight"], r["size"], float(r["spacing"])))


def resolve(skin):
    s = TOK["skins"][skin]
    out = {k: v["hex"] for k, v in TOK["base"]["colors"].items()}
    out.update(s["colors"])
    comp = dict(TOK["base"]["components"]); comp.update(s["components"])
    strings = dict(TOK["base"]["strings"]); strings.update(s["strings"])
    sw = dict(TOK["base"]["switches"]); sw.update(s["switches"])
    return out, comp, strings, sw


advance = B.advance
vcb = B.vcentre_baseline


def cap_height(role):
    """qRound(QFontMetricsF::capHeight()): OS/2 sCapHeight scaled (Inter: 1490 / 2048)."""
    return round(1490 * B.ROLES[role][1] / 2048)


# ---------------------------------------------------------------- data
def jp08_rows():
    rows = []
    with open(os.path.join(DATA, "jp08-patches.tsv"), encoding="utf-8") as f:
        next(f)
        for line in f:
            b, n, name = line.rstrip("\n").split("\t")
            rows.append(dict(addr=f"{b}-{n}", bank=int(b), num=int(n), name=name, engine=None))
    return rows


def ju06a_rows():
    rows = []
    with open(os.path.join(DATA, "ju06a-patches.tsv"), encoding="utf-8") as f:
        next(f)
        for line in f:
            e, b, n, name = line.rstrip("\n").split("\t")
            rows.append(dict(addr=f"{b}-{n}", bank=int(b), num=int(n), name=name, engine=int(e)))
    return rows


def ranges_for(name, q):
    low, out, i = name.lower(), [], name.lower().find(q)
    while q and i >= 0:
        out.append((i, len(q)))
        i = low.find(q, i + len(q))
    return out


# ---------------------------------------------------------------- panel parts (DESIGN-2 5)
def section_rule(svg, S, style, x, y, w, title, band="ruleBand"):
    """DESIGN-2 5.1. 12 px tall in every style."""
    if style == "hairline":
        adv = advance(title, "panelLabel")
        tl = x + round((w - adv) / 2)
        tr = tl + round(adv)
        svg.rect(x, y + 8, tl - 6 - x, 1, S["ruleAccent"])
        svg.rect(tr + 6, y + 8, x + w - (tr + 6), 1, S["ruleAccent"])
        svg.text(tl, y + 10, title, "panelLabel", S["labelAccent"])
        return
    adv = advance(title, "ruleTitle")
    tl = x + round((w - adv) / 2)
    tr = tl + round(adv)
    if style == "doubleBar":
        svg.rect(x, y + 2, w, 3, S["ruleAccent"])
        svg.rect(x, y + 7, w, 3, S["ruleAccent"])
        svg.rrect(tl - 8, y, tr - tl + 16, 12, 2, S["ruleTitleBack"])
        svg.text(tl, y + 10, title, "ruleTitle", S["ruleTitleText"])
    elif style == "band":
        svg.rect(x, y, w, 12, S[band])
        svg.text(tl, y + 10, title, "ruleTitle", S[band + "Text"])


def secondary_rule(svg, S, x, y, w, title):
    adv = advance(title, "panelLabel")
    tl = x + round((w - adv) / 2)
    tr = tl + round(adv)
    svg.rect(x, y + 8, tl - 6 - x, 1, S["ruleSecondary"])
    svg.rect(tr + 6, y + 8, x + w - (tr + 6), 1, S["ruleSecondary"])
    svg.text(tl, y + 10, title, "panelLabel", S["labelSecondary"])


def slit(svg, S, x, y, w, h, lit, core=True):
    svg.rect(x, y, w, h, S["ledOn"] if lit else S["ledOff"])
    if lit and core:
        svg.rect(x + 2, y + 1, w - 4, max(1, h - 2), S["ledCore"])


def armed_slit(svg, S, x, y, w, h):
    """DESIGN-2 6.2: armed = the unlit LED (x, y, w, h) inside a 1 px ledOn frame drawn outside it."""
    svg.frame(x - 1, y - 1, w + 2, h + 2, 1, S["ledOn"])
    svg.rect(x, y, w, h, S["ledOff"])


def circle(svg, cx, cy, r, fill):
    svg.add(f'<circle cx="{cx:g}" cy="{cy:g}" r="{r:g}" fill="{fill}"/>')


def lens(svg, S, x, y, state):
    """DESIGN-2 5.2: rim bbox (x, y, 10, 10), lens (x+1, y+1, 8, 8). state: lit, off, armed."""
    if state == "armed":  # 2 px ledOn ring around a 6 px unlit centre
        circle(svg, x + 5, y + 5, 5, S["ledOn"])
        circle(svg, x + 5, y + 5, 3, S["ledOff"])
        return
    circle(svg, x + 5, y + 5, 5, S["lensRim"])
    circle(svg, x + 5, y + 5, 4, S["ledOn"] if state == "lit" else S["ledOff"])


FACE_KEYS = {
    "button": ("buttonFace", "buttonFaceHover", "buttonPressed", "buttonHighlight", "buttonFaceLit"),
}
for _r in ("bank", "view", "commit", "transport"):
    FACE_KEYS[_r] = (f"{_r}Face", f"{_r}FaceHover", f"{_r}Pressed", f"{_r}Highlight", f"{_r}FaceLit")


def panel_button(svg, S, comp, x, y, kind, faces="button", lit=False, legend="", state="normal", wedge=None,
                 armed=False):
    f, fh, fp, fhl, fl = (S[k] for k in FACE_KEYS[faces])
    pressed, disabled = state == "pressed", state == "disabled"
    face = S["buttonDisabled"] if disabled else fp if pressed else fh if state == "hover" else fl if lit else f
    svg.rrect(x - 1, y - 1, 46, 26, 3, S["buttonSlot"])
    svg.rrect(x, y, 44, 24, 2, face)
    if not pressed and not disabled:
        svg.rect(x + 2, y + 1, 40, 1, fhl)
    dy = 1 if pressed else 0
    on = lit and not disabled
    lst = "armed" if armed and not on else ("lit" if on else "off")
    if comp["buttonLed"] == "slit":
        if kind == "A":
            slit(svg, S, x + 15, y + 10 + dy, 14, 4, on)
        elif kind == "B":
            svg.text_centred(x + 22, y + 11 + dy, legend, "panelLabel",
                             S["labelDisabled"] if disabled else S["buttonLegend"])
            slit(svg, S, x + 15, y + 16 + dy, 14, 3, on)
    else:
        if kind == "A":
            lens(svg, S, x + 17, y + 7 + dy, lst)
        elif kind == "B":
            lens(svg, S, x + 5, y + 7 + dy, lst)
            svg.text_centred(x + 28, y + 16 + dy, legend, "panelLabel",
                             S["labelDisabled"] if disabled else S["buttonLegend"])
    if kind == "D":
        pts = [(18, 12), (25, 8), (25, 16)] if wedge == "left" else [(26, 12), (19, 8), (19, 16)]
        svg.poly([(x + a, y + b + dy) for a, b in pts], S["labelDisabled"] if disabled else S["wedge"])
    if state == "focused":
        svg.frame(x - 4, y - 4, 52, 32, 2, S["focusRing"])


# ---------------------------------------------------------------- LED readout (DESIGN-2 4.2)
SEG_W, SEG_H, T_, E_, SLANT = 8.0, 15.0, 2.0, 0.25, 0.1
_h = T_ / 2


def _hseg(yc):
    x0, x1 = _h + E_, SEG_W - _h - E_
    return [(x0, yc), (x0 + _h, yc - _h), (x1 - _h, yc - _h), (x1, yc), (x1 - _h, yc + _h), (x0 + _h, yc + _h)]


def _vseg(xc, y0, y1):
    return [(xc, y0), (xc + _h, y0 + _h), (xc + _h, y1 - _h), (xc, y1), (xc - _h, y1 - _h), (xc - _h, y0 + _h)]


MID = SEG_H / 2
SEGS = {
    "a": _hseg(_h), "g": _hseg(MID), "d": _hseg(SEG_H - _h),
    "f": _vseg(_h, _h + E_, MID - E_), "b": _vseg(SEG_W - _h, _h + E_, MID - E_),
    "e": _vseg(_h, MID + E_, SEG_H - _h - E_), "c": _vseg(SEG_W - _h, MID + E_, SEG_H - _h - E_),
}
DOT = [(8.5, 13.0), (10.5, 13.0), (10.5, 15.0), (8.5, 15.0)]
GLYPH7 = {"0": "abcdef", "1": "bc", "2": "abdeg", "3": "abcdg", "4": "bcfg", "5": "acdfg", "6": "acdefg",
          "7": "abc", "8": "abcdefg", "9": "abcdfg", "-": "g", "_": "d", " ": ""}


def _slanted(pts, ox, oy, P):
    return [(ox + (x + SLANT * (SEG_H - y)) * P, oy + y * P) for x, y in pts]


LED_FIELD_W = {2: 27, 3: 38}


def led_window_width(cells, p):
    return LED_FIELD_W[cells] * p + 22


def draw_led_readout(svg, S, sw, x0, y0, p, cells, text, dot=False):
    """Window (x0, y0, LED_FIELD_W*p + 22, 19p + 22); text is `cells` characters."""
    ww, wh = led_window_width(cells, p), 19 * p + 22
    svg.rect(x0, y0, ww, wh, S["bezel"])
    svg.rect(x0, y0, ww, 1, S["bezelRimTop"])
    svg.rect(x0, y0 + 1, 1, wh - 1, S["bezelRim"])
    svg.rect(x0 + ww - 1, y0 + 1, 1, wh - 1, S["bezelRim"])
    svg.rect(x0 + 1, y0 + wh - 1, ww - 2, 1, S["bezelRim"])
    fx, fy = x0 + 11, y0 + 11
    svg.rect(fx, fy, LED_FIELD_W[cells] * p, 19 * p, S["ledBack"])
    assert len(text) == cells, text
    for k, ch in enumerate(text):
        ox, oy = fx + 3 * p + 11 * p * k, fy + 2 * p
        lit = GLYPH7[ch]
        for name, pts in SEGS.items():
            if name in lit:
                svg.poly(_slanted(pts, ox, oy, p), S["ledSegOn"], crisp=False)
            elif sw["ledShowGhost"]:
                svg.poly(_slanted(pts, ox, oy, p), S["ledSegGhost"], crisp=False)
        if dot and k == cells - 1:
            svg.poly(_slanted(DOT, ox, oy, p), S["ledSegOn"], crisp=False)
        elif sw["ledShowGhost"]:
            svg.poly(_slanted(DOT, ox, oy, p), S["ledSegGhost"], crisp=False)
    return ww, wh


CONTEXT_COLOUR = {"idle": "labelSecondary", "matches": "labelSecondary", "sent": "labelPrimary",
                  "hint": "labelPrimary", "prompt": "ledSegOn", "nooutput": "labelRed", "firstrun": "labelPrimary"}


def draw_led_display(svg, S, comp, sw, x0, y0, p, readout, name, context, ctx_state, dot=False):
    """DESIGN-2 4.2: LED window, then the name and the context line in panel lettering."""
    ww, wh = draw_led_readout(svg, S, sw, x0, y0, p, comp["ledCells"], readout, dot)
    nx = x0 + ww + 24
    digit_top, digit_bottom = y0 + 11 + 2 * p, y0 + 11 + 17 * p
    nrole, crole = f"displayName{p}", f"displayContext{p}"
    if name:
        svg.text(nx, digit_top + cap_height(nrole), name, nrole, S["labelPrimary"])
    if context:
        svg.text(nx, digit_bottom, context, crole, S[CONTEXT_COLOUR[ctx_state]])
    return nx


# ---------------------------------------------------------------- nameplate and switcher (DESIGN-2 4.4)
def nameplate(svg, S, strings, x, y, devices=None):
    """devices: None or one entry -> wordmark + sub; several -> wordmark + tabs.
    Each device: (name, current, connected)."""
    svg.text(x, y + 20, "PATCH SWITCHER", "wordmark", S["labelAccent"])
    rx = x + round(advance("PATCH SWITCHER", "wordmark")) + 14
    svg.rect(rx, y + 9, 1, 12, S["ruleSecondary"])
    tx = rx + 15
    if not devices or len(devices) == 1:
        svg.text(tx, y + 20, strings["wordmarkSub"], "wordmarkSub", S["labelSecondary"])
        return []
    hits = []
    for name, current, connected in devices:
        slit(svg, S, tx, y + 14, 10, 4, current)
        colour = S["labelRed"] if not connected else S["labelPrimary"] if current else S["labelSecondary"]
        svg.text(tx + 16, y + 20, name, "switcherTab", colour)
        w = 16 + round(advance(name, "switcherTab"))
        hits.append((name, tx - 6, w + 12))
        tx += w + 18
    return hits


# ---------------------------------------------------------------- list rows (DESIGN-2 6)
def list_well(svg, S, comp, X, wy, R, wh, rows, first, highlighted, focused, cur_key, stars, query="",
              engine=None, remembered_other=None, armed_key=None, user=None):
    """rows: dicts with addr, name, engine; user: {key: user name}. Keys are (engine, addr)."""
    user = user or {}
    svg.rect(X, wy, R, wh, S["well"])
    if focused:
        svg.frame(X, wy, R, wh, 1, S["focusRing"])
    else:
        svg.frame(X, wy, R, wh, 1, S["wellEdgeDark"], S["wellEdgeDark"], S["wellEdgeLight"], S["wellEdgeLight"])
    ix, iy, iw, ih = X + 1, wy + 1, R - 2, wh - 2
    total = len(rows)
    need_scroll = total * 28 > ih
    Wl = iw - (10 if need_scroll else 0)
    cid = f"well{len(svg.defs)}"
    svg.defs.append(f'<clipPath id="{cid}"><rect x="{ix}" y="{iy}" width="{iw}" height="{ih}"/></clipPath>')
    svg.add(f'<g clip-path="url(#{cid})">')
    visible = (ih + 27) // 28
    for vi in range(visible):
        ri = first + vi
        if ri >= total:
            break
        r = rows[ri]
        key = (r.get("engine"), r["addr"])
        ry = iy + vi * 28
        is_cur = key == cur_key
        if ri == highlighted:
            svg.rect(ix, ry, Wl, 28, S["selection"])
            svg.rect(ix, ry, 3, 28, S["focusRing"] if focused else S["selectionBarIdle"])
        if key == armed_key:
            armed_slit(svg, S, ix + 10, ry + 12, 10, 4)
        elif is_cur:
            slit(svg, S, ix + 10, ry + 12, 10, 4, True)
        elif key == remembered_other:
            slit(svg, S, ix + 10, ry + 12, 10, 4, False)
        acol = S["labelPrimary"] if is_cur else S["labelSecondary"]
        for ci, ch in enumerate(r["addr"]):
            svg.text_centred(ix + 28 + 9 * ci + 4.5, ry + 19, ch, "listAddress", acol)
        uname = user.get(key)
        shown = uname or r["name"]
        x_name = ix + 84
        for (st, ln) in ranges_for(shown, query):
            hx = x_name + round(advance(shown[:st], "listName"))
            hw = round(advance(shown[:st + ln], "listName")) - round(advance(shown[:st], "listName"))
            svg.rect(hx, ry + 4, hw, 20, S["matchBack"])
        if not shown:  # unnamed D-05 user slot: the address in labelDim (DESIGN 7.2)
            svg.text(x_name, ry + 19, r["addr"], "listName", S["labelDim"])
        svg.text(x_name, ry + 19, shown, "listName", S["labelPrimary"])
        if uname:
            fx = x_name + round(advance(uname, "listName")) + 12
            for (st, ln) in ranges_for(r["name"], query):
                hx = fx + round(advance(r["name"][:st], "listName"))
                hw = round(advance(r["name"][:st + ln], "listName")) - round(advance(r["name"][:st], "listName"))
                svg.rect(hx, ry + 4, hw, 20, S["matchBack"])
            svg.text(fx, ry + 19, r["name"], "listName", S["labelDim"])
        if comp["rowTag"] == "engine":
            tag = str(r["engine"])
            bx, by = ix + Wl - 58, ry + 6
            if r["engine"] == engine:
                svg.frame(bx, by, 28, 16, 1, S["tagBorder"])
                svg.text_centred(bx + 14 + 0.25, vcb(by, 16, "listMeta"), tag, "listMeta", S["labelSecondary"])
            else:
                svg.text_centred(bx + 14 + 0.25, vcb(by, 16, "listMeta"), tag, "listMeta", S["labelDim"])
        elif comp["rowTag"] == "keyMode" and r.get("mode"):
            bx, by = ix + Wl - 52, ry + 6
            svg.frame(bx, by, 16, 16, 1, S["tagBorder"])
            svg.text_centred(bx + 8 + 0.25, vcb(by, 16, "listMeta"), r["mode"], "listMeta", S["labelSecondary"])
        pts = B.star_points(ix + Wl - 16, ry + 14)
        if key in stars:
            svg.poly(pts, S["labelPrimary"], crisp=False)
        else:
            pp = " ".join(f"{a:g},{b:g}" for a, b in pts)
            svg.add(f'<polygon points="{pp}" fill="none" stroke="{S["starOff"]}" stroke-width="1"/>')
    svg.add("</g>")
    if need_scroll:
        sx = ix + iw - 10
        svg.rect(sx, iy, 10, ih, S["well"])
        hl = max(24, round(ih * ih / (total * 28)))
        hy = iy + round((ih - hl) * (first * 28) / (total * 28 - ih))
        svg.rect(sx + 2, hy, 6, hl, S["controlFace"])
    return Wl


# ---------------------------------------------------------------- engine control (DESIGN-2 4.3)
CAPTION = ("The app's belief about the 60/106 switch.", "Ctrl+E changes the belief, never the unit.")


def engine_control(svg, S, comp, strings, x, y, engine, armed_engine=None, focused=None):
    """Slot (x, y, 394, 68) = left column rows B+12 .. B+80."""
    section_rule(svg, S, comp["sectionRule"], x, y, 394, strings["engineTitle"], band="ruleBand2")
    for i, e in enumerate((60, 106)):
        panel_button(svg, S, comp, x + 50 * i, y + 16, "B", faces="button", lit=(e == engine), legend=str(e),
                     armed=(e == armed_engine), state="focused" if focused == e else "normal")
    a, d = B.ascent_descent("note")
    top = y + 16 + (24 - 2 * (a + d)) // 2
    for i, line in enumerate(CAPTION):
        svg.text(x + 112, top + a + i * (a + d), line, "note", S["labelDim"])


# ---------------------------------------------------------------- main window (DESIGN-2 4)
W, H = 960, 560


def main_window(skin, cfg, out_svg):
    S, comp, strings, sw = resolve(skin)
    p = 3
    bw, bh = B.BEZEL[p]
    Bt = 46 + bh + 14
    svg = B.Svg(W, H)
    svg.rect(0, 0, W, H, S["panel"])
    nameplate(svg, S, strings, 16, 10, cfg.get("devices"))

    if comp["display"] == "lcd":
        B.draw_display(svg, 16, 46, cfg["lcd1"], cfg["lcd2"], p)
    else:
        draw_led_display(svg, S, comp, sw, 16, 46, p, cfg["readout"], cfg["name"], cfg["context"], cfg["ctx_state"])

    svg.rect(0, Bt, W, H - 22 - Bt, S["band"])
    svg.rect(0, Bt, W, 1, S["bandEdge"])
    col = lambda i: 16 + 50 * i
    if comp["groupSlot"] == "groupBar":
        section_rule(svg, S, comp["sectionRule"], 16, Bt + 12, 394, strings["groupTitle"])
        for i in range(8):
            panel_button(svg, S, comp, col(i), Bt + 28, "B", lit=(f"U{i + 1}" == cfg["group"]), legend=f"U{i + 1}")
        for i in range(6):
            panel_button(svg, S, comp, col(i), Bt + 56, "B", lit=(f"P{i + 1}" == cfg["group"]), legend=f"P{i + 1}")
    elif comp["groupSlot"] == "engine":
        engine_control(svg, S, comp, strings, 16, Bt + 12, cfg["engine"], cfg.get("armed_engine"))
    G = Bt - (80 if cfg.get("grid_up") else 0)  # owner alternative for the JP-08 (DESIGN-2 4.3)
    for title, ry, ny, by, curv, faces in ((strings["bankTitle"], 92, 106, 119, cfg["bank"], "bank"),
                                           (strings["numberTitle"], 155, 169, 182, cfg["num"], "button")):
        section_rule(svg, S, comp["sectionRule"], 16, G + ry, 394, title)
        for i in range(8):
            svg.text_centred(col(i) + 22, G + ny + 9, str(i + 1), "panelLabel", S["labelNumeral"])
            panel_button(svg, S, comp, col(i), G + by, "A", faces=faces, lit=(i + 1 == curv))
    for i, (lab, wd) in enumerate((("PREV", "left"), ("NEXT", "right"))):
        svg.text_centred(col(i) + 22, G + 218 + 9, lab, "panelLabel", S["labelPrimary"])
        panel_button(svg, S, comp, col(i), G + 231, "D", faces="transport", wedge=wd)

    X, R = 434, W - 450
    view = cfg.get("view", "ALL")
    controls = [(X + 0, "A", "ALL", "labelPrimary", view == "ALL", "view"),
                (X + 72, "A", "FAVOURITES", "labelPrimary", view == "FAVOURITES", "view"),
                (X + 150, "A", "RECENTS", "labelPrimary", view == "RECENTS", "view"),
                (X + R - 124, "A", "AUTO SEND", "labelCommit", cfg.get("autosend", False), "commit"),
                (X + R - 44, "C", "SYSTEM", "labelPrimary", False, "transport")]
    for fx, kind, lab, colr, lit, faces in controls:
        svg.text_centred(fx + 22, Bt + 15 + 9, lab, "panelLabel", S[colr])
        panel_button(svg, S, comp, fx, Bt + 28, kind, faces=faces, lit=lit)

    sy = Bt + 64
    svg.rect(X, sy, R, 32, S["fieldBack"])
    fb = vcb(sy + 2, 28, "field")
    tx = X + 13
    q = cfg.get("query", "")
    if cfg.get("focus") == "search":
        svg.frame(X, sy, R, 32, 2, S["focusRing"])
        svg.text(tx, fb, q, "field", S["labelPrimary"])
        a, d = B.ascent_descent("field")
        svg.rect(tx + round(advance(q, "field")), fb - a, 1, a + d, S["labelPrimary"])
    else:
        svg.frame(X, sy, R, 32, 1, S["wellEdgeDark"], S["wellEdgeDark"], S["fieldBorder"], S["fieldBorder"])
        if q:
            svg.text(tx, fb, q, "field", S["labelPrimary"])
        else:
            svg.text(tx, fb, strings["searchPlaceholder"], "field", S["labelDim"])

    wy = Bt + 104
    wh = H - 22 - Bt - 116
    list_well(svg, S, comp, X, wy, R, wh, cfg["rows"], cfg["first"], cfg["highlighted"], cfg.get("focus") == "list",
              cfg["cur_key"], cfg.get("stars", set()), q.lower(), cfg.get("engine"), cfg.get("remembered_other"),
              cfg.get("armed_key"), cfg.get("user"))

    # status bar (DESIGN 8.1, per current device)
    sy = H - 22
    svg.rect(0, sy, W, 22, S["panel"])
    svg.rect(0, sy, W, 1, S["bandEdge"])
    slit(svg, S, 16, sy + 9, 8, 4, False, core=False)
    bl = vcb(sy, 22, "statusLabel")
    x = 30

    def item(x, s, role, colour, gap):
        svg.text(x, bl, s, role, colour)
        return x + round(advance(s, role)) + gap

    st = cfg["status"]
    x = item(x, "OUT", "statusLabel", S["labelSecondary"], 6)
    if st.get("out_missing"):
        x = item(x, st["out"], "statusValue", S["labelPrimary"], 6)
        x = item(x, "NOT FOUND", "statusLabel", S["labelRed"], 16)
    else:
        x = item(x, st["out"], "statusValue", S["labelPrimary"], 16)
    x = item(x, "CH", "statusLabel", S["labelSecondary"], 6)
    x = item(x, str(st["ch"]), "statusValue", S["labelPrimary"], 16)
    x = item(x, "IN", "statusLabel", S["labelSecondary"], 6)
    if st.get("in"):
        x = item(x, st["in"], "statusValue", S["labelPrimary"], 16)
        if st.get("follow"):
            x = item(x, "FOLLOW", "statusLabel", S["labelPrimary"], 16)
    else:
        x = item(x, "None", "statusValue", S["labelDim"], 16)
    svg.write(out_svg)


# ---------------------------------------------------------------- the four main frames (DESIGN-2 10)
def frame_jp08_grid_up(path):
    frame_jp08(path, grid_up=True)


def frame_jp08(path, grid_up=False):
    rows = jp08_rows()
    cur = (None, "3-5")
    first = next(i for i, r in enumerate(rows) if r["addr"] == "2-7")
    cfg = dict(readout="35", name="Lo Brass", context="JP-08", ctx_state="idle", bank=3, num=5,
               rows=rows, first=first, highlighted=first + 5, focus="list", cur_key=cur,
               stars={(None, "3-8"), (None, "4-5")}, user={(None, "3-4"): "CN Strings Lift"},
               status=dict(out="Hapax MIDI 1", ch=5, **{"in": "Hapax MIDI 1"}, follow=True), grid_up=grid_up)
    main_window("jp08", cfg, path)


def frame_ju60(path):
    rows = [r for r in ju06a_rows() if "pad" in r["name"].lower()]
    cfg = dict(readout="4-5", name="LD Silo Lead", context=f"{len(rows)} MATCHES", ctx_state="matches",
               bank=4, num=5, engine=60, rows=rows, first=0, highlighted=0, focus="search", query="pad",
               cur_key=(60, "4-5"), remembered_other=(106, "7-1"), stars={(106, "7-3")},
               status=dict(out="JU-06A", ch=4, **{"in": "JU-06A"}, follow=True))
    main_window("ju06a-60", cfg, path)
    return len(rows)


def frame_ju106(path):
    rows = [r for r in ju06a_rows() if "bass" in r["name"].lower()]
    armed = (60, "3-6")
    cfg = dict(readout="1-1", name="Brass", context="FLIP SWITCH TO 60", ctx_state="prompt",
               bank=1, num=1, engine=106, armed_engine=60, rows=rows,
               first=0, highlighted=next(i for i, r in enumerate(rows) if (r["engine"], r["addr"]) == armed),
               focus="list", query="bass", cur_key=(106, "1-1"), remembered_other=(60, "4-5"), armed_key=armed,
               stars={(106, "5-6")}, user={(106, "5-2"): "CN Sub Drone"},
               status=dict(out="JU-06A", ch=4, **{"in": "JU-06A"}, follow=True))
    main_window("ju06a-106", cfg, path)
    return len(rows)


def frame_devices(path):
    rows = [dict(r, engine=None) for r in B.catalogue()]
    cur_i = next(i for i, r in enumerate(rows) if r["addr"] == "P1-11")
    cfg = dict(lcd1=B.line1("P1-11", "Fantasia", "D"), lcd2=B.line2_group("P1", "D"), group="P1", bank=1, num=1,
               rows=rows, first=cur_i - 3, highlighted=cur_i, focus="list", cur_key=(None, "P1-11"),
               stars={(None, "P1-12"), (None, "P1-16")},
               devices=[("D-05", True, True), ("JP-08", False, True), ("JU-06A", False, False)],
               status=dict(out="Hapax MIDI 1", ch=1, **{"in": None}))
    main_window("d05", cfg, path)


# ---------------------------------------------------------------- state sheets
def led_states(path):
    """Every display state of the LED slot (DESIGN-2 4.2 states table), JU-06A 106 and JP-08."""
    states = [
        ("ju06a-106", "1-1", "Brass", "JU-06A  JUNO-106", "idle", False, "Normal, idle"),
        ("ju06a-106", "1-1", "Brass", "SENT", "sent", False, "Sent flash, 400 ms"),
        ("ju06a-106", "1-1", "Brass", "7 MATCHES", "matches", False, "Search active"),
        ("ju06a-106", "1-1", "Brass", "FLIP SWITCH TO 60", "prompt", False, "Armed, input and follow on"),
        ("ju06a-106", "1-1", "Brass", "FLIP TO 60, THEN CTRL+E", "prompt", False, "Armed, no input path"),
        ("ju06a-60", "4-5", "LD Silo Lead", "FLIPPED TO 106? CTRL+E", "hint", False, "Hint"),
        ("ju06a-60", "---", "", "JU-06A  JUNO-60", "idle", False, "No patch on this engine"),
        ("ju06a-106", "1-1", "Brass", "NO MIDI OUTPUT", "nooutput", False, "Disconnected"),
        ("ju06a-106", "4-_", "", "JU-06A  JUNO-106", "idle", False, "Entry echo after the bank digit"),
        ("jp08", "71", "Big Saw Lead", "JP-08", "idle", False, "JP-08, two cells"),
        ("jp08", "4_", "", "JP-08", "idle", False, "JP-08, entry echo after the bank digit"),
        ("ju06a-60", "---", "", "OPEN SETTINGS: CTRL+,", "firstrun", False, "Enabled, no output port saved yet"),
    ]
    bw, bh = B.BEZEL[3]
    rowh = bh + 34
    svg = B.Svg(bw + 32, 16 + rowh * len(states))
    svg.rect(0, 0, svg.w, svg.h, "#0B0C0D")
    for i, (skin, ro, name, ctx, cst, dot, label) in enumerate(states):
        S, comp, strings, sw = resolve(skin)
        y = 16 + i * rowh
        svg.rect(0, y - 6, svg.w, rowh - 4, S["panel"])
        svg.text(16, y + 8, f"{skin}: {label}".upper(), "panelLabel", S["labelSecondary"])
        draw_led_display(svg, S, comp, sw, 16, y + 16, 3, ro, name, ctx, cst, dot)
    svg.write(path)
    return svg.w


def engine_states(path):
    rows = [("ju06a-60", 60, None, None, "60 believed"), ("ju06a-106", 106, None, None, "106 believed"),
            ("ju06a-106", 106, 60, None, "106, armed for 60 (or hint: flipped to 60?)"),
            ("ju06a-60", 60, None, 106, "60, keyboard focus on 106")]
    svg = B.Svg(394 + 32, 16 + 96 * len(rows))
    svg.rect(0, 0, svg.w, svg.h, "#0B0C0D")
    for i, (skin, eng, armed, focus, label) in enumerate(rows):
        S, comp, strings, sw = resolve(skin)
        y = 16 + 96 * i
        svg.rect(0, y - 6, svg.w, 90, S["band"])
        svg.text(16, y + 6, label.upper(), "panelLabel", S["labelSecondary"])
        engine_control(svg, S, comp, strings, 16, y + 14, eng, armed, focus)
    svg.write(path)
    return svg.w


# ---------------------------------------------------------------- Settings (DESIGN-2 7)
NOTES = {
    "d05": ("The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also "
            "has a Bank.S switch, turn it ON too."),
    "jp08": ("Set the JP-08's MIDI channel with [MANUAL] + [2], then [1] to [16]. The JP-08 has no "
             "program change switch: the channel is the only setting that matters."),
    "ju06a": ("Set the JU-06A's MIDI channel: hold ARPEGGIO [ON/OFF], press [2], turn [VALUE]. The app "
              "cannot read the 60/106 switch. When you flip it, the app follows if it sees the flip on "
              "the input; otherwise press Ctrl+E."),
}


def settings(out_svg, skin="ju06a-106"):
    S, comp, strings, sw = resolve(skin)
    a, d = B.ascent_descent("note")
    lh = a + d
    note_lines = {k: B.wrap(v, "note", 560) for k, v in NOTES.items()}
    ju_note_y = 350 + 20
    page_h = max(ju_note_y + len(note_lines["ju06a"]) * lh, 262 + len(note_lines["d05"]) * lh)
    glob_y = page_h + 24
    btn_y = glob_y + 48 + 20 + 15 + 20  # second checkbox row, its note line, 20 px gap
    Hd = 20 + btn_y + 28 + 20
    svg = B.Svg(760, Hd)
    svg.rect(0, 0, 760, Hd, S["panel"])
    ox, oy = 20, 20

    # device list well (7.1)
    lx, ly, lw, lhh = ox, oy, 140, 134
    svg.rect(lx, ly, lw, lhh, S["well"])
    svg.frame(lx, ly, lw, lhh, 1, S["wellEdgeDark"], S["wellEdgeDark"], S["wellEdgeLight"], S["wellEdgeLight"])
    devices = [("D-05", True, "Connected", "ok"), ("JP-08", False, "Off", "off"), ("JU-06A", True, "Connected", "ok")]
    for i, (name, enabled, state, kind) in enumerate(devices):
        ry = ly + 1 + 44 * i
        if i == 2:
            svg.rect(lx + 1, ry, 138, 44, S["selection"])
            svg.rect(lx + 1, ry, 3, 44, S["selectionBarIdle"])
        ix = lx + 1
        cy = ry + 10
        if enabled:
            svg.rect(ix + 10, cy, 16, 6, S["ledOn"])
        else:
            svg.rect(ix + 10, cy, 16, 6, S["ledOff"])
            svg.frame(ix + 10, cy, 16, 6, 1, S["fieldBorderHover"])
        svg.text(ix + 36, ry + 19, name, "body", S["labelPrimary"] if enabled else S["labelSecondary"])
        if kind != "off":
            slit(svg, S, ix + 36, ry + 29, 10, 4, kind == "ok", core=False)
            svg.text(ix + 52, ry + 34, state, "note", S["labelSecondary"] if kind == "ok" else S["labelRed"])
        else:
            svg.text(ix + 36, ry + 34, state, "note", S["labelDim"])

    px = ox + 160

    def form_label(y, h, s):
        svg.text_right(px + 100, vcb(oy + y, h, "panelLabel"), s, "panelLabel", S["labelPrimary"])

    def combo(x, y, w, s, focused=False):
        x, y = px + x, oy + y
        svg.rect(x, y, w, 26, S["fieldBack"])
        svg.frame(x, y, w, 26, 2 if focused else 1, S["focusRing"] if focused else S["fieldBorder"])
        svg.text(x + 9, vcb(y, 26, "body"), s, "body", S["labelPrimary"])
        ax, ay = x + w - 1 - 20 + 6, y + 11
        for i, wd in enumerate((8, 6, 4, 2)):
            svg.rect(ax + i, ay + i, wd, 1, S["arrow"])

    def button(x, y, w, h, s, default=False, disabled=False, absolute=False):
        if not absolute:
            x, y = px + x, oy + y
        svg.rrect(x, y, w, h, 2, S["labelRed"] if default else S["buttonSlot"])
        svg.rrect(x + 1, y + 1, w - 2, h - 2, 1, S["buttonDisabled"] if disabled else S["controlFace"])
        svg.text_centred(x + w / 2, vcb(y, h, "panelLabel"), s, "panelLabel",
                         S["labelDisabled"] if disabled else S["labelPrimary"])

    def checkbox(x, y, s, checked, disabled=False):
        x, y = x, oy + y
        iy = y + 7
        if disabled:
            svg.rect(x, iy, 16, 6, S["buttonDisabled"]); svg.frame(x, iy, 16, 6, 1, S["fieldBorder"])
        elif checked:
            svg.rect(x, iy, 16, 6, S["ledOn"])
        else:
            svg.rect(x, iy, 16, 6, S["ledOff"]); svg.frame(x, iy, 16, 6, 1, S["fieldBorderHover"])
        svg.text(x + 26, vcb(y, 20, "body"), s, "body", S["labelDisabled"] if disabled else S["labelPrimary"])

    secondary_rule(svg, S, px, oy + 0, 560, "MIDI OUTPUT")
    form_label(20, 26, "PORT")
    combo(112, 20, 260, "JU-06A · detected", focused=True)
    button(380, 20, 72, 26, "USE", disabled=True)
    button(460, 20, 100, 26, "REFRESH")
    form_label(54, 26, "CHANNEL")
    sx, sy = px + 112, oy + 54
    svg.rect(sx, sy, 72, 26, S["fieldBack"]); svg.frame(sx, sy, 72, 26, 1, S["fieldBorder"])
    svg.text_centred(sx + 36, vcb(sy, 26, "body"), "4", "body", S["labelPrimary"])
    slit(svg, S, px + 112, oy + 88 + 8, 10, 4, True, core=False)
    svg.text(px + 130, vcb(oy + 88, 20, "body"), "Connected to JU-06A", "body", S["labelPrimary"])
    secondary_rule(svg, S, px, oy + 124, 560, "MIDI INPUT")
    form_label(144, 26, "PORT")
    combo(112, 144, 260, "JU-06A · detected")
    button(380, 144, 72, 26, "USE", disabled=True)
    checkbox(px + 112, 178, "Follow the panel", True)
    checkbox(px + 112, 206, "Resend the current patch on connect", False)
    secondary_rule(svg, S, px, oy + 242, 560, "ENGINE")
    for y, lab, val, extra in ((262, "BELIEVED", "106", "Ctrl+E or the 60 / 106 buttons change it"),
                               (288, "ON 60", "4-5", "LD Silo Lead"), (314, "ON 106", "1-1", "Brass")):
        form_label(y, 20, lab)
        bl = vcb(oy + y, 20, "body")
        svg.text(px + 112, bl, val, "body", S["labelPrimary"])
        svg.text(px + 112 + 48, bl, extra, "body" if lab != "BELIEVED" else "note",
                 S["labelPrimary"] if lab != "BELIEVED" else S["labelDim"])
    secondary_rule(svg, S, px, oy + 350, 560, "ON THE JU-06A")
    for i, ln in enumerate(note_lines["ju06a"]):
        svg.text(px, oy + ju_note_y + i * lh + a, ln, "note", S["labelSecondary"])

    secondary_rule(svg, S, ox, oy + glob_y, 720, "ALL DEVICES")
    checkbox(ox + 112 + 160, glob_y + 20, "Auto-send on highlight", False)
    checkbox(ox + 112 + 160, glob_y + 48, "Follow switches device", True)
    svg.text(ox + 112 + 160 + 26, oy + glob_y + 48 + 20 + a, "An incoming patch change from another device switches the window to it.",
             "note", S["labelDim"])
    button(ox, oy + btn_y, 100, 28, "TEST", absolute=True)
    button(ox + 720 - 208, oy + btn_y, 100, 28, "CANCEL", absolute=True)
    button(ox + 720 - 100, oy + btn_y, 100, 28, "OK", default=True, absolute=True)
    svg.write(out_svg)
    return dict(height=Hd, client=(760, Hd), page_h=page_h, glob_y=glob_y, btn_y=btn_y,
                notes={k: len(v) for k, v in note_lines.items()})


# ---------------------------------------------------------------- measurements for DESIGN-2
def measurements():
    m = {}
    for n in ("D-05", "JP-08", "JU-06A"):
        m[f"tab {n}"] = round(advance(n, "switcherTab"), 1)
    m["wordmark PATCH SWITCHER"] = round(advance("PATCH SWITCHER", "wordmark"), 1)
    for s in CAPTION:
        m[f"caption {s!r}"] = round(advance(s, "note"), 1)
    for s in ("FLIP TO 106, THEN CTRL+E", "FOUND JU-06A ON USB: OPEN SETTINGS", "FLIPPED TO 106? CTRL+E"):
        for p in (3, 4, 5):
            m[f"context{p} {s}"] = round(advance(s, f"displayContext{p}"), 1)
    for s in ("Alpha Omega Ensemble", "Space Boomerang", "Brass & Strings", "WWWWWWWWWWWWWWWWWW"):
        for p in (3, 4, 5):
            m[f"name{p} {s}"] = round(advance(s, f"displayName{p}"), 1)
    for s in ("ENGINE", "BANK", "NUMBER", "PATCH", "PATCH BANK"):
        m[f"ruleTitle {s}"] = round(advance(s, "ruleTitle"), 1)
    m["106 panelLabel"] = round(advance("106", "panelLabel"), 1)
    m["106 listMeta"] = round(advance("106", "listMeta"), 1)
    m["JU-06A · detected body"] = round(advance("JU-06A · detected", "body"), 1)
    for s in ("Port not found", "No output port", "Connected"):
        m[f"note {s}"] = round(advance(s, "note"), 1)
    m["note follow"] = round(advance("An incoming patch change from another device switches the window to it.", "note"), 1)
    for p in (3, 4, 5):
        m[f"capHeight displayName{p}"] = cap_height(f"displayName{p}")
        m[f"capHeight displayContext{p}"] = cap_height(f"displayContext{p}")
    return m


# ---------------------------------------------------------------- render
def main():
    tmp = tempfile.mkdtemp(prefix="bps-mockup-fc-")
    conf = os.path.join(tmp, "fonts.conf")
    with open(conf, "w") as f:
        f.write('<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig>'
                f'<dir>{B.esc(B.FONTS)}</dir><cachedir>{B.esc(tmp)}/cache</cachedir></fontconfig>')
    env = dict(os.environ, FONTCONFIG_FILE=conf, HOME=tmp)
    design = os.path.join(ROOT, "docs", "design")
    jobs = []
    for name, fn in (("jp08", frame_jp08), ("ju06a-60", frame_ju60), ("ju06a-106", frame_ju106),
                     ("devices", frame_devices)):
        s = os.path.join(HERE, f"main-{name}.svg")
        info = fn(s)
        print(name, info or "")
        jobs.append((s, os.path.join(design, f"mockup-{name}"), W))
    s = os.path.join(HERE, "jp08-grid-up.svg")
    frame_jp08_grid_up(s)
    jobs.append((s, os.path.join(HERE, "jp08-grid-up"), W))
    s = os.path.join(HERE, "settings-devices.svg")
    info = settings(s)
    print("settings:", info)
    jobs.append((s, os.path.join(design, "mockup-settings-devices"), 760))
    s = os.path.join(HERE, "led-states.svg")
    jobs.append((s, os.path.join(HERE, "led-states"), led_states(s)))
    s = os.path.join(HERE, "engine-states.svg")
    jobs.append((s, os.path.join(HERE, "engine-states"), engine_states(s)))
    for k, v in measurements().items():
        print(f"  measure {k}: {v}")
    for svg_path, base, w in jobs:
        B.render(svg_path, base + ".png", w, env)
        B.render(svg_path, base + "@2x.png", 2 * w, env)
        print("wrote", base + ".png", "and @2x")


if __name__ == "__main__":
    sys.exit(main())
