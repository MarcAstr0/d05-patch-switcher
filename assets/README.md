# Assets

Everything committed here must be redistributable under a licence compatible with
MIT, and must not reproduce Roland's trademarks (the Roland logo, the Boutique
wordmark artwork, the D-50 or D-05 logotypes as drawn by Roland). Naming the
product in plain text is fine; copying its artwork is not.

| Path | What goes there |
|---|---|
| `fonts/` | Fonts used by the UI, each with its licence file next to it. Open Font Licence or similar only. |
| `icons/` | The application icon as SVG (source) plus the rasters the packagers need (`d05-patch-switcher.png`, `d05-patch-switcher.icns`, `d05-patch-switcher.ico`). `icons/README.md` has the commands that make them. |
| `theme/` | `app.qss` (the style sheet, with the colour tokens as its header comment) and the combo-box arrow (`arrow-down.svg` source, `arrow-down.png` and `@2x` rasters, `make-arrows.py`). |
| `local/` | **Git-ignored.** Anything the owner wants on screen that cannot be published, e.g. a manufacturer logo. The app also looks for `logo.png` in its data directory at runtime. |

Raster chrome (gradients, bezels, panel textures) is drawn with QPainter or QSS,
not shipped as images, unless the designer documents why an image is cheaper.
The one documented exception is the combo-box arrow (`theme/arrow-down*.png`, 8 x 4
and 16 x 8 px). QSS can show an image but cannot draw a wedge: a border-drawn
triangle renders as a bar in Qt 6.4, and an SVG would need the QtSvg plugin, which
counts as a new dependency. See `docs/design/DESIGN.md` 10.2.
