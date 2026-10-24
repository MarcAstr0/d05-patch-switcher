# Assets

Everything committed here must be redistributable under a licence compatible with
MIT, and must not reproduce Roland's trademarks (the Roland logo, the Boutique
wordmark artwork, the D-50 or D-05 logotypes as drawn by Roland). Naming the
product in plain text is fine; copying its artwork is not.

| Path | What goes there |
|---|---|
| `fonts/` | Fonts used by the UI, each with its licence file next to it. Open Font Licence or similar only. |
| `icons/` | The application icon as SVG (source) plus the rasters the packagers need (`icon-256.png`, `icon.icns`, `icon.ico`). |
| `local/` | **Git-ignored.** Anything the owner wants on screen that cannot be published, e.g. a manufacturer logo. The app also looks for `logo.png` in its data directory at runtime. |

Raster chrome (gradients, bezels, panel textures) is drawn with QPainter or QSS,
not shipped as images, unless the designer documents why an image is cheaper.
