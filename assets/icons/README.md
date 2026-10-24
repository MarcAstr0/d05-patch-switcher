# Application icon

Drawn for this project from `docs/design/DESIGN.md` section 14.1: a charcoal tile, a
green dot-matrix window showing `P11` (glyphs from DESIGN.md Appendix A), and three
panel buttons with the middle LED lit. No trademarked artwork, no text objects.

| File | What it is | Size |
|---|---|---|
| `d05-patch-switcher.svg` | Source, 256 x 256 viewBox | 2,138 bytes |
| `d05-patch-switcher.png` | 256 x 256 RGBA, for the AppImage and the Linux desktop file | 3,519 bytes |
| `d05-patch-switcher.ico` | Windows: 16, 24, 32, 48, 64, 128, 256 px, each stored as PNG | 9,751 bytes |
| `d05-patch-switcher.icns` | macOS: `ic07`..`ic14` (16@2x to 512@2x), PNG entries | 42,317 bytes |

`P11` stays legible from 24 px up. At 16 px the dots merge into a green block, which
still reads as the display.

## Regenerating

Run from this directory; `$T` is any empty temporary directory.

256 px PNG (Inkscape 1.2):

```sh
inkscape d05-patch-switcher.svg --export-type=png --export-width=256 \
  --export-filename=d05-patch-switcher.png
```

Intermediate sizes, used by both `.ico` and `.icns`:

```sh
for s in 16 24 32 48 64 128 256 512 1024; do
  inkscape d05-patch-switcher.svg --export-type=png --export-width=$s \
    --export-filename=$T/icon-$s.png
done
```

`.ico` with `icotool` (Debian/Ubuntu package `icoutils`). `-r` stores each size as a
PNG (Windows Vista and later), which keeps the file under 10 KB:

```sh
icotool -c -o d05-patch-switcher.ico \
  -r $T/icon-16.png -r $T/icon-24.png -r $T/icon-32.png -r $T/icon-48.png \
  -r $T/icon-64.png -r $T/icon-128.png -r $T/icon-256.png
```

`.icns` on Linux with Pillow (12.2 was used):

```sh
python3 -I -c "
from PIL import Image
import sys
t = sys.argv[1]
Image.open(t + '/icon-1024.png').save('d05-patch-switcher.icns',
    append_images=[Image.open('%s/icon-%d.png' % (t, s)) for s in (16, 32, 64, 128, 256, 512)])
" $T
```

Pillow writes the retina and large entries (`ic07` 128, `ic08` 256, `ic09` 512, `ic10`
1024, `ic11` 32, `ic12` 64, `ic13` 256, `ic14` 512) but not the 1x 16 and 32 px
entries (`icp4`, `icp5`); macOS scales down from the 32 and 64 px images. The
canonical alternative on a Mac, which writes the full set:

```sh
mkdir -p $T/d05.iconset
for s in 16 32 128 256 512; do
  cp $T/icon-$s.png          $T/d05.iconset/icon_${s}x${s}.png
  cp $T/icon-$((s * 2)).png  $T/d05.iconset/icon_${s}x${s}@2x.png
done
iconutil -c icns -o d05-patch-switcher.icns $T/d05.iconset
```
