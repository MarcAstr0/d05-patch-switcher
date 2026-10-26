# Data

## `d05-presets.tsv`

The 384 factory preset patches of the Roland Boutique D-05: six preset groups
(P1–P6) × eight banks × eight numbers. Tab-separated, UTF-8, one header row.

| Column | Meaning |
|---|---|
| `group` | `P1` … `P6`. P1 = Original D-50, P2 = New for D-05, P3–P6 = the PN-D50-01 … 04 ROM cards |
| `bank` | 1–8, the PATCH BANK button |
| `number` | 1–8, the NUMBER button |
| `name` | the patch name exactly as printed in the guide, quirks included (`Griitttarr`, `Zean- - -`) |
| `mode` | key mode: `W` whole, `D` dual, `S` split |

The front-panel address is `<group>-<bank><number>`, e.g. `P1-11` is *Fantasia*.
The MIDI mapping is in `docs/MIDI.md`.

### Provenance

Extracted from the *D-05 Parameter Guide* (Roland, `D-05_ParameterGuide_eng02_W.pdf`,
2018), "Sound List", pages 23–28, with `scripts/extract-presets.py`, which reads
word coordinates (`pdftotext -bbox`) rather than the flowed text so that the two
names the PDF wraps onto the bank-label line (`PressureMe Strings`, `Strings Horn`)
land in the right column. Every name was then checked against the plain-text
extraction of the same pages: 384 of 384 matched.

To regenerate:

```sh
pdftotext -bbox -f 23 -l 28 D-05_ParameterGuide_eng02_W.pdf /tmp/sound-list.html
python3 scripts/extract-presets.py /tmp/sound-list.html data/d05-presets.tsv
```

The PDF itself is Roland's and is not committed.

### User banks

U1–U8 (another 512 addresses) have no fixed names: they hold whatever the owner
wrote there. The application shows them by address and lets the user name them;
those names live in the user's library file, not here.

## `jp08-patches.tsv`

The 64 factory patches of the Roland Boutique JP-08: eight banks × eight numbers.
Tab-separated, UTF-8, one header row: `bank`, `number`, `name`.

| Column | Meaning |
|---|---|
| `bank` | 1–8, the front-panel bank |
| `number` | 1–8, the patch number within the bank |
| `name` | the patch name exactly as printed in the sound list |

The front-panel address is `<bank>-<number>`, e.g. `1-1` is *NEG Sync*. Names are
unique within the table.

## `ju06a-patches.tsv`

The 128 factory patches of the Roland Boutique JU-06A: two sound engines
(Juno-60 and Juno-106 modes) × eight banks × eight numbers. Same format, one header
row: `engine`, `bank`, `number`, `name`.

| Column | Meaning |
|---|---|
| `engine` | `60` or `106`, the engine the patch belongs to |
| `bank` | 1–8 |
| `number` | 1–8 |
| `name` | the patch name exactly as printed, including `’` and `&` (`Louis’ Weapon`, `Brass & Strings`) |

Rows are in catalogue order: all of `60`, then all of `106`. The same address exists in
both engines with different names (`60:1-1` is *LD Echolocator*, `106:1-1` is *Brass*),
so the key is `engine` + `bank` + `number`; names are unique within an engine, and a few
repeat across engines.

### Provenance

Extracted from Roland's *JP-08 Sound List* (`JP-08_Sound_List_e01_W.pdf`) and
*JU-06A Sound List* (`JU-06A_Sound_List_e01_W.pdf`), one page each, with
`scripts/extract-boutique-lists.py`, which reads `pdftotext -layout` output (columns are
separated by runs of two or more spaces; names contain single spaces only). The script
asserts 64 and 64 + 64 rows with no repeated address and no empty name. Every name was
then checked against the plain-text page: 192 of 192 matched, none fixed by hand.

To regenerate:

```sh
python3 scripts/extract-boutique-lists.py JP-08_Sound_List_e01_W.pdf \
    JU-06A_Sound_List_e01_W.pdf data/jp08-patches.tsv data/ju06a-patches.tsv
```

The PDFs are Roland's and are not committed. Both tables are embedded as Qt resources
(`:/data/jp08-patches.tsv`, `:/data/ju06a-patches.tsv`) and loaded by `PatchCatalog`.
