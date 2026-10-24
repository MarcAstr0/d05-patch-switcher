# Design: D-05 Patch Switcher

Task T2.2. The builder implements every visual from this file and invents nothing.
Evidence and sources are in `docs/design/REFERENCES.md` (cited here as S1 to S9); where
this file departs from a decision there, section 13 says why. Behaviour is
`docs/SPEC.md`'s; where a visual needs a behavioural reading the spec does not give,
section 13 states the reading taken so the foreman can confirm or change it.

All sizes are **logical pixels at 1x** (Qt device-independent pixels) unless marked
"device px". All fonts are set with `QFont::setPixelSize`, never point sizes. Coordinates
are integers; x grows right, y grows down; a rect is written `(x, y, w, h)`.

Contents: 1 Principles · 2 Colour tokens · 3 Typography · 4 Window and layout ·
5 Display · 6 Panel parts (section rule, panel button, group bar, bank/number grid,
control row) · 7 Search field and patch list · 8 Status bar, menus, tooltips ·
9 Keyboard focus · 10 Dialogs · 11 Drawing rules and HiDPI · 12 Implementation map ·
13 Deviations, readings, open items · 14 Hand-off to T2.3 and T2.4 · Appendix A glyph table.

---

## 1. Principles

1. The window is a piece of panel: charcoal black, mostly empty, with precise small
   capitals. Groups are made by hairline rules with the title set into the rule, and by
   empty space. No boxes, cards, shadows, gradients, glow or animation.
2. Colour has exactly four roles, as on the real panels (S2, S3):
   off-white for labels and data; blue for the patch-selection section (titles, rules,
   numerals) and keyboard focus; red for lit LEDs and commit-like lettering; yellow-green
   only inside the display. Pale tan is the secondary label colour.
3. Lit LED means "current". The same red slit marks the current group, bank and number
   on the grid, the current patch in the list, the active view and auto-send.
4. Keyboard first: every focusable thing has a visible blue focus state (section 9).
5. Nothing moves except the single `SENT` flash (400 ms), which also lights the status
   bar's activity LED for the same 400 ms.

---

## 2. Colour tokens

One theme. `ui/Theme` exposes each token as a `QColor` constant with the name below;
`assets/theme/app.qss` repeats them in its header comment (T2.3).

### 2.1 Panel and surfaces

| Token | Hex | Use | Source |
|---|---|---|---|
| `panel` | `#202326` | Window background, upper panel, status bar, dialogs | S2 upper panel `#212528` |
| `band` | `#26292B` | Lower band holding the grid, controls and list | S2 lower band `#262827` |
| `bandEdge` | `#3A3E41` | 1 px line on the top edge of the band, status bar top edge | Panel step, S2/S4 |
| `well` | `#141617` | List background (a recess in the band) | Drawn for the app |
| `wellEdgeDark` | `#0B0C0D` | Top and left 1 px border of the list well and fields | Recess shading |
| `wellEdgeLight` | `#34383B` | Bottom and right 1 px border of the list well and fields | Recess shading |
| `fieldBack` | `#0E0F10` | Search field, combo boxes, spin box, inline editor, tooltip | |
| `fieldBorder` | `#3A3D40` | Field border, normal; disabled checkbox border | |
| `fieldBorderHover` | `#4C5053` | Field border, hover; unchecked checkbox border | |
| `rowHover` | `#1B1D1F` | List row under the mouse | |
| `selection` | `#33373B` | Highlighted list row, menu item hover | |
| `selectionBarIdle` | `#6B6E70` | 3 px selection bar when the list is not focused | |
| `matchBack` | `#2E4366` | Search-match background in list rows; text selection in fields | Blue role, dark |

### 2.2 Display

| Token | Hex | Use | Source |
|---|---|---|---|
| `bezel` | `#050606` | Display window (the black recess around the lit field) | S2, S4 |
| `bezelRim` | `#2E3134` | 1 px frame around the window, left, right and bottom | S2 hairline frame |
| `bezelRimTop` | `#45494D` | 1 px frame on the top edge of the window | S4 lighter top rim |
| `lcdBack` | `#071104` | Lit field background (near black, faint green cast) | S2 `#020E02` to `#041700`, S4 `#0A140C` |
| `lcdOn` | `#98CF1C` | Lit dots. Hue 78, contrast 10.3 : 1 on `lcdBack` | S2 `#8CC509` to `#A1D624` |
| `lcdGhost` | `#0F1D09` | Unlit dots (contrast 1.10 : 1 on `lcdBack`, "very faint") | Stylisation, REFERENCES decision 5 |

Switch `lcdShowGhost` (bool, default `true`). When `false` the unlit dots are not drawn
(equivalent to `lcdGhost = lcdBack`). This is the G2 decision on ghost dots; nothing else
changes.

### 2.3 Lettering and rules

| Token | Hex | Use | Source |
|---|---|---|---|
| `labelPrimary` | `#E2E0D4` | Button legends, list names, values, dialog text | S4 warm white, S2 off-white |
| `labelSecondary` | `#A3A293` | Secondary labels, addresses in the list, status labels, tags | S2 pale tan `#ABAB9D` |
| `labelDim` | `#6E6D64` | Placeholder text, unnamed user slots, hints | |
| `labelDisabled` | `#5F5F58` | Any disabled text | |
| `labelAccent` | `#86A6D4` | Section titles and numerals of the patch section, the wordmark | S2 `#819CC9`, `#7FA1CC`, hue 215 |
| `ruleAccent` | `#6E8EBA` | Rules of the patch section | S2 rule `#6E8EBA` |
| `ruleSecondary` | `#8C8B7E` | Rules of secondary sections (dialogs, list section), wordmark divider | S2 tan rules, dimmed |
| `labelRed` | `#E8565C` | `AUTO SEND` label, `NOT FOUND`, error text, default-button border | S2 red lettering hue 355 to 357, lifted for screen legibility (4.1 : 1 on `band`) |
| `arrow` | `#BDB9A0` | Solid wedges on PREV / NEXT and the combo arrow | S4 arrows `#BDB9A0` |
| `starOff` | `#5C5E5A` | Unfilled star outline | |
| `tagBorder` | `#4A4C48` | 1 px box around the key-mode letter in list rows | |

### 2.4 Buttons and LEDs

| Token | Hex | Use | Source |
|---|---|---|---|
| `buttonSlot` | `#121415` | 1 px recess around every button face | S2 darker surround |
| `buttonFace` | `#3F4244` | Face, normal | S2 `#424546` |
| `buttonFaceHover` | `#4A4D50` | Face, hover | |
| `buttonPressed` | `#2F3133` | Face, pressed | |
| `buttonHighlight` | `#56595C` | 1 px line on the top inside edge of the face (not when pressed) | S4 faint top highlight |
| `buttonFaceLit` | `#4B3B3C` | Face of a button whose LED is lit (flat red spill instead of a bloom) | S2 lit faces read red |
| `buttonDisabled` | `#2C2E30` | Face, disabled | |
| `ledOn` | `#F0383F` | Lit LED | S2 `#D82938`, `#DF3D24` |
| `ledCore` | `#FF9C8C` | Lighter core inside a lit LED (the only "glow") | REFERENCES decision 12 |
| `ledOff` | `#2A1A1A` | Unlit LED window (dark lens) | S2 dark lens, S4 `#170909` |
| `focusRing` | `#9DBBE8` | Every keyboard focus indicator (2 px) | Blue role, lightened: 7.5 : 1 on `band` |

No other colours appear anywhere. The builder must not use Qt's default palette colours:
`Theme` sets the application `QPalette` from these tokens (Window `panel`, Base
`fieldBack`, AlternateBase `well`, Text/WindowText/ButtonText `labelPrimary`, Button
`buttonFace`, Highlight `matchBack`, HighlightedText `labelPrimary`, PlaceholderText
`labelDim`, ToolTipBase `fieldBack`, ToolTipText `labelPrimary`, Disabled group
Text/WindowText/ButtonText `labelDisabled`).

---

## 3. Typography

### 3.1 Files

One family, **Inter 4.1** by Rasmus Andersson, SIL Open Font License 1.1, no Reserved
Font Name (verified in the release's `LICENSE.txt`). Chosen because it is the closest
OFL neo-grotesque to the Helvetica-like panel lettering (compared side by side against
S2 `PATCH BANK`, `FUNCTION`, `PRESET/USER`; IBM Plex Sans was the runner-up and is
more mannered in the caps). Source:
<https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip>.

| Committed path (T2.3) | From the release zip | sha256 of the original file |
|---|---|---|
| `assets/fonts/Inter-Regular.ttf` | `extras/ttf/Inter-Regular.ttf` | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
| `assets/fonts/Inter-Medium.ttf` | `extras/ttf/Inter-Medium.ttf` | `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` |
| `assets/fonts/OFL.txt` | `LICENSE.txt` | |
| `assets/fonts/README.md` | written by T2.3: source URL, version, hashes above, the subset command | |

The originals are 411,640 and 417,300 bytes. T2.3 subsets them (allowed: no Reserved Font Name) with
`pyftsubset` to `U+0020-007E,U+00A0-017F,U+2018-201E,U+2022,U+2026,U+2190-2193,U+21E7,U+2318,U+2325,U+2212`
keeping `--layout-features='kern,tnum,case'` and the name table, so the embedded fonts
cost well under 200 KB together (SPEC §5 binary budget). If subsetting cannot be run,
commit the originals unchanged. Result (T2.3): 57,968 and 58,816 bytes, 114 KB together;
command, hashes and checks in `assets/fonts/README.md`. Characters outside the subset (a user typing other
scripts into the search field) fall back to the system font through Qt; that is
accepted.

The display uses **no font**: it is drawn from the 5 x 7 glyph table in Appendix A.

The fonts are compiled into the resource file and registered at startup with
`QFontDatabase::addApplicationFont(":/fonts/Inter-Regular.ttf")` and `...Medium.ttf`.
Family name `Inter`; Medium is selected with `QFont::setWeight(QFont::Medium)` (verified
with Qt 6.4.2 on Linux with no system fonts: resolves to style `Medium`, weight 500). The
Medium file registers two family names, `Inter` and `Inter Medium` (its legacy name ID 1
is `Inter Medium`). Platform font databases that read only the legacy names may expose
just `Inter Medium`, so `Theme` checks once at startup that
`QFontInfo(QFont("Inter") with weight Medium).weight() == 500`; if it is not, every Medium
role uses family `Inter Medium` at the normal weight instead. The
application font is Inter Regular 13 px.

### 3.2 Roles

Letter spacing is `QFont::setLetterSpacing(QFont::AbsoluteSpacing, n)`. "Caps" means the
string is stored and drawn in capitals (do not rely on `QFont::AllUppercase` for strings
the user can see in tests).

| Role | Face | Size | Spacing | Case | Colour | Used for |
|---|---|---|---|---|---|---|
| `type.panelLabel` | Medium | 10 | 0.5 | Caps | per use | Section titles, numerals 1 to 8, labels above buttons, button legends `U1`..`P6` |
| `type.listName` | Regular | 14 | 0 | As stored | `labelPrimary` | Patch names in the list, inline editor |
| `type.listAddress` | Medium | 13 | n/a (fixed cells, 7.2) | Caps | `labelSecondary` | Address in list rows |
| `type.listMeta` | Medium | 10 | 0.5 | Caps | `labelSecondary` | Key-mode letter, recents count, edit counter |
| `type.field` | Regular | 14 | 0 | As typed | `labelPrimary` | Search field text and placeholder |
| `type.statusLabel` | Medium | 10 | 0.5 | Caps | `labelSecondary` | `OUT`, `CH`, `IN`, `FOLLOW`, `NOT FOUND` |
| `type.statusValue` | Regular | 11 | 0 | As named | `labelPrimary` | Port names, channel number |
| `type.body` | Regular | 13 | 0 | Sentence | `labelPrimary` | Dialog text, checkbox text, menu items, combo text |
| `type.note` | Regular | 12 | 0 | Sentence | `labelSecondary` | Settings reminder, About notices, empty-list second line |
| `type.emptyTitle` | Medium | 11 | 1.0 | Caps | `labelSecondary` | Empty-list first line |
| `type.keycap` | Medium | 10 | 0.5 | As named | `labelPrimary` | Key caps in the Shortcuts sheet |
| `type.wordmark` | Medium | 15 | 3.0 | Caps | `labelAccent` | `PATCH SWITCHER` |
| `type.wordmarkSub` | Medium | 10 | 1.0 | Caps | `labelSecondary` | `FOR ROLAND D-05` |

Inter's default figures are proportional (measured: `1` is 0.41 em, `4` is 0.66 em), and
Qt 6.4 has no API to switch on `tnum`. Wherever digits must align (the list address
column) they are drawn in fixed cells: see 7.2.

---

## 4. Window and layout

### 4.1 Sizes

| | Width x height |
|---|---|
| Minimum (`setMinimumSize`) | 800 x 480 |
| Default (first run, no saved geometry) | 960 x 560 |
| Maximum | none |

Window title: `D-05 Patch Switcher`. Saved geometry is restored per SPEC §3.3.

### 4.2 Regions

```
y=0   +----------------------------------------------------------------------------+
      |  [nameplate slot 360x28]                                         panel      |
y=46  |  +--------------------------- display bezel ---------------------------+   |
      |  |  P1-11:Fantasia                                               DUAL |   |
      |  |  PRESET 1: ORIGINAL D-50                                           |   |
      |  +--------------------------------------------------------------------+   |
B     +============================== band (bandEdge on top) ====================+
      |  --PATCH GROUP--                    ALL  FAVOURITES  RECENTS   AUTO SEND SYSTEM
      |  [U1][U2][U3][U4][U5][U6][U7][U8]   [==] [==]  [==]            [==]   [  ] |
      |  [P1][P2][P3][P4][P5][P6]                                                 |
      |  ----------PATCH BANK----------     +-------- search field --------------+|
      |   1   2   3   4   5   6   7   8     +-------------------------------------+|
      |  [==][==][==][==][==][==][==][==]   +-------- list well ------------------+|
      |  ---------PATCH NUMBER---------     | ▪ P1-11  Fantasia            D  ☆ ||
      |   1   2   3   4   5   6   7   8     |   P1-12  Metal Harp          D  ★ ||
      |  [==][==][==][==][==][==][==][==]   |   ...                               ||
      |   PREV NEXT                         |                                     ||
      |  [◀ ][ ▶]                           +-------------------------------------+|
H-22  +-------------------------------- status bar ------------------------------+
      | ▬ OUT Hapax MIDI 1   CH 1   IN None                             <message> |
H     +----------------------------------------------------------------------------+
```

### 4.3 Coordinates

Outer margin `M = 16` on the left and right. The window width selects the display
step `p` (3, 4 or 5) from the table below; together with the device pixel ratio it sets
the device dot pitch `P` and the bezel size `bezelW x bezelH` (section 5.1). Every rect
below uses the bezel size the display actually reports, never the 1x values.

| Element | Rect | Notes |
|---|---|---|
| Nameplate slot | `(16, 10, min(360, W-32), 28)` | Section 4.5 |
| Display bezel | `(16, 46, bezelW, bezelH)` | Left aligned; the space to its right stays empty panel |
| Band | `(0, B, W, H-22-B)` with `B = 46 + bezelH + 14` | `band` fill, 1 px `bandEdge` line at its top row |
| Status bar | `(0, H-22, W, 22)` | Section 8.1 |
| Left column | `x = 16`, width 394 | Fixed size, top aligned in the band |
| Right column | `x = 434`, width `W - 450` | 24 px gutter after the left column; grows |

Steps (values at 1x and 2x; other DPRs in the table in 5.1):

| p | bezelW x bezelH | B | Step used while |
|---|---|---|---|
| 3 | 767 x 79 | 139 | `W < 1042` |
| 4 | 1010 x 98 | 158 | `1042 <= W < 1285` |
| 5 | 1253 x 117 | 177 | `W >= 1285` |

The step thresholds 1042 and 1285 are logical and do not depend on the DPR. At
fractional DPR the pitch inside a step may take one of two values depending on `W`
(5.1); the display never exceeds `W - 32`, so it always keeps the 16 px margins.

Left column, y measured from `B` (all rows span x 16 to 410; button column `i` (0..7)
has its face at `x = 16 + 50 i`, width 44, so the faces are 44 wide with 6 px gaps):

| Offset from B | Height | Element |
|---|---|---|
| +12 | 12 | Section rule `PATCH GROUP`, blue (6.1) |
| +28 | 24 | Group row U: `U1`..`U8` in columns 0..7 |
| +56 | 24 | Group row P: `P1`..`P6` in columns 0..5; columns 6, 7 empty |
| +92 | 12 | Section rule `PATCH BANK`, blue |
| +106 | 11 | Numerals `1`..`8`, each centred over its column |
| +119 | 24 | Bank buttons, columns 0..7 |
| +155 | 12 | Section rule `PATCH NUMBER`, blue |
| +169 | 11 | Numerals `1`..`8` |
| +182 | 24 | Number buttons, columns 0..7 |
| +218 | 11 | Labels `PREV` over column 0, `NEXT` over column 1: `type.panelLabel`, `labelPrimary`, centred on the button, baseline at the row's `y + 9` (as 6.5) |
| +231 | 24 | PREV button column 0, NEXT button column 1 |
| +267 | | End of content (12 px bottom padding) |

Right column, y from `B`, x from the column's left edge `X = 434`, width `R = W - 450`:

| Offset from B | Height | Element |
|---|---|---|
| +15 | 11 | Control labels (6.5) |
| +28 | 24 | Control buttons, aligned with group row U |
| +64 | 32 | Search field `(X, B+64, R, 32)` |
| +104 | to band bottom - 12 | List well `(X, B+104, R, H-22-B-116)` |

Check at the extremes: the left column needs 267 px of band; the smallest band is
`480 - 22 - 180 = 278` (dpr 1.75, P = 9, at minimum height; 281 at 1x and 2x). The
smallest list is at 800 x 480: `350 x 203` (B = 139 at 1x and 2x; every other DPR has a
smaller B at 800 px, so a taller list), seven full rows. The widest display at
`W = 800` is 767 (1x, 2x), which leaves 17 px on the right.

### 4.4 Resize rules

1. Width and height never go below 800 x 480.
2. The device dot pitch `P` changes only at the width thresholds in 4.3 and the
   DPR-dependent ones in the 5.1 table, which changes `bezelH` and therefore `B`. The
   glyph atlas is rebuilt only when `P` or `g` changes (a resize across a threshold, or
   a move to a screen with another DPR, which also re-evaluates the thresholds).
3. The left column (group bar, grid, transport) has a fixed size and position relative
   to `B`; extra height stays empty band below it.
4. The right column takes all extra width; the list well takes all extra height. Rows
   keep their height (28); more rows show.
5. The nameplate slot and status bar items do not scale; the port name in the status
   bar elides in the middle (8.1).
6. The display and buttons never stretch non-uniformly. No element scales with the
   window except the display pitch steps.
7. Dialogs have fixed widths and are not resizable (section 10).

### 4.5 Nameplate slot and wordmark

The slot is `(16, 10, min(360, W-32), 28)` on `panel`. It shows exactly one of:

**logo.png present** (`<data dir>/logo.png`, SPEC FR-14). Load once at startup. Fit
inside the slot preserving aspect ratio, left aligned, vertically centred, never
upscaled: logical size = the largest size that fits `360 x 28` and is not larger than
`source px / devicePixelRatio`. Scale once with `Qt::SmoothTransformation` to device
pixels and cache the pixmap with `setDevicePixelRatio`; rescale only when the DPR
changes. No tint, frame or background. This is the one documented raster on screen
because the file is the owner's own artwork; nothing is shipped.

**Absent: the wordmark**, drawn with text, all baselines at slot `y + 20`:

| Part | x from slot left | Content | Style |
|---|---|---|---|
| 1 | 0 | `PATCH SWITCHER` | `type.wordmark`, `labelAccent` (about 174 px wide) |
| 2 | end of part 1 + 14 | 1 px vertical rule from slot `y+9` to `y+21` | `ruleSecondary` |
| 3 | rule + 15 | `FOR ROLAND D-05` | `type.wordmarkSub`, `labelSecondary` (about 106 px) |

Total about 310 px, inside the 360 px slot. The wordmark is plain set type in Inter: no
Roland logo, no imitation of the `D-50` / `D-05` logotypes (REFERENCES §4). The same
wordmark heads the About box.

---

## 5. Display (`ui/DisplayWidget`)

### 5.1 Geometry

Two lines of 40 cells, 5 x 7 dots per cell (S4, S5, REFERENCES decisions 1 to 3).

Let `p` be the display step (3, 4 or 5, chosen by window width in 4.3), `dpr` the
device pixel ratio and `W` the window width in logical px. All drawing of the field
happens in **device pixels**:

| Quantity | Device px | At 1x, p = 3 | At 2x, p = 3 |
|---|---|---|---|
| Pitch `P` | integer, chosen by the rule below | 3 | 6 |
| Gap `g` | `clamp(qRound(dpr), 1, P / 3)` (integer division) | 1 | 2 |
| Dot `d` | `P - g` (square) | 2 | 4 |
| Cell advance | `6 P` (5 dots + 1 empty pitch) | 18 | 36 |
| Line advance | `8 P` (7 dots + 1 empty pitch) | 24 | 48 |
| Dot area | `(40 * 6 - 1) P x (2 * 8 - 1) P` = `239 P x 15 P` | 717 x 45 | 1434 x 90 |
| Lit field | dot area + `2 P` padding on every side = `243 P x 19 P` | 729 x 57 | 1458 x 114 |

Logical sizes are device sizes divided by `dpr` (exact at 1x and 2x; at fractional
DPR round the field up to whole logical pixels and centre the dot area in it).

**Choosing `P`.** The step `p` has a design bezel width `T = 243 p + 38` (767, 1010,
1253). Write `fieldW(P) = ceil(243 P / dpr)`, `fieldH(P) = ceil(19 P / dpr)` and
`bezelW(P) = fieldW(P) + 38` (logical px). Then:

```
Pa = floor(p * dpr)            // never wider than T, always fits the step
Pb = Pa + 1
P  = Pb  if  p * dpr is not an integer
         and bezelW(Pb) + 32 <= W                       // fits with the 16 px margins
         and |bezelW(Pb) - T| < |bezelW(Pa) - T|        // strictly closer; ties keep Pa
     Pa  otherwise
```

So `P` is the integer pitch whose logical width is nearest the design width, taken only
once the window is wide enough to hold it. `Pa` always fits: `bezelW(Pa) <= T` and
every step starts at `W >= T + 32` (step 3 at the 800 px minimum, `T + 32 = 799`). At
1x and 2x, `p * dpr` is an integer and `P = p * dpr` exactly, so those two cases are
unchanged. Compute `Pa` as `int(std::floor(p * dpr + 1e-9))` and treat `p * dpr` as an
integer when `p * dpr - Pa < 1e-9`, so float error at DPRs that are not exact binary
fractions (1.1, 1.15) cannot drop a pixel. The gap cap `P / 3` keeps the dot at least two thirds of the pitch, as
at 1x (`d / P` is 2/3, 3/4 or 4/5 at 1x).

Resulting geometry (logical px unless marked; "vs T" is `bezelW - T`; band and list at
`H = 480`):

| dpr | p | P dev | pitch log | g dev | d dev | field | bezel | vs T | B | band at H 480 | Used while |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 3 | 3 | 3 | 1 | 2 | 729 x 57 | 767 x 79 | 0 | 139 | 319 | `800 <= W < 1042` |
| 1 | 4 | 4 | 4 | 1 | 3 | 972 x 76 | 1010 x 98 | 0 | 158 | 300 | `1042 <= W < 1285` |
| 1 | 5 | 5 | 5 | 1 | 4 | 1215 x 95 | 1253 x 117 | 0 | 177 | 281 | `W >= 1285` |
| 1.25 | 3 | 3 | 2.4 | 1 | 2 | 584 x 46 | 622 x 68 | -145 | 128 | 330 | `800 <= W < 848` |
| 1.25 | 3 | 4 | 3.2 | 1 | 3 | 778 x 61 | 816 x 83 | +49 | 143 | 315 | `848 <= W < 1042` |
| 1.25 | 4 | 5 | 4 | 1 | 4 | 972 x 76 | 1010 x 98 | 0 | 158 | 300 | `1042 <= W < 1285` |
| 1.25 | 5 | 6 | 4.8 | 1 | 5 | 1167 x 92 | 1205 x 114 | -48 | 174 | 284 | `W >= 1285` |
| 1.5 | 3 | 4 | 2.67 | 1 | 3 | 648 x 51 | 686 x 73 | -81 | 133 | 325 | `800 <= W < 1042` (P = 5 would be 848, +81: a tie, so P stays 4) |
| 1.5 | 4 | 6 | 4 | 2 | 4 | 972 x 76 | 1010 x 98 | 0 | 158 | 300 | `1042 <= W < 1285` |
| 1.5 | 5 | 7 | 4.67 | 2 | 5 | 1134 x 89 | 1172 x 111 | -81 | 171 | 287 | `W >= 1285` (P = 8 would be 1334, +81: tie) |
| 1.75 | 3 | 5 | 2.86 | 1 | 4 | 695 x 55 | 733 x 77 | -34 | 137 | 321 | `800 <= W < 1042` |
| 1.75 | 4 | 7 | 4 | 2 | 5 | 972 x 76 | 1010 x 98 | 0 | 158 | 300 | `1042 <= W < 1285` |
| 1.75 | 5 | 8 | 4.57 | 2 | 6 | 1111 x 87 | 1149 x 109 | -104 | 169 | 289 | `1285 <= W < 1320` |
| 1.75 | 5 | 9 | 5.14 | 2 | 7 | 1250 x 98 | 1288 x 120 | +35 | 180 | 278 | `W >= 1320` |
| 2 | 3 | 6 | 3 | 2 | 4 | 729 x 57 | 767 x 79 | 0 | 139 | 319 | `800 <= W < 1042` |
| 2 | 4 | 8 | 4 | 2 | 6 | 972 x 76 | 1010 x 98 | 0 | 158 | 300 | `1042 <= W < 1285` |
| 2 | 5 | 10 | 5 | 2 | 8 | 1215 x 95 | 1253 x 117 | 0 | 177 | 281 | `W >= 1285` |

Fit check at 800 x 480: the bezel is at most 767 wide at `W = 800` (16 + 767 = 783, 17
px to the right edge); the band is at least 278 against the 267 the left column needs;
the right column (x 434, width 350) does not depend on the display; the list well is at
least `350 x 203` (B 139). The one case a 1x layout check would miss is 1.25 at
`848 <= W < 1042`: the display is 49 px wider than at 1x and still leaves 16 px of
margin, because the bezel sits above the band and only its own row is affected.

`DisplayWidget` needs `W` for this rule: the main window passes the window's width
(not the widget's) through a setter, for example `setStep(int p, int windowWidth)`,
and reads `sizeHint()` back for `bezelW x bezelH`. The widget recomputes `P` and `g`
on that call and on a DPR change (handle `QEvent::ScreenChangeInternal` in `event()`
on the 6.4 floor, plus `QEvent::DevicePixelRatioChange` when built against 6.6+; in
both, compare `devicePixelRatioF()` with the cached value), and rebuilds the atlas only
if `P` or `g` changed.

Around the field, in logical px:

| Part | Rect relative to the bezel's top-left | Fill |
|---|---|---|
| Bezel (whole widget) | `(0, 0, fieldW + 38, fieldH + 22)` | `bezel` |
| Frame | 1 px around the bezel edge: top row `bezelRimTop`, the other three sides `bezelRim` | |
| Lit field | `(19, 11, fieldW, fieldH)` | `lcdBack` |
| Dot area | field inset by `2P` device px | dots only |

So at 1x and 2x: p = 3 gives a 767 x 79 widget, p = 4 gives 1010 x 98, p = 5 gives
1253 x 117; fractional DPRs follow the table above.
Corners are square everywhere (S2, S4). The margins (18 px beside the field, 10 above
and below) are tighter than the real windows (about 2 to 3 character pitches, S2, S4)
so that 40 columns fit an 800 px window; see 13.

Dot placement: cell `k` (0..39) on line `l` (0..1) has its top-left at
`(fieldX + 2P + 6P k, fieldY + 2P + 8P l)`; dot (column `c` 0..4, row `r` 0..6) is the
square `(cellX + c P, cellY + r P, d, d)` filled with `lcdOn` when its bit is set and
`lcdGhost` otherwise (when `lcdShowGhost`). No antialiasing, no rounding, no blur.

### 5.2 Content

Line 1, columns numbered 1 to 40:

| Columns | Width | Content | Alignment |
|---|---|---|---|
| 1 to 5 | 5 | Address `P1-11` (always 5 characters) | left |
| 6 | 1 | `:` (as on the D-05, S2: `U1-11:Fantasia`) | |
| 7 to 26 | 20 | Name, as stored (mixed case): presets up to 20 (`Alpha Omega Ensemble`), user names up to 18. Never truncated in practice; if longer than 20, cut at 20 | left |
| 27 to 35 | 9 | Spaces (at least 9 empty cells between name and key mode) | |
| 36 to 40 | 5 | Key mode `WHOLE`, `DUAL` or `SPLIT` for `W`, `D`, `S`; blank for user slots | right |

Examples (dots `.` = blank cells, shown for counting):

```
         1         2         3         4
1234567890123456789012345678901234567890
P1-11:Fantasia......................DUAL
P1-18:Alpha Omega Ensemble..........DUAL   (20-character name)
P2-11:Some Name....................WHOLE   (WHOLE fills 36 to 40)
U3-45:..................................   (unnamed user slot: name and mode blank)
U1-11:Warm Pad Mk2......................   (named user slot, mode unknown)
```

Line 1 shows the **current patch** (the one last sent or followed), not the list
highlight; see 13.

Line 2, left aligned from column 1, always capitals, first matching rule wins (FR-12):

| Priority | Condition | Text |
|---|---|---|
| 1 | No output port open | `NO MIDI OUTPUT` |
| 2 | Within 400 ms after a send | `SENT` |
| 3 | Search field non-empty | `NO MATCHES`, `1 MATCH`, or `<n> MATCHES` (`23 MATCHES`) |
| 4 | Otherwise | Group long name of the current patch, table below |
| (M5) | No port configured yet, instead of 1 | `OPEN SETTINGS: CTRL+,` (`CMD+,` on macOS) |

| Group | Long name | Group | Long name |
|---|---|---|---|
| U1..U8 | `USER 1` .. `USER 8` | P4 | `PRESET 4: PN-D50-02` |
| P1 | `PRESET 1: ORIGINAL D-50` | P5 | `PRESET 5: PN-D50-03` |
| P2 | `PRESET 2: NEW FOR D-05` | P6 | `PRESET 6: PN-D50-04` |
| P3 | `PRESET 3: PN-D50-01` | | |

(Names from `docs/MIDI.md`, set in capitals.)

**Entry echo** (switch `displayEntryEcho`, default `true`): while a grid entry is pending
(6.4), line 1 shows only the address being entered, the rest of line 1 blank:
`P3-__` after choosing a group or pressing `Ctrl+G`, `P3-4_` after the bank digit. On
commit or cancel line 1 returns to the current patch. Line 2 is unaffected.

### 5.3 States

| State | Line 1 | Line 2 | Other |
|---|---|---|---|
| Normal, connected | current patch | group long name | |
| Disconnected | current patch | `NO MIDI OUTPUT` | Status bar 8.1 shows `NOT FOUND` |
| Sent flash | new current patch | `SENT` for 400 ms | Status LED lit for the same 400 ms; one single-shot timer drives both |
| Search active | current patch | `<n> MATCHES` | |
| Empty search result | current patch | `NO MATCHES` | List empty state 7.5 |
| Entry pending | `P3-4_` | unchanged | |

The display has no hover, pressed, focus or disabled state. It is not focusable and
ignores clicks.

### 5.4 Rendering and footprint

- The glyph atlas: one `QImage` (`Format_RGB32`) per (`P`, `g`, `lcdShowGhost`), holding
  all 95 glyphs (0x20 to 0x7E) as `5P x 7P` device-px cells drawn on `lcdBack`. Built
  once, rebuilt only when `P` or `g` changes (5.1).
- The frame: one `QPixmap` of the lit field (device px, `setDevicePixelRatio(dpr)`).
  `setLines(line1, line2)` compares with the stored text; if equal it returns without
  doing anything. Otherwise it blits only the changed cells from the atlas into the
  frame and calls `update()`.
- `paintEvent` draws the bezel fill, the four frame lines and the cached field pixmap.
  No text layout, no allocation.
- Characters outside 0x20..0x7E (not expected: names are printable ASCII) render as `?`.

---

## 6. Panel parts

### 6.1 Section rule (title set into a hairline)

Observed on the D-05 (S2, re-checked for this document at 840 px crops): the title sits
centred in a gap in the rule, and the rule passes through the **lower part** of the
capitals, not their middle; the numerals sit just below the rule.

Widget height 12. Text `type.panelLabel`, baseline at `y + 10` (caps occupy about
`y + 3` to `y + 10`). The rule is 1 px tall at row `y + 8`, spanning the full width,
interrupted from `textLeft - 6` to `textRight + 6`; the title is centred horizontally.

| Variant | Title colour | Rule colour | Used for |
|---|---|---|---|
| Accent | `labelAccent` | `ruleAccent` | `PATCH GROUP`, `PATCH BANK`, `PATCH NUMBER` |
| Secondary | `labelSecondary` | `ruleSecondary` | Dialog sections |

The rule ends are plain (no ticks). Width is the full column (394 in the main window).

### 6.2 Panel button (shared by every button in the main window)

A custom-painted button. Face 44 x 24, radius 2, antialiased edges.

| Layer | Geometry (relative to the face) | Colour |
|---|---|---|
| Slot | rect `(-1, -1, 46, 26)`, radius 3 | `buttonSlot` |
| Face | `(0, 0, 44, 24)`, radius 2 | `buttonFace` (state table) |
| Top highlight | `(2, 1, 40, 1)` | `buttonHighlight` |
| LED (kinds A, B, C) | see the kinds | `ledOn` / `ledOff` |

Kinds:

| Kind | Content of the face | Used for |
|---|---|---|
| A. LED | LED window `(15, 10, 14, 4)`, centred, as the D-05 bank buttons (S2) | Bank, number, ALL, FAVOURITES, RECENTS, AUTO SEND |
| B. Legend + LED | Legend `type.panelLabel` centred, baseline at y 11; LED window `(15, 16, 14, 3)` | Group buttons `U1`..`P6` |
| C. Plain | nothing | SYSTEM |
| D. Wedge | solid triangle in `arrow`: PREV points left with vertices `(18,12) (25,8) (25,16)`; NEXT points right `(26,12) (19,8) (19,16)` | PREV, NEXT |

Lit LED: fill `ledOn`, plus core `(x+2, y+1, w-4, max(1, h-2))` of the LED rect in
`ledCore` (for a 14 x 4 LED: core 10 x 2). Unlit LED: fill `ledOff`, no core.

States (any combination of "lit" with the others is possible):

| State | Face | Highlight | Content | Ring |
|---|---|---|---|---|
| Normal | `buttonFace` | yes | as kind | |
| Hover | `buttonFaceHover` | yes | as kind | |
| Pressed | `buttonPressed` | no | legend, LED and wedge shifted 1 px down | |
| Lit (current, active) | `buttonFaceLit` (hover: `buttonFaceHover`; pressed: `buttonPressed`) | yes | LED lit | |
| Focused (keyboard cursor) | as above | | | 2 px `focusRing` around the face: the rect `(-4, -4, 52, 32)` stroked 2 px inside, square corners |
| Disabled | `buttonDisabled` | no | legend `labelDisabled`, LED `ledOff`, wedge `labelDisabled` | none |

Legend colour on kind B is `labelPrimary`. The mouse cursor stays the arrow (hardware
feel). Each button has a tooltip naming the action and its shortcut.

### 6.3 Group bar (`ui/GroupBar`)

Fourteen kind-B buttons in two rows at the positions in 4.3 (row U: `U1`..`U8`; row P:
`P1`..`P6` in the first six columns). Under the `PATCH GROUP` rule.

- **Current**: the group of the current patch is lit; exactly one lit unless an entry is
  pending.
- **Click** on a group starts an entry (6.4): that group lights, the previously lit
  group goes dark, bank and number LEDs go dark, display shows `P3-__`.
- Hover, pressed, focused, as 6.2. Never disabled.
- One focus widget. Its keyboard cursor (a focused button ring) starts on the current
  group; `Left`/`Right` move within a row, `Up`/`Down` switch rows (clamped to the
  nearest existing column), `Enter`/`Space` press the cursor's group.

### 6.4 Bank / number grid (`ui/BankGrid`)

Two rows of eight kind-A buttons under `PATCH BANK` and `PATCH NUMBER`, numerals 1..8
above each button: `type.panelLabel`, colour `labelAccent`, centred on the button, text
baseline at the numeral row's `y + 9` (S2: numerals blue on the D-05).

The grid always shows the **current** patch: its bank and number buttons lit (one each).

Entry (keyboard `Ctrl+G` then two digits, or mouse group / bank / number clicks):

| Step | Group LED | Bank LEDs | Number LEDs | Numerals | Display line 1 |
|---|---|---|---|---|---|
| Idle | current | current lit | current lit | all `labelAccent` | current patch |
| Awaiting bank (`Ctrl+G`, or a group click) | chosen | all dark | all dark | bank numerals `labelPrimary` | `P3-__` |
| Awaiting number (bank digit or bank click) | chosen | chosen bank lit | all dark | number numerals `labelPrimary` | `P3-4_` |
| Committed (number digit or number click) | new current | new current | new current | all `labelAccent` | new current; line 2 `SENT` |
| Cancelled (`Esc`, focus leaves the grid, or a click outside the grid) | back to idle | | | | |

A click on a bank button while idle starts at "awaiting number" with the current group.
A click on a number button while idle sends `current group, current bank, that number`
immediately (SPEC FR-2 "click on a bank/number button"). The brightened numerals are
the only "awaiting" cue: no blinking (footprint).

Keyboard: one focus widget with a button cursor (ring, 6.2). Cursor starts on the
current bank button; `Left`/`Right` move within a row, `Up`/`Down` switch rows,
`Enter`/`Space` press the cursor's button, digits `1`..`8` act on the awaiting row
regardless of the cursor, which follows the digit.

### 6.5 Control row: views, auto-send, system menu

Kind-A and kind-C buttons in the right column at `y = B + 28`, labels above at
`y = B + 15` (`type.panelLabel`, baseline at label row `y + 9`, centred on the button,
allowed to be wider than the button). x relative to the right column's left `X`, `R` its
width:

| Button | Face x | Kind | Label | Label colour | Lit when |
|---|---|---|---|---|---|
| All | `X + 0` | A | `ALL` | `labelPrimary` | All view active |
| Favourites | `X + 72` | A | `FAVOURITES` | `labelPrimary` | Favourites view active |
| Recents | `X + 150` | A | `RECENTS` | `labelPrimary` | Recents view active |
| Auto-send | `X + R - 124` | A | `AUTO SEND` | `labelRed` | auto-send on |
| System | `X + R - 44` | C | `SYSTEM` | `labelPrimary` | never |

The three view buttons are a radio group: exactly one lit. Auto-send is a toggle; its
red label follows the D-05's red commit lettering (`WRITE`, `ENTER`, S2) because it
makes highlights send. SYSTEM opens the system menu (8.2) below the button, its left
edge aligned to the menu's right edge. Each is its own focus stop (6.2 ring).

---

## 7. Search field and patch list

### 7.1 Search field

`QLineEdit` `#searchField` at `(X, B+64, R, 32)`. No clear button, no icon.

| State | Background | Border | Padding (left, right) |
|---|---|---|---|
| Normal | `fieldBack` | 1 px: top and left `wellEdgeDark`, bottom and right `fieldBorder` | 10, 10 |
| Hover | `fieldBack` | 1 px `fieldBorderHover` all sides | 10, 10 |
| Focused | `fieldBack` | 2 px `focusRing` all sides | 9, 9 (text does not move) |
| Disabled | not used | | |

Text `type.field` `labelPrimary`; selection `matchBack` with `labelPrimary` text; caret
`labelPrimary` 1 px. With QLineEdit's built-in 2 px horizontal and 1 px vertical text
margins the text starts at `X + 13` and its baseline is at field `y + 21` in every state
(the mockup is drawn so; a screenshot that differs means the padding is wrong). Placeholder (`labelDim`): `Search names or addresses: brass, P3-45`.
No border radius. `Esc` behaviour per FR-13.

### 7.2 List row (`ui/PatchDelegate`)

List view `#patchList` fills the well `(X, B+104, R, ...)`. Well: background `well`,
1 px border with top and left `wellEdgeDark`, bottom and right `wellEdgeLight`; focused:
1 px `focusRing` all sides. Uniform row height **28**, no grid lines, no alternating
colours, no header. Inner width `Wl = R - 2` (minus the scrollbar when shown).

Columns, x from the inner left edge:

| Column | Rect in the row | Content |
|---|---|---|
| Selection bar | `(0, 0, 3, 28)` | Highlighted row only: `focusRing` if the list has focus, else `selectionBarIdle` |
| Current LED | `(10, 12, 10, 4)` | Current patch only: lit LED (`ledOn`, core `(12, 13, 6, 2)` `ledCore`). Other rows: nothing |
| Address | `x = 28`, five cells of 9 px (45 px) | `type.listAddress`, each character centred in its cell, baseline y 19. `labelSecondary`; `labelPrimary` on the current row |
| Name | `x = 84` to `Wl - 100` | `type.listName`, baseline y 19, `labelPrimary`, elided right. Unnamed user slot: the display name (the address) in `labelDim` |
| Recents count | `(Wl - 100, 0, 40, 28)` | Recents view only: `×12` right aligned, `type.listMeta`, `labelSecondary`; blank in other views |
| Key mode | box `(Wl - 52, 6, 16, 16)` | 1 px `tagBorder` square, letter `W`/`D`/`S` centred `type.listMeta` `labelSecondary`; nothing for user slots |
| Star | centre `(Wl - 16, 14)` | 5-point star, outer radius 6, inner radius 2.6, one point up. Off: 1 px `starOff` stroke, no fill; on: `labelPrimary` fill, no stroke. Row hover: off-star stroke `labelSecondary`. Antialiased |

Address cells: build once (in the delegate's constructor) a `QStaticText` for each of
`0`..`9`, `U`, `P`, `-`, and draw each address character from them, so digits align and
the paint path does not allocate.

Row states:

| State | Background | Notes |
|---|---|---|
| Normal | none (well shows) | |
| Hover | `rowHover` | |
| Highlighted (selection) | `selection` | + selection bar |
| Current | as normal/hover/highlighted | + lit LED, address in `labelPrimary` |
| Favourite | | star on |
| Editing | | 7.4 |

### 7.3 Search-match highlight

`SearchQuery` returns `(start, length)` ranges into the displayed name. For each range,
fill the rect `x = 84 + advance(name, start)`, width
`advance(name, start + length) - advance(name, start)`, `y = 4`, height 20, with
`matchBack`, before drawing the name. `advance(name, n)` is
`QFontMetrics::horizontalAdvance(name, n)` on `type.listName` (no substring copies). Text
colour stays `labelPrimary`. Ranges past an elision point are clipped to the visible
text. Highlighted rows show the same `matchBack` on `selection`. Address tokens produce
no ranges: nothing is highlighted for them.

### 7.4 User-slot inline editor (F2 or double-click on a U row)

A `QLineEdit` `#nameEditor` over `(80, 2, Wl - 180, 24)` of the row, where `Wl` is the
same inner width the row paint uses (scrollbar excluded), so the editor ends 20 px before
the counter column at `Wl - 100`: background
`fieldBack`, 1 px `focusRing` border, padding 0 1 px (1 px border + 1 px padding + QLineEdit's
built-in 2 px margin put the text origin at x 84, where the name is drawn when not editing),
`type.listName`, `labelPrimary`, max length 18, printable ASCII validator. While
editing, the recents-count column shows `n/18` (`type.listMeta`, `labelSecondary`;
`labelRed` at 18/18). `Enter` commits, `Esc` cancels. Preset rows never open it.

### 7.5 Empty states

Drawn centred in the well, the block's top at 38 % of the well height:
line 1 `type.emptyTitle` `labelSecondary`; 8 px gap; line 2 `type.note` `labelDim`.

| Situation | Line 1 | Line 2 |
|---|---|---|
| Search with no result (any view) | `NO MATCHES` | `Esc clears the search` |
| Favourites view, none | `NO FAVOURITES` | `Space or Ctrl+D adds the highlighted patch` |
| Recents view, none | `NOTHING SENT YET` | `Patches you send appear here` |

Key names come from `QKeySequence::toString(QKeySequence::NativeText)` so macOS shows its
own symbols.

### 7.6 Scrollbar (list only)

Vertical only, 10 px wide, inside the well's border. Track `well`; handle `buttonFace`,
hover `buttonFaceHover`, pressed `buttonPressed`, square corners, minimum length 24,
2 px margin to the track edges horizontally; no arrow buttons. Horizontal scrollbar off.

---

## 8. Status bar, menus, tooltips

### 8.1 Status bar

`(0, H-22, W, 22)`, `panel` fill, 1 px `bandEdge` line on its top row. Contents,
vertically centred, from x 16:

| Item | Geometry | Content |
|---|---|---|
| Activity LED | `(16, 9, 8, 4)` | Lit (no core) for the 400 ms of the `SENT` flash, also on an incoming follow message (same timer); `ledOff` otherwise |
| `OUT` | x 30 | `type.statusLabel` |
| Port name | 6 px after `OUT`, max 280 px, elided in the middle | `type.statusValue`; `None` in `labelDim` when no port is set |
| `NOT FOUND` | 6 px after the name, only when disconnected | `type.statusLabel` in `labelRed` |
| `CH` + number | 16 px after the previous item | `CH` label, 6 px, value `type.statusValue` |
| `IN` + port | 16 px after | as `OUT` (label, 6 px, value); `None` in `labelDim` |
| `FOLLOW` | 16 px after, only when follow-the-panel is on and an input is set | `type.statusLabel` in `labelPrimary` |
| Message | right aligned, ending at `W - 16` | `type.statusValue` in `labelSecondary`, e.g. the broken-library notice; stays until replaced |

Gaps are measured from the end of the previous text's advance. Every status text,
10 px and 11 px alike, is vertically centred in the 22 px bar, which puts all baselines
on the bar's `y + 15` (ascent + descent are 10 + 2 and 11 + 3 px). No size grip.

### 8.2 System menu and context menus

`QMenu`: background `panel`, 1 px `fieldBorder` border, no radius, padding 4 px top and
bottom. Item: `type.body`, `labelPrimary`, padding 6 px 24 px 6 px 12 px, height 28;
hover/selected background `selection`; disabled text `labelDisabled`; separator 1 px
`fieldBorder` with 4 px vertical margin; shortcut text drawn by Qt.

System menu items, in order: `Settings...` (`Ctrl+,`), `Keyboard Shortcuts` (`F1`),
`Clear Recents...`, separator, `About D-05 Patch Switcher`, separator, `Quit` (`Ctrl+Q`).
On macOS the same actions also go into the native menu bar (`QMenuBar` without parent);
no in-window menu bar on any platform.

### 8.3 Tooltips

`QToolTip`: background `fieldBack`, 1 px `fieldBorder`, `type.note` in `labelPrimary`,
padding 4 px 6 px, no radius.

---

## 9. Keyboard focus

Every focus indicator uses `focusRing`, 2 px, square corners, never a dotted Qt focus
rect (disable `Qt::WA_MacShowFocusRect` and the style's focus frame).

| Widget | Focus visual |
|---|---|
| Search field | 2 px border (7.1) |
| Patch list | 1 px well border in `focusRing`, highlighted row's bar in `focusRing` |
| Group bar, grid | Ring around the cursor button (6.2); during an entry, the awaiting row's numerals in `labelPrimary` |
| Panel buttons (views, AUTO SEND, SYSTEM, PREV, NEXT) | Ring (6.2) |
| Dialog buttons, combos, spin box | 2 px `focusRing` border (10.2) |
| Checkboxes | Indicator border `focusRing`, text `labelPrimary` stays (10.2) |

Tab order in the main window: search field, list, ALL, FAVOURITES, RECENTS, AUTO SEND,
SYSTEM, group bar, grid, PREV, NEXT, then back to the search field. Initial focus: the
search field. Focus is shown only after keyboard navigation or on widgets that always
show it (search field, list); mouse clicks on panel buttons do not leave a ring
(`Qt::TabFocus` policy for panel buttons, `Qt::StrongFocus` for the list and field).

---

## 10. Dialogs

All dialogs: background `panel`, content margins 20, title bar native, fixed size
(`setFixedWidth`, height from the layout), not resizable. Section titles use the
secondary section rule (6.1) across the full content width.

### 10.1 Settings (`ui/SettingsDialog`, FR-9)

Width 520 (content 480). Window title `Settings`. Two-column form: label column 112 px
(`type.panelLabel` caps, `labelPrimary`, right edge at x 100, vertically centred on the
control), control column from x 112.

| y (from content top) | Row |
|---|---|
| 0 | Rule `MIDI OUTPUT` |
| 20 | `PORT` · combo `(112, y, 260, 26)` · button `REFRESH` `(380, y, 100, 26)` (26 tall to match the combo: object name `refreshButton`, sized by the `QPushButton#refreshButton` rule in `app.qss`; no per-widget style sheet) |
| 54 | `CHANNEL` · spin box `(112, y, 72, 26)`, range 1..16 |
| 88 | Connection line at x 112, height 20: LED `(112, y+8, 10, 4)` + text at x 130, `type.body`: `Connected to <port>` (`labelPrimary`, LED lit, no core) / `Port not found` (`labelRed`, LED off) / `Error: <text>` (`labelRed`, LED off, elided right at 350 px) |
| 124 | Rule `MIDI INPUT` |
| 144 | `PORT` · combo `(112, y, 260, 26)`, first item `None` |
| 178 | checkbox `Follow the panel` at x 112; disabled while the input is `None` |
| 214 | Rule `BEHAVIOUR` |
| 234 | checkbox `Auto-send on highlight` |
| 262 | checkbox `Resend the current patch on connect` |
| 298 | Rule `ON THE D-05` |
| 318 | Note (`type.note`, `labelSecondary`, wrapped at 480 px, from x 0): `The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also has a Bank.S switch, turn it ON too.` |
| after note + 20 | Button row, height 28: `TEST` at x 0; `CANCEL` and `OK` right aligned, 8 px apart, `OK` rightmost |

Checkbox rows are 20 px tall: indicator at row `y + 7`, text vertically centred
(baseline `y + 15`). The note sets on two lines at 480 px (line height 15), so the button
row is at content y 368 and the client area is **520 x 436** at 1x (title bar native, not
included). Initial focus: the output port combo.

`TEST` is disabled while disconnected. `OK` is the default button. Escape = Cancel.

### 10.2 Controls in dialogs (QSS)

| Control | Spec |
|---|---|
| Dialog button | min width 100, height 28 (`REFRESH` 26, 10.1); background `buttonFace`; 1 px `buttonSlot` border, radius 2; text `type.panelLabel` caps `labelPrimary` (QSS cannot set its 0.5 spacing: the code calls `setFont(Theme::font(PanelLabel))` on every dialog and message-box `QPushButton` before it is shown); hover `buttonFaceHover`; pressed `buttonPressed`; focus 2 px `focusRing` border (padding reduced by 1); default button 1 px `labelRed` border; disabled `buttonDisabled`, text `labelDisabled` |
| Combo box | height 26; `fieldBack`; 1 px `fieldBorder` (hover `fieldBorderHover`, focus 2 px `focusRing`); padding left 8; text `type.body`; drop-down area 20 px wide, no separator line, arrow image `:/theme/arrow-down.png` (8 x 4 solid wedge in `arrow`, 45-degree steps, rows 8, 6, 4, 2 px; `arrow-down@2x.png` 16 x 8 is picked at DPR 2); popup as `QMenu` (8.2) with `selection` for the current item |
| Spin box | height 26, width 72, `fieldBack`, borders as combo, text centred `type.body`, `buttonSymbols = NoButtons` (keys and wheel change it) |
| Checkbox | indicator 16 x 6 rect, spacing 10 to the text; unchecked `ledOff` with 1 px `fieldBorderHover` border (a `buttonSlot` border vanished on `panel` in the T2.4 mockup); checked `ledOn` with 1 px `ledOn` border; focus: indicator border `focusRing`; disabled: indicator `buttonDisabled` with 1 px `fieldBorder` border, text `labelDisabled`; text `type.body` |

The checkbox indicator is an LED: the panel vocabulary for "on" (S2).

### 10.3 Shortcuts sheet (`ui/ShortcutsDialog`, F1)

Width 600 (content 560), window title `Keyboard Shortcuts`, modal, closed by `Esc` or
`F1`. Two columns 270 px wide, 20 px apart. Each section: secondary rule, then rows of
height 26: key caps in a 120 px column, action text after it in `type.body`.

| Column | Section | Rows (keys · action) |
|---|---|---|
| Left | `FIND` | `/` or `Ctrl+F` · Focus the search field; `Esc` · Clear the search, then back to the list |
| Left | `LIST` | `Up` `Down` · Move the highlight; `Home` `End` · First or last row; `Enter` · Send the highlighted patch; `Space` or `Ctrl+D` · Toggle favourite; `F2` · Rename a user slot |
| Left | `VIEW` | `Ctrl+1` · All; `Ctrl+2` · Favourites; `Ctrl+3` · Recents |
| Right | `PLAY` | `PgUp` · Previous patch; `PgDn` · Next patch; `Ctrl+G` then `1-8` `1-8` · Enter bank and number; `Ctrl+Shift+A` · Auto-send on or off |
| Right | `APP` | `Ctrl+,` · Settings; `F1` · This sheet; `Ctrl+Q` · Quit |

Key cap: `QLabel` with object name `keycap`: `type.keycap`, `labelPrimary`, background
`buttonFace`, 1 px `buttonSlot` border with a 2 px bottom border `buttonSlot`, radius 2,
padding 1 px 6 px, height 18, min width 18, text centred. Combined keys are one cap
(`Ctrl+G`). Joining words `or` and `then` are `type.note` in `labelDim`, 4 px each side.
Footer, right aligned under the columns: `Esc or F1 closes` in `type.note` `labelDim`.
Key text from `QKeySequence::toString(NativeText)` (macOS shows its symbols).

### 10.4 About box

Width 440 (content 400), window title `About`. Top: the wordmark (4.5) at its natural
size. 16 px gap. Then, each a `type.body` or `type.note` paragraph with 8 px between:

1. `Version <x.y.z>` (`type.body`)
2. `Selects patches on a Roland Boutique D-05 over MIDI.` (`type.body`)
3. `MIT licence. Copyright 2026 Mario Castro Squella.` (`type.note`)
4. `Roland, Boutique, D-05 and D-50 are trademarks of Roland Corporation. This app is not affiliated with or endorsed by Roland.` (`type.note`)
5. `Inter typeface, SIL Open Font License 1.1.` (`type.note`)

20 px gap, `OK` dialog button right aligned (default).

### 10.5 Clear recents confirmation

A small `QDialog` (object name `clearRecentsBox`), not a `QMessageBox`: Qt's message box
inserts a fixed 15 px indent spacer before the text when there is no icon and uses the
style's 11 px margins, so its text sat 32 px in from the left edge while the buttons
sat 11 px from the right (D3 round 2). Built like the other dialogs:

| Part | Spec |
|---|---|
| Window | title `Clear Recents`, modal (`open()`), width fixed 360 (content 320), height from the layout, not resizable, background `panel` |
| Margins | 20 on all four sides (as every dialog, section 10) |
| Text | `QLabel`, `type.body`, `labelPrimary`, word wrap on, left aligned at content x 0: `Clear the list of recently sent patches?` (one line at 13 px, about 236 px) |
| Gap | 20 px between the text and the button row |
| Button row | height 28, right aligned: `CLEAR` (object name `clearButton`, text `labelRed`), 8 px, `CANCEL` rightmost. Both 100 x 28 dialog buttons (10.2) with `Theme::applyDialogButtonFonts` |
| Default and escape | `CANCEL` is the default button (1 px `labelRed` border when not focused) and has initial focus; `Esc` rejects; `CLEAR` accepts |
| Icon | none |

At 1x the client area is 360 x 104 (20 + text line 16 + 20 + 28 + 20). The QSS
selector for the red label is `QDialog#clearRecentsBox QPushButton#clearButton`
(app.qss carries it beside the old `QMessageBox` one; drop the old one with the
switch).

---

## 11. Drawing rules and HiDPI

1. Every coordinate in this document is an integer logical pixel. Fill rects with
   `fillRect` on integer rects; draw 1 px lines as 1 x n rects, not with `drawLine`
   through pixel centres. This is crisp at 1x and doubles cleanly at 2x.
2. Antialiasing is on only for button-face corners (radius 2 and 3) and the star.
   Everything else (dots, LEDs, rules, rings, wedges) is drawn with antialiasing off.
   Wedges have axis-aligned or 45-degree-ish edges at these sizes; with antialiasing
   off they stay sharp.
3. The display computes its geometry in device pixels (5.1) so dots stay square,
   equal and on whole device pixels at any DPR, including 1.25, 1.5 and 1.75. The
   device pitch `P` is the integer nearest the design width that fits the window (5.1,
   "Choosing `P`"), so the logical width moves off the 1x value at fractional DPR (from
   -145 to +49 px at the listed DPRs) instead of the dots going uneven. Draw the
   cached field pixmap with `drawPixmap(QPoint)` at its logical origin, never into a
   scaled target rect: with the pixmap's DPR equal to the widget's, the painter does
   an unscaled blit and the dots stay crisp.
4. No raster image ships for chrome except the combo-box arrow (10.2): QSS cannot draw
   a wedge (a border-drawn triangle renders as a bar in Qt 6.4) and an SVG in QSS needs
   the QtSvg image-format plugin, a new dependency. The other rasters on screen are the owner's
   `logo.png` (4.5) and the display's own atlas, which is generated at run time.
5. Fonts are sized in pixels; Qt scales them with the DPR.

---

## 12. Implementation map

| Part | Drawn by | Notes |
|---|---|---|
| Window, band, status bar, fields, list frame, scrollbar, menus, tooltips, dialogs, dialog buttons, combos, spin box, checkboxes, key caps | QSS (`assets/theme/app.qss`) | Object names below |
| Display | `ui/DisplayWidget` (QPainter) | 5 |
| Section rule | QPainter, a small widget (suggested `ui/SectionRule`) | 6.1; used in the main window and dialogs |
| Panel buttons | QPainter (suggested `ui/PanelButton`, a `QAbstractButton`) | 6.2; kinds A to D |
| Group bar, bank grid | `ui/GroupBar`, `ui/BankGrid`, painting panel buttons themselves or composing `PanelButton`s | 6.3, 6.4 |
| Nameplate | QPainter (suggested `ui/Nameplate`) | 4.5 |
| List rows | `ui/PatchDelegate` | 7.2 to 7.5 |

`SectionRule`, `PanelButton` and `Nameplate` are not in SPEC §6's class list; they are
suggested so the drawing lives in one place (foreman's call).

Object names and dynamic properties the QSS relies on (T2.3 writes against these):

| Selector | Widget |
|---|---|
| `QMainWindow`, `QDialog`, `QMessageBox` | backgrounds (`QMessageBox` is no longer used by the app after 10.5; the rule is harmless) |
| `QFrame#band` | the band container (must be a `QFrame`: a plain `QWidget` ignores QSS borders) |
| `#searchField` | search `QLineEdit` |
| `#patchList` | list `QListView` |
| `#nameEditor` | inline editor |
| `QStatusBar#statusBar` | status bar; its labels use `[role="statusLabel"]` (`OUT`, `CH`, `IN`), `[role="statusValue"]`, `[role="statusDim"]` (`None`), `[role="statusError"]` (`NOT FOUND`), `[role="statusFollow"]` (`FOLLOW`), `[role="statusMessage"]` (the right-aligned message) |
| `QLabel#keycap` | Shortcuts key caps |
| `QLabel[role="note"]`, `[role="dim"]`, `[role="body"]`, `[role="formLabel"]` | dialog text roles |
| `QLabel#connectionText[state="ok"]` / `[state="error"]` | Settings connection line |
| `QPushButton` (inside dialogs) | dialog buttons, `:default` |
| `QPushButton#refreshButton` | `REFRESH` in Settings, 26 px tall (10.1) |
| `QDialog#clearRecentsBox QPushButton#clearButton` | `CLEAR` in the confirmation (red text, 10.5). Set the object names before the dialog is first shown |

Object names and `role` / `state` properties are set before a widget is first shown;
changing a property later needs `style()->unpolish(w); style()->polish(w);`. QSS cannot
set letter spacing: `Theme` sets it on the widget fonts (section 3.2 spacing column) and
the QSS size and weight do not reset it (verified, Qt 6.4.2). Combo popups take the
item rules only with `setItemDelegate(new QStyledItemDelegate(combo))`.

Footprint, restated for the builder: the display repaints only from `setLines` with
changed text; the `SENT` flash and the activity LED share one 400 ms single-shot timer;
nothing else is time based; no allocation in `paintEvent`s or `PatchDelegate::paint`
(prebuilt `QStaticText`, `QFont`, `QPen`, `QPainterPath` for the star).

---

## 13. Deviations, readings and open items

### 13.1 Deviations from REFERENCES.md

1. **Zero has a diagonal stroke** (Appendix A, 0x30), against REFERENCES decision 2
   ("plain zero"). With a plain zero, `0` and `O` are the same 5 x 7 bitmap; S4 suggests
   the real display slashes it (low confidence). Legibility wins.
2. **Bezel margins are tighter** than the real windows (18 px beside, 10 px above and
   below the lit field, against about 2 to 3 character pitches): needed for 40 columns at
   800 px wide.
3. **The display grows in steps** (pitch 3, 4, 5) with the window width rather than
   staying fixed: the owner reads it at arm's length on a 16-inch laptop.
4. **The list address column draws digits in cells** because Inter's figures are
   proportional (measured), not because the panel does.

### 13.2 Readings of the spec (foreman to confirm)

1. **Display line 1 shows the current patch** (last sent or followed), not the list
   highlight. With auto-send off, arrowing through the list does not change the display.
   Reason: the display mirrors the synth.
2. **Line 2 count wording**: `NO MATCHES` for zero and `1 MATCH` for one, refining
   FR-12's `<n> MATCHES`.
3. **Unnamed user slot on the display**: name and key mode blank (`U3-45:` and nothing
   else), rather than repeating the address; the list shows the address in `labelDim`
   so search ranges still line up.
4. **Group click starts an entry** (6.3, 6.4), like PRESET/USER then bank then number on
   the D-05; a number click while idle sends at once (FR-2). SPEC does not say what a
   group click does.
5. **Recents count** shows only in the Recents view.
6. **Activity LED** shares the `SENT` timer (keeps "one transient").

### 13.3 For the owner at G2 (each is a token or a switch)

1. Key mode position: SPEC FR-12 keeps it at the right of line 1 (designed). The real
   units show it at the left of line 2 (S2, S4); the alternative is to print it at line 2
   columns 36 to 40 when line 2 is showing the group name. Switch `keyModeOnLine2`,
   default `false`; nothing else moves.
2. Ghost dots: `lcdShowGhost`, default `true`.
3. Display colour: `lcdOn` / `lcdBack` / `lcdGhost`, yellow-green from S2. A photo of
   the owner's D-05 in `assets/local/` can retune these three hex values.
4. Entry echo on the display (`displayEntryEcho`, default `true`).

---

## 14. Hand-off

### 14.1 T2.3 assets

- Fonts: section 3.1 (files, licence, subset command, README).
- `assets/theme/app.qss`: every row marked QSS in section 12, with sections 2 and 3 as
  the header comment (token name, hex, use).
- `assets/theme/arrow-down.svg`: the combo arrow source, an 8 x 4 solid downward wedge
  with 45-degree steps (rows 8, 6, 4, 2 px), fill `#BDB9A0`, viewBox `0 0 8 4`; shipped
  as the pixel-exact rasters `arrow-down.png` (8 x 4) and `arrow-down@2x.png` (16 x 8),
  written by `assets/theme/make-arrows.py` (see 10.2 and 11.4).
- `assets/icons/d05-patch-switcher.svg`, 256 x 256 viewBox, no text objects (convert
  everything to paths or rects):
  - tile: rect `(8, 8, 240, 240)`, radius 44, fill `panel` `#202326`;
  - band: the tile's area below y 168 in `band` `#26292B` (clip to the tile), with a
    2 px line at y 168 in `bandEdge` `#3A3E41`;
  - display window: rect `(32, 48, 192, 96)`, fill `bezel` `#050606`, 2 px inner frame
    `bezelRim` `#2E3134`;
  - lit field: rect `(44, 60, 168, 72)`, fill `lcdBack` `#071104`;
  - dots: the characters `P11` from Appendix A, pitch 8, dot 6 x 6, `lcdOn` `#98CF1C`,
    no ghost dots; text block 134 x 54 placed at `(61, 69)`;
  - three button faces `(40, 192, 48, 24)`, `(104, 192, 48, 24)`, `(168, 192, 48, 24)`,
    radius 3, fill `buttonFace` `#3F4244`; each with an LED `(+16, +9, 16, 6)`: the middle
    one `ledOn` `#F0383F`, the others `ledOff` `#2A1A1A`.
- `assets/icons/d05-patch-switcher.png` from the SVG with
  `inkscape --export-type=png --export-width=256`; `.ico` and `.icns` (or the commands).

### 14.2 T2.4 mockups

Main mockup at 960 x 560 (p = 3), state "search": search field focused with `brass`,
list showing several matches with highlights, one row highlighted, the current patch
`P1-11 Fantasia` lit in the grid, display line 2 `<n> MATCHES`, one favourite star on,
status connected to `Hapax MIDI 1`, channel 1. Only one widget has focus, so with the
search field focused the highlighted row shows the idle bar (`selectionBarIdle`), and
`Fantasia` (no match for `brass`) is not in the list. A second main frame in the
disconnected state shows the rest: list focused (focus border, `focusRing` bar), the
current row with its lit LED, unnamed user slots in `labelDim`, `NO MIDI OUTPUT`,
`NOT FOUND`. Settings mockup at 520 wide with a connected port and auto-send off. Use
the exact tokens, fonts and glyph table of this file.

Done (T2.4). Source: `docs/design/mockup/build.py` writes the SVGs next to itself and
renders them with Inkscape (fontconfig restricted to `assets/fonts`), the display dots
parsed from Appendix A of this file:

| File | Size (1x; `@2x` twice) | State |
|---|---|---|
| `docs/design/mockup-main.png` | 960 x 560 | search `brass`, `17 MATCHES` |
| `docs/design/mockup-settings.png` | 520 x 436 | Settings, connected, auto-send off |
| `docs/design/mockup/main-disconnected.png` | 960 x 560 | disconnected, list focused |
| `docs/design/mockup/display-ghost-on.png`, `-ghost-off.png` | 799 x 111 | 13.3 item 2 |
| `docs/design/mockup/display-keymode-line1.png`, `-line2.png` | 799 x 111 | 13.3 item 1 |

---

## Appendix A: 5 x 7 glyph table

Drawn for this project in the style of the dot-matrix displays in S4 and S5:
descender-less lowercase (`g`, `j`, `p`, `q`, `y` sit inside the 7 rows), diagonal zero.
Each line: character code (hex), seven row bytes top to bottom, the character. In each
row byte bit 4 (0x10) is the leftmost column and bit 0 (0x01) the rightmost; bits 5 to
7 are always 0. Example `A` = `0E 11 11 11 1F 11 11`:

```
.###.   0E
#...#   11
#...#   11
#...#   11
#####   1F
#...#   11
#...#   11
```

Codes 0x20 to 0x7E, 95 glyphs. The builder copies this table into a
`static const quint8 kGlyphs[95][7]` indexed by `code - 0x20`.

```
20  00 00 00 00 00 00 00  SP
21  04 04 04 04 04 00 04  !
22  0A 0A 0A 00 00 00 00  "
23  0A 0A 1F 0A 1F 0A 0A  #
24  04 0F 14 0E 05 1E 04  $
25  18 19 02 04 08 13 03  %
26  0C 12 14 08 15 12 0D  &
27  04 04 08 00 00 00 00  '
28  02 04 08 08 08 04 02  (
29  08 04 02 02 02 04 08  )
2A  00 04 15 0E 15 04 00  *
2B  00 04 04 1F 04 04 00  +
2C  00 00 00 00 0C 04 08  ,
2D  00 00 00 1F 00 00 00  -
2E  00 00 00 00 00 0C 0C  .
2F  00 01 02 04 08 10 00  /
30  0E 11 13 15 19 11 0E  0
31  04 0C 04 04 04 04 0E  1
32  0E 11 01 02 04 08 1F  2
33  1F 02 04 02 01 11 0E  3
34  02 06 0A 12 1F 02 02  4
35  1F 10 1E 01 01 11 0E  5
36  06 08 10 1E 11 11 0E  6
37  1F 01 02 04 08 08 08  7
38  0E 11 11 0E 11 11 0E  8
39  0E 11 11 0F 01 02 0C  9
3A  00 0C 0C 00 0C 0C 00  :
3B  00 0C 0C 00 0C 04 08  ;
3C  02 04 08 10 08 04 02  <
3D  00 00 1F 00 1F 00 00  =
3E  08 04 02 01 02 04 08  >
3F  0E 11 01 02 04 00 04  ?
40  0E 11 01 0D 15 15 0E  @
41  0E 11 11 11 1F 11 11  A
42  1E 11 11 1E 11 11 1E  B
43  0E 11 10 10 10 11 0E  C
44  1C 12 11 11 11 12 1C  D
45  1F 10 10 1E 10 10 1F  E
46  1F 10 10 1E 10 10 10  F
47  0E 11 10 17 11 11 0F  G
48  11 11 11 1F 11 11 11  H
49  0E 04 04 04 04 04 0E  I
4A  07 02 02 02 02 12 0C  J
4B  11 12 14 18 14 12 11  K
4C  10 10 10 10 10 10 1F  L
4D  11 1B 15 15 11 11 11  M
4E  11 11 19 15 13 11 11  N
4F  0E 11 11 11 11 11 0E  O
50  1E 11 11 1E 10 10 10  P
51  0E 11 11 11 15 12 0D  Q
52  1E 11 11 1E 14 12 11  R
53  0F 10 10 0E 01 01 1E  S
54  1F 04 04 04 04 04 04  T
55  11 11 11 11 11 11 0E  U
56  11 11 11 11 11 0A 04  V
57  11 11 11 15 15 15 0A  W
58  11 11 0A 04 0A 11 11  X
59  11 11 11 0A 04 04 04  Y
5A  1F 01 02 04 08 10 1F  Z
5B  0E 08 08 08 08 08 0E  [
5C  00 10 08 04 02 01 00  \
5D  0E 02 02 02 02 02 0E  ]
5E  04 0A 11 00 00 00 00  ^
5F  00 00 00 00 00 00 1F  _
60  08 04 02 00 00 00 00  `
61  00 00 0E 01 0F 11 0F  a
62  10 10 16 19 11 11 1E  b
63  00 00 0E 10 10 11 0E  c
64  01 01 0D 13 11 11 0F  d
65  00 00 0E 11 1F 10 0E  e
66  06 09 08 1C 08 08 08  f
67  00 0F 11 11 0F 01 0E  g
68  10 10 16 19 11 11 11  h
69  04 00 0C 04 04 04 0E  i
6A  02 00 06 02 02 12 0C  j
6B  10 10 12 14 18 14 12  k
6C  0C 04 04 04 04 04 0E  l
6D  00 00 1A 15 15 11 11  m
6E  00 00 16 19 11 11 11  n
6F  00 00 0E 11 11 11 0E  o
70  00 00 1E 11 1E 10 10  p
71  00 00 0D 13 0F 01 01  q
72  00 00 16 19 10 10 10  r
73  00 00 0E 10 0E 01 1E  s
74  08 08 1C 08 08 09 06  t
75  00 00 11 11 11 13 0D  u
76  00 00 11 11 11 0A 04  v
77  00 00 11 11 15 15 0A  w
78  00 00 11 0A 04 0A 11  x
79  00 00 11 11 0F 01 0E  y
7A  00 00 1F 02 04 08 1F  z
7B  02 04 04 08 04 04 02  {
7C  04 04 04 04 04 04 04  |
7D  08 04 04 02 04 04 08  }
7E  00 00 08 15 02 00 00  ~
```
