# Design addendum: the web version

Task W2. The browser version of the panel, for the web builder (`docs/WEB.md`, task W3).
`docs/design/DESIGN.md` stays the source of truth for everything it covers; this file
says only what changes in a browser and specifies the two layouts the desktop app does
not have (phone portrait and phone landscape). Where this file is silent, DESIGN.md
applies unchanged, with "logical px" read as "CSS px". The builder invents nothing
visual; if a value is missing here or in DESIGN.md, ask the designer.

All sizes are **CSS px** unless marked "device px". Coordinates are integers, x grows
right, y grows down, a rect is `(x, y, w, h)`, as in DESIGN.md. `W` and `H` are the
layout viewport (`document.documentElement.clientWidth` and `clientHeight`; the page
never shows a page scrollbar, see 4), `dpr` is `window.devicePixelRatio`.

Contents: 1 What changes · 2 Tokens · 3 Type and fonts · 4 Layouts and breakpoints ·
5 Display on a canvas · 6 MIDI access panel · 7 Phone portrait · 8 Phone landscape ·
9 Components in HTML and CSS · 10 Focus · 11 Dialogs and menus · 12 Motion, footprint,
drawing rules · 13 PWA visuals · 14 Mockups · 15 Readings and open items · 16 Sources.

---

## 1. What changes in a browser

| Desktop (DESIGN.md) | Web |
|---|---|
| Native title bar `D-05 Patch Switcher` | None. The page title is the same string. The layout starts at y 0 exactly as the Qt client area does, so every DESIGN.md coordinate holds |
| Window min 800 x 480, default 960 x 560 | No minimum. At `W >= 800` and `H >= 480` the desktop layout is DESIGN.md; below that the phone layouts take over (4) |
| Logical px and the Qt DPR | CSS px and `devicePixelRatio`, which also changes with browser zoom (5.1) |
| Qt focus states | `:focus-visible` for buttons, `:focus` for the search field and list (10) |
| Separate dialog windows | In-page `<dialog>` elements with a title strip; full-screen sheets on phones (11) |
| `logo.png` in the data dir | No file system. The nameplate always shows the wordmark (15, item 5) |
| Port missing = `NO MIDI OUTPUT` | Same, plus three browser states before any port exists: unsupported, permission not yet given, permission denied (6) |
| `SYSTEM > Quit` | Gone. `Import Library...` and `Export Library` are added (11.6) |
| Styled `QToolTip` | The browser's own tooltip from the `title` attribute (accepted deviation from DESIGN.md 8.3: a styled tooltip needs a hover timer) |

---

## 2. Tokens

`docs/design/tokens.json` (version 2 since T8.1, DESIGN-2.md 2.2) is the single source of
every colour and type value. Same names as DESIGN.md sections 2 and 3. The web reads only
its version 1 subset (the 41 colours of groups 2.1 to 2.4, the 13 version 1 type roles,
`webColors`, `webType`, `size`, `breakpoints`): the web stays the D-05.
`docs/design/gen-tokens.py` (Python 3, stdlib only) writes:

- `assets/theme/<skin>.qss`, one per desktop skin, from the template
  `assets/theme/app.qss.in` (DESIGN-2.md 2.5), and `src/ui/SkinTables.inc`, the desktop
  runtime palette, and
- `web/src/ui/theme.css`: one custom property per token (`--panel`, `--lcdOn`, ...),
  per type role (`--type-<role>-weight`, `-size`, `-spacing`, `-line`), per size
  (`--size-desktop-<name>`, `--size-phone-<name>`), and one class per type role
  (`.type-panelLabel` sets family, weight, size, letter spacing, line height and the
  role's colour when it has a fixed one).

`python3 docs/design/gen-tokens.py --check` writes nothing and fails if a `<skin>.qss`,
`theme.css`, `SkinTables.inc`, the `ui/Theme` sources, `docs/design/mockup/build.py`,
the DESIGN-2.md 2.2 invariants or the line heights disagree with `tokens.json`, or if
`app.qss.in` holds a literal hex or an unknown placeholder. Suggested for the `web` CI
job and the desktop job alike. To change a colour: edit `tokens.json`, run the
generator (the style sheets, `theme.css` and the runtime palette follow), update
DESIGN.md's or DESIGN-2.md's table.

Rules for `web/`:

1. No hex, `rgb()` or named colour anywhere in `web/src` except `theme.css`. Every
   colour is `var(--<token>)`. The only exceptions are `index.html`'s
   `<meta name="theme-color">` and the manifest (13), which cannot read CSS variables;
   both use `panel` `#202326`.
2. One web-only colour exists, `--scrim`: `bezel` at alpha 0.72, `rgba(5, 6, 6, 0.72)`,
   behind modal dialogs (11). Nothing else is added.
3. `theme.css` is imported first; nothing overrides its custom properties.

---

## 3. Type and fonts

### 3.1 Files the web serves

The same Inter 4.1 subset as the desktop (`assets/fonts/`, DESIGN.md 3.1), converted to
WOFF2 without any other change. Both weights:

| Served path (`web/public/`) | From | Bytes | sha256 |
|---|---|---|---|
| `fonts/Inter-Regular.woff2` | `assets/fonts/Inter-Regular.ttf` | 22,664 | `a8a0bf4b3eecae1b2fe703cd2f7d04a6e997395310ab1d112df072dfa06d4375` |
| `fonts/Inter-Medium.woff2` | `assets/fonts/Inter-Medium.ttf` | 23,084 | `9a5c338ed57e8636b4619de0e119776aeb7f3582f3e5da21a504571bc7741185` |
| `fonts/OFL.txt` | `assets/fonts/OFL.txt`, unchanged | 4,380 | as in `assets/fonts/README.md` |

45.7 KB together (the TTFs are 114 KB). The fonts are outside the 60 KB JS + CSS budget
(WEB.md 4). Conversion, in a scratch virtual environment (not a system package, no
network at run time). `recalcTimestamp=False` keeps the TTF's `head.modified`: without
it fontTools writes the current time into `head`, and every run gives different bytes
and a slightly different size (that is why the first version of this command, without
the flag, produced 22,644 / 23,052 here and 22,744 / 23,060 for the builder). With the
flag, two runs at different times gave identical bytes (checked 2026-10-24, Python
3.12.4, fonttools 4.67.0, brotli 1.2.1):

```sh
python3 -m venv /tmp/venv-fonts
/tmp/venv-fonts/bin/pip install fonttools==4.67.0 brotli==1.2.1
for w in Regular Medium; do
  /tmp/venv-fonts/bin/python -I -c "
import sys
from fontTools.ttLib import TTFont
f = TTFont(sys.argv[1], recalcTimestamp=False); f.flavor = 'woff2'; f.save(sys.argv[2])
" assets/fonts/Inter-$w.ttf web/public/fonts/Inter-$w.woff2
done
cp assets/fonts/OFL.txt web/public/fonts/OFL.txt
```

Checked on the outputs: same glyph count (446), same cmap, same `head.modified` as the
TTF, family `Inter`, GSUB features `case` and `tnum` kept (kerning is in GPOS, kept). Record the command and hashes in a
`web/public/fonts/README.md` (builder, W3). The OFL allows this: no Reserved Font Name,
and the WOFF2 files stay under the OFL.

### 3.2 `@font-face`

In the web builder's own CSS (not in `theme.css`, because the URL depends on Vite's
base path); with `public/` files Vite serves them at `<base>fonts/...`:

```css
@font-face {
  font-family: "Inter";
  src: url("/fonts/Inter-Regular.woff2") format("woff2");
  font-weight: 400;
  font-style: normal;
  font-display: block;
  unicode-range: U+0020-007E, U+00A0-017F, U+2018-201E, U+2022, U+2026, U+2190-2193, U+21E7, U+2318, U+2325, U+2212;
}
@font-face {
  font-family: "Inter";
  src: url("/fonts/Inter-Medium.woff2") format("woff2");
  font-weight: 500;
  font-style: normal;
  font-display: block;
  unicode-range: U+0020-007E, U+00A0-017F, U+2018-201E, U+2022, U+2026, U+2190-2193, U+21E7, U+2318, U+2325, U+2212;
}
```

and in `index.html`, before any stylesheet:

```html
<link rel="preload" href="fonts/Inter-Regular.woff2" as="font" type="font/woff2" crossorigin>
<link rel="preload" href="fonts/Inter-Medium.woff2" as="font" type="font/woff2" crossorigin>
```

**`font-display: block`**, with the preloads. The files are 23 KB each, requested before
the CSS is parsed and served by the service worker from cache after the first visit, so
the block period ends in milliseconds; a swap would flash the panel lettering in a
system face, which is exactly the "modern app" look the brief rules out. If the fonts
fail to load, the browser falls back after its block period (about 3 s) to
`--font-family`'s fallbacks, `"Helvetica Neue", Arial, sans-serif`. The service worker
precaches both WOFF2 files with the app shell.

### 3.3 Roles

Every DESIGN.md 3.2 role is used unchanged. Two web roles are added (`tokens.json`
`webType`):

| Role | Face | Size | Spacing | Line | Case | Colour | Used for |
|---|---|---|---|---|---|---|---|
| `type.padLabel` | Medium | 12 | 0.5 | 15 | Caps | per use | Legends inside phone pads and buttons: group legends, numerals in pads, `ALL` `FAVOURITES` `RECENTS`, `SYSTEM`, `PREV` `NEXT`, `DONE`, `CONNECT MIDI`, phone dialog buttons |
| `type.dialogTitle` | Medium | 11 | 1.0 | 14 | Caps | `labelPrimary` | The title in a dialog's title strip (11.1) |

CSS mapping for every role:

- `font-weight` 400 or 500, `font-size` in px, `letter-spacing` in px (CSS adds it after
  every character, as Qt's `AbsoluteSpacing` does).
- `line-height` is the role's **Line** value in px (`--type-<role>-line`): ascent plus
  descent rounded, as `QFontMetrics` gives (12, 14, 15, 16, 17, 19 px for sizes 10, 11,
  12, 13, 14, 15; the generator verifies them against the font files). Never
  `line-height: normal` (it gives 12.1 px at 10 px). Vertical centring of a single line
  in a box of height `h` is then `top + floor((h - line) / 2)`, matching DESIGN.md.
- "Caps" strings are stored in capitals by the code. No `text-transform`, no
  `font-variant-caps`.
- `font-feature-settings` is left at the default (kerning on). `tnum` is not used: the
  list address column draws each character in a 9 px cell exactly as DESIGN.md 7.2
  (five inline-block spans, `width: 9px; text-align: center`, or absolutely positioned),
  so the web and desktop columns match.
- `-webkit-font-smoothing` and `text-rendering` are not set.

---

## 4. Layouts and breakpoints

### 4.1 Which layout

Exactly one of three, chosen from the layout viewport:

| Layout | Condition | Media query |
|---|---|---|
| **Desktop** | `W >= 800` and `H >= 480` | `(min-width: 800px) and (min-height: 480px)` |
| **Landscape** | not desktop, `W >= 680` and `W > H` | `(min-width: 680px) and (orientation: landscape)` and not desktop |
| **Portrait** | everything else | default |

Landscape has two densities: **normal** when `H >= 380`, **compact** when `H <= 379`
(`(max-height: 379px)`), section 8. A desktop browser window narrowed below 800 or
shortened below 480 takes the matching phone layout, and works with a mouse.

The layout is re-evaluated on every `resize` (which browser zoom also fires) and on
`orientationchange`; switching layout re-creates no state (current patch, list scroll,
search text, view and pending entry all survive). The display canvas recomputes its
geometry on the same events (5.1).

### 4.2 Page frame (all layouts)

```html
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
```

- `html, body`: margin 0, `background: var(--panel)`, `color: var(--labelPrimary)`,
  `font: 400 13px/16px var(--font-family)` (the application font, `type.body`),
  `overflow: hidden`, `overscroll-behavior: none`, `-webkit-tap-highlight-color:
  transparent`.
- The app root is `position: fixed; inset: 0` with padding
  `env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom)
  env(safe-area-inset-left)`. All layout coordinates in this file are inside that
  padding. Height uses `100dvh` semantics (the fixed root does this by itself).
- Panel parts (buttons, rules, numerals, status bar, display) have `user-select: none`
  and `touch-action: manipulation` (no double-tap zoom). Fields and dialog text keep the
  defaults. Pinch zoom is not disabled.
- The page itself never scrolls. Only these scroll: the list, the group strip and the
  favourites chips (horizontally), dialog content, and, as a fallback only, the phone
  band when the viewport is shorter than the layout's minimum (7.1, 8.1).

### 4.3 Desktop layout (`W >= 800`, `H >= 480`)

DESIGN.md section 4 to 10, unchanged in geometry, with `W` and `H` read from the layout
viewport. Differences:

1. **Nameplate**: always the wordmark (DESIGN.md 4.5, "Absent"). No `logo.png` slot on
   the web (15, item 5).
2. **Disabled panel while MIDI is not available**: section 6.
3. **SYSTEM menu**: section 11.6.
4. **Tooltips**: `title` attributes with the same texts as the desktop tooltips.
5. **Status bar**: unchanged. The right-aligned message also carries the import result
   (11.6).
6. **No size grip, no resize handling beyond 4.1**: the right column and the list take
   all extra width and height exactly as DESIGN.md 4.4.

---

## 5. Display on a canvas

### 5.1 Geometry and crisp dots

One `<canvas>` for the lit field, inside a `<div>` that draws the bezel (fill `bezel`,
1 px frame `bezelRimTop` on top and `bezelRim` on the other sides, square corners, as
DESIGN.md 5.1). The canvas sits at the bezel's `(padX, padY)`.

The dot pitch `P` is always an integer number of **device** pixels, `g =
clamp(round(dpr), 1, floor(P / 3))` with round half up (`Math.round`), `d = P - g`. A
line of `n` columns has a field of `(6n + 3) P x 19 P` device px (40 columns: `243 P`;
20 columns: `123 P`). The canvas's CSS size is `fieldW x fieldH` with
`fieldW = ceil((6n + 3) P / dpr)` and `fieldH = ceil(19 P / dpr)` (whole CSS px, as
DESIGN.md does at fractional DPR).

How `P` is chosen:

| Layout | Columns | Pitch rule | Bezel padding (x, y) |
|---|---|---|---|
| Desktop | 40 | DESIGN.md 5.1 "Choosing `P`" exactly, with the step `p` from `W` (3 below 1042, 4 below 1285, else 5). Same thresholds, same tie rule, same `1e-9` guard | 19, 11 |
| Portrait | 20 | `P = floor(dpr * (spanW - 20) / 123)`: the largest whole device pitch whose field fits the face span (7.1) | 10, 7 |
| Landscape normal | 20 | `P = min(floor(dpr * (spanW - 20) / 123), floor(3 * dpr))` (field pitch at most 3 CSS px) | 10, 7 |
| Landscape compact | 20 | as normal with `floor(2 * dpr)` (at most 2 CSS px) | 10, 7 |

All `floor` calls add `1e-9` before flooring, as DESIGN.md does. On the phone layouts
the "design width" is the available width, so "nearest that fits" is simply "largest
that fits".

**Device pixel backing store.** Observe the canvas with
`new ResizeObserver(cb).observe(canvas, { box: "device-pixel-content-box" })` and size
the backing store from `entry.devicePixelContentBoxSize[0]` (`inlineSize` x
`blockSize`); where that entry is missing, use `round(fieldW * dpr) x round(fieldH *
dpr)`. The dot area (`(6n - 1) P x 15 P`) is drawn at the integer device offset
`(floor((backingW - (6n - 1) P) / 2), floor((backingH - 15 P) / 2))`, which is `2P` at
1x and 2x. Draw with `imageSmoothingEnabled = false`, never scale. This is what keeps
the dots square and equal at 1.25, 1.5, 1.75, 2.625 and 3.

**When to recompute.** On `resize`, on a change of `matchMedia("(resolution: <dpr>dppx)")`
(re-armed with the new value after each change; browser zoom changes `dpr`), and on a
layout switch. Compute `P` and `g`; if both are unchanged and the backing size is
unchanged, do nothing. If `P` or `g` changed, rebuild the glyph atlas. If the backing
size changed, set `canvas.width/height` (which clears it) and redraw every cell once.

Computed (from `docs/design/mockup-web/build.py --print-geometry`):

| Viewport @ dpr | Layout | P dev | Pitch CSS | g | d | Field CSS | Bezel CSS | 40 columns would be |
|---|---|---|---|---|---|---|---|---|
| 960 x 560 @ 1 | desktop | 3 | 3 | 1 | 2 | 729 x 57 | 767 x 79 | (40 columns) |
| 960 x 560 @ 2 | desktop | 6 | 3 | 2 | 4 | 729 x 57 | 767 x 79 | |
| 960 x 560 @ 1.25 | desktop | 4 | 3.2 | 1 | 3 | 778 x 61 | 816 x 83 | |
| 360 x 640 @ 2 (owner's Redmi 10A, inferred) | portrait | 5 | 2.5 | 1 | 4 | 308 x 48 | 328 x 62 | pitch 1 CSS px, glyphs 6.5 px tall |
| 390 x 844 @ 3 | portrait | 8 | 2.67 | 2 | 6 | 328 x 51 | 348 x 65 | pitch 1.33, glyphs 8.7 px |
| 390 x 844 @ 2 | portrait | 5 | 2.5 | 1 | 4 | 308 x 48 | 328 x 62 | |
| 393 x 851 @ 2.75 | portrait | 7 | 2.55 | 2 | 5 | 314 x 49 | 334 x 63 | |
| 412 x 915 @ 2.625 | portrait | 7 | 2.67 | 2 | 5 | 328 x 51 | 348 x 65 | pitch 1.14, glyphs 7.2 px |
| 768 x 1024 @ 2 | portrait | 8 | 4 | 2 | 6 | 492 x 76 | 512 x 90 | |
| 844 x 390 @ 3 | landscape | 8 | 2.67 | 2 | 6 | 328 x 51 | 348 x 65 | |
| 800 x 360 @ 2 | landscape compact | 4 | 2 | 1 | 3 | 246 x 38 | 266 x 52 | |

### 5.2 Why 20 columns on a phone

Forty columns across a 360 px phone give a 1 CSS px pitch: 6.5 px tall glyphs, not
readable at arm's length on the synth. Twenty columns give 2.5 to 2.7 px, glyphs about
17 px tall, about the size of the D-05's own 16-column display (REFERENCES 2.1). The
phone keeps the two lines and the 5 x 7 glyph table; only the column count and the
content order change. Both phone layouts use 20 columns.

### 5.3 Content

**Desktop layout: 40 columns**, DESIGN.md 5.2 and 5.3 unchanged, with three web states
added at the top of the line-2 priority list (6.1 says when each applies):

| Priority | Condition | Line 2 (40 columns) |
|---|---|---|
| W1 | No Web MIDI, or the page is not a secure context | `NO WEB MIDI IN THIS BROWSER` |
| W2 | Permission denied | `MIDI ACCESS BLOCKED` |
| W3 | Permission not yet given (including while the browser's prompt is open) | `PRESS CONNECT MIDI` |
| 1a | Access granted, no output port ever configured | `CHOOSE A PORT IN SETTINGS` (replaces DESIGN.md's `OPEN SETTINGS: CTRL+,`: the browser may own `Ctrl+,`/`Cmd+,`, see 15 item 7) |
| 1b to 4 | as DESIGN.md 5.2 | `NO MIDI OUTPUT`, `SENT`, `<n> MATCHES`, group long name |

**Phone layouts: 20 columns.** Line 1 holds the name, line 2 the address and a
14-character context. The name moves to its own line so that no preset name is ever
cut (145 of the 384 names are longer than the 14 characters that `P1-11:` would leave;
the longest is 20).

Line 1, columns 1 to 20: the name as stored (mixed case), left aligned, cut at 20
(never needed). Unnamed user slot: blank. During an entry (DESIGN.md 5.2 "Entry echo"):
blank.

Line 2:

| Columns | Content |
|---|---|
| 1 to 5 | Address of the current patch (`P1-11`); during an entry the echo `P3-__` / `P3-4_` |
| 6 | Space |
| 7 to 20 | Context, left aligned, at most 14 characters, first matching row wins |

| Priority | Condition | Context |
|---|---|---|
| W1 | No Web MIDI or not a secure context | `NO WEB MIDI` |
| W2 | Permission denied | `MIDI BLOCKED` |
| W3 | Permission not yet given | `CONNECT MIDI` |
| 1a | Granted, no output port ever configured | `CHOOSE A PORT` |
| 1b | Port configured but not open | `NO MIDI OUTPUT` (exactly 14) |
| 2 | Within 400 ms after a send | `SENT` |
| 3 | Portrait: the list overlay is open and its search is non-empty. Landscape: the search field is non-empty | `NO MATCHES`, `1 MATCH`, `<n> MATCHES` (at most `896 MATCHES`, 11) |
| 4 | Otherwise | Key mode word `WHOLE`, `DUAL`, `SPLIT`; blank for user slots |

Examples, 20 columns (`.` = blank cell):

```
12345678901234567890
Fantasia............
P1-11.DUAL..........

Alpha.Omega.Ensemble
P1-18.SENT..........

....................
U3-45.NO.MIDI.OUTPUT
```

The group long name (`PRESET 1: ORIGINAL D-50`) does not appear on a phone display; the
lit group pad and the list rows carry the group. On the real D-05 line 2 shows the key
mode (`DUAL`, REFERENCES 2.2), so the phone reads like the unit.

### 5.4 Rendering and footprint

As DESIGN.md 5.4, in canvas terms:

- Atlas: one `OffscreenCanvas` (or a detached `<canvas>`) per (`P`, `g`, ghost) holding
  the 95 glyphs, each `5P x 7P` device px drawn on `lcdBack` with `fillRect` per dot
  (`lcdOn` lit, `lcdGhost` unlit when `lcdShowGhost`). Built once; rebuilt only when `P`
  or `g` changes.
- `setLines(line1, line2)`: if both strings equal the stored ones, return. Otherwise
  `drawImage` only the changed cells from the atlas (integer source and destination
  rects, no scaling). No `requestAnimationFrame` loop; the canvas is touched only here
  and on a geometry change (5.1).
- The `SENT` flash and the status LED share one 400 ms `setTimeout` (DESIGN.md 5.3).
- The display ignores pointer events (`pointer-events: none`), has no focus, no hover.

---

## 6. MIDI access panel

### 6.1 States

Decided once at start, then updated on events:

| State | How it is detected | Leaves the state when |
|---|---|---|
| `insecure` | `window.isSecureContext === false` | never (reload over https) |
| `unsupported` | `typeof navigator.requestMIDIAccess !== "function"` in a secure context | never |
| `prompt` | the Permissions API reports `"prompt"` for `{ name: "midi" }`, or the API is unavailable and access has not been requested in this page yet | `requestMIDIAccess` resolves (`granted`) or rejects (`denied`) |
| `waiting` | after a press of `CONNECT MIDI` / `TRY AGAIN`, until the promise settles | as above |
| `denied` | the Permissions API reports `"denied"`, or `requestMIDIAccess` rejected | `PermissionStatus` `change` event to `"granted"`, or `TRY AGAIN` resolves |
| `granted` | the Permissions API reports `"granted"` (then request access at start without a button) or the request resolved | (normal app) |

While the state is not `granted`, the access panel is shown (6.3) and the panel controls
listed in 6.2 are disabled. The display shows the W1 to W3 lines of 5.3 (`insecure`
and `unsupported` use W1, `prompt` and `waiting` W3, `denied` W2). The status bar shows
`OUT None` in `labelDim` and no `NOT FOUND`.

On the first change to `granted` with no output port ever saved, the Settings dialog
opens by itself with the first listed output preselected (as DESIGN.md's first run,
one step shorter; 15 item 2). Only the change opens it: a page that loads with access
already granted and no saved port opens nothing and shows line 2 `CHOOSE A PORT IN
SETTINGS` (phones `CHOOSE A PORT`, 5.3 row 1a) until a port is chosen.

### 6.2 What is disabled

| Layout | Disabled (DESIGN.md 6.2 "Disabled": `buttonDisabled` face, `labelDisabled` legends, dark LEDs, no ring) | Replaced by the access panel | Stays enabled |
|---|---|---|---|
| Desktop | group bar, bank and number buttons, PREV, NEXT, ALL, FAVOURITES, RECENTS, AUTO SEND | search field and list well | SYSTEM (and every menu item: Settings opens with an empty port list, About, Import, Export) |
| Portrait | (hidden instead) | the whole band between the display and the bottom row; the bottom row shows only SYSTEM | SYSTEM |
| Landscape | group strip, pads, PREV, NEXT | the right column below the top (views row, search, list) | SYSTEM, drawn in the access panel's bottom-right corner at the bottom row position (8.3) |

Silkscreen does not change: section rules, titles and numerals keep their colours.

### 6.3 Placement and geometry

| Layout | Panel rect | Look |
|---|---|---|
| Desktop | `(X, B + 64, R, H - 22 - B - 76)`: the search field and list well together (at 960 x 560: `(434, 203, 510, 323)`) | A well: `well` fill, 1 px border top/left `wellEdgeDark`, bottom/right `wellEdgeLight` |
| Portrait | `(spanX, B + 12, spanW, H - 74 - 12 - (B + 12))` | No well: content set directly on `band` |
| Landscape | `(X, 8, Rw, H - 22 - 8 - 8 - 52)` | A well, as desktop |

Content is one centred block, width `min(panelW - 48, 400)` on desktop and landscape,
`spanW` in portrait, vertically centred in the panel with at least 24 px above it.
From the top of the block:

| Part | Spec |
|---|---|
| Title | `type.emptyTitle`, centred, `labelSecondary` (`labelRed` in `denied`) |
| Gap | 8 |
| Body | `type.body`, `labelPrimary`, centred, wrapped at the block width, line 16 |
| Steps (`denied` only) | 12 px gap, then one 20 px row per step: the number `1.` at the block's step column x 0 in `labelSecondary`, the text at x 20 in `labelPrimary`, `type.body`, vertically centred. The step column is as wide as the widest step + 20 and centred in the block (portrait: left aligned at the block's left) |
| Support matrix (`unsupported`, `insecure`) | 16 px gap, four 20 px rows: browser at x 0, answer at x 200, both `type.body`; browser `labelPrimary`, answer `labelSecondary`; the 380 px wide table centred in the block. Rows: `Chrome, Edge, Brave, Opera` · `Yes`; `Chrome on Android` · `Yes, with a USB OTG cable`; `Firefox 108 or later` · `Yes, after a site permission`; `Safari, iPhone and iPad` · `No`. **Stacked form** whenever the block is narrower than 380 (portrait, landscape, the phone About sheet): each entry is two 16 px lines at x 0, browser (`labelPrimary`) above its answer (`labelSecondary`), 8 px between entries (152 px for the four), left aligned at the block's left edge. Nothing in the matrix ever wraps |
| Button | 20 px gap, then a dialog button (DESIGN.md 10.2) with the default style (1 px `labelRed` border): `CONNECT MIDI` 160 x 32 on desktop and landscape; full block width x 48 with `type.padLabel` in portrait. `TRY AGAIN` 120 x 28 (portrait: full width x 48). Disabled with the text `WAITING` while `waiting` |
| Note (`prompt` only) | 16 px gap, `type.note`, `labelDim`, centred, wrapped: `Only Bank Select and Program Change are sent, never SysEx.` |

The button has initial keyboard focus when the panel appears (desktop and landscape; a
focused button opens no soft keyboard, so this is the one exception to 10's "no initial
focus on phones"). The ring then shows at load, drawn over the red default border (9.7);
the mockups omit it.

### 6.4 Texts

`prompt` (any browser that has Web MIDI):

- Title `MIDI IS NOT CONNECTED`
- Body: `The browser asks once for permission to use MIDI devices. Allow it so this page can change patches on the D-05.`
- On Android (`navigator.userAgentData?.mobile` or `/Android/` in the user agent) the
  body is instead: `Plug the D-05, or the interface it is on, into this phone with a USB OTG cable. Then connect and allow MIDI when Chrome asks.`
- Firefox (`/Firefox\//` in the user agent) adds a second paragraph after the body,
  same style: `Firefox installs a small site permission add-on the first time. That is expected.`

`denied`: title `MIDI ACCESS IS BLOCKED`, body `<Browser> is blocking MIDI for this page. To allow it:`, then the steps for the detected browser:

| Browser (detection) | Steps |
|---|---|
| Chrome desktop (brand `Google Chrome`) | 1 `Click View site information, left of the address.` 2 `Choose Site settings.` 3 `Set MIDI devices to Allow.` 4 `Come back here: the page connects by itself.` |
| Edge, Brave, Opera desktop (brands `Microsoft Edge`, `Opera`; `navigator.brave`) | Same four steps with step 1 `Click the site information icon, left of the address.` |
| Chrome on Android | 1 `Tap the icon left of the address.` 2 `Tap Permissions.` 3 `Allow MIDI devices.` 4 `Come back here: the page connects by itself.` |
| Firefox | 1 `Press TRY AGAIN.` 2 `Accept the site permission add-on when Firefox offers it.` 3 `If Firefox does not ask, open about:addons, choose Site Permissions, remove this site and press TRY AGAIN.` |
| Anything else | 1 `Allow MIDI for this site in the browser's site settings.` 2 `Come back here and press TRY AGAIN.` |

`<Browser>` is `Chrome`, `Edge`, `Brave`, `Opera`, `Firefox` or `The browser`.
"Connects by itself" depends on the `PermissionStatus` `change` event (6.1); where the
Permissions API cannot query `midi`, step 4 reads `Come back here and press TRY AGAIN.`

`unsupported`:

| Platform (detection) | Title | Body |
|---|---|---|
| iPhone, iPad (`/iPhone\|iPad/`, or `Macintosh` with `navigator.maxTouchPoints > 1`) | `NO WEB MIDI ON IPHONE OR IPAD` | `Every browser on iPhone and iPad uses Safari's engine, which has no Web MIDI. Use a computer or an Android phone.` |
| Safari on macOS (`Safari/` without `Chrome\|Chromium\|Edg\|OPR\|Firefox`) | `NO WEB MIDI IN SAFARI` | `Safari does not support Web MIDI, so this page cannot reach the D-05. Open it in Chrome, Edge, Brave or Firefox.` |
| Anything else | `NO WEB MIDI IN THIS BROWSER` | `Open this page in Chrome, Edge, Brave, Opera, or Firefox 108 or later.` |

`insecure`: title `MIDI NEEDS A SECURE PAGE`, body `Browsers offer MIDI only to pages served over https:// or from localhost. Open https://marcastr0.github.io/d05-patch-switcher/ instead.`
The URL is one unbreakable run (`white-space: nowrap` on a span around it); the line
breaks before it, never inside it.

All of these are user-facing strings: no em dashes, `D-05` as plain text.

---

## 7. Phone portrait

Reference frame 390 x 844 at dpr 3; must also fit 360 x 640 at dpr 2 without
scrolling (verified, 7.4). Mockups: `portrait-main`, `portrait-search`,
`portrait-access`.

### 7.1 Geometry

```
CW    = min(W, 560)                 content column, centred: cx0 = floor((W - CW) / 2)
m     = 12
G     = CW - 2m
cw    = floor(G / 4)                pad cell width
gx    = cx0 + m + floor((G - 4 cw) / 2)
spanX = gx + 3,  spanW = 4 cw - 6   "face span": left edge of the first pad face to the
                                    right edge of the last; rules, strip, transport,
                                    chips, bottom row and display align to it
```

Top-anchored, from y 0 (inside the safe-area padding):

| y | Height | Element |
|---|---|---|
| 8 | `bezelH` | Display bezel, x = `spanX + floor((spanW - bezelW) / 2)`, on `panel` |
| `B = 8 + bezelH + 10` | | Band starts: `band` fill to the status bar, 1 px `bandEdge` on its top row |
| B + 12 | 12 | Section rule `PATCH GROUP` (accent, DESIGN.md 6.1) across the face span |
| B + 28 | 44 | Group strip (7.2) |
| B + 84 | 12 | Rule `PATCH BANK` |
| B + 100 | 2 `hp` | Bank pads, 2 rows of 4 (7.3) |
| B + 100 + 2 hp + 12 | 12 | Rule `PATCH NUMBER` |
| B + 128 + 2 hp | 2 `hp` | Number pads, 2 rows of 4 |
| B + 128 + 4 hp | | End of the top group |

Bottom-anchored:

| y | Height | Element |
|---|---|---|
| H - 210 | 52 | Transport (7.4) |
| H - 146 | 12 | Rule `FAVOURITES` (secondary variant: `labelSecondary` title, `ruleSecondary` rule) |
| H - 130 | 44 | Favourites chips (7.5) |
| H - 74 | 44 | Bottom row: search button and SYSTEM (7.6) |
| H - 22 | 22 | Status bar (7.7) |

Pad row height: `hp = clamp(floor((H - 222 - (B + 128)) / 4), 52, 88)`. The space left
between the top group and the transport is empty band (at least 12). If `hp` would be
below 52 (viewports shorter than about 640), `hp = 52` and the band scrolls vertically
as a whole (the display stays fixed); this is the only case where the band scrolls.

| | 390 x 844 @ 3 | 360 x 640 @ 2 | 360 x 668 @ 2 |
|---|---|---|---|
| `cw`, `gx`, face span | 91, 13, `(16, 358)` | 84, 12, `(15, 330)` | 84, 12, `(15, 330)` |
| Bezel | `(21, 8, 348, 65)` | `(16, 8, 328, 62)` | same |
| `B` | 83 | 80 | 80 |
| `hp` | 88 (pad faces 85 x 80) | 52 (78 x 44) | 59 (78 x 51) |
| Transport `y` | 634 | 430 | 458 |
| Empty band between | 71 | 14 | 14 |

### 7.2 Group strip

A horizontal scroller `(0, B + 28, W, 44)`, full viewport width (it bleeds past the
margins so a cut-off pad at the right edge says "more"). `overflow-x: auto`, scrollbar
hidden (`scrollbar-width: none`, `::-webkit-scrollbar { display: none }`), no snap, no
smooth scrolling. Contents from x = `spanX - 3` (the first cell's left edge):

- Cells 52 wide, 44 tall: **`P1` to `P6`, a 12 px gap, then `U1` to `U8`**. Presets come
  first on the phone (15 item 3): all six fit on a 360 px screen without scrolling.
  14 cells + gap = 740 px of strip.
- Each cell's face: `(3, 6, 46, 32)` inside the cell, a pad (7.3) with the group legend
  (`labelPrimary`) and a 16 x 3 LED.
- Right padding after `U8`: `m` (12).
- When the current group changes, the strip scrolls (instantly) the minimum amount that
  brings that cell fully into view; at start, it scrolls so the current cell is fully
  visible. The user's own scrolling is otherwise left alone.

### 7.3 Pads (bank and number)

Eight per section in two rows of four (`1 2 3 4` / `5 6 7 8`), cell `(gx + cw * col,
rowY + hp * row, cw, hp)`, face inset `(3, 4)`: face `(cw - 6) x (hp - 8)`. Kind B pad
(9.2): numeral legend in `type.padLabel`, colour `labelAccent` (the D-05's blue
numerals), `labelPrimary` on the awaiting section during an entry (DESIGN.md 6.4), and a
16 x 4 LED. Lit LED = current bank / current number, as DESIGN.md 6.4. The whole cell is
the touch target (at least 84 x 52).

The grid is 2 x 4 rather than 1 x 8 in portrait: eight across leaves 44 px cells at
360 px, the minimum touch size with nothing to spare; four across gives 78 to 91 px
pads for a finger at arm's length while playing. Landscape keeps eight across (8).

Entry, as DESIGN.md 6.3 and 6.4: tap a group, then a bank pad, then a number pad (sends);
a number pad while idle sends at once in the current group and bank; a tap anywhere on
the band outside the strip and pads cancels a pending entry. The display echo is on
line 2 (5.3).

### 7.4 Transport

Row `(gx, H - 210, 4 cw, 52)`. PREV face `(spanX, H - 206, 2 cw - 6, 44)` over pad
columns 1 and 2; NEXT face `(gx + 2 cw + 3, H - 206, 2 cw - 6, 44)` over columns 3 and 4.
Kind E (9.2): a solid 7 x 8 wedge in `arrow` plus the legend `PREV` / `NEXT` in
`type.padLabel` `labelPrimary`, wedge and legend 8 px apart, the pair centred in the
face; PREV's wedge points left and comes first, NEXT's points right and comes last.
Touch target: the whole 52 px row half.

### 7.5 Favourites chips

Horizontal scroller `(0, H - 130, W, 44)`, bleeding like the strip, scrollbar hidden,
content from x = `spanX`, chips 8 px apart, right padding 12. Favourites in catalogue
order.

Chip: face height 32 at y + 6, width `min(220, 73 + nameAdvance + 12)`. Face as a pad
(lit face `buttonFaceLit` when the chip is the current patch). Contents: LED
`(8, 14, 8, 4)`, no core, lit when current; address at x 22 in five 9 px cells,
`type.listAddress`, `labelSecondary` (`labelPrimary` when current); name at x 73,
`type.body`, `labelPrimary`, elided with an ellipsis at the chip's max width. Unnamed
user slot: the address only, width 79. Both text baselines are vertically centred in
the 32 px face. Tap sends (FR-2 "favourite row activation"). The touch target is the
chip plus 6 px above and below (44).

Empty: in place of the chips, one line at x `spanX`, vertically centred in the 44 px
row: `NO FAVOURITES` in `type.emptyTitle` `labelSecondary`, 8 px, then
`Star a patch in the list` in `type.note` `labelDim`.

### 7.6 Bottom row: search button and SYSTEM

- Search button `(spanX, H - 74, spanW - 80, 44)`: drawn as the search field in its
  normal state (DESIGN.md 7.1: `fieldBack`, 1 px top/left `wellEdgeDark`, bottom/right
  `fieldBorder`), text at x + 13 vertically centred, `type.field`. Shows the placeholder
  `Search patches` in `labelDim`, or the kept query in `labelPrimary`. It is a `<button>`,
  not an input: a tap opens the list overlay (7.8) with its field focused (the soft
  keyboard opens).
- SYSTEM `(spanX + spanW - 72, H - 74, 72, 44)`: kind C pad with the legend `SYSTEM`
  (`type.padLabel`, `labelPrimary`) vertically centred, no LED. Opens the menu (11.6)
  upward.
- AUTO SEND is not on the phone panel; it stays in Settings (with tap-to-send it only
  affects keyboard highlight moves).

### 7.7 Status bar

`(0, H - 22, W, 22)`, as DESIGN.md 8.1 (`panel`, 1 px `bandEdge` top line, baselines at
y + 15). Items from x 16: activity LED `(16, 9, 8, 4)`, `OUT`, port name (elided in the
middle to fit `W - 150`), output `NOT FOUND` when disconnected, `CH` and the channel.
`IN`, the input name, its `NOT FOUND` and `FOLLOW` follow only when `W >= 480`. No
message slot in portrait (messages that the desktop puts there show in the Settings
sheet, in the message card 7.10, or are dropped).

**`NOT SAVED` marker (all layouts).** While storage is blocked or the last save failed
(WEB.md, W1 storage rules), `NOT SAVED` in `type.statusLabel` `labelRed` (role
`statusError`) follows the last left item shown (`CH` value in narrow portrait, else the
input name, its `NOT FOUND` or `FOLLOW`) at a 16 px gap, as an item of its own. Its
`title` carries the reason. On desktop and landscape the reason also shows as the status
message in `labelRed`; in portrait the Settings sheet shows it.

### 7.8 List overlay (search)

Opened by the search button. Covers `(0, B, W, H - B)`: everything below the display,
which stays visible (it shows `SENT`, the match count and the current patch while the
user browses). `band` background, 1 px `bandEdge` on its top row.

| y from B | Element |
|---|---|
| +6 | Search field `(spanX, B + 6, spanW - 80, 44)`, DESIGN.md 7.1 states (focused: 2 px `focusRing`), text at x + 13, `type.field`. `<input type="text" inputmode="search" enterkeyhint="search" autocomplete="off" autocapitalize="off" spellcheck="false">`; no clear button |
| +6 | `DONE` `(spanX + spanW - 72, B + 6, 72, 44)`: a dialog button (DESIGN.md 10.2 colours) with `type.padLabel` |
| +56 | Views row, cells `vw = floor((spanW + 6) / 3)` wide and 44 tall from x `spanX`; faces `(spanX + vw * i, B + 62, vw - 6, 32)`: `ALL`, `FAVOURITES`, `RECENTS` as kind B pads (legend `labelPrimary` + 16 x 3 LED, lit = active view, radio) |
| +108 | List: `(0, B + 108, W, H - B - 108)`, full bleed, `well` fill, 1 px `wellEdgeDark` on its top row. Native overlay scrolling, no styled scrollbar |

Rows are 52 tall; the DESIGN.md 7.2 row is centred vertically in them (all its y values
+12), with these changes for touch: key-mode box at `x = Wl - 88`; star centred at
`(Wl - 26, 26)` with outer radius 7 and inner radius 3; the star's touch target is the
row's right 52 x 52. The recents count, when shown, is `(Wl - 136, 0, 40, 52)`, right
aligned; the name ends at `Wl - 140`.

**Highlight on phones.** The highlighted row keeps existing in the model (Enter on a
hardware keyboard sends it), but on the phone layouts its `selection` background and
selection bar are drawn only under `@media (pointer: fine)` (a mouse is the primary
pointer). On a touch phone no row ever looks highlighted, whatever the typing, the view
switch or the start-up position did. A narrowed desktop window keeps DESIGN.md 7.2.

Behaviour: a tap on a row sends that patch and marks it current; the overlay **stays
open** (auditioning several patches in a row is the point). A tap on the star toggles
the favourite without sending. A long press on a user-slot row (the browser's
`contextmenu` event, so no timer of our own) opens the inline editor DESIGN.md 7.4 at
`(80, 14, Wl - 224, 24)` of the row, so it ends at `Wl - 144`, 8 px before the count
column where `n/18` shows while editing (the desktop's `Wl - 180` would run 4 px into
it); Enter on the soft keyboard commits. Its text selection is `matchBack` with
`labelPrimary` text (`.nameEditor::selection`, as the search field 9.4 and the desktop's
QSS), on every layout. `DONE`, the
Android back gesture (the overlay pushes one history entry when it opens and closes on
`popstate`) and `Esc` close it. The query is kept when it closes and shows on the search
button; display line 2 shows the count only while the overlay is open (5.3). Empty
states: DESIGN.md 7.5 in the list area, line 2 texts `Clear the search to see all patches`
(no match), `Tap a star to add a favourite` (favourites), `Patches you send appear here`
(recents).

### 7.9 Portrait states

| State | What differs |
|---|---|
| Connected, idle | as mockup `portrait-main` |
| Disconnected | line 2 `NO MIDI OUTPUT`; status `NOT FOUND`; pads stay enabled (a send attempt says `Not sent` as desktop) |
| Sent | line 2 `SENT` and the status LED for 400 ms |
| Entry pending | chosen group lit, other LEDs dark, awaiting section's numerals `labelPrimary`, line 2 echo |
| Overlay open | 7.8, mockup `portrait-search` |
| No MIDI access | 6, mockup `portrait-access` |
| Pressed pad | DESIGN.md 6.2 pressed (`buttonPressed`, no highlight, legend and LED 1 px down) for as long as the finger is down (`:active`) |
| Storage blocked | `NOT SAVED` in the status bar (7.7); the Settings sheet carries the reason |

### 7.10 Message card (portrait only)

Portrait has no status message slot, so the results the desktop reports there and that
the user must see appear as a centred card (the confirmation card of 11.2: `min(360, W -
24)` wide, title strip 36, text `type.body` `labelPrimary` with 20 px margins, one `OK`
button 44 tall, default style, at the right). It opens for:

| Event | Title | Text |
|---|---|---|
| Import done | `IMPORT LIBRARY` | the result line of 11.6, `labelPrimary` |
| Import refused | `IMPORT LIBRARY` | `Not a library file: <reason>`, `labelRed` |

`Library exported` gets no card (the browser's own download notice is the feedback);
it is dropped in portrait. `OK`, `CLOSE`, `Esc` and the back gesture close the card.

---

## 8. Phone landscape

Reference 844 x 390 at dpr 3 (normal) and 800 x 300 (compact). Must fit 800 x 280
without scrolling (verified). Mockups: `landscape-main`, `landscape-compact`.

### 8.1 Left column: display and panel

```
c   = clamp(floor((W / 2 - 24) / 8), 44, 56)     cell width (844: 49; 800: 47; 680: 44)
L   = 8c + 24                                    left column width (844: 416)
spanX = 14, spanW = 8c - 4                       face span
```

Bezel at `(spanX + floor((spanW - bezelW) / 2), 8)` on `panel`; the band (`band` fill,
1 px `bandEdge` top line) starts at `bandTop = 8 + bezelH + 4` and runs to the status
bar, across the left column only. Let `y = 8 + bezelH`.

| Element | Normal (`H >= 380`) | Compact (`H <= 379`) |
|---|---|---|
| Rule `PATCH GROUP` | `y + 8`, across the face span | none |
| Group strip | scroller `(0, y + 22, L, 44)` | `(0, y + 6, L, 44)` |
| Rule `PATCH BANK` | `y + 72` | none |
| Numerals 1 to 8 | row `y + 86`, 14 tall, `type.padLabel`, `labelAccent`, centred over each face, baseline row + 11 | inside the faces (legend) |
| Bank buttons | row `y + 100`, 44 tall | row `y + 54` |
| Rule `PATCH NUMBER` | `y + 150` | none |
| Numerals | row `y + 164` | inside the faces |
| Number buttons | row `y + 178` | row `y + 102` |
| Transport | row `y + 228`, 44 tall | row `y + 150` |
| Needs `H >=` | `y + 298` (at most 377 with the pitch cap) | `y + 220` (at most 280) |

Strip: as 7.2 (cells 52, faces 46 x 32 at +6, P before U), first face at x 14, clipped
to the left column (x 0 to L).

Buttons: eight across, cell `(12 + c * i, rowY, c, 44)`, face `(14 + c * i, rowY + 4,
c - 4, 36)`. Normal: kind A pads (16 x 4 LED centred, no legend; numerals are above, as
the D-05). Compact: kind B pads with the numeral as the legend (`labelAccent`, awaiting
`labelPrimary`) and a 16 x 4 LED.

Transport: PREV face `(14, rowY + 4, 4c - 4, 36)`, NEXT face `(14 + 4c, rowY + 4, 4c - 4,
36)`, kind E as 7.4.

If `H` is below what the density needs (only possible under 280), the left column
scrolls vertically below the display.

### 8.2 Right column: views, search, list

`X = L + 12`, `Rw = W - L - 24` (844: `X 428`, `Rw 404`). The right column is `band`
from y 0 to the status bar, with a 1 px `bandEdge` line at x = L running its full height.

| Element | Rect |
|---|---|
| Views | cells `vw = floor((Rw + 6) / 3)`, faces `(X + vw * i, 12, vw - 6, 36)`: kind B pads `ALL`, `FAVOURITES`, `RECENTS` with 16 x 3 LEDs (radio). `FAVOURITES` needs a face of at least 87 px, which `W >= 680` guarantees |
| Search field | `(X, 56, Rw - 80, 44)`, a real `<input>` (DESIGN.md 7.1 states, text at x + 13) |
| SYSTEM | `(X + Rw - 72, 56, 72, 44)`, kind C pad, as 7.6 |
| List well | `(X, 108, Rw, H - 22 - 8 - 108)`, DESIGN.md 7.2 well and border |

List rows are 44 tall (touch), the 28 px DESIGN.md row centred in them (+8), with the
7.8 touch changes (key-mode box at `Wl - 88`, star at `Wl - 26` radius 7 / 3, 44 x 44 star
target, recents count `(Wl - 136, 0, 40, 44)`, inline editor `(80, 10, Wl - 224, 24)`,
highlight drawn only under `(pointer: fine)`). Tap sends, as 7.8. The styled scrollbar
(9.6) applies. AUTO SEND is not shown (Settings has it).

**Where the current row lands.** When the list scrolls to put the current patch in view
with context above it (start-up, view change, a send from the panel), the desktop puts
it 3 rows below the top. The phone layouts use `k = min(3, floor((listH - rowH) / (2 *
rowH)))` rows instead, so the current row is never pushed to the bottom edge of a short
list: 844 x 390 (`listH` 252) gives 2; 800 x 300 compact (162) gives 1; portrait overlay
at 390 x 844 gives 3. The desktop keeps 3.

### 8.3 Status bar and states

Status bar as DESIGN.md 8.1 across the full width, all items. States as 7.9, with the
list always visible (no overlay); the match count shows on line 2 whenever the search
field is non-empty. No MIDI access (6.2): the access panel covers the right column from
y 8 to `H - 22 - 60` as a well, and SYSTEM sits at `(X + Rw - 72, H - 22 - 52, 72, 44)`.

---

## 9. Components in HTML and CSS

### 9.1 Panel button, desktop sizes

DESIGN.md 6.2 as a `<button>` that is the face (44 x 24; hit area = face, as Qt):

```css
.pb {
  position: relative; box-sizing: border-box; width: 44px; height: 24px;
  padding: 0; border: 0; border-radius: 2px;
  background: var(--buttonFace);
  box-shadow: 0 0 0 1px var(--buttonSlot);        /* the slot; follows the radius: 3 px */
  font: inherit; color: var(--labelPrimary); cursor: default;
}
.pb::before {                                     /* top highlight (2, 1, w - 4, 1) */
  content: ""; position: absolute; left: 2px; right: 2px; top: 1px; height: 1px;
  background: var(--buttonHighlight);
}
.pb.lit { background: var(--buttonFaceLit); }
@media (hover: hover) { .pb:hover:not(:disabled) { background: var(--buttonFaceHover); } }
.pb:active:not(:disabled) { background: var(--buttonPressed); }
.pb:active::before, .pb:disabled::before { display: none; }
.pb:active:not(:disabled) > * { transform: translateY(1px); }
.pb:disabled { background: var(--buttonDisabled); color: var(--labelDisabled); }
.pb:focus { outline: none; }
.pb:focus-visible::after {                        /* ring (-4, -4, 52, 32), 2 px, square */
  content: ""; position: absolute; inset: -4px; border: 2px solid var(--focusRing);
  border-radius: 0; pointer-events: none;
}
.led { position: absolute; background: var(--ledOff); }
.lit:not(:disabled) .led { background: var(--ledOn); }
.lit:not(:disabled) .led::after {                 /* core (x + 2, y + 1, w - 4, h - 2) */
  content: ""; position: absolute; inset: 1px 2px; background: var(--ledCore);
}
```

Kind A: `.led` at `(15, 10, 14, 4)`. Kind B: legend span `type.panelLabel` centred with
its baseline at y 11, `.led` at `(15, 16, 14, 3)`. Kind D: the wedge as an inline SVG
`<polygon>` with `fill: var(--arrow)` (disabled `var(--labelDisabled)`) and
`shape-rendering: crispEdges`, vertices as DESIGN.md 6.2. The status LED and the chip
LED use `.led` without the core.

### 9.2 Pads, phone sizes

Same layers and states as 9.1, but the `<button>` is the **hit cell** (transparent,
no border, the cell's full rect) and the face is a child `<span class="face">` at the
inset given in 7 and 8; `.lit`, `:active`, `:disabled` and the ring rules target the
face through the button (`.pad:active .face`, `.pad:focus-visible .face::after`).

| Kind | Content of the face (face width `w`, height `h`) |
|---|---|
| A, LED only | LED `(floor(w/2) - 8, floor(h/2) - 2, 16, 4)` |
| B, legend + LED | Legend (`type.padLabel`, line 15) and LED stacked with 4 px between, the stack centred: `top = floor((h - 23) / 2)`, legend line box at `top`, baseline `top + 12`, LED `(floor(w/2) - 8, top + 19, 16, 4)` (strip and views: height 3). At `h = 32`: baseline 16, LED y 23 |
| C, legend | Legend `type.padLabel` centred both ways (`SYSTEM`) |
| E, wedge + legend | 7 x 8 wedge in `arrow` and the legend, 8 px apart, centred (7.4) |

### 9.3 Section rule

DESIGN.md 6.1, 12 px tall. A flex row: rule segment, title, rule segment, with 6 px gaps;
the segments are 1 px tall `div`s at `top: 8px` (not `border` on a baseline-aligned
box, which lands on a fractional pixel at some zooms).

### 9.4 Search field

DESIGN.md 7.1. `<input type="text">` (not `type="search"`, which adds a clear button):
`box-sizing: border-box; height: 32px` (phones 44), `padding: 0 12px` with the 1 px
border (so text starts at x + 13; focused: 2 px border, padding 11 px), `background:
var(--fieldBack)`, `caret-color: var(--labelPrimary)`, `::placeholder { color:
var(--labelDim); opacity: 1 }`, `::selection { background: var(--matchBack); color:
var(--labelPrimary) }`, `border-radius: 0`. The browser caret blinks; that cannot be
switched off in CSS and is accepted on the web (WEB.md 1, footprint).

### 9.5 List

A focusable container (`tabindex="0"`, `role="listbox"`) with a virtual list: only the
visible rows plus a few are in the DOM, each `position: absolute; top: index * rowH`,
fixed row height (28 desktop, 52 portrait overlay, 44 landscape). Row contents as
DESIGN.md 7.2 with absolutely positioned children; the address in five 9 px cells
(3.3); match ranges as `<mark>` elements with `background: var(--matchBack); color:
inherit` (DESIGN.md 7.3: 20 px tall boxes at y 4 of the 28 px row; set `mark` to
`display: inline-block; height: 20px; line-height: 20px`). The star is an inline SVG
`<polygon>` with the DESIGN.md points (`stroke: var(--starOff)` off, `fill:
var(--labelPrimary)` on, row hover: stroke `labelSecondary`). Rows re-render only on a
model change or a scroll that exposes new rows.

### 9.6 Scrollbar (desktop and landscape list)

Chromium honours `::-webkit-scrollbar` only while `scrollbar-color` and
`scrollbar-width` are unset on the element (Chrome 121 and later), so the two are kept
apart:

```css
.list::-webkit-scrollbar { width: 10px; }
.list::-webkit-scrollbar-track { background: var(--well); }
.list::-webkit-scrollbar-thumb {
  background: var(--buttonFace); border: 2px solid var(--well); border-top: 0; border-bottom: 0;
  border-radius: 0; min-height: 24px;
}
.list::-webkit-scrollbar-thumb:hover { background: var(--buttonFaceHover); }
.list::-webkit-scrollbar-thumb:active { background: var(--buttonPressed); }
.list::-webkit-scrollbar-button { display: none; }
@supports not selector(::-webkit-scrollbar) {      /* Firefox */
  .list { scrollbar-width: thin; scrollbar-color: var(--buttonFace) var(--well); }
}
```

Firefox's thin scrollbar is narrower than 10 px; accepted.

### 9.7 Combo box, channel, checkbox, dialog buttons

- **Combo** (`<select>` inside a `<span class="combo">`): `appearance: none`, height 26
  (phone 44), `background: var(--fieldBack)`, 1 px `fieldBorder` (hover
  `fieldBorderHover`, `:focus-visible` 2 px `focusRing`), `padding-left: 8px`,
  `padding-right: 20px`, `type.body`. The arrow is the wrapper's `::after`: 8 x 4 at
  `right: 7px; top: 11px` (phone: vertically centred), `background: var(--arrow)` with
  `mask: url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='8' height='4'><path d='M0 0h8v1H7v1H6v1H5v1H3V3H2V2H1V1H0z'/></svg>")`
  (URL-encode `<`, `>`, `#` when writing it): the same stepped wedge as
  `assets/theme/arrow-down.png`, with the colour from the token. The open list of
  options is drawn by the browser (dark because of `color-scheme: dark`); accepted
  deviation from DESIGN.md 10.2's popup.
- **Channel**: a combo 72 wide (phone 96) with options `1` to `16`, instead of the
  desktop spin box (a number input's spinners cannot be styled to the panel and are
  poor on touch).
- **Checkbox**: `<input type="checkbox">` with `appearance: none; box-sizing:
  border-box; width: 16px; height: 6px; margin: 0`, states exactly DESIGN.md 10.2
  (unchecked `ledOff` + 1 px `fieldBorderHover`; checked `ledOn` + 1 px `ledOn`;
  `:focus-visible` border `focusRing`; disabled `buttonDisabled` + 1 px `fieldBorder`).
  The whole `<label>` row is the target: 20 tall on desktop, 44 on phones.
- **Dialog buttons**: DESIGN.md 10.2. `min-width: 100px; height: 28px` (phones 44),
  `background: var(--buttonFace); border: 1px solid var(--buttonSlot); border-radius:
  2px`, `type.panelLabel` (phones `type.padLabel`) in `labelPrimary`; default button
  `border-color: var(--labelRed)`; `:focus-visible { outline: 2px solid
  var(--focusRing); outline-offset: -2px }` (draws over the border, no layout shift);
  disabled `buttonDisabled`, `labelDisabled`.

---

## 10. Focus

`:focus { outline: none }` everywhere, then:

| Element | Shown on | Visual (DESIGN.md 9) |
|---|---|---|
| Panel buttons and pads | `:focus-visible` (keyboard only; a mouse click or tap shows nothing) | 2 px `focusRing` ring, square, 2 px outside the face (9.1) |
| Group bar and grid (one focus stop each, DESIGN.md 6.3, 6.4) | the container has `tabindex="0"`; the cursor button gets class `cursor` and shows the ring while the container `:focus-visible` matches | ring on the cursor button |
| Search field | `:focus` (always, as DESIGN.md) | 2 px `focusRing` border |
| List | `:focus` | 1 px `focusRing` well border; highlighted row bar `focusRing` (else `selectionBarIdle`) |
| Dialog buttons, combos | `:focus-visible` | 2 px `focusRing` (9.7) |
| Checkboxes | `:focus-visible` | indicator border `focusRing` |
| Access panel button | `:focus-visible` | as dialog buttons |

Tab order on desktop: DESIGN.md 9 (search field, list, ALL, FAVOURITES, RECENTS, AUTO
SEND, SYSTEM, group bar, grid, PREV, NEXT). Use DOM order, never positive `tabindex`.
Initial focus: the search field when access is granted, else the access panel button.
Phone layouts (portrait and landscape): no initial focus (a focused field opens the soft
keyboard), at start and after a layout switch; the only exception is the landscape
access panel button (6.3). A phone sheet or card focuses the `<dialog>` element itself
(`tabindex="-1"`), never a field or combo inside it, so no ring shows when it opens.

---

## 11. Dialogs and menus

### 11.1 In-page dialogs, desktop and landscape

`<dialog>` opened with `showModal()`; `Esc` closes it (the `cancel` event equals
Cancel). Geometry:

| Part | Spec |
|---|---|
| Width | the DESIGN.md width: Settings 520, Shortcuts 680, About 440, Clear Recents 360, Import confirmation 360 |
| Height | `36` (title strip) + the DESIGN.md client height + 2 (the border); at most `H - 16`, then the content area scrolls |
| Position | centred: `x = floor((W - w) / 2)`, `y = max(8, floor((H - h) / 2))` |
| Frame | `panel` fill, 1 px `fieldBorder` border inside the width above (Settings is 520 including it), no radius, no shadow |
| Backdrop | `::backdrop { background: var(--scrim) }` |
| Title strip | `(0, 0, w, 36)`, `panel`, 1 px `bandEdge` line on its bottom row. Title at x 20, `type.dialogTitle`, `labelPrimary`, vertically centred in the 35 px above the line: `SETTINGS`, `KEYBOARD SHORTCUTS`, `ABOUT`, `CLEAR RECENTS`, `IMPORT LIBRARY`. At the right, `CLOSE` (`type.panelLabel`, `labelSecondary`, hover `labelPrimary`, right edge at w - 20), a `<button>` whose hit area is 64 x 36; same as Cancel / Esc. Not in the tab order (Esc does the same) |
| Content | margins 20 as DESIGN.md 10, measured inside the border: content x = dialog x + 21, first content row at title strip bottom + 20 |

Landscape uses the same dialogs when they fit in both directions (`W >= w + 16` and
`H >= h + 16`); otherwise the phone sheet (11.2), whose button bar stays on screen. At
844 x 390 this makes Settings (472 tall) and About sheets; the confirmations are cards
(11.2) and fit.

### 11.2 Phone sheets (portrait, and landscape when narrower than the dialog)

Full-viewport `<dialog>`: `(0, 0, W, H)`, `panel`, no border, no backdrop visible.
Opening pushes a history entry; `popstate` (the Android back gesture) closes it as
Cancel.

| Part | Spec |
|---|---|
| Title strip | `(0, 0, W, 48)`, `panel`, 1 px `bandEdge` bottom line; title at x 16, `type.dialogTitle`, vertically centred; `CLOSE` at the right, `type.padLabel`, `labelSecondary`, hit 72 x 48 |
| Content | from y 48 to the button bar, scrolls vertically, padding 16 |
| Button bar | `(0, H - 68, W, 68)`, `panel`, 1 px `bandEdge` top line; buttons 44 tall at y + 12, from x 16 to W - 16, equal widths, 8 px apart; `type.padLabel` |

**Settings sheet** (content x 16, width `W - 32`, y from the content top; every row at
exactly this y, whatever the gaps in the desktop dialog's CSS are):

| y | Element |
|---|---|
| 0 | Rule `MIDI OUTPUT` (secondary) |
| 20 | Label `PORT` (`type.panelLabel`, `labelPrimary`, line box 16) |
| 40 | Output combo, full width x 44 |
| 96 | Label `CHANNEL` |
| 116 | Channel combo 96 x 44 |
| 172 | Connection line, 20 tall: LED `(0, 8, 10, 4)` + text at x 18 (DESIGN.md 10.1 texts and colours) |
| 212 | Rule `MIDI INPUT` |
| 232 | Label `PORT` |
| 252 | Input combo, full width x 44 |
| 300 | Input problem line, 20 tall, `type.body` `labelRed`, elided right (DESIGN.md 10.1 texts); empty when there is nothing to say, and the 20 px stay reserved (the rows below never move) |
| 324 | Checkbox row `Follow the panel`, 44 tall (indicator at y + 19) |
| 388 | Rule `BEHAVIOUR` |
| 408 | Checkbox row `Auto-send on highlight`, 44 |
| 452 | Checkbox row `Resend the current patch on connect`, 44 |
| 516 | Rule `ON THE D-05` |
| 536 | Note (DESIGN.md 10.1), `type.note`, `labelSecondary`, wrapped at `W - 32` |

Button bar: `TEST`, `CANCEL`, `OK` (default style). `CLOSE` = `CANCEL`.

**About sheet**: content as 11.5 at width `W - 32`; the wordmark wraps `FOR ROLAND D-05`
under `PATCH SWITCHER` (8 px below, no divider) when `W - 32 < 310`. The support matrix
uses its stacked form (6.3) whenever `W - 32 < 380`. Button bar: `OK` only.

**Clear Recents and Import confirmations** are not sheets on phones: centred cards
`min(360, W - 24)` wide, as 11.1 but with 44 px buttons (`type.padLabel`), in portrait
and landscape alike. The portrait message card (7.10) is the same card with one button.

**Shortcuts** is not offered in the phone layouts (no keyboard).

### 11.3 Settings, web

DESIGN.md 10.1 with two changes:

1. **No `REFRESH` button.** Web MIDI updates the port list live (`onstatechange`); the
   output combo widens to `(112, 20, 368, 26)`.
2. The channel is a combo (9.7).

Client height stays 436, so the dialog is 472 tall. Below `H = 488` it is capped at
`H - 16` and its content scrolls (by 8 px at the 480 minimum); accepted.

### 11.4 Shortcuts, web

DESIGN.md 10.3 with the web key map (`web/src/app.ts`, same table as SPEC FR-13): the
`Ctrl+Q` row is removed from `APP`. On macOS (`navigator.userAgentData?.platform` or
`navigator.platform` starting `Mac`) caps use DESIGN.md 10.3's macOS column. Any shortcut
the web builder finds the browser keeps for itself (W3 verifies `Ctrl+D`, `Ctrl+1` to
`Ctrl+3`, `Ctrl+,` / `Cmd+,`, `Ctrl+G`, `F1` in Chrome and Firefox on Linux and macOS)
is either rebound (designer to approve the new cap text) or dropped from the sheet;
never shown if it does not work. Rows keep 26 px; the right column loses one row, so
the client height becomes 386 (`340 - 26 = 314`, `+ 16 + 16`, `+ 40` margins).

Decided at W3: on macOS `Cmd+,` stays with the browser, so the `Settings` row is dropped
from the sheet there (the `APP` section then holds only `F1`; the sheet's height follows
its taller column, so it shrinks only if the right column was the taller one); the SYSTEM menu shows no shortcut text for `Settings...` on macOS. The
sheet is offered in the desktop layout only (`F1` and the menu item are absent on the
phone layouts, 11.2).

### 11.5 About, web

DESIGN.md 10.4, with paragraph 2 `Selects patches on a Roland Boutique D-05 over MIDI, in the browser.`
and, after paragraph 6, a secondary rule `BROWSERS` (16 px above, full width) followed
by the support matrix of 6.3 (four 20 px rows, browser at x 0, answer at x 200), then
`Runs offline once loaded. Nothing leaves this page except MIDI.` (`type.note`, 8 px
above). `OK` as DESIGN.md.

### 11.6 SYSTEM menu

DESIGN.md 8.2 styling (`panel`, 1 px `fieldBorder`, 4 px padding, items 28 tall with
`type.body`, hover / keyboard-current `selection`, shortcut text right aligned in
`labelSecondary`, separators). A `role="menu"` element positioned under the SYSTEM button
with its right edge on the button's right edge; on phones items are 44 tall and the
width is `min(240, W - 8)`. Direction on phones: **upward** (bottom edge 4 px above the
button) when the menu fits above the button, which is always the case in portrait;
otherwise **downward** (top edge 4 px below the button), which is the landscape case,
where SYSTEM sits at y 56. Desktop: directly under the button, as Qt. Arrow keys move,
Enter activates, Esc or a click / tap outside closes.

Current item: opened with the keyboard (Enter or Space on SYSTEM), the first item is
current and shows `selection`; opened by mouse or touch, no item is current until the
pointer hovers or an arrow key is pressed. In CSS: `.mi:hover, .mi:focus-visible`, not
`.mi:focus` (the menu still moves focus to its first item so the arrows work). Items:

`Settings...` (`Ctrl+,`), `Keyboard Shortcuts` (`F1`, desktop layout only),
`Clear Recents...`, separator, `Import Library...`, `Export Library`, separator,
`About D-05 Patch Switcher`.

- `Export Library` downloads `library.json` (the desktop schema, WEB.md 3) and shows
  `Library exported` as the status message.
- `Import Library...` opens the file picker (`accept=".json,application/json"`), then
  the Import confirmation: a 360 px dialog built as DESIGN.md 10.5 with title
  `IMPORT LIBRARY`, text `Replace favourites, recents and user names with the ones in <file name>?`
  (wraps to two lines), buttons `IMPORT` (red text, object role as `CLEAR`) and `CANCEL`
  (default). The result goes to the status bar message (portrait: the card 7.10):
  `Imported <n> favourites, <m> names`, plus ` (<k> entries skipped)` when the import
  dropped entries it could not read; singular forms `1 favourite`, `1 name`,
  `1 entry skipped`; `0` stays plural (`0 favourites`). Or, in `labelRed` through the
  `statusError` role, `Not a library file: <reason>`. If
  W1 makes import a merge instead of a replace, the text becomes `Add the favourites,
  recents and user names in <file name>?` and the button `ADD`.

---

## 12. Motion, footprint, drawing rules

1. Nothing moves except the `SENT` flash: no CSS `transition`, no `animation`, no
   smooth scrolling (`scroll-behavior: auto`; `scrollIntoView({ behavior: "instant" })`).
2. Integer CSS px everywhere in this file. Draw 1 px lines as 1 px boxes
   (`height: 1px` divs or borders on integer boxes); at fractional DPR browsers snap them
   to device pixels, which is accepted for chrome (only the LCD is guaranteed pixel-exact).
3. No `box-shadow` other than the 1 px button slot; no `filter`, no blur, no gradients,
   no opacity except `--scrim`.
4. The canvas redraws only when the text or its geometry changes (5.4). Hover and
   pressed states are CSS only (no JavaScript on hover). The list re-renders only on a
   model change or when scrolling exposes rows.
5. Timers: the 400 ms `SENT` timeout and the library's debounced save only. Hot-plug is
   `onstatechange`, never a poll. Long press uses the browser's `contextmenu` event.
6. Icons are inline SVG with `fill: var(--token)`; no raster images in the UI except the
   canvas.

---

## 13. PWA visuals

- `<meta name="theme-color" content="#202326">` (`panel`) and in the manifest
  `"theme_color": "#202326"`, `"background_color": "#202326"`, `"display": "standalone"`,
  `"orientation": "any"`, `"name": "D-05 Patch Switcher"`, `"short_name": "D-05 Switcher"`.
- Icons: the desktop icon renders (`assets/icons/d05-patch-switcher.svg` at 192 and 512
  with `inkscape --export-type=png --export-width=<n>`), `"purpose": "any"`, as
  `web/public/icons/icon-192.png` and `icon-512.png`.
- **Maskable icon** (Android adaptive icons crop to a circle, squircle or rounded
  square): `assets/icons/d05-patch-switcher-maskable.svg`, a full-bleed opaque 512
  square (`panel`, the band from y 310 to the bottom edge with a 3 px `bandEdge` line),
  the application icon's display and buttons scaled by 1.5 around their centre (dot
  pitch 12, dot 9). Everything drawn lies inside the central 80 % circle (radius 204.8
  around 256, 256; the farthest point, a display rim corner, is at radius 191). Rendered
  as `assets/icons/d05-patch-switcher-maskable-192.png` and `-512.png` (Inkscape, command
  in `assets/icons/README.md`). The builder copies them unchanged to
  `web/public/icons/icon-maskable-192.png` and `icon-maskable-512.png`, adds both to the
  service worker precache, and appends to the manifest's `icons` array, after the two
  `any` entries:

  ```json
  { "src": "icons/icon-maskable-192.png", "sizes": "192x192", "type": "image/png", "purpose": "maskable" },
  { "src": "icons/icon-maskable-512.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable" }
  ```

  Separate entries, never `"purpose": "any maskable"` (the full-bleed square would then
  also be used unmasked, where the rounded tile is meant).

---

## 14. Mockups

`docs/design/mockup-web/build.py` writes the SVGs and renders them with Inkscape (fonts
restricted to `assets/fonts`), reusing `docs/design/mockup/build.py`'s helpers and glyph
table and reading colours and sizes from `tokens.json`. Each frame is drawn twice, at
dpr 1 and 2, with that DPR's LCD geometry (5.1), and exported at width x dpr.

| File (`docs/design/mockup-web/`, `@2x` beside each) | Size | State |
|---|---|---|
| `desktop-access-prompt.png` | 960 x 560 | First visit in Chrome: access `prompt`, panel disabled, line 2 `PRESS CONNECT MIDI` |
| `desktop-access-denied.png` | 960 x 560 | Chrome, permission denied, the four steps |
| `desktop-unsupported.png` | 960 x 560 | Safari on macOS: no Web MIDI, support matrix |
| `desktop-settings.png` | 960 x 560 | Connected (`Boutique MIDI 1`, channel 3); in-page Settings over the scrim |
| `portrait-main.png` | 390 x 844 | Connected, `P1-11 Fantasia` current, five favourites |
| `portrait-search.png` | 390 x 844 | List overlay, `brass`, `17 MATCHES` on line 2 |
| `portrait-access.png` | 390 x 844 | Android, access `prompt`, OTG text |
| `landscape-main.png` | 844 x 390 | Normal density, All view at the current patch |
| `landscape-compact.png` | 800 x 300 | Compact density (numerals in the pads) |

`python3 docs/design/mockup-web/build.py --print-geometry` prints the 5.1 and 7.1 tables.
Port names in the mockups are sample text.

---

## 15. Readings and open items

For the foreman to confirm (behaviour readings the visuals depend on):

1. **Tap on a list row sends** in the phone layouts (portrait overlay and landscape),
   and the overlay stays open. On desktop the DESIGN.md / SPEC behaviour is unchanged.
2. **First grant opens Settings** when no output port was ever saved (6.1).
3. **Presets first in the phone group strip** (`P1`..`P6`, then `U1`..`U8`), because
   the owner uses presets and all six then fit on screen; desktop order unchanged.
4. **Phone display: name on line 1, address and context on line 2** (5.3); the group
   long name is not shown on phones.
5. **No `logo.png` on the web**: there is no data directory. If the owner wants it, the
   cheapest form is an `Import Logo...` item storing the image in IndexedDB; not
   designed here.
6. **Import replaces** the library (11.6); W1 decides, and the confirmation text
   follows.
7. **Browser-owned shortcuts**: `Cmd+,` opens the browser's own settings in Chrome on
   macOS as far as is known (unverified here), which is why the first-run line no longer
   names a shortcut. W3 tests each FR-13 key in Chrome and Firefox and reports which the
   page cannot receive (11.4).

For the owner:

8. **Maskable PWA icon** (13): done at the W3 design review
   (`assets/icons/d05-patch-switcher-maskable.svg` and its 192 / 512 renders). The owner
   may want to see it on the Redmi's launcher at GW2.
9. **Portrait pads 2 x 4** instead of the panel's 1 x 8 (7.3): bigger targets at the cost
   of a slightly less literal panel. Landscape and desktop keep 1 x 8.
10. **Viewport of the Redmi 10A** is inferred (720 x 1600 panel, so 360 x 800 at dpr 2),
    not measured. At GW2, open the page and read `innerWidth`, `innerHeight` and
    `devicePixelRatio` (or `chrome://inspect`); the layout already fits 360 x 640.
11. **Firefox steps** (6.4) are from Mozilla's bug tracker, not from a current help
    page, and the Edge / Brave / Opera menu names are assumed to match Chrome's
    (Chromium). GW3 confirms the Firefox wording; the texts are one table to edit.

---

## 16. Sources

Panel, display and colour evidence: `docs/design/REFERENCES.md` (S1 to S9), unchanged.
Checked for this addendum on 2026-10-24:

| ID | Source | Used for |
|---|---|---|
| W1 | Google Chrome Help, "Change site settings permissions", <https://support.google.com/chrome/answer/114662>, desktop and Android variants | Desktop: "To the left of the web address, select View site information", then "Site settings". Android: tap View site information left of the address bar, then "Permissions", then the permission. MIDI is listed as "control and reprogram ... MIDI devices" |
| W2 | Google Chrome Help, MIDI device permission, <https://support.google.com/chrome/answer/14871962> | The Block / Allow prompt and the View site information route for MIDI |
| W3 | Mozilla Bugzilla 1801337, <https://bugzilla.mozilla.org/show_bug.cgi?id=1801337> | Firefox MIDI permission is a site permission add-on listed under about:addons, "Site Permissions", where a site's entry is removed (fixed in Firefox 109) |
| W4 | WebKit Bugzilla 107250 "Web MIDI API", <https://bugs.webkit.org/show_bug.cgi?id=107250>; Can I use, <https://caniuse.com/midi> | Safari and every iOS browser: no Web MIDI (bug still NEW; caniuse lists no Safari support). Firefox 108 and Chrome 43 as first versions |
| W5 | web.dev, "Pixel-perfect rendering with devicePixelContentBox", <https://web.dev/articles/device-pixel-content-box>; MDN `ResizeObserverEntry.devicePixelContentBoxSize` | Device-pixel backing store for the LCD (5.1); Chrome 84+, Firefox listed, Safari not |
| W6 | Chrome for Developers, "Scrollbar styling", <https://developer.chrome.com/docs/css-ui/scrollbar-styling>; Frontend Masters, "Heads up on custom scrollbars", <https://frontendmasters.com/blog/heads-up-on-custom-scrollbars-chrome-is-supporting-the-standard-now-which-overrides-the-old-pseudo-elements/> | Chrome 121+ ignores `::-webkit-scrollbar` when `scrollbar-color` / `scrollbar-width` are set (9.6) |
| W7 | GSMArena, Xiaomi Redmi 10A, <https://m.gsmarena.com/xiaomi_redmi_10a-11449.php> | 6.53 in, 720 x 1600 panel; the CSS viewport is inferred, not sourced (15 item 10) |

Not verified and therefore not asserted: Chrome's exact per-site label for MIDI in the
site settings page (the steps say "MIDI devices", which matches both the help text and
the prompt), the Firefox add-on prompt's button wording, the Edge / Brave / Opera menu
names.
