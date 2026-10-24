#!/usr/bin/env python3
"""Regenerate the combo-box arrow rasters (DESIGN.md 10.2) without antialiasing.

    python3 assets/theme/make-arrows.py

Writes arrow-down.png (8 x 4) and arrow-down@2x.png (16 x 8) next to this script:
a solid downward wedge in `arrow` #BDB9A0 with 45-degree steps (rows 8, 6, 4, 2 px
wide at 1x), on a transparent background. arrow-down.svg is the same drawing as a
vector source. Rasters are shipped because the app links only Qt Widgets and a QSS
`image: url(...svg)` would need the QtSvg image-format plugin on every platform.
Needs Pillow.
"""
from pathlib import Path

from PIL import Image

ARROW = (0xBD, 0xB9, 0xA0, 255)
HERE = Path(__file__).resolve().parent


def wedge(scale: int) -> Image.Image:
    width, rows = 8 * scale, 4 * scale
    img = Image.new("RGBA", (width, rows), (0, 0, 0, 0))
    for y in range(rows):
        # one device pixel in per row on each side: 45-degree edges at any scale
        for x in range(y, width - y):
            img.putpixel((x, y), ARROW)
    return img


if __name__ == "__main__":
    wedge(1).save(HERE / "arrow-down.png", optimize=True)
    wedge(2).save(HERE / "arrow-down@2x.png", optimize=True)
