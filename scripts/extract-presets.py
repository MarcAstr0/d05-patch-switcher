#!/usr/bin/env python3
"""Parse the D-05 Parameter Guide sound list (pages 23-28) from pdftotext -bbox output.

Output: TSV with columns group, bank, number, name, mode  (384 rows expected).
"""
import re
import sys
from collections import defaultdict

src = sys.argv[1]
html = open(src, encoding="utf-8").read()

pages = re.split(r'<page ', html)[1:]
MODE = {"(W)": "W", "(D)": "D", "(S)": "S"}
rows = []

for pidx, page in enumerate(pages):
    words = []
    for m in re.finditer(r'<word xMin="([\d.]+)" yMin="([\d.]+)" xMax="([\d.]+)" yMax="([\d.]+)">(.*?)</word>', page):
        x0, y0, x1, y1 = map(float, m.group(1, 2, 3, 4))
        words.append(dict(x0=x0, y0=y0, x1=x1, y1=y1, t=m.group(5)))

    # group id: first word matching P1..P6 near the top
    group = None
    header_y = None
    for w in sorted(words, key=lambda w: w["y0"]):
        if re.fullmatch(r"P[1-6]", w["t"]):
            group = w["t"]
            break
    assert group, f"page {pidx}: no group header"
    for w in words:
        if w["t"] == "No.1":
            header_y = w["y0"]
    assert header_y is not None, f"page {pidx}: no No.1 header"
    reverb_y = min(w["y0"] for w in words if w["t"] == "Reverb")

    body = [w for w in words if header_y < w["y0"] < reverb_y]
    modes = [w for w in body if w["t"] in MODE]
    # cluster mode tokens by y to find the 8 main mode rows (rows with >= 6 tokens)
    byy = defaultdict(list)
    for w in modes:
        key = round(w["y0"] / 4)  # 4pt buckets
        byy[key].append(w)
    main_rows = sorted((min(ws, key=lambda w: w["y0"])["y0"], ws) for ws in byy.values() if len(ws) >= 6)
    assert len(main_rows) == 8, f"page {pidx} {group}: found {len(main_rows)} mode rows"
    # column x ranges from mode tokens in the main rows: cluster xMin into 8
    xs = sorted(w["x0"] for _, ws in main_rows for w in ws)
    cols = []
    for x in xs:
        if cols and x - cols[-1][-1] < 20:
            cols[-1].append(x)
        else:
            cols.append([x])
    assert len(cols) == 8, f"page {pidx} {group}: {len(cols)} columns: {[c[0] for c in cols]}"
    col_x = [min(c) for c in cols]

    def col_of(w):
        # nearest column start at or left of the word's x0 (names are left-aligned with modes)
        best = None
        for i, cx in enumerate(col_x):
            if w["x0"] >= cx - 6:
                best = i
        return best

    prev_y = header_y
    for bank, (row_y, ws) in enumerate(main_rows, start=1):
        band_bottom = row_y + 12
        band = [w for w in body if prev_y < w["y0"] <= band_bottom]
        prev_y = band_bottom
        names = defaultdict(list)
        bmode = {}
        for w in band:
            if w["t"] == "BANK" or (re.fullmatch(r"[1-8]", w["t"]) and w["x0"] < col_x[0] - 6):
                continue
            c = col_of(w)
            assert c is not None, f"{group} bank {bank}: word {w['t']!r} at x={w['x0']} left of columns"
            if w["t"] in MODE:
                assert c not in bmode, f"{group} bank {bank} col {c+1}: two modes"
                bmode[c] = MODE[w["t"]]
            else:
                names[c].append(w)
        for c in range(8):
            ws_ = sorted(names[c], key=lambda w: (round(w["y0"]), w["x0"]))
            name = " ".join(w["t"] for w in ws_)
            assert name, f"{group} bank {bank} col {c+1}: empty name"
            assert c in bmode, f"{group} bank {bank} col {c+1} ({name}): no mode"
            rows.append((group, bank, c + 1, name, bmode[c]))

assert len(rows) == 384, len(rows)
out = sys.argv[2]
with open(out, "w", encoding="utf-8") as f:
    f.write("group\tbank\tnumber\tname\tmode\n")
    for g, b, n, name, mode in rows:
        f.write(f"{g}\t{b}\t{n}\t{name}\t{mode}\n")
print(f"wrote {len(rows)} rows to {out}")
