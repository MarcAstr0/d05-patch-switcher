# Fonts

| File | What it is | Size |
|---|---|---|
| `Inter-Regular.ttf` | Inter 4.1 Regular, subsetted | 57,968 bytes |
| `Inter-Medium.ttf` | Inter 4.1 Medium, subsetted | 58,816 bytes |
| `OFL.txt` | The release's `LICENSE.txt`, unchanged: SIL Open Font License 1.1 | 4,380 bytes |

Together the two fonts are 116,784 bytes (114 KB), against 829 KB for the originals.

## Licence

Inter is Copyright (c) 2016 The Inter Project Authors, licensed under the SIL Open
Font License 1.1 (`OFL.txt`). The licence names no Reserved Font Name, so the
subsetted files keep the name `Inter`. Under the OFL they are Modified Versions; they
must stay under the OFL and must not be sold on their own.

## Source

- Project: <https://github.com/rsms/inter>, release v4.1.
- Download: <https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip>
- sha256 of `Inter-4.1.zip`: `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`
- Files used from the zip, with the sha256 of each before subsetting:

| Path in the zip | sha256 | Size |
|---|---|---|
| `extras/ttf/Inter-Regular.ttf` | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` | 411,640 |
| `extras/ttf/Inter-Medium.ttf` | `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` | 417,300 |
| `LICENSE.txt` (committed as `OFL.txt`) | `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a` | 4,380 |

The font version string inside both files is `Version 4.001;git-9221beed3`.

## Subsetting (reproducible)

`pyftsubset` from fontTools 4.67.0 (`pip install fonttools==4.67.0`, in a virtual
environment; not a system package). From the directory where the zip was unpacked:

```sh
for w in Regular Medium; do
  pyftsubset extras/ttf/Inter-$w.ttf \
    --unicodes='U+0020-007E,U+00A0-017F,U+2018-201E,U+2022,U+2026,U+2190-2193,U+21E7,U+2318,U+2325,U+2212' \
    --layout-features='kern,tnum,case' \
    --name-IDs='*' \
    --output-file=assets/fonts/Inter-$w.ttf
done
cp LICENSE.txt assets/fonts/OFL.txt
```

The unicode ranges and features are from `docs/design/DESIGN.md` section 3.1:
printable ASCII, Latin-1 Supplement, Latin Extended-A, curly quotes, bullet, ellipsis,
arrows, minus, and the macOS key symbols (shift, command, option). Every other
pyftsubset option is left at its default; in particular TrueType hinting is kept.

The command is deterministic: a second run produced byte-identical files. sha256 of
the committed results:

| File | sha256 |
|---|---|
| `Inter-Regular.ttf` | `b96b3ee811085da1dd9fc39fef3f612d7499968315b175774a65e321e0c84eb4` |
| `Inter-Medium.ttf` | `cf3845a6a7ef729a1251cb6fa5ee4ff1d1ca3e33dc0736676dc74c6f9b7f7087` |

Checked after subsetting (fontTools and Qt 6.4.2):

- Every requested code point is present except U+00AD (soft hyphen) and U+0149
  (deprecated `ŉ`), which Inter 4.1 does not contain.
- GSUB/GPOS keep exactly `case`, `kern`, `tnum`.
- Registered with `QFontDatabase::addApplicationFont` on a fontconfig with no system
  fonts: `Inter-Regular.ttf` reports family `Inter`; `Inter-Medium.ttf` reports
  `Inter` and `Inter Medium`. `QFont("Inter")` with `setWeight(QFont::Medium)`
  resolves to style `Medium`, weight 500, and with the default weight to `Regular`, 400.

Characters outside the subset fall back to a system font through Qt (accepted in
DESIGN.md 3.1).
