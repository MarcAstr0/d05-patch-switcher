#!/usr/bin/env python3
"""Resolves and checks docs/design/tokens-2.json (DESIGN-2.md section 2). Stdlib only.

Usage (from anywhere):
  python3 docs/design/resolve-tokens-2.py --check        exit 1 on any problem
  python3 docs/design/resolve-tokens-2.py --dump SKIN    print the resolved colours of one skin

Checks:
  - every override name in skins.<id>.{colors,type,switches,components,strings} exists in base;
  - every colour resolves to #RRGGBB in every skin;
  - the D-05 skin equals tokens.json version 1: every version 1 colour, type role and switch,
    and the font, webColors, webType, breakpoints and size blocks, byte for byte as JSON values;
  - every new colour token's base value equals the version 1 token it stands in for
    (so the D-05 skin draws the version 2 widgets exactly as version 1 draws them);
  - type lineHeight equals round(ascent) + round(descent) from the font files, as gen-tokens.py does,
    for every role in every skin, read from the file of the role's family and weight (a role with no
    family is font.family, Inter; a role naming another family takes it from extraFonts);
  - every family a resolved role names is font.family or an extraFonts entry with a file for that weight,
    each extraFonts file and licence exists, and every character of the entry's coverage is in the
    file's cmap (the nameplate faces are subset to PATCH SWITCHER, DESIGN-2.md 3.1).

T8.1 folds this into gen-tokens.py; until then it is the designer's check.
"""

import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
V1 = os.path.join(HERE, "tokens.json")
V2 = os.path.join(HERE, "tokens-2.json")

# New version 2 tokens whose D-05 value must equal a version 1 token (DESIGN-2.md 2.3).
STANDS_IN = {
    "buttonLegend": "labelPrimary", "wedge": "arrow", "lensRim": "buttonSlot",
    "controlFace": "buttonFace", "controlFaceHover": "buttonFaceHover", "controlPressed": "buttonPressed",
    "labelNumeral": "labelAccent", "labelCommit": "labelRed",
    "ruleTitleBack": "panel", "ruleTitleText": "labelPrimary",
    "ruleBand": "ruleAccent", "ruleBandText": "labelPrimary", "ruleBand2": "ruleAccent", "ruleBand2Text": "labelPrimary",
}
for role in ("bank", "view", "commit", "transport"):
    STANDS_IN.update({f"{role}Face": "buttonFace", f"{role}FaceHover": "buttonFaceHover",
                      f"{role}Pressed": "buttonPressed", f"{role}Highlight": "buttonHighlight",
                      f"{role}FaceLit": "buttonFaceLit"})
NEW_UNUSED_ON_D05 = {"ledBack", "ledSegOn", "ledSegGhost"}


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def resolve(tok, skin):
    s = tok["skins"][skin]
    out = {"colors": {k: v["hex"] for k, v in tok["base"]["colors"].items()}}
    out["colors"].update(s.get("colors", {}))
    types = {k: dict(v) for k, v in tok["base"]["type"].items()}
    for k, v in s.get("type", {}).items():
        types[k].update(v)
    out["type"] = types
    for key in ("switches", "components", "strings"):
        d = dict(tok["base"][key])
        d.update(s.get(key, {}))
        out[key] = d
    return out


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
    fam = role.get("family", tok["font"]["family"])
    if fam == tok["font"]["family"]:
        files = tok["font"]["files"]
    else:
        files = tok.get("extraFonts", {}).get(fam, {}).get("files", {})
    f = files.get(str(role["weight"]))
    return os.path.join(ROOT, f) if f else None


def check(tok, v1):
    errors = []
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
    d05 = resolve(tok, "d05")
    for name, c in v1["colors"].items():
        if d05["colors"].get(name) != c["hex"]:
            errors.append(f"d05: colour {name} is {d05['colors'].get(name)}, version 1 {c['hex']}")
        if base["colors"].get(name) != c:
            errors.append(f"base.colors.{name} metadata differs from version 1")
    for name, r in v1["type"].items():
        if d05["type"].get(name) != r:
            errors.append(f"d05: type role {name} differs from version 1")
    for name, v in v1["switches"].items():
        if d05["switches"].get(name) != v:
            errors.append(f"d05: switch {name} differs from version 1")
    for key in ("font", "webColors", "webType", "breakpoints", "size"):
        if tok[key] != v1[key]:
            errors.append(f"{key} differs from version 1")
    for name in base["colors"]:
        if name in v1["colors"] or name in NEW_UNUSED_ON_D05:
            continue
        want = STANDS_IN.get(name)
        if want is None:
            errors.append(f"new colour {name} has no stand-in rule (add it to STANDS_IN or NEW_UNUSED_ON_D05)")
        elif base["colors"][name]["hex"] != v1["colors"][want]["hex"]:
            errors.append(f"base.colors.{name} is {base['colors'][name]['hex']}, should equal {want} "
                          f"{v1['colors'][want]['hex']}")
    for name, r in base["type"].items():
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
                errors.append(f"{skin}: type.{name} family {r.get('family', tok['font']['family'])!r} weight "
                              f"{r['weight']} has no font file")
                continue
            upm, asc, desc = hhea(path)
            lh = round(asc * r["size"] / upm) + round(-desc * r["size"] / upm)
            if lh != r["lineHeight"]:
                errors.append(f"{skin}: type.{name}: lineHeight {r['lineHeight']}, the font gives {lh}")
    return errors


def main(argv):
    tok = load(V2)
    if "--dump" in argv:
        skin = argv[argv.index("--dump") + 1]
        r = resolve(tok, skin)
        for name, h in r["colors"].items():
            print(f"{name:<22}{h}")
        for key in ("switches", "components", "strings"):
            for name, v in r[key].items():
                print(f"{key}.{name} = {v!r}")
        wm = r["type"]["wordmark"]
        print(f"type.wordmark = {wm.get('family', tok['font']['family'])} {wm['weight']} {wm['size']} px, "
              f"spacing {wm['spacing']}, {wm['colour']}, lineHeight {wm['lineHeight']}")
        return 0
    errors = check(tok, load(V1))
    for e in errors:
        print("ERROR", e, file=sys.stderr)
    if not errors:
        print(f"check ok: {len(tok['base']['colors'])} colours and {len(tok['base']['type'])} type roles resolve in "
              f"{len(tok['skinOrder'])} skins; d05 equals tokens.json version 1; nameplate faces cover their text")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
