#!/usr/bin/env python3
"""Builds the W2 web mockups from docs/design/DESIGN-WEB.md and docs/design/tokens.json.

Usage (from anywhere):  python3 docs/design/mockup-web/build.py [--print-geometry]

Writes one SVG per frame and per device pixel ratio into docs/design/mockup-web/ and
renders each with Inkscape (private fontconfig that sees only assets/fonts):

  desktop-*.png        960 x 560    (@2x: 1920 x 1120)
  portrait-*.png       390 x 844    (@2x)
  landscape-*.png      844 x 390    (@2x)
  landscape-compact    800 x 300    (@2x)

The LCD geometry depends on the device pixel ratio (DESIGN.md 5.1, DESIGN-WEB.md 5), so
the 1x and 2x frames are separate SVGs, each drawn in CSS px with the dot grid of that
DPR, and exported at width x DPR. Colours come from tokens.json; drawing helpers, text
measurement (Pillow + raqm on the committed Inter files) and the Appendix A glyph table
come from docs/design/mockup/build.py, which this script imports.

--print-geometry prints the layout tables quoted in DESIGN-WEB.md.
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.dont_write_bytecode = True  # no __pycache__ beside the desktop mockup script
sys.path.insert(0, os.path.join(ROOT, "docs", "design", "mockup"))
import build as M  # noqa: E402  (desktop mockup helpers)

TOK = json.load(open(os.path.join(ROOT, "docs", "design", "tokens.json"), encoding="utf-8"))
T = {k: v["hex"] for k, v in TOK["base"]["colors"].items() if v["group"] in ("2.1", "2.2", "2.3", "2.4")}  # version 1 subset
assert {k: v.upper() for k, v in T.items()} == {k: v.upper() for k, v in M.T.items()}, \
    "tokens.json and mockup/build.py differ: run docs/design/gen-tokens.py --check"
for name, r in TOK["webType"].items():
    if not name.startswith("_"):
        M.ROLES[name] = (r["weight"], r["size"], r["spacing"])
SZ = TOK["size"]["phone"]
BP = TOK["breakpoints"]
SCRIM = TOK["webColors"]["scrim"]


def rnd(x):  # round half up, as Math.round and qRound
    return int(math.floor(x + 0.5))


# ------------------------------------------------------------------ LCD geometry (DESIGN-WEB 5)
def lcd(cols, P, dpr, pad_x, pad_y):
    g = max(1, min(rnd(dpr), P // 3))
    fw = math.ceil((6 * cols + 3) * P / dpr - 1e-9)
    fh = math.ceil(19 * P / dpr - 1e-9)
    return dict(cols=cols, P=P, g=g, d=P - g, dpr=dpr, fieldW=fw, fieldH=fh,
                bezelW=fw + 2 * pad_x, bezelH=fh + 2 * pad_y, padX=pad_x, padY=pad_y)


def pitch_fit(avail_css, cols, dpr, cap_css=None):
    P = int(math.floor(dpr * avail_css / (6 * cols + 3) + 1e-9))
    if cap_css is not None:
        P = min(P, int(math.floor(cap_css * dpr + 1e-9)))
    return max(P, 1)


def desktop_lcd(W, dpr):
    p = 3 if W < 1042 else 4 if W < 1285 else 5
    T_ = 243 * p + 38
    Pa = int(math.floor(p * dpr + 1e-9))
    P = Pa
    if p * dpr - Pa >= 1e-9:
        Pb = Pa + 1
        bw = lambda P_: math.ceil(243 * P_ / dpr - 1e-9) + 38
        if bw(Pb) + 32 <= W and abs(bw(Pb) - T_) < abs(bw(Pa) - T_):
            P = Pb
    return lcd(40, P, dpr, 19, 11)


def draw_lcd(svg, x0, y0, geo, l1, l2, ghost=True):
    bw, bh = geo["bezelW"], geo["bezelH"]
    svg.rect(x0, y0, bw, bh, T["bezel"])
    svg.rect(x0, y0, bw, 1, T["bezelRimTop"])
    svg.rect(x0, y0 + 1, 1, bh - 1, T["bezelRim"])
    svg.rect(x0 + bw - 1, y0 + 1, 1, bh - 1, T["bezelRim"])
    svg.rect(x0 + 1, y0 + bh - 1, bw - 2, 1, T["bezelRim"])
    fx, fy = x0 + geo["padX"], y0 + geo["padY"]
    svg.rect(fx, fy, geo["fieldW"], geo["fieldH"], T["lcdBack"])
    P, d, dpr, cols = geo["P"], geo["d"], geo["dpr"], geo["cols"]
    # dot area (6 cols - 1) P x 15 P device px, centred in the field's device box
    dev_w, dev_h = rnd(geo["fieldW"] * dpr), rnd(geo["fieldH"] * dpr)
    ox = (dev_w - (6 * cols - 1) * P) // 2
    oy = (dev_h - 15 * P) // 2
    on, off = [], []
    for li, line in enumerate((l1, l2)):
        assert len(line) == cols, (len(line), line)
        for k, ch in enumerate(line):
            code = ord(ch) if 0x20 <= ord(ch) <= 0x7E else ord("?")
            rows = M.GLYPHS[code]
            cx, cy = ox + 6 * P * k, oy + 8 * P * li
            for r in range(7):
                for c in range(5):
                    (on if rows[r] & (0x10 >> c) else off).append((cx + c * P, cy + r * P))
    s = 1.0 / dpr
    path = lambda pts: "".join(f"M{x},{y}h{d}v{d}h-{d}z" for x, y in pts)
    svg.add(f'<g transform="translate({fx:g},{fy:g}) scale({s:.6f})">')
    if ghost:
        svg.add(f'<path d="{path(off)}" fill="{T["lcdGhost"]}" shape-rendering="crispEdges"/>')
    svg.add(f'<path d="{path(on)}" fill="{T["lcdOn"]}" shape-rendering="crispEdges"/>')
    svg.add("</g>")


# ------------------------------------------------------------------ display text
MODE = {"W": "WHOLE", "D": "DUAL", "S": "SPLIT", "": ""}


def phone_lines(addr, name, mode, context=None):
    """DESIGN-WEB 5.3: line 1 the name (20); line 2 address, space, context (14)."""
    l1 = name[:20].ljust(20)
    ctx = MODE[mode] if context is None else context
    assert len(ctx) <= 14, ctx
    return l1, (addr + " " + ctx).ljust(20)


def desktop_lines(addr, name, mode, line2):
    return M.line1(addr, name, mode), line2.ljust(40)[:40]


# ------------------------------------------------------------------ parts
def face(svg, x, y, w, h, state="normal", lit=False):
    """Panel button face of any size (DESIGN 6.2 layers). Returns content dy (pressed)."""
    if state == "disabled":
        f = T["buttonDisabled"]
    elif state == "pressed":
        f = T["buttonPressed"]
    elif state == "hover":
        f = T["buttonFaceHover"]
    else:
        f = T["buttonFaceLit"] if lit else T["buttonFace"]
    svg.rrect(x - 1, y - 1, w + 2, h + 2, 3, T["buttonSlot"])
    svg.rrect(x, y, w, h, 2, f)
    if state not in ("pressed", "disabled"):
        svg.rect(x + 2, y + 1, w - 4, 1, T["buttonHighlight"])
    return 1 if state == "pressed" else 0


def pad(svg, x, y, w, h, legend="", lit=False, state="normal", legend_colour=None, led_h=None):
    """Phone pad (DESIGN-WEB 7.1): kind B when legend, else kind A."""
    dy = face(svg, x, y, w, h, state, lit)
    on = lit and state != "disabled"
    if legend:
        top = (h - 23) // 2
        col = T["labelDisabled"] if state == "disabled" else (legend_colour or T["labelPrimary"])
        svg.text_centred(x + w / 2, y + top + 12 + dy, legend, "padLabel", col)
        M.led(svg, x + w // 2 - 8, y + top + 19 + dy, 16, led_h or 4, on)
    else:
        M.led(svg, x + w // 2 - 8, y + h // 2 - 2 + dy, 16, 4, on)


def pad_plain(svg, x, y, w, h, legend, state="normal"):
    dy = face(svg, x, y, w, h, state)
    col = T["labelDisabled"] if state == "disabled" else T["labelPrimary"]
    svg.text_centred(x + w / 2, M.vcentre_baseline(y + dy, h, "padLabel"), legend, "padLabel", col)


def transport(svg, x, y, w, h, which, state="normal"):
    dy = face(svg, x, y, w, h, state)
    col = T["labelDisabled"] if state == "disabled" else T["arrow"]
    tcol = T["labelDisabled"] if state == "disabled" else T["labelPrimary"]
    lab = "PREV" if which == "prev" else "NEXT"
    tw = round(M.advance(lab, "padLabel"))
    gw = 7 + 8 + tw
    gx = x + (w - gw) // 2
    cy = y + h // 2 + dy
    bl = M.vcentre_baseline(y + dy, h, "padLabel")
    if which == "prev":
        svg.poly([(gx, cy), (gx + 7, cy - 4), (gx + 7, cy + 4)], col)
        svg.text(gx + 15, bl, lab, "padLabel", tcol)
    else:
        svg.text(gx, bl, lab, "padLabel", tcol)
        wx = gx + tw + 8
        svg.poly([(wx + 7, cy), (wx, cy - 4), (wx, cy + 4)], col)


def well(svg, x, y, w, h, focused=False):
    svg.rect(x, y, w, h, T["well"])
    if focused:
        svg.frame(x, y, w, h, 1, T["focusRing"])
    else:
        svg.frame(x, y, w, h, 1, T["wellEdgeDark"], T["wellEdgeDark"], T["wellEdgeLight"], T["wellEdgeLight"])


def field(svg, x, y, w, h, text, placeholder=False, focused=False):
    svg.rect(x, y, w, h, T["fieldBack"])
    if focused:
        svg.frame(x, y, w, h, 2, T["focusRing"])
    else:
        svg.frame(x, y, w, h, 1, T["wellEdgeDark"], T["wellEdgeDark"], T["fieldBorder"], T["fieldBorder"])
    bl = M.vcentre_baseline(y + 1, h - 2, "field")
    svg.text(x + 13, bl, text, "field", T["labelDim"] if placeholder else T["labelPrimary"])
    if focused:
        a, d = M.ascent_descent("field")
        cx = x + 13 + round(M.advance(text, "field"))
        svg.rect(cx, bl - a, 1, a + d, T["labelPrimary"])


def dialog_button(svg, x, y, w, h, s, default=False, disabled=False, red=False, role="panelLabel"):
    svg.rrect(x, y, w, h, 2, T["labelRed"] if default else T["buttonSlot"])
    svg.rrect(x + 1, y + 1, w - 2, h - 2, 1, T["buttonDisabled"] if disabled else T["buttonFace"])
    tc = T["labelDisabled"] if disabled else (T["labelRed"] if red else T["labelPrimary"])
    svg.text_centred(x + w / 2, M.vcentre_baseline(y, h, role), s, role, tc)


def star(svg, cx, cy, on, ro=6.0, ri=2.6):
    pts = M.star_points(cx, cy, ro, ri)
    if on:
        svg.poly(pts, T["labelPrimary"], crisp=False)
    else:
        pp = " ".join(f"{a:g},{b:g}" for a, b in pts)
        svg.add(f'<polygon points="{pp}" fill="none" stroke="{T["starOff"]}" stroke-width="1"/>')


def list_rows(svg, ix, iy, iw, ih, rows, row_h, cur, stars, highlighted=None, focused=False,
              big=False, clip_id="well"):
    svg.defs.append(f'<clipPath id="{clip_id}"><rect x="{ix}" y="{iy}" width="{iw}" height="{ih}"/></clipPath>')
    svg.add(f'<g clip-path="url(#{clip_id})">')
    Wl = iw
    off = (row_h - 28) // 2
    for vi, r in enumerate(rows[: (ih + row_h - 1) // row_h]):
        ry = iy + vi * row_h
        is_cur = r["addr"] == cur
        if highlighted is not None and vi == highlighted:
            svg.rect(ix, ry, Wl, row_h, T["selection"])
            svg.rect(ix, ry, 3, row_h, T["focusRing"] if focused else T["selectionBarIdle"])
        if is_cur:
            M.led(svg, ix + 10, ry + off + 12, 10, 4, True)
        acol = T["labelPrimary"] if is_cur else T["labelSecondary"]
        for ci, ch in enumerate(r["addr"]):
            svg.text_centred(ix + 28 + 9 * ci + 4.5, ry + off + 19, ch, "listAddress", acol)
        name = r["name"] or r["addr"]
        for st, ln in r.get("ranges", []):
            hx = ix + 84 + round(M.advance(name[:st], "listName"))
            hw = round(M.advance(name[:st + ln], "listName")) - round(M.advance(name[:st], "listName"))
            svg.rect(hx, ry + off + 4, hw, 20, T["matchBack"])
        svg.text(ix + 84, ry + off + 19, name, "listName", T["labelPrimary"] if r["name"] else T["labelDim"])
        star_cx = ix + Wl - (26 if big else 16)
        tag_x = ix + Wl - (88 if big else 52)
        if r["mode"]:
            svg.frame(tag_x, ry + off + 6, 16, 16, 1, T["tagBorder"])
            svg.text_centred(tag_x + 8 + 0.25, M.vcentre_baseline(ry + off + 6, 16, "listMeta"), r["mode"],
                             "listMeta", T["labelSecondary"])
        if big:
            star(svg, star_cx, ry + row_h / 2, r["addr"] in stars, 7.0, 3.0)
        else:
            star(svg, star_cx, ry + off + 14, r["addr"] in stars)
    svg.add("</g>")


def status_bar(svg, W, H, port, connected=True, dim_port=False, show_in=True, ch="1"):
    sy = H - 22
    svg.rect(0, sy, W, 22, T["panel"])
    svg.rect(0, sy, W, 1, T["bandEdge"])
    M.led(svg, 16, sy + 9, 8, 4, False, core=False)
    bl = M.vcentre_baseline(sy, 22, "statusLabel")

    def item(x, s, role, colour, gap):
        svg.text(x, bl, s, role, colour)
        return x + round(M.advance(s, role)) + gap

    x = item(30, "OUT", "statusLabel", T["labelSecondary"], 6)
    if dim_port:
        x = item(x, "None", "statusValue", T["labelDim"], 16)
    elif connected:
        x = item(x, port, "statusValue", T["labelPrimary"], 16)
    else:
        x = item(x, port, "statusValue", T["labelPrimary"], 6)
        x = item(x, "NOT FOUND", "statusLabel", T["labelRed"], 16)
    x = item(x, "CH", "statusLabel", T["labelSecondary"], 6)
    x = item(x, ch, "statusValue", T["labelPrimary"], 16)
    if show_in:
        x = item(x, "IN", "statusLabel", T["labelSecondary"], 6)
        item(x, "None", "statusValue", T["labelDim"], 16)


def wrap(s, role, width):
    return M.wrap(s, role, width)


def block_text(svg, cx, y, s, role, colour, width, centred=True, x_left=None):
    a, d = M.ascent_descent(role)
    lines = wrap(s, role, width)
    for i, ln in enumerate(lines):
        bl = y + i * (a + d) + a
        if centred:
            svg.text_centred(cx, bl, ln, role, colour)
        else:
            svg.text(x_left, bl, ln, role, colour)
    return y + len(lines) * (a + d)


# ------------------------------------------------------------------ data
PRESETS = M.presets()
BY_ADDR = {r["addr"]: r for r in PRESETS}
CUR = BY_ADDR["P1-11"]
FAVS = ["P1-11", "P1-35", "P2-57", "P3-45", "P5-12"]


def brass_matches():
    out = []
    for r in PRESETS:
        low = r["name"].lower()
        i = low.find("brass")
        if i >= 0:
            out.append(dict(r, ranges=[(i, 5)]))
    return out


# ------------------------------------------------------------------ access panel content (DESIGN-WEB 6)
ACCESS = {
    "prompt": dict(
        title="MIDI IS NOT CONNECTED", title_colour="labelSecondary",
        body="The browser asks once for permission to use MIDI devices. Allow it so this page can "
             "change patches on the D-05.",
        button=("CONNECT MIDI", 160, 32),
        note="Only Bank Select and Program Change are sent, never SysEx."),
    "prompt-android": dict(
        title="MIDI IS NOT CONNECTED", title_colour="labelSecondary",
        body="Plug the D-05, or the interface it is on, into this phone with a USB OTG cable. Then "
             "connect and allow MIDI when Chrome asks.",
        button=("CONNECT MIDI", None, 48),
        note="Only Bank Select and Program Change are sent, never SysEx."),
    "denied-chrome": dict(
        title="MIDI ACCESS IS BLOCKED", title_colour="labelRed",
        body="Chrome is blocking MIDI for this page. To allow it:",
        steps=["Click View site information, left of the address.",
               "Choose Site settings.",
               "Set MIDI devices to Allow.",
               "Come back here: the page connects by itself."],
        button=("TRY AGAIN", 120, 28)),
    "unsupported-safari": dict(
        title="NO WEB MIDI IN SAFARI", title_colour="labelSecondary",
        body="Safari does not support Web MIDI, so this page cannot reach the D-05. Open it in "
             "Chrome, Edge, Brave or Firefox.",
        matrix=True),
}
MATRIX = [("Chrome, Edge, Brave, Opera", "Yes"),
          ("Chrome on Android", "Yes, with a USB OTG cable"),
          ("Firefox 108 or later", "Yes, after a site permission"),
          ("Safari, iPhone and iPad", "No")]


def access_content(svg, x, y, w, h, kind, phone=False):
    c = ACCESS[kind]
    bw = w if phone else min(w - 48, 400)
    bx = x + (w - bw) // 2
    cx = bx + bw / 2

    def layout(draw):
        yy = 0

        def at(v):
            return y0 + v

        a, d = M.ascent_descent("emptyTitle")
        if draw:
            svg.text_centred(cx, at(yy) + a, c["title"], "emptyTitle", T[c["title_colour"]])
        yy += a + d + 8
        if draw:
            yy = block_text(svg, cx, at(yy), c["body"], "body", T["labelPrimary"], bw) - y0
        else:
            yy += len(wrap(c["body"], "body", bw)) * 16
        if "steps" in c:
            yy += 12
            sw = max(round(M.advance(s, "body")) for s in c["steps"]) + 20
            sx = bx + (bw - sw) // 2 if not phone else bx
            for i, s in enumerate(c["steps"]):
                if draw:
                    bl = M.vcentre_baseline(at(yy), 20, "body")
                    svg.text(sx, bl, f"{i + 1}.", "body", T["labelSecondary"])
                    svg.text(sx + 20, bl, s, "body", T["labelPrimary"])
                yy += 20
        if "matrix" in c:
            yy += 16
            mx = bx + (bw - 380) // 2
            for left, right in MATRIX:
                if draw:
                    bl = M.vcentre_baseline(at(yy), 20, "body")
                    svg.text(mx, bl, left, "body", T["labelPrimary"])
                    svg.text(mx + 200, bl, right, "body", T["labelSecondary"])
                yy += 20
        if "button" in c:
            lab, bwid, bh = c["button"]
            bwid = bwid or bw
            yy += 20
            if draw:
                dialog_button(svg, round(cx - bwid / 2), at(yy), bwid, bh, lab, default=True,
                              role="padLabel" if phone else "panelLabel")
            yy += bh
        if "note" in c:
            yy += 16
            if draw:
                yy = block_text(svg, cx, at(yy), c["note"], "note", T["labelDim"], bw) - y0
            else:
                yy += len(wrap(c["note"], "note", bw)) * 15
        return yy

    y0 = 0
    total = layout(False)
    y0 = y + max(24, (h - total) // 2)
    layout(True)
    return total


# ------------------------------------------------------------------ desktop (DESIGN.md 4, DESIGN-WEB 4.1)
def desktop(name, W, H, dpr, state):
    """state: access-prompt | access-denied | unsupported | settings"""
    svg = M.Svg(W, H)
    svg.rect(0, 0, W, H, T["panel"])
    M.wordmark(svg, 16, 10)
    geo = desktop_lcd(W, dpr)
    granted = state == "settings"
    l2 = {"access-prompt": "PRESS CONNECT MIDI", "access-denied": "MIDI ACCESS BLOCKED",
          "unsupported": "NO WEB MIDI IN THIS BROWSER", "settings": "PRESET 1: ORIGINAL D-50"}[state]
    l1, l2 = desktop_lines(CUR["addr"], CUR["name"], CUR["mode"], l2)
    draw_lcd(svg, 16, 46, geo, l1, l2)
    B = 46 + geo["bezelH"] + 14
    svg.rect(0, B, W, H - 22 - B, T["band"])
    svg.rect(0, B, W, 1, T["bandEdge"])
    st = "normal" if granted else "disabled"
    col = lambda i: 16 + 50 * i
    M.section_rule(svg, 16, B + 12, 394, "PATCH GROUP")
    for i in range(8):
        dis_button(svg, col(i), B + 28, "B", f"U{i + 1}", False, st)
    for i in range(6):
        dis_button(svg, col(i), B + 56, "B", f"P{i + 1}", f"P{i + 1}" == "P1", st)
    for title, ry, ny, by in (("PATCH BANK", 92, 106, 119), ("PATCH NUMBER", 155, 169, 182)):
        M.section_rule(svg, 16, B + ry, 394, title)
        for i in range(8):
            svg.text_centred(col(i) + 22, B + ny + 9, str(i + 1), "panelLabel", T["labelAccent"])
            dis_button(svg, col(i), B + by, "A", "", i == 0, st)
    for i, (lab, wd) in enumerate((("PREV", "left"), ("NEXT", "right"))):
        svg.text_centred(col(i) + 22, B + 218 + 9, lab, "panelLabel", T["labelPrimary"])
        M.panel_button(svg, col(i), B + 231, "D", wedge=wd, state=st)
    X, R = 434, W - 450
    for fx, kind, lab, colr, lit, enabled in ((X, "A", "ALL", "labelPrimary", True, granted),
                                              (X + 72, "A", "FAVOURITES", "labelPrimary", False, granted),
                                              (X + 150, "A", "RECENTS", "labelPrimary", False, granted),
                                              (X + R - 124, "A", "AUTO SEND", "labelRed", False, granted),
                                              (X + R - 44, "C", "SYSTEM", "labelPrimary", False, True)):
        svg.text_centred(fx + 22, B + 15 + 9, lab, "panelLabel", T[colr])
        dis_button(svg, fx, B + 28, kind, "", lit, "normal" if enabled else "disabled")
    if granted:
        field(svg, X, B + 64, R, 32, "Search names or addresses: brass, P3-45", placeholder=True)
        wy, wh = B + 104, H - 22 - B - 116
        well(svg, X, wy, R, wh)
        rows = PRESETS[:12]
        list_rows(svg, X + 1, wy + 1, R - 2, wh - 2, rows, 28, CUR["addr"], {"P1-12"}, highlighted=0)
        status_bar(svg, W, H, "Boutique MIDI 1", ch="3")
    else:
        py, ph = B + 64, H - 22 - B - 64 - 12
        well(svg, X, py, R, ph)
        kind = {"access-prompt": "prompt", "access-denied": "denied-chrome", "unsupported": "unsupported-safari"}[state]
        access_content(svg, X, py, R, ph, kind)
        status_bar(svg, W, H, "", dim_port=True)
    if state == "settings":
        settings_dialog(svg, W, H)
    return svg


def dis_button(svg, x, y, kind, legend, lit, state):
    M.panel_button(svg, x, y, kind, lit=lit and state != "disabled", legend=legend, state=state)


def settings_dialog(svg, W, H):
    svg.add(f'<rect x="0" y="0" width="{W}" height="{H}" fill="{T[SCRIM["base"]]}" fill-opacity="{SCRIM["alpha"]}"/>')
    note = ("The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also "
            "has a Bank.S switch, turn it ON too.")
    nlines = wrap(note, "note", 480)
    btn_y = 318 + len(nlines) * 15 + 20
    content_h = btn_y + 28
    dw, th = 520, TOK["size"]["desktop"]["dialogTitleHeight"]
    dh = th + 20 + content_h + 20
    dx, dy = (W - dw) // 2, max(8, (H - dh) // 2)
    svg.rect(dx, dy, dw, dh, T["panel"])
    svg.frame(dx, dy, dw, dh, 1, T["fieldBorder"])
    svg.rect(dx + 1, dy + th - 1, dw - 2, 1, T["bandEdge"])
    svg.text(dx + 20, M.vcentre_baseline(dy, th - 1, "dialogTitle"), "SETTINGS", "dialogTitle", T["labelPrimary"])
    svg.text_right(dx + dw - 20, M.vcentre_baseline(dy, th - 1, "panelLabel"), "CLOSE", "panelLabel",
                   T["labelSecondary"])
    ox, oy = dx + 20, dy + th + 20

    def form_label(yy, h, s):
        svg.text_right(ox + 100, M.vcentre_baseline(oy + yy, h, "panelLabel"), s, "panelLabel", T["labelPrimary"])

    def combo(x, yy, w, s, focused=False):
        x, yy = ox + x, oy + yy
        svg.rect(x, yy, w, 26, T["fieldBack"])
        if focused:
            svg.frame(x, yy, w, 26, 2, T["focusRing"])
        else:
            svg.frame(x, yy, w, 26, 1, T["fieldBorder"])
        svg.text(x + 9, M.vcentre_baseline(yy, 26, "body"), s, "body", T["labelPrimary"])
        ax, ay = x + w - 1 - 20 + 6, yy + 11
        for i, wd in enumerate((8, 6, 4, 2)):
            svg.rect(ax + i, ay + i, wd, 1, T["arrow"])

    def checkbox(yy, s, checked, disabled=False):
        x, yy = ox + 112, oy + yy
        iy = yy + 7
        if disabled:
            svg.rect(x, iy, 16, 6, T["buttonDisabled"])
            svg.frame(x, iy, 16, 6, 1, T["fieldBorder"])
        elif checked:
            svg.rect(x, iy, 16, 6, T["ledOn"])
        else:
            svg.rect(x, iy, 16, 6, T["ledOff"])
            svg.frame(x, iy, 16, 6, 1, T["fieldBorderHover"])
        svg.text(x + 26, M.vcentre_baseline(yy, 20, "body"), s, "body",
                 T["labelDisabled"] if disabled else T["labelPrimary"])

    M.section_rule(svg, ox, oy, 480, "MIDI OUTPUT", accent=False)
    form_label(20, 26, "PORT")
    combo(112, 20, 368, "Boutique MIDI 1", focused=True)
    form_label(54, 26, "CHANNEL")
    combo(112, 54, 72, "3")
    M.led(svg, ox + 112, oy + 88 + 8, 10, 4, True, core=False)
    svg.text(ox + 130, M.vcentre_baseline(oy + 88, 20, "body"), "Connected to Boutique MIDI 1", "body",
             T["labelPrimary"])
    M.section_rule(svg, ox, oy + 124, 480, "MIDI INPUT", accent=False)
    form_label(144, 26, "PORT")
    combo(112, 144, 260, "None")
    checkbox(178, "Follow the panel", False, disabled=True)
    M.section_rule(svg, ox, oy + 214, 480, "BEHAVIOUR", accent=False)
    checkbox(234, "Auto-send on highlight", False)
    checkbox(262, "Resend the current patch on connect", True)
    M.section_rule(svg, ox, oy + 298, 480, "ON THE D-05", accent=False)
    for i, ln in enumerate(nlines):
        svg.text(ox, oy + 318 + i * 15 + 12, ln, "note", T["labelSecondary"])
    dialog_button(svg, ox, oy + btn_y, 100, 28, "TEST")
    dialog_button(svg, ox + 272, oy + btn_y, 100, 28, "CANCEL")
    dialog_button(svg, ox + 380, oy + btn_y, 100, 28, "OK", default=True)
    return dict(dialog=(dx, dy, dw, dh))


# ------------------------------------------------------------------ portrait (DESIGN-WEB 7)
def portrait_geometry(W, H, dpr):
    m = SZ["margin"]
    CW = min(W, BP["phoneContentMaxWidth"])
    cx0 = (W - CW) // 2
    G = CW - 2 * m
    cw = G // 4
    gx = cx0 + m + (G - 4 * cw) // 2
    span_x, span_w = gx + 3, 4 * cw - 6
    P = pitch_fit(span_w - 2 * SZ["bezelPadX"], 20, dpr)
    geo = lcd(20, P, dpr, SZ["bezelPadX"], SZ["bezelPadY"])
    bx = span_x + (span_w - geo["bezelW"]) // 2
    by = SZ["displayTop"]
    B = by + geo["bezelH"] + SZ["displayToBand"]
    bottom = 22 + 8 + 44 + 12 + 44 + 4 + 12 + 12 + 52  # status .. transport
    free = H - bottom - 12 - (B + 128)
    hp = max(SZ["padRowMinHeight"], min(SZ["padRowMaxHeight"], free // 4))
    yT = H - 210
    return dict(W=W, H=H, dpr=dpr, CW=CW, cx0=cx0, G=G, cw=cw, gx=gx, span_x=span_x, span_w=span_w,
                lcd=geo, bezel=(bx, by), B=B, hp=hp,
                rule_group=B + 12, strip=B + 28, rule_bank=B + 84, bank=B + 100,
                rule_number=B + 100 + 2 * hp + 12, number=B + 100 + 2 * hp + 28,
                top_end=B + 128 + 4 * hp, transport=yT, rule_fav=H - 146, chips=H - 130,
                bottom_row=H - 74, status=H - 22, gap=yT - (B + 128 + 4 * hp),
                fits=yT - (B + 128 + 4 * hp) >= 12)


def strip(svg, x_first_face, y, groups_lit, clip, state="normal"):
    """Group strip: P1..P6, 12 px gap, U1..U8; cells 52, faces 46 x 32 at +6."""
    x0, x1 = clip
    svg.defs.append(f'<clipPath id="strip{y}"><rect x="{x0}" y="{y}" width="{x1 - x0}" height="44"/></clipPath>')
    svg.add(f'<g clip-path="url(#strip{y})">')
    x = x_first_face
    for i, g in enumerate([f"P{k}" for k in range(1, 7)] + [f"U{k}" for k in range(1, 9)]):
        if i == 6:
            x += SZ["stripGroupGap"]
        pad(svg, x, y + 6, SZ["stripFaceWidth"], SZ["stripFaceHeight"], g, lit=g in groups_lit, state=state,
            led_h=3)
        x += SZ["stripCellWidth"]
    svg.add("</g>")


def chip(svg, x, y, addr, name, lit):
    nw = round(M.advance(name, "body"))
    w = min(SZ["chipMaxWidth"], 73 + nw + 12)
    face(svg, x, y, w, SZ["chipFaceHeight"], lit=lit)
    M.led(svg, x + 8, y + 14, 8, 4, lit, core=False)
    bl = M.vcentre_baseline(y, SZ["chipFaceHeight"], "listAddress")
    for ci, ch in enumerate(addr):
        svg.text_centred(x + 22 + 9 * ci + 4.5, bl, ch, "listAddress",
                         T["labelPrimary"] if lit else T["labelSecondary"])
    svg.text(x + 73, M.vcentre_baseline(y, SZ["chipFaceHeight"], "body"), name, "body", T["labelPrimary"])
    return w


def portrait(name, W, H, dpr, state):
    """state: main | search | access"""
    g = portrait_geometry(W, H, dpr)
    svg = M.Svg(W, H)
    svg.rect(0, 0, W, H, T["panel"])
    if state == "access":
        ctx = "CONNECT MIDI"
    elif state == "search":
        ctx = f"{len(brass_matches())} MATCHES"
    else:
        ctx = None
    l1, l2 = phone_lines(CUR["addr"], CUR["name"], CUR["mode"], ctx)
    draw_lcd(svg, g["bezel"][0], g["bezel"][1], g["lcd"], l1, l2)
    B = g["B"]
    svg.rect(0, B, W, H - 22 - B, T["band"])
    svg.rect(0, B, W, 1, T["bandEdge"])
    sx, sw, gx, cw, hp = g["span_x"], g["span_w"], g["gx"], g["cw"], g["hp"]
    if state == "search":
        overlay(svg, g, W, H)
        return svg, g
    if state == "access":
        bottom = g["bottom_row"]
        access_content(svg, sx, B + 12, sw, bottom - 12 - (B + 12), "prompt-android", phone=True)
        pad_plain(svg, sx + sw - 72, bottom, 72, 44, "SYSTEM")
        status_bar(svg, W, H, "", dim_port=True, show_in=False)
        return svg, g
    M.section_rule(svg, sx, g["rule_group"], sw, "PATCH GROUP")
    strip(svg, sx, g["strip"], {"P1"}, (0, W))
    for title, ry, rows_y in (("PATCH BANK", g["rule_bank"], g["bank"]), ("PATCH NUMBER", g["rule_number"], g["number"])):
        M.section_rule(svg, sx, ry, sw, title)
        for k in range(8):
            r, c = divmod(k, 4)
            pad(svg, gx + cw * c + 3, rows_y + hp * r + 4, cw - 6, hp - 8, str(k + 1), lit=k == 0,
                legend_colour=T["labelAccent"])
    yT = g["transport"]
    transport(svg, sx, yT + 4, 2 * cw - 6, 44, "prev")
    transport(svg, gx + 2 * cw + 3, yT + 4, 2 * cw - 6, 44, "next")
    M.section_rule(svg, sx, g["rule_fav"], sw, "FAVOURITES", accent=False)
    svg.defs.append(f'<clipPath id="chips"><rect x="0" y="{g["chips"]}" width="{W}" height="44"/></clipPath>')
    svg.add('<g clip-path="url(#chips)">')
    x = sx
    for a in FAVS:
        r = BY_ADDR[a]
        x += chip(svg, x, g["chips"] + 6, a, r["name"], a == CUR["addr"]) + SZ["chipGap"]
    svg.add("</g>")
    yb = g["bottom_row"]
    field(svg, sx, yb, sw - 72 - 8, 44, "Search patches", placeholder=True)
    pad_plain(svg, sx + sw - 72, yb, 72, 44, "SYSTEM")
    status_bar(svg, W, H, "Boutique MIDI 1", ch="3", show_in=W >= 480)
    return svg, g


def overlay(svg, g, W, H):
    B, sx, sw = g["B"], g["span_x"], g["span_w"]
    svg.rect(0, B, W, H - B, T["band"])
    svg.rect(0, B, W, 1, T["bandEdge"])
    field(svg, sx, B + 6, sw - 72 - 8, 44, "brass", focused=True)
    dialog_button(svg, sx + sw - 72, B + 6, 72, 44, "DONE", role="padLabel")
    vw = (sw + 6) // 3
    for i, (lab, lit) in enumerate((("ALL", True), ("FAVOURITES", False), ("RECENTS", False))):
        pad(svg, sx + vw * i, B + 56 + 6, vw - 6, 32, lab, lit=lit, led_h=3)
    ly = B + 108
    svg.rect(0, ly, W, H - ly, T["well"])
    svg.rect(0, ly, W, 1, T["wellEdgeDark"])
    rows = brass_matches()
    list_rows(svg, 0, ly + 1, W, H - ly - 1, rows, SZ["listRowHeight"], CUR["addr"], {"P1-35"}, big=True,
              clip_id="ovl")


# ------------------------------------------------------------------ landscape (DESIGN-WEB 8)
def landscape_geometry(W, H, dpr):
    m = SZ["margin"]
    compact = H <= BP["landscapeCompactMaxHeight"]
    c = max(SZ["landscapeCellMin"], min(SZ["landscapeCellMax"], int((W / 2 - 24) // 8)))
    L = 8 * c + 2 * m
    span_x, span_w = m + 2, 8 * c - 4
    P = pitch_fit(span_w - 2 * SZ["bezelPadX"], 20, dpr, 2 if compact else 3)
    geo = lcd(20, P, dpr, SZ["bezelPadX"], SZ["bezelPadY"])
    bx = span_x + (span_w - geo["bezelW"]) // 2
    y = 8 + geo["bezelH"]
    out = dict(W=W, H=H, dpr=dpr, compact=compact, c=c, L=L, span_x=span_x, span_w=span_w, lcd=geo,
               bezel=(bx, 8))
    if compact:
        out.update(strip=y + 6, bank=y + 54, number=y + 102, transport=y + 150, end=y + 194)
    else:
        out.update(rule_group=y + 8, strip=y + 22, rule_bank=y + 72, num_bank=y + 86, bank=y + 100,
                   rule_number=y + 150, num_number=y + 164, number=y + 178, transport=y + 228, end=y + 272)
    out["fits"] = out["end"] + 4 <= H - 22
    out.update(band_top=8 + geo["bezelH"] + 4, X=L + m, Rw=W - L - 2 * m, views=8, search=56, list=108,
               list_h=H - 22 - 8 - 108)
    return out


def landscape(name, W, H, dpr):
    g = landscape_geometry(W, H, dpr)
    svg = M.Svg(W, H)
    svg.rect(0, 0, W, H, T["panel"])
    l1, l2 = phone_lines(CUR["addr"], CUR["name"], CUR["mode"])
    draw_lcd(svg, g["bezel"][0], g["bezel"][1], g["lcd"], l1, l2)
    B = g["band_top"]  # left column: panel above, band below; right column: band full height
    svg.rect(0, B, g["L"], H - 22 - B, T["band"])
    svg.rect(0, B, g["L"], 1, T["bandEdge"])
    svg.rect(g["L"], 0, W - g["L"], H - 22, T["band"])
    svg.rect(g["L"], 0, 1, H - 22, T["bandEdge"])
    sx, sw, c = g["span_x"], g["span_w"], g["c"]
    if not g["compact"]:
        M.section_rule(svg, sx, g["rule_group"], sw, "PATCH GROUP")
    strip(svg, sx, g["strip"], {"P1"}, (0, g["L"]))
    rows = (("PATCH BANK", "bank"), ("PATCH NUMBER", "number"))
    for title, key in rows:
        if not g["compact"]:
            M.section_rule(svg, sx, g["rule_" + key], sw, title)
        for k in range(8):
            x = 12 + c * k + 2
            if g["compact"]:
                pad(svg, x, g[key] + 4, c - 4, 36, str(k + 1), lit=k == 0, legend_colour=T["labelAccent"])
            else:
                nb = g["num_" + key] + 11
                svg.text_centred(x + (c - 4) / 2, nb, str(k + 1), "padLabel", T["labelAccent"])
                pad(svg, x, g[key] + 4, c - 4, 36, "", lit=k == 0)
    transport(svg, sx, g["transport"] + 4, 4 * c - 4, 36, "prev")
    transport(svg, 12 + 4 * c + 2, g["transport"] + 4, 4 * c - 4, 36, "next")
    X, Rw = g["X"], g["Rw"]
    vw = (Rw + 6) // 3
    for i, (lab, lit) in enumerate((("ALL", True), ("FAVOURITES", False), ("RECENTS", False))):
        pad(svg, X + vw * i, 8 + 4, vw - 6, 36, lab, lit=lit, led_h=3)
    field(svg, X, g["search"], Rw - 80, 44, "Search patches", placeholder=True)
    pad_plain(svg, X + Rw - 72, g["search"], 72, 44, "SYSTEM")
    lh = g["list_h"]
    well(svg, X, g["list"], Rw, lh)
    allrows = M.catalogue()
    i0 = next(i for i, r in enumerate(allrows) if r["addr"] == "P1-11")
    list_rows(svg, X + 1, g["list"] + 1, Rw - 2, lh - 2, allrows[i0:i0 + 20], SZ["landscapeListRowHeight"],
              CUR["addr"], {"P1-12", "P1-16"}, big=True, clip_id="lwell")
    status_bar(svg, W, H, "Boutique MIDI 1", ch="3")
    return svg, g


# ------------------------------------------------------------------ geometry report
def report():
    print("Portrait geometry (W x H @ dpr): P dev, pitch css, g, d, field, bezel, B, hp, gap, fits")
    for W, H, dpr in ((360, 640, 2), (360, 668, 2), (360, 800, 2), (390, 844, 3), (390, 844, 2), (390, 844, 1),
                      (393, 851, 2.75), (412, 915, 2.625), (412, 892, 2.625), (430, 932, 3), (768, 1024, 2)):
        g = portrait_geometry(W, H, dpr)
        L = g["lcd"]
        P40 = pitch_fit(g["span_w"] - 20, 40, dpr)
        print(f"  {W}x{H}@{dpr}: P {L['P']} ({L['P'] / dpr:.2f} css), g {L['g']}, d {L['d']}, field "
              f"{L['fieldW']}x{L['fieldH']}, bezel {L['bezelW']}x{L['bezelH']} at {g['bezel']}, B {g['B']}, "
              f"cw {g['cw']} gx {g['gx']}, hp {g['hp']}, gap {g['gap']}, fits {g['fits']}; "
              f"40 cols would be P {P40} ({P40 / dpr:.2f} css, glyph {(6 * P40 + P40 - L['g']) / dpr:.1f} css tall)")
    print("Landscape geometry:")
    for W, H, dpr in ((844, 390, 3), (844, 390, 2), (844, 390, 1), (800, 360, 2), (800, 304, 2), (800, 300, 2),
                      (800, 280, 2), (915, 412, 2.625), (640, 360, 2), (568, 320, 2)):
        g = landscape_geometry(W, H, dpr)
        L = g["lcd"]
        print(f"  {W}x{H}@{dpr}: compact {g['compact']}, c {g['c']}, L {g['L']}, P {L['P']} ({L['P'] / dpr:.2f}), "
              f"field {L['fieldW']}x{L['fieldH']}, bezel {L['bezelW']}x{L['bezelH']} at {g['bezel']}, "
              f"end {g['end']}, fits {g['fits']}, band top {g['band_top']}, right col x {g['X']} w {g['Rw']}, "
              f"list h {g['list_h']}, strip {g['strip']} bank {g['bank']} number {g['number']} "
              f"transport {g['transport']}")
    print("Desktop LCD:")
    for W, dpr in ((960, 1), (960, 2), (960, 1.25), (1100, 1.1)):
        L = desktop_lcd(W, dpr)
        print(f"  {W}@{dpr}: P {L['P']} g {L['g']} field {L['fieldW']}x{L['fieldH']} bezel {L['bezelW']}x{L['bezelH']}")
    for kind in ("prompt", "denied-chrome", "unsupported-safari"):
        print("  wrap", kind, wrap(ACCESS[kind]["body"], "body", 400))
    print("  wrap android", wrap(ACCESS["prompt-android"]["body"], "body", 358))
    for s in ACCESS["denied-chrome"]["steps"]:
        print("  step", s, round(M.advance(s, "body")))
    for left, right in MATRIX:
        print("  matrix", left, round(M.advance(left, "body")), right, round(M.advance(right, "body")))
    for s in ("SYSTEM", "PREV", "NEXT", "FAVOURITES", "RECENTS", "ALL", "U8", "P6", "8"):
        print("  padLabel", s, round(M.advance(s, "padLabel"), 1))
    print("  dialogTitle SETTINGS", round(M.advance("SETTINGS", "dialogTitle"), 1),
          "CLOSE", round(M.advance("CLOSE", "panelLabel"), 1))


# ------------------------------------------------------------------ render
def main(argv):
    if "--print-geometry" in argv:
        report()
        return 0
    import tempfile
    tmp = tempfile.mkdtemp(prefix="d05-mockup-web-fc-")
    conf = os.path.join(tmp, "fonts.conf")
    with open(conf, "w") as f:
        f.write('<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig>'
                f'<dir>{M.esc(M.FONTS)}</dir><cachedir>{M.esc(tmp)}/cache</cachedir></fontconfig>')
    env = dict(os.environ, FONTCONFIG_FILE=conf, HOME=tmp)
    frames = []
    for dpr in (1, 2):
        sfx = "" if dpr == 1 else "@2x"
        for st in ("access-prompt", "access-denied", "unsupported", "settings"):
            frames.append((f"desktop-{st}{sfx}", desktop(st, 960, 560, dpr, st), 960 * dpr))
        for st in ("main", "search", "access"):
            svg, _ = portrait(st, 390, 844, dpr, st)
            frames.append((f"portrait-{st}{sfx}", svg, 390 * dpr))
        svg, _ = landscape("main", 844, 390, dpr)
        frames.append((f"landscape-main{sfx}", svg, 844 * dpr))
        svg, _ = landscape("compact", 800, 300, dpr)
        frames.append((f"landscape-compact{sfx}", svg, 800 * dpr))
    for base, svg, width in frames:
        sp = os.path.join(HERE, base + ".svg")
        svg.write(sp)
        text = open(sp, encoding="utf-8").read().replace(
            "Generated by docs/design/mockup/build.py from docs/design/DESIGN.md.",
            "Generated by docs/design/mockup-web/build.py from docs/design/DESIGN-WEB.md and tokens.json.")
        open(sp, "w", encoding="utf-8").write(text)
        M.render(sp, os.path.join(HERE, base + ".png"), width, env)
        print("wrote", os.path.relpath(os.path.join(HERE, base + ".png"), ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
