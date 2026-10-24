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
