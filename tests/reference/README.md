# D-05 reference screenshots

These PNGs are every version 1 `--screenshot-state` at 1x and 2x, rendered by the `v1.0.0` tag
(see `tests/test_reference.cpp`). The exact-pixel comparison is valid only against the setup that
rendered them:

- Qt: 6.4.2 (Ubuntu 24.04 `qt6-base-dev`)
- Platform: Linux, `QT_QPA_PLATFORM=offscreen` (`QT_SCALE_FACTOR=2` for the `@2x` files)
- Fonts: the bundled Inter Regular and Medium from `assets/fonts` only; no system font is used
- Text rasterisation: FreeType through the offscreen plugin

`test_reference` runs the comparison only when the runtime Qt version equals the one above
(`kReferenceQt` in the test) and the platform is Linux; otherwise it skips with a message naming
both versions. To regenerate after an intended change on a different Qt, render each state with
`./build/d05-patch-switcher --screenshot tests/reference/<state>@<scale>x.png --screenshot-state <state>`
under the environment above, review the diff, and update `kReferenceQt` in the same commit.
