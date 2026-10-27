#!/usr/bin/env python3
"""Generates the token outputs from docs/design/tokens.json, version 2 (stdlib only).

Usage (from anywhere):
  python3 docs/design/gen-tokens.py               write every output (only files whose text changes)
  python3 docs/design/gen-tokens.py --check       write nothing; exit 1 if any output or cross-check differs
  python3 docs/design/gen-tokens.py --dump SKIN   print the resolved tokens of one skin

Outputs (DESIGN-2.md section 2.5):
  assets/theme/<skin>.qss   one per skin in skinOrder, from the template assets/theme/app.qss.in:
                            @{<colour token>} -> the skin's resolved #RRGGBB, @{skin} -> the skin id,
                            a line holding only @{TOKEN_BLOCK} -> the token comment block. Anything
                            else of the form @{word}, and any literal #RRGGBB after the first "*/",
                            is an error.
  web/src/ui/theme.css      CSS custom properties for the web version (DESIGN-WEB.md section 2) from
                            the version 1 subset of base (the 41 colours of groups 2.1 to 2.4, the 13
                            version 1 type roles, webColors, webType, size, breakpoints). The web
                            stays the D-05: this file is byte-identical to the version 1 output.
  src/ui/SkinTables.inc     the runtime palette of ui/Theme: one row per skin with every resolved
                            colour, switch, component and string, and the resolved type.wordmark.

Cross-checks run by --check (read only; they never write):
  - every output above equals what this script would write; assets/theme/app.qss is gone;
  - the DESIGN-2.md 2.2 invariants (override names exist in base, #RRGGBB everywhere, the D-05 skin
    is base, every new colour's base value equals the version 1 token it stands in for, line heights
    per skin from the font files' hhea, families and extraFonts files, licences, cmap coverage);
  - src/ui/Theme.h includes SkinTables.inc and declares no colour of its own, Theme.cpp holds no
    hex colour, and its type roles equal base.type;
  - docs/design/mockup/build.py: its T dict and ROLES table equal the version 1 subset of base.
  (docs/design/mockup/build2.py reads tokens.json directly and needs no check.)
"""

import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TOKENS = os.path.join(HERE, "tokens.json")
THEME_DIR = os.path.join(ROOT, "assets", "theme")
TEMPLATE = os.path.join(THEME_DIR, "app.qss.in")
OLD_QSS = os.path.join(THEME_DIR, "app.qss")
CSS = os.path.join(ROOT, "web", "src", "ui", "theme.css")
INC = os.path.join(ROOT, "src", "ui", "SkinTables.inc")
THEME_H = os.path.join(ROOT, "src", "ui", "Theme.h")
THEME_CPP = os.path.join(ROOT, "src", "ui", "Theme.cpp")
MOCKUP = os.path.join(ROOT, "docs", "design", "mockup", "build.py")

DASH = " * " + "-" * 82

# Colour groups and type roles that existed in version 1 (the web and build.py use only these).
V1_GROUPS = ("2.1", "2.2", "2.3", "2.4")
V2_ROLES = {"displayName3", "displayName4", "displayName5", "displayContext3", "displayContext4",
            "displayContext5", "ruleTitle", "switcherTab"}

# New version 2 tokens whose D-05 value must equal a version 1 token (DESIGN-2.md 2.3).
STANDS_IN = {
    "buttonLegend": "labelPrimary", "wedge": "arrow", "lensRim": "buttonSlot",
    "controlFace": "buttonFace", "controlFaceHover": "buttonFaceHover", "controlPressed": "buttonPressed",
    "labelNumeral": "labelAccent", "labelCommit": "labelRed",
    "ruleTitleBack": "panel", "ruleTitleText": "labelPrimary",
    "ruleBand": "ruleAccent", "ruleBandText": "labelPrimary", "ruleBand2": "ruleAccent",
    "ruleBand2Text": "labelPrimary",
}
for _role in ("bank", "view", "commit", "transport"):
    STANDS_IN.update({f"{_role}Face": "buttonFace", f"{_role}FaceHover": "buttonFaceHover",
                      f"{_role}Pressed": "buttonPressed", f"{_role}Highlight": "buttonHighlight",
                      f"{_role}FaceLit": "buttonFaceLit"})
NEW_UNUSED_ON_D05 = {"ledBack", "ledSegOn", "ledSegGhost"}

# Values the components may take (Theme.cpp turns the strings into enums).
COMPONENT_VALUES = {
    "display": {"lcd", "led"},
    "groupSlot": {"groupBar", "empty", "engine"},
    "rowTag": {"keyMode", "none", "engine"},
    "sectionRule": {"hairline", "doubleBar", "band"},
    "buttonLed": {"slit", "lens"},
}


def load():
    with open(TOKENS, encoding="utf-8") as f:
        return json.load(f)


def roles(d):
    return {k: v for k, v in d.items() if not k.startswith("_")}


# ------------------------------------------------------------------ resolution (DESIGN-2.md 2.2)
def resolve(tok, skin):
    s = tok["skins"][skin]
    base = tok["base"]
    out = {"colors": {k: v["hex"] for k, v in base["colors"].items()}}
    out["colors"].update(s.get("colors", {}))
    types = {k: dict(v) for k, v in roles(base["type"]).items()}
    for k, v in s.get("type", {}).items():
        types[k].update(v)
    out["type"] = types
    for key in ("switches", "components", "strings"):
        d = dict(roles(base[key]))
        d.update(s.get(key, {}))
        out[key] = d
    return out


def family_of(tok, role):
    return role.get("family", tok["font"]["family"])


def v1_view(tok):
    """The version 1 token file as the web output and the mockup check see it."""
    colors = {k: v for k, v in tok["base"]["colors"].items() if v["group"] in V1_GROUPS}
    types = {k: v for k, v in roles(tok["base"]["type"]).items() if k not in V2_ROLES}
    return {"colorGroups": [g for g in tok["colorGroups"] if g["id"] in V1_GROUPS], "colors": colors,
            "type": types, "font": tok["font"], "webColors": tok["webColors"], "webType": tok["webType"],
            "size": tok["size"], "breakpoints": tok["breakpoints"]}


def spacing_text(role):
    if "cells" in role:
        return "cells"
    s = role["spacing"]
    return "0" if s == 0 else f"{float(s):.1f}"


# ------------------------------------------------------------------ <skin>.qss
def skin_title(tok, skin):
    s = tok["skins"][skin]
    name = resolve(tok, skin)["strings"]["modelName"]
    eng = f", engine {s['engine']}" if "engine" in s else ""
    return f"{skin} ({name}{eng})"


def token_block(tok, skin):
    r = resolve(tok, skin)
    other_family = any(family_of(tok, v) != tok["font"]["family"] for v in r["type"].values())
    family_line = ('Family "Inter" (assets/fonts, OFL 1.1) unless the line names another (DESIGN-2.md 3.1), sizes in'
                   if other_family else 'Family "Inter" (assets/fonts, OFL 1.1), sizes in')
    out = [f" * Skin: {skin_title(tok, skin)}",
           DASH,
           " * COLOUR TOKENS (DESIGN.md section 2, DESIGN-2.md section 2.3). No other colours appear anywhere.",
           DASH]
    for g in tok["colorGroups"]:
        out.append(f" * {g['id']} {g['title']}")
        for name, c in tok["base"]["colors"].items():
            if c["group"] == g["id"]:
                out.append(f" *   {name:<18}{r['colors'][name]}  {c['qss']}")
    out += [" *",
            DASH,
            f" * TYPE ROLES (DESIGN.md section 3). {family_line}",
            *([" * px. Weight 400 = Regular, 500 = Medium, 600 = SemiBold, 700 = Bold. \"Caps\" strings are stored in",
               " * capitals by the code: QSS cannot change case."]
              if other_family else
              [" * px. Weight 400 = Regular, 500 = Medium. \"Caps\" strings are stored in capitals by the",
               " * code: QSS cannot change case."]),
            DASH,
            f" *   {'role':<18}{'weight':<8}{'size':<6}{'spacing':<9}{'case':<12}colour"]
    for name, role in r["type"].items():
        line = (f" *   {'type.' + name + ' ':<19}{role['weight']:<8}{role['size']:<6}{spacing_text(role):<9}"
                f"{role['case']:<12}{role['colour']}")
        fam = family_of(tok, role)
        if fam != tok["font"]["family"]:
            line += f"  {fam}"
        out.append(line)
    af = tok["font"]["appFont"]
    face = {400: "Regular", 500: "Medium"}[af["weight"]]
    out.append(f" * Application font (Theme): {tok['font']['family']} {face} {af['size']} px.")
    return out


PLACEHOLDER = re.compile(r"@\{(\w+)\}")


def qss_text(tok, skin, template, errors):
    r = resolve(tok, skin)
    lines = template.split("\n")
    first_close = template.index("*/") + 2
    body_start = template[:first_close].count("\n")  # index of the line holding the first */
    out = []
    for i, line in enumerate(lines):
        if line.strip() == "@{TOKEN_BLOCK}":
            out += token_block(tok, skin)
            continue

        def sub(m):
            name = m.group(1)
            if name == "skin":
                return skin
            if name in r["colors"]:
                return r["colors"][name]
            errors.append(f"app.qss.in line {i + 1}: unknown placeholder @{{{name}}}")
            return m.group(0)

        new = PLACEHOLDER.sub(sub, line)
        if i > body_start:
            for h in re.findall(r"#[0-9A-Fa-f]{6}\b", line):  # a literal in the rules (before substitution)
                errors.append(f"app.qss.in line {i + 1}: literal {h}; use a @{{token}}")
        out.append(new)
    return "\n".join(out)


# ------------------------------------------------------------------ theme.css
def css_num(v):
    return f"{v:g}"


def hex_rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def css_text(tok):
    fam = tok["font"]["family"]
    stack = ", ".join([f'"{fam}"'] + [f'"{f}"' if " " in f else f for f in tok["font"]["fallback"]])
    o = ["/*",
         " * Generated by docs/design/gen-tokens.py from docs/design/tokens.json. Do not edit:",
         " * change tokens.json and run the generator. Names are the DESIGN.md token names.",
         " * Colours: --<token>. Type roles: --type-<role>-{weight,size,spacing,line} and the",
         " * class .type-<role> (font, letter spacing, line height and the role's colour when",
         ' * it has one; "caps" strings are stored in capitals by the code, never text-transform).',
         " * Sizes: --size-<layout>-<name> in px. Breakpoints are listed for reference only:",
         " * custom properties cannot be used inside media queries.",
         " */",
         "",
         ":root {",
         "  color-scheme: dark;",
         f"  --font-family: {stack};"]
    for g in tok["colorGroups"]:
        o.append(f"  /* {g['id']} {g['title']} */")
        for name, c in tok["colors"].items():
            if c["group"] == g["id"]:
                o.append(f"  --{name}: {c['hex']};")
    o.append("  /* web only (DESIGN-WEB.md 2) */")
    for name, c in tok["webColors"].items():
        if name.startswith("_"):
            continue
        r, g, b = hex_rgb(tok["colors"][c["base"]]["hex"])
        o.append(f"  --{name}: rgba({r}, {g}, {b}, {css_num(c['alpha'])});")
    for key in ("type", "webType"):
        o.append(f"  /* {'type roles (DESIGN.md 3.2)' if key == 'type' else 'web type roles (DESIGN-WEB.md 3)'} */")
        for name, r in roles(tok[key]).items():
            o.append(f"  --type-{name}-weight: {r['weight']};")
            o.append(f"  --type-{name}-size: {r['size']}px;")
            o.append(f"  --type-{name}-spacing: {css_num(r['spacing'])}px;")
            o.append(f"  --type-{name}-line: {r['lineHeight']}px;")
            if "cells" in r:
                o.append(f"  --type-{name}-cell: {r['cells']}px;")
    for layout, sizes in tok["size"].items():
        if layout.startswith("_"):
            continue
        o.append(f"  /* sizes: {layout} */")
        for name, v in sizes.items():
            o.append(f"  --size-{layout}-{name}: {css_num(v)}px;")
    o.append("}")
    o.append("")
    bp = {k: v for k, v in tok["breakpoints"].items() if not k.startswith("_")}
    o.append("/* breakpoints (px): " + ", ".join(f"{k} {v}" for k, v in bp.items()) + " */")
    o.append("")
    for key in ("type", "webType"):
        for name, r in roles(tok[key]).items():
            o.append(f".type-{name} {{")
            o.append(f"  font-family: var(--font-family);")
            o.append(f"  font-weight: var(--type-{name}-weight);")
            o.append(f"  font-size: var(--type-{name}-size);")
            o.append(f"  letter-spacing: var(--type-{name}-spacing);")
            o.append(f"  line-height: var(--type-{name}-line);")
            if r["colour"] in tok["colors"]:
                o.append(f"  color: var(--{r['colour']});")
            o.append("}")
    o.append("")
    return "\n".join(o)


# ------------------------------------------------------------------ SkinTables.inc
def c_str(s):
    return json.dumps(s, ensure_ascii=True)


def inc_text(tok):
    names = list(tok["base"]["colors"].keys())
    n = len(names)
    o = ["// Generated by docs/design/gen-tokens.py from docs/design/tokens.json. Do not edit:",
         "// change tokens.json and run the generator. The runtime palette of ui/Theme (DESIGN-2.md 2.5):",
         "// one row per skin with every resolved colour, switch, component and string, and the",
         "// resolved type.wordmark. Colours are 0xRRGGBB in the order of D05_COLOUR_TOKENS.",
         "#pragma once",
         "",
         "#include <QtGlobal>",
         "",
         "namespace d05 {",
         "namespace SkinTables {",
         "",
         "#define D05_COLOUR_TOKENS(X) \\"]
    for i, name in enumerate(names):
        o.append(f"    X({name})" + (" \\" if i + 1 < n else ""))
    o += ["",
          f"inline constexpr int kSkinCount = {len(tok['skinOrder'])};",
          f"inline constexpr int kColourCount = {n};",
          "",
          "struct Row {",
          "    const char *id;",
          "    const char *model;  // d05, jp08 or ju06a",
          "    int engine;         // 60 or 106 on the JU-06A skins, else 0",
          "    // components",
          "    const char *display;      // lcd, led",
          "    const char *groupSlot;    // groupBar, empty, engine",
          "    const char *rowTag;       // keyMode, none, engine",
          "    const char *sectionRule;  // hairline, doubleBar, band",
          "    const char *buttonLed;    // slit, lens",
          "    int ledCells;",
          "    // switches",
          "    bool lcdShowGhost;",
          "    bool keyModeOnLine2;",
          "    bool displayEntryEcho;",
          "    bool ledShowGhost;",
          "    // strings",
          "    const char *modelName;",
          "    const char *groupTitle;",
          "    const char *bankTitle;",
          "    const char *numberTitle;",
          "    const char *engineTitle;",
          "    const char *wordmarkSub;",
          "    const char *idleContext;",
          "    const char *searchPlaceholder;",
          "    // resolved type.wordmark (DESIGN-2.md 3.1)",
          "    const char *wordmarkFamily;",
          "    int wordmarkWeight;",
          "    int wordmarkSize;",
          "    double wordmarkSpacing;",
          "    int wordmarkColour;  // index into colours",
          "    quint32 colours[kColourCount];",
          "};",
          "",
          "inline constexpr Row kSkins[kSkinCount] = {"]
    for skin in tok["skinOrder"]:
        r = resolve(tok, skin)
        s = tok["skins"][skin]
        wm = r["type"]["wordmark"]
        c, sw, st = r["components"], r["switches"], r["strings"]
        b = lambda v: "true" if v else "false"
        o.append("    {")
        o.append(f"        {c_str(skin)}, {c_str(s['model'])}, {s.get('engine', 0)},")
        o.append(f"        {c_str(c['display'])}, {c_str(c['groupSlot'])}, {c_str(c['rowTag'])}, "
                 f"{c_str(c['sectionRule'])}, {c_str(c['buttonLed'])}, {c['ledCells']},")
        o.append(f"        {b(sw['lcdShowGhost'])}, {b(sw['keyModeOnLine2'])}, {b(sw['displayEntryEcho'])}, "
                 f"{b(sw['ledShowGhost'])},")
        for key in ("modelName", "groupTitle", "bankTitle", "numberTitle", "engineTitle", "wordmarkSub",
                    "idleContext", "searchPlaceholder"):
            o.append(f"        {c_str(st[key])},")
        o.append(f"        {c_str(family_of(tok, wm))}, {wm['weight']}, {wm['size']}, {float(wm['spacing']):.1f}, "
                 f"{names.index(wm['colour'])},")
        o.append("        {")
        for i in range(0, n, 6):
            chunk = ", ".join("0x" + r["colors"][nm][1:] for nm in names[i:i + 6])
            o.append(f"            {chunk},")
        o.append("        },")
        o.append("    },")
    o += ["};", "", "}  // namespace SkinTables", "}  // namespace d05", ""]
    return "\n".join(o)


# ------------------------------------------------------------------ font access
def hhea(path):
    data = open(path, "rb").read()
    num = struct.unpack(">H", data[4:6])[0]
    tables = {}
    for i in range(num):
        tag, _, off, _ = struct.unpack(">4sIII", data[12 + 16 * i: 28 + 16 * i])
        tables[tag.decode()] = off
    upm = struct.unpack(">H", data[tables["head"] + 18: tables["head"] + 20])[0]
    asc, desc = struct.unpack(">hh", data[tables["hhea"] + 4: tables["hhea"] + 8])
    return upm, asc, desc


def cmap_chars(path):
    """Code points mapped by the font's cmap (formats 4 and 12, Unicode subtables)."""
    data = open(path, "rb").read()
    num = struct.unpack(">H", data[4:6])[0]
    off = None
    for i in range(num):
        tag, _, o, _ = struct.unpack(">4sIII", data[12 + 16 * i: 28 + 16 * i])
        if tag == b"cmap":
            off = o
    chars = set()
    n = struct.unpack(">H", data[off + 2: off + 4])[0]
    for i in range(n):
        pid, eid, so = struct.unpack(">HHI", data[off + 4 + 8 * i: off + 12 + 8 * i])
        if (pid, eid) not in ((3, 1), (3, 10), (0, 3), (0, 4)):
            continue
        t = off + so
        fmt = struct.unpack(">H", data[t: t + 2])[0]
        if fmt == 4:
            segx2 = struct.unpack(">H", data[t + 6: t + 8])[0]
            ends = struct.unpack(f">{segx2 // 2}H", data[t + 14: t + 14 + segx2])
            starts = struct.unpack(f">{segx2 // 2}H", data[t + 16 + segx2: t + 16 + 2 * segx2])
            for a, b in zip(starts, ends):
                if a != 0xFFFF:
                    chars.update(range(a, b + 1))
        elif fmt == 12:
            ng = struct.unpack(">I", data[t + 12: t + 16])[0]
            for g in range(ng):
                a, b, _ = struct.unpack(">III", data[t + 16 + 12 * g: t + 28 + 12 * g])
                chars.update(range(a, b + 1))
    return chars


def font_file(tok, role):
    """Path of the file a resolved role draws with, or None if its family or weight has none."""
    fam = family_of(tok, role)
    if fam == tok["font"]["family"]:
        files = tok["font"]["files"]
    else:
        files = tok.get("extraFonts", {}).get(fam, {}).get("files", {})
    f = files.get(str(role["weight"]))
    return os.path.join(ROOT, f) if f else None


# ------------------------------------------------------------------ invariants (DESIGN-2.md 2.2)
def check_invariants(tok, errors):
    base = tok["base"]
    for skin in tok["skinOrder"]:
        s = tok["skins"][skin]
        for key in ("colors", "type", "switches", "components", "strings"):
            for name in s.get(key, {}):
                if name not in base[key]:
                    errors.append(f"skins.{skin}.{key}.{name} is not in base.{key}")
        r = resolve(tok, skin)
        for name, h in r["colors"].items():
            if not re.fullmatch(r"#[0-9A-F]{6}", h):
                errors.append(f"{skin}: colour {name} = {h!r} is not #RRGGBB (upper case)")
        for key, allowed in COMPONENT_VALUES.items():
            if r["components"][key] not in allowed:
                errors.append(f"{skin}: components.{key} = {r['components'][key]!r}, expected one of {sorted(allowed)}")
        if r["components"]["ledCells"] not in (2, 3):
            errors.append(f"{skin}: components.ledCells must be 2 or 3")
        wm = r["type"]["wordmark"]
        if wm["colour"] not in r["colors"]:
            errors.append(f"{skin}: type.wordmark colour {wm['colour']!r} is not a colour token")
    for key in ("colors", "type", "switches", "components", "strings"):
        if tok["skins"]["d05"].get(key):
            errors.append(f"skins.d05.{key} must be empty: the D-05 skin is base")
    for name in base["colors"]:
        c = base["colors"][name]
        if c["group"] not in {g["id"] for g in tok["colorGroups"]}:
            errors.append(f"base.colors.{name}: group {c['group']} is not in colorGroups")
        if c["group"] in V1_GROUPS or name in NEW_UNUSED_ON_D05:
            continue
        want = STANDS_IN.get(name)
        if want is None:
            errors.append(f"new colour {name} has no stand-in rule (add it to STANDS_IN or NEW_UNUSED_ON_D05)")
        elif base["colors"][name]["hex"] != base["colors"][want]["hex"]:
            errors.append(f"base.colors.{name} is {base['colors'][name]['hex']}, should equal {want} "
                          f"{base['colors'][want]['hex']}")
    for name, r in roles(base["type"]).items():
        if "family" in r:
            errors.append(f"base.type.{name} names a family; only skin overrides may (DESIGN-2.md 2.2)")
    for fam, e in tok.get("extraFonts", {}).items():
        if fam.startswith("_"):
            continue
        for path in list(e["files"].values()) + [e["licence"]]:
            if not os.path.isfile(os.path.join(ROOT, path)):
                errors.append(f"extraFonts.{fam}: {path} does not exist")
        for w, path in e["files"].items():
            full = os.path.join(ROOT, path)
            if os.path.isfile(full):
                missing = sorted({c for c in e["coverage"]} - {chr(c) for c in cmap_chars(full)})
                if missing:
                    errors.append(f"extraFonts.{fam} {w}: {path} lacks {''.join(missing)!r} from its coverage")
    for skin in tok["skinOrder"]:
        for name, r in resolve(tok, skin)["type"].items():
            path = font_file(tok, r)
            if path is None or not os.path.isfile(path):
                errors.append(f"{skin}: type.{name} family {family_of(tok, r)!r} weight {r['weight']} has no font file")
                continue
            upm, asc, desc = hhea(path)
            lh = round(asc * r["size"] / upm) + round(-desc * r["size"] / upm)
            if lh != r["lineHeight"]:
                errors.append(f"{skin}: type.{name}: lineHeight {r['lineHeight']}, the font gives {lh}")
    for name, r in roles(tok["webType"]).items():  # the web roles are Inter, as version 1 checked
        path = os.path.join(ROOT, tok["font"]["files"][str(r["weight"])])
        upm, asc, desc = hhea(path)
        lh = round(asc * r["size"] / upm) + round(-desc * r["size"] / upm)
        if lh != r["lineHeight"]:
            errors.append(f"webType.{name}: lineHeight {r['lineHeight']}, the font gives {lh}")


# ------------------------------------------------------------------ cross-checks
def check_theme_sources(tok, errors):
    if not os.path.exists(THEME_H):
        errors.append("src/ui/Theme.h not found")
    else:
        text = open(THEME_H, encoding="utf-8").read()
        if '#include "ui/SkinTables.inc"' not in text:
            errors.append('Theme.h does not include "ui/SkinTables.inc"')
        if re.search(r"inline const QColor\b|QColor\s+\w+\s*\(\s*0x", text):
            errors.append("Theme.h declares a colour of its own; colours come from SkinTables.inc")
    if not os.path.exists(THEME_CPP):
        errors.append("src/ui/Theme.cpp not found")
        return
    text = open(THEME_CPP, encoding="utf-8").read()
    if re.search(r"0x[0-9A-Fa-f]{6}\b|#[0-9A-Fa-f]{6}\b", text):
        errors.append("Theme.cpp holds a hex colour; colours come from SkinTables.inc")
    # Type roles: `case Role::Xyz: f = makeMedium(10); spacing = 0.5; break;` equals base.type.
    found = {}
    for m in re.finditer(r"case Role::(\w+):\s*f = make(Medium|Regular)\((\d+)\);(?:\s*spacing = ([\d.]+);)?", text):
        found[m.group(1)] = (500 if m.group(2) == "Medium" else 400, int(m.group(3)), float(m.group(4) or 0))
    for name, r in roles(tok["base"]["type"]).items():
        if name == "wordmark":
            continue  # per skin, from SkinTables.inc
        key = name[0].upper() + name[1:]
        want = (r["weight"], r["size"], float(r["spacing"]))
        if found.get(key) != want:
            errors.append(f"Theme.cpp: Role::{key} is {found.get(key)}, tokens.json {want}")
    for key in found:
        if key[0].lower() + key[1:] not in tok["base"]["type"]:
            errors.append(f"Theme.cpp: Role::{key} is not a type role")


def check_mockup(tok, errors):
    if not os.path.exists(MOCKUP):
        return
    v1 = v1_view(tok)
    text = open(MOCKUP, encoding="utf-8").read()
    t = dict(re.findall(r'(\w+)="(#[0-9A-Fa-f]{6})"', text[text.index("T = dict("):text.index("ROLES =")]))
    want = {k: v["hex"] for k, v in v1["colors"].items()}
    if {k: v.upper() for k, v in t.items()} != {k: v.upper() for k, v in want.items()}:
        errors.append("mockup/build.py: T dict differs from the version 1 colours of tokens.json")
    rt = dict((m[0], (int(m[1]), int(m[2]), float(m[3]))) for m in
              re.findall(r'"(\w+)": \((\d+), (\d+), ([\d.]+)\)', text[text.index("ROLES ="):]))
    for name, r in v1["type"].items():
        if rt.get(name) != (r["weight"], r["size"], float(r["spacing"])):
            errors.append(f"mockup/build.py: ROLES[{name}] is {rt.get(name)}, tokens.json "
                          f"{(r['weight'], r['size'], float(r['spacing']))}")


# ------------------------------------------------------------------ main
def main(argv):
    tok = load()
    if "--dump" in argv:
        skin = argv[argv.index("--dump") + 1]
        r = resolve(tok, skin)
        for name, h in r["colors"].items():
            print(f"{name:<22}{h}")
        for key in ("switches", "components", "strings"):
            for name, v in r[key].items():
                print(f"{key}.{name} = {v!r}")
        wm = r["type"]["wordmark"]
        print(f"type.wordmark = {family_of(tok, wm)} {wm['weight']} {wm['size']} px, spacing {wm['spacing']}, "
              f"{wm['colour']}, lineHeight {wm['lineHeight']}")
        return 0

    check = "--check" in argv
    errors = []
    template = open(TEMPLATE, encoding="utf-8").read()
    outputs = []
    for skin in tok["skinOrder"]:
        outputs.append((os.path.join(THEME_DIR, skin + ".qss"), qss_text(tok, skin, template, errors)))
    outputs.append((CSS, css_text(v1_view(tok))))
    outputs.append((INC, inc_text(tok)))
    for path, new in outputs:
        rel = os.path.relpath(path, ROOT)
        now = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if now == new:
            print(f"unchanged  {rel}")
        elif check:
            errors.append(f"{rel} is out of date (run docs/design/gen-tokens.py)")
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(new)
            print(f"wrote      {rel}")
    if check:
        if os.path.exists(OLD_QSS):
            errors.append("assets/theme/app.qss still exists; the style sheets are <skin>.qss (DESIGN-2.md 2.5)")
        check_invariants(tok, errors)
        check_theme_sources(tok, errors)
        check_mockup(tok, errors)
    for e in errors:
        print("ERROR", e, file=sys.stderr)
    if check and not errors:
        print(f"check ok: {len(tok['skinOrder'])} style sheets, theme.css, SkinTables.inc, Theme sources, "
              "mockup/build.py, the DESIGN-2.md 2.2 invariants and line heights agree with tokens.json")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
