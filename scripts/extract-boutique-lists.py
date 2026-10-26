#!/usr/bin/env python3
"""Parse the JP-08 and JU-06A sound lists from `pdftotext -layout` output.

Usage: extract-boutique-lists.py JP-08_Sound_List_e01_W.pdf JU-06A_Sound_List_e01_W.pdf \
           data/jp08-patches.tsv data/ju06a-patches.tsv

Output: data/jp08-patches.tsv with columns bank, number, name (64 rows expected);
        data/ju06a-patches.tsv with columns engine, bank, number, name (128 rows expected,
        engine 60 then 106).

Each list is a four-column table, one page. Columns are separated by runs of two or
more spaces; names contain only single spaces. No name is fixed by hand.
"""
import re
import subprocess
import sys

ENTRY = re.compile(r"([1-8])-([1-8])\s+(.+?)(?=\s{2,}[1-8]-[1-8]\s|\s*$)")


def fail(message):
    print(f"extract-boutique-lists: {message}", file=sys.stderr)
    sys.exit(1)


def layout_text(pdf):
    return subprocess.run(["pdftotext", "-layout", pdf, "-"], check=True,
                          capture_output=True, text=True).stdout


def parse_block(lines, label):
    rows = []
    for line in lines:
        rows.extend((int(m[1]), int(m[2]), m[3].strip()) for m in ENTRY.finditer(line))
    if len(rows) != 64:
        fail(f"{label}: {len(rows)} rows")
    if len({(b, n) for b, n, _ in rows}) != 64:
        fail(f"{label}: duplicate address")
    if not all(name for _, _, name in rows):
        fail(f"{label}: empty name")
    return sorted(rows)


def main():
    jp_pdf, ju_pdf, jp_out, ju_out = sys.argv[1:5]

    jp_lines = layout_text(jp_pdf).split("Ribbon Controller")[0].splitlines()
    jp = parse_block(jp_lines, "JP-08")
    with open(jp_out, "w", encoding="utf-8") as f:
        f.write("bank\tnumber\tname\n")
        for b, n, name in jp:
            f.write(f"{b}\t{n}\t{name}\n")
    print(f"wrote {len(jp)} rows to {jp_out}")

    text = layout_text(ju_pdf).split("Chord Memory List")[0]
    # Engine headings are a line holding only "60" or "106".
    parts = re.split(r"^\s*(60|106)\s*$", text, flags=re.M)
    if not (len(parts) == 5 and parts[1] == "60" and parts[3] == "106"):
        fail("engine blocks")
    ju = []
    for engine, body in ((parts[1], parts[2]), (parts[3], parts[4])):
        ju.extend((engine, b, n, name) for b, n, name in parse_block(body.splitlines(), engine))
    with open(ju_out, "w", encoding="utf-8") as f:
        f.write("engine\tbank\tnumber\tname\n")
        for e, b, n, name in ju:
            f.write(f"{e}\t{b}\t{n}\t{name}\n")
    print(f"wrote {len(ju)} rows to {ju_out}")


if __name__ == "__main__":
    main()
