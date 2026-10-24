# Application icon

Drawn for this project from `docs/design/DESIGN.md` section 14.1: a charcoal tile, a
green dot-matrix window showing `P11` (glyphs from DESIGN.md Appendix A), and three
panel buttons with the middle LED lit. No trademarked artwork, no text objects.

| File | What it is | Size |
|---|---|---|
| `d05-patch-switcher.svg` | Source, 256 x 256 viewBox | 2,138 bytes |
| `d05-patch-switcher.png` | 256 x 256 RGBA, for the AppImage and the Linux desktop file | 3,519 bytes |
| `d05-patch-switcher-{16,24,32}.png` | Hand placed on whole pixels by `make-small-icons.py` (not from the SVG) | 280 to 430 bytes |
| `d05-patch-switcher-{48,64,128}.png` | The SVG rendered at each size | 1,051 to 1,949 bytes |
| `d05-patch-switcher.ico` | Windows: 16, 24, 32, 48, 64, 128, 256 px, each stored as PNG | 8,893 bytes |
| `d05-patch-switcher.icns` | macOS: `icp4` 16, `icp5` 32, `ic07` 128, `ic08` 256, `ic09` 512, `ic11` 16@2x, `ic12` 32@2x, `ic13` 128@2x, `ic14` 256@2x, `ic10` 512@2x; PNG entries | 43,184 bytes |
| `make-small-icons.py` | Writes the 16, 24 and 32 px PNGs (Pillow) | |
| `make-icns.py` | Writes the `.icns` from a directory of PNGs (standard library only) | |

The PNG sets (`-16` to `-128`) are the window icon (Qt resource) and
`share/icons/hicolor/<n>x<n>/apps/` on install.

Why the small ones are hand placed: rendered from the SVG, the 16 px icon's dots merged
into a green block and the 24 and 32 px ones were soft. `make-small-icons.py` draws
`P11` on whole pixels (a 3 x 5 cut of the letters at 16 px, the Appendix A 5 x 7 glyphs
at 24 and 32 px) with the same colour tokens; only the rounded tile corners carry
partial alpha. The 32 px file is also the `.icns` 16@2x entry and the `.ico` 32 px entry.

## Regenerating

Run from this directory; `$T` is any empty temporary directory. Tools: Inkscape 1.2,
Pillow 12.2 (for the small icons), `icotool` (Debian/Ubuntu package `icoutils`).

Small icons (writes `d05-patch-switcher-16.png`, `-24`, `-32` here):

```sh
python3 -I make-small-icons.py
```

256 px PNG and the SVG-rendered sizes:

```sh
inkscape d05-patch-switcher.svg --export-type=png --export-width=256 \
  --export-filename=d05-patch-switcher.png
for s in 48 64 128; do
  inkscape d05-patch-switcher.svg --export-type=png --export-width=$s \
    --export-filename=d05-patch-switcher-$s.png
done
```

Collect every size under the names the `.ico` and `.icns` commands use:

```sh
for s in 16 24 32 48 64 128; do cp d05-patch-switcher-$s.png $T/icon-$s.png; done
cp d05-patch-switcher.png $T/icon-256.png
for s in 512 1024; do
  inkscape d05-patch-switcher.svg --export-type=png --export-width=$s \
    --export-filename=$T/icon-$s.png
done
```

`.ico` with `icotool`. `-r` stores each size as a PNG (Windows Vista and later), which
keeps the file under 10 KB:

```sh
icotool -c -o d05-patch-switcher.ico \
  -r $T/icon-16.png -r $T/icon-24.png -r $T/icon-32.png -r $T/icon-48.png \
  -r $T/icon-64.png -r $T/icon-128.png -r $T/icon-256.png
```

`.icns` with the script (the same entry set Apple's `iconutil` writes; Pillow's writer
omits `icp4` and `icp5`):

```sh
python3 -I make-icns.py d05-patch-switcher.icns $T
```

Checks used (any one is enough): `icotool -l d05-patch-switcher.ico` lists 7 images;
`icnsutil info -v d05-patch-switcher.icns` and `icnsutil test` (pip package
`icnsutil`, installed in a throwaway venv) list the 10 entries above and report `OK`;
Pillow opens the `.icns` with sizes 16, 32, 128, 256 and 512 at scale 1 and 2.

On a Mac, the canonical alternative writes the same set:

```sh
mkdir -p $T/d05.iconset
for s in 16 32 128 256 512; do
  cp $T/icon-$s.png          $T/d05.iconset/icon_${s}x${s}.png
  cp $T/icon-$((s * 2)).png  $T/d05.iconset/icon_${s}x${s}@2x.png
done
iconutil -c icns -o d05-patch-switcher.icns $T/d05.iconset
```
