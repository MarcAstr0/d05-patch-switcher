# Design, version 2: Boutique Patch Switcher skins

Tasks T7.2 to T7.4. The builder implements every version 2 visual from this file and
invents nothing. It is a **delta over `docs/design/DESIGN.md`**, which stays the
complete specification of the D-05 skin and of every rule this file does not change:
units (logical px at 1x, `QFont::setPixelSize`), rect notation `(x, y, w, h)`, the
drawing and HiDPI rules (DESIGN.md 11), the footprint rules (DESIGN.md 12). Evidence and
sources: `docs/design/REFERENCES.md` sections 8 to 14 (cited as S10 to S18 and
"decision n"). Behaviour: `docs/SPEC-2.md`; where a visual needs a reading the spec does
not give, section 13.2 states it.

Contents: 1 Principles · 2 Skins and tokens · 3 Typography · 4 Main window (display
slot, group-bar slot, nameplate and device switcher) · 5 Panel parts per skin · 6 List
rows · 7 Settings · 8 Shortcuts, About, menus, titles · 9 Icon · 10 Mockups · 11 Skin
palettes · 12 Implementation map · 13 Deviations, readings, owner decisions.

---

## 1. Principles

1. **One layout, four skins.** Every rect in DESIGN.md 4.3 is the same on every skin:
   the display slot, the band, the left and right columns, the search field, the list,
   the status bar, the dialogs. A device or engine switch is a repaint with new tokens
   and, in three places, a different component in the same rect (section 2.4). No
   animation, no transient of the old device's content.
2. **Skins borrow colour, proportion and lettering style, never artwork.** Panels,
   section bands and button colours come from the 1981 JUPITER-8, the 1982 JUNO-60 and
   the 1984 JUNO-106 (decisions 13, 17 to 19). No logo, logotype, drawn lettering or
   silkscreen. Model names appear as plain text in Inter. The `PATCH SWITCHER` nameplate is
   set per skin in an OFL face chosen from the panel's own non-logo lettering (3.1).
3. **The display slot mirrors the unit.** On the D-05 it is the D-50 LCD of DESIGN.md 5.
   On the JP-08 and JU-06A it is the Boutique's own seven-segment readout beside the
   name and the context line in panel lettering (decision 13).
4. **The JUNO engine is the loudest thing on the JU-06A.** The two JU-06A sub-skins are
   the darkest (60) and the lightest (106) panels in the app, so the app's belief is
   visible from across the room.
5. **The D-05 skin does not change.** With only the D-05 enabled, the main window renders
   pixel for pixel as version 1.0.0 (section 13.1 lists the dialogs that change).

---

## 2. Skins and tokens

### 2.1 What a skin is

A skin is the resolved set of: colour tokens (every name in section 2.3 and DESIGN.md 2),
type roles (section 3 and DESIGN.md 3.2), switches, component choices (2.4) and
strings (2.4). Four skins exist:

| Skin id | Used when | Modelled on |
|---|---|---|
| `d05` | current device is the D-05, or no device is enabled | DESIGN.md (the D-50 panel) |
| `jp08` | current device is the JP-08 | JUPITER-8, with the JP-08's readout |
| `ju06a-60` | current device is the JU-06A and its engine state is 60 | JUNO-60, with the JU-06A's readout |
| `ju06a-106` | current device is the JU-06A and its engine state is 106 | JUNO-106, with the JU-06A's readout |

The skin changes exactly when the current device changes (switcher, `Ctrl+Tab`, FR-20
follow) or when the JU-06A's engine state changes while it is current (`Ctrl+E`, engine
button, armed confirmation). Dialogs take the skin that is current when they open and
re-skin if it changes while they are open.

### 2.2 Token file, version 2

**Where it is.** `docs/design/tokens.json`, `"version": 2`. T8.1 replaced the version 1
file with it (the version 1 file is in git at `v1.0.0`) and folded the version 2 checks
into `python3 docs/design/gen-tokens.py --check`, which is the one check for every
generated output and every invariant below.

**Schema** (also at the top of the file, key `_schema`):

| Key | Content |
|---|---|
| `version` | `2` |
| `skinOrder` | `["d05", "jp08", "ju06a-60", "ju06a-106"]`: the order outputs are written in |
| `colorGroups` | `[{id, title}]`: `2.1` to `2.4` as version 1, plus `2.5` LED readout, `2.6` Panel faces by role, `2.7` Version 2 lettering and section rules |
| `base.colors` | every colour token: `{hex, group, qss, use, source?}`. The hex values are the D-05 skin. The 41 version 1 entries are byte-identical to version 1 |
| `base.type` | every type role: `{weight, size, spacing, case, colour, lineHeight, usedFor[, cells][, family]}`. The 13 version 1 roles are identical; 8 roles are new (section 3). No base role has `family`: a role without one is drawn in `font.family` (Inter) |
| `base.switches` | `lcdShowGhost`, `keyModeOnLine2`, `displayEntryEcho` (version 1 values), `ledShowGhost` (`true`) |
| `base.components` | `display`, `groupSlot`, `rowTag`, `sectionRule`, `buttonLed`, `ledCells`: the D-05 values (2.4) |
| `base.strings` | `modelName`, `groupTitle`, `bankTitle`, `numberTitle`, `engineTitle`, `wordmarkSub`, `idleContext`, `searchPlaceholder`: the D-05 values (2.4) |
| `skins.<id>` | `{model, engine?, colors, type, switches, components, strings}`; every map holds **overrides only**. `colors` overrides are `name: "#RRGGBB"`; `type` overrides are `name: {partial role}` merged field by field. Used today only for `type.wordmark` on `jp08`, `ju06a-60`, `ju06a-106` (3.1), which set `family`, `weight`, `size`, `spacing`, `lineHeight` and, on the JUNO skins, `colour` |
| `extraFonts` | families other than `font.family` that a resolved role may name: `{<family>: {files: {<weight>: path}, licence, coverage, source}}`. Today only `Archivo` (3.1). `coverage` is every character the roles using the family may draw; the files are subset to exactly that |
| `geometry` | LED readout numbers in units of the dot pitch P (section 4.2). Not emitted to CSS |
| `font`, `webColors`, `webType`, `breakpoints`, `size` | copied unchanged from version 1; they describe the D-05 and the web |

**Resolution.** `resolved(skin).X[name] = skins[skin].X[name]` if present, else
`base.X[name]`, for X in colors, type, switches, components, strings. An override name
that is not in `base` is an error. `skins.d05` is empty in every map, so the D-05 skin
is `base`. Hex values are upper case `#RRGGBB`.

**Invariants** (`gen-tokens.py --check` enforces them):

1. Every version 1 colour, type role and switch resolves for `d05` to its version 1 value,
   and `font`, `webColors`, `webType`, `breakpoints`, `size` equal version 1.
2. Every new colour's base value equals the version 1 token it stands in for (table in
   2.3), so the D-05 skin draws the version 2 widgets exactly as version 1 draws them.
   The three LED readout tokens are the only new colours the D-05 never draws.
3. Every colour resolves to `#RRGGBB` in every skin.
4. `lineHeight` equals `round(ascent) + round(descent)` from the font file's `hhea`, as
   version 1 checks, for every role **in every skin**, read from the file of the role's
   resolved family and weight.
5. A resolved role's `family` is `font.family` or a key of `extraFonts` with a file for its
   weight; no `base` role names a family; every `extraFonts` file and licence exists; every
   character of `coverage` is in each file's `cmap`.

### 2.3 New colour tokens

Version 1 tokens keep their names, meanings and D-05 values (DESIGN.md 2). Three of
their uses move to new tokens so light-faced skins work: dialog buttons, key caps and
the list scrollbar handle use `controlFace*` (they stay dark on every skin, under
`labelPrimary` text); PREV / NEXT wedges use `wedge` (the combo arrow keeps `arrow`);
legends on faces use `buttonLegend`. The panel-button colours split into five face sets
by role (section 5.2): the version 1 `button*` names are the grid's number row, the group
bar and the engine buttons; `bank*`, `view*`, `commit*`, `transport*` are new.

| Token | Group | D-05 value (= base) | Use |
|---|---|---|---|
| `ledBack` | 2.5 | `#0A0302` | Field behind the seven-segment digits |
| `ledSegOn` | 2.5 | `#FF3A2A` | Lit segments and decimal point; also the flip prompt text |
| `ledSegGhost` | 2.5 | `#1C0806` | Unlit segments, very faint |
| `bankFace` | 2.6 | `#3F4244` | Bank row of the grid: face, normal |
| `bankFaceHover` | 2.6 | `#4A4D50` | Bank row of the grid: face, hover |
| `bankPressed` | 2.6 | `#2F3133` | Bank row of the grid: face, pressed |
| `bankHighlight` | 2.6 | `#56595C` | Bank row of the grid: 1 px top inside edge |
| `bankFaceLit` | 2.6 | `#4B3B3C` | Bank row of the grid: face when its LED is lit |
| `viewFace` | 2.6 | `#3F4244` | ALL, FAVOURITES, RECENTS: face, normal |
| `viewFaceHover` | 2.6 | `#4A4D50` | ALL, FAVOURITES, RECENTS: face, hover |
| `viewPressed` | 2.6 | `#2F3133` | ALL, FAVOURITES, RECENTS: face, pressed |
| `viewHighlight` | 2.6 | `#56595C` | ALL, FAVOURITES, RECENTS: 1 px top inside edge |
| `viewFaceLit` | 2.6 | `#4B3B3C` | ALL, FAVOURITES, RECENTS: face when its LED is lit |
| `commitFace` | 2.6 | `#3F4244` | AUTO SEND: face, normal |
| `commitFaceHover` | 2.6 | `#4A4D50` | AUTO SEND: face, hover |
| `commitPressed` | 2.6 | `#2F3133` | AUTO SEND: face, pressed |
| `commitHighlight` | 2.6 | `#56595C` | AUTO SEND: 1 px top inside edge |
| `commitFaceLit` | 2.6 | `#4B3B3C` | AUTO SEND: face when its LED is lit |
| `transportFace` | 2.6 | `#3F4244` | PREV, NEXT, SYSTEM: face, normal |
| `transportFaceHover` | 2.6 | `#4A4D50` | PREV, NEXT, SYSTEM: face, hover |
| `transportPressed` | 2.6 | `#2F3133` | PREV, NEXT, SYSTEM: face, pressed |
| `transportHighlight` | 2.6 | `#56595C` | PREV, NEXT, SYSTEM: 1 px top inside edge |
| `transportFaceLit` | 2.6 | `#4B3B3C` | PREV, NEXT, SYSTEM: face when its LED is lit |
| `buttonLegend` | 2.6 | `#E2E0D4` | Legend drawn on a panel button face (group buttons, engine buttons) |
| `wedge` | 2.6 | `#BDB9A0` | Solid wedges on PREV / NEXT (the combo arrow stays `arrow`) |
| `lensRim` | 2.6 | `#121415` | 1 px ring around a round LED (lens style) |
| `controlFace` | 2.6 | `#3F4244` | Dialog buttons, key caps, list scrollbar handle |
| `controlFaceHover` | 2.6 | `#4A4D50` | Same, hover |
| `controlPressed` | 2.6 | `#2F3133` | Same, pressed |
| `labelNumeral` | 2.7 | `#86A6D4` | Numerals 1 to 8 above the bank and number buttons |
| `labelCommit` | 2.7 | `#E8565C` | The `AUTO SEND` label above its button |
| `ruleTitleBack` | 2.7 | `#202326` | Rounded plate behind the title of a `doubleBar` section rule |
| `ruleTitleText` | 2.7 | `#E2E0D4` | Title text on that plate |
| `ruleBand` | 2.7 | `#6E8EBA` | Fill of a `band` section rule over the grid |
| `ruleBandText` | 2.7 | `#E2E0D4` | Title on that band |
| `ruleBand2` | 2.7 | `#6E8EBA` | Fill of the `ENGINE` band rule on the JU-06A |
| `ruleBand2Text` | 2.7 | `#E2E0D4` | Title on that band |

Stand-ins (invariant 2): each face-set token equals the `button*` token with the same suffix (`bankFace` = `buttonFace`, `bankPressed` = `buttonPressed`, and so on); `controlFace`, `controlFaceHover`, `controlPressed` = `buttonFace`, `buttonFaceHover`, `buttonPressed`; `buttonLegend` = `labelPrimary`; `wedge` = `arrow`; `lensRim` = `buttonSlot`; `labelNumeral` = `labelAccent`; `labelCommit` = `labelRed`; `ruleTitleBack` = `panel`; `ruleTitleText`, `ruleBandText`, `ruleBand2Text` = `labelPrimary`; `ruleBand`, `ruleBand2` = `ruleAccent`. `ledBack`, `ledSegOn`, `ledSegGhost` have no stand-in (the D-05 never draws them); their base values are the JU-06A's.

`arrow`, `ledCore`, `lcdBack`, `lcdOn`, `lcdGhost` are overridden by no skin: one combo
arrow raster serves every skin, and the LCD is drawn only on the D-05.

### 2.4 Components and strings per skin

| Key | `d05` (base) | `jp08` | `ju06a-60` | `ju06a-106` |
|---|---|---|---|---|
| `components.display` | `lcd` (DESIGN.md 5) | `led` (4.2) | `led` | `led` |
| `components.ledCells` | (3, unused) | `2` | `3` | `3` |
| `components.groupSlot` | `groupBar` (DESIGN.md 6.3) | `empty` (4.3) | `engine` (4.3) | `engine` |
| `components.rowTag` | `keyMode` (DESIGN.md 7.2) | `none` | `engine` (6.3) | `engine` |
| `components.sectionRule` | `hairline` (DESIGN.md 6.1) | `doubleBar` (5.1) | `band` (5.1) | `band` |
| `components.buttonLed` | `slit` (DESIGN.md 6.2) | `lens` (5.2) | `lens` | `lens` |
| `strings.modelName` | `D-05` | `JP-08` | `JU-06A` | `JU-06A` |
| `strings.groupTitle` | `PATCH GROUP` | (empty) | (empty) | (empty) |
| `strings.bankTitle` | `PATCH BANK` | `BANK` | `BANK` | `BANK` |
| `strings.numberTitle` | `PATCH NUMBER` | `NUMBER` | `PATCH` | `PATCH` |
| `strings.engineTitle` | (empty) | (empty) | `ENGINE` | `ENGINE` |
| `strings.wordmarkSub` | `FOR ROLAND D-05` | `FOR ROLAND JP-08` | `FOR ROLAND JU-06A` | `FOR ROLAND JU-06A` |
| `strings.idleContext` | (empty: the group long name, DESIGN.md 5.2) | `JP-08` | `JU-06A  JUNO-60` | `JU-06A  JUNO-106` |
| `strings.searchPlaceholder` | `Search names or addresses: brass, P3-45` | `Search names or addresses: brass, 4-5` | `Search names or addresses: brass, 106:4-5` | same as 60 |
| `switches.ledShowGhost` | (true, unused) | true | true | true |

`BANK` / `PATCH` are the JU-06A's own labels (S16); `BANK` / `NUMBER` split the
JUPITER-8's `PATCH NUMBER 11-88` into the app's two rows. Idle contexts have two spaces
between the device and the engine name, as the brief gives them.

### 2.5 Generated outputs (the builder owns the generator; this is its contract)

`docs/design/gen-tokens.py` version 2 reads `tokens.json` (version 2) and writes:

| Output | From | Content |
|---|---|---|
| `assets/theme/<skin>.qss`, one per skin in `skinOrder` | `assets/theme/app.qss.in` | the template with every placeholder replaced (below) |
| `web/src/ui/theme.css` | `base` and the version 1 blocks only | **byte-identical to today's output**: the web stays the D-05. The CSS function reads `base.colors` filtered to the 41 version 1 names (groups 2.1 to 2.4), `base.type` filtered to the 13 version 1 roles, `webColors`, `webType`, `size`, `breakpoints`, exactly as version 1 |
| a runtime palette for `Theme` (builder's choice of form; recommended: generated `src/ui/SkinTables.inc` with one row per skin and one `QColor` member per token name, plus the components and strings) | `skins` resolved | every resolved colour, switch, component and string per skin, and the resolved `type.wordmark` (family, weight, size, spacing, colour token) |

**Template syntax** of `assets/theme/app.qss.in` (committed with this task):

- `@{<colour token name>}` is replaced by that token's resolved `#RRGGBB` for the skin
  being written. Only colour names are allowed.
- `@{skin}` is replaced by the skin id.
- A line consisting of `@{TOKEN_BLOCK}` alone is replaced by the token comment block.
- Anything else of the form `@{...}`, and any literal `#RRGGBB` in the rules (after the
  first `*/`), is a generator error.

**Token comment block** (replaces the `@{TOKEN_BLOCK}` line; every line starts with ` *`):
the version 1 block format of `gen-tokens.py` `qss_block()` extended: a first line
` * Skin: <id> (<modelName>[, engine <n>])`, then the dashed line, `COLOUR TOKENS
(DESIGN.md section 2, DESIGN-2.md section 2.3)`, one sub-heading per `colorGroups` entry
and one line per token in that group in `base.colors` order with the **resolved** hex
(`name<pad to 18>#RRGGBB  qss`), then the `TYPE ROLES` block with all 21 roles in
`base.type` order, **resolved for the skin**, then the `Application font` line. Column
widths as version 1, except that a name longer than its column (`type.displayContext3`
to `5`, 20 characters) is followed by exactly one space, never run into the weight. A
role whose resolved family is not `font.family` gets two spaces and the family after the colour (`type.wordmark      600     16    2.0      caps        labelAccent  Archivo`),
and the block's second header line reads `Family "Inter" (assets/fonts, OFL 1.1) unless
the line names another (DESIGN-2.md 3.1), sizes in`. In `d05.qss` every role line is as
version 1.

**Rules part.** `app.qss.in` holds version 1's `app.qss` rules with each hex replaced by its
token (one-to-one; the three button colours used by dialog buttons, key caps and the
scrollbar handle became `controlFace`, `controlFaceHover`, `controlPressed`), plus the
version 2 rules: `#useOutputButton` / `#useInputButton` (7.3), `QLabel[role="engineValue"]`,
`QLabel[role="engineUnknown"]`, `QLabel#channelConflictText` (7.3). Consequence: the rules
part of `d05.qss` is version 1's `app.qss` rules with those additions and nothing else
changed; the template was produced and checked that way.

**`--check`** (read only) verifies: each `<skin>.qss` equals the rendered template;
`theme.css` equals the version 1 function's output; the 2.2 invariants; the runtime
palette agrees with the resolved tokens; `docs/design/mockup/build.py`'s `T` and `ROLES`
equal the version 1 subset of `base` (`build2.py` reads `tokens.json` directly and
needs no check); line heights per skin and the `extraFonts` coverage (invariants 4
and 5). Version 1's `assets/theme/app.qss` was deleted in T8.1; the resources are
`:/theme/d05.qss`, `:/theme/jp08.qss`, `:/theme/ju06a-60.qss`, `:/theme/ju06a-106.qss`.

### 2.6 Applying a skin (`ui/Theme`)

`Theme::setSkin(id)`, called with updates disabled on the main window
(`setUpdatesEnabled(false)` before, `true` after, so the window paints once):

1. Swap the runtime palette (all colours, switches, components, strings).
2. Set the application `QPalette` from the tokens as DESIGN.md 2 says.
3. `qApp->setStyleSheet(<contents of :/theme/<id>.qss>)` (Qt re-polishes every widget).
4. Emit `skinChanged()`. Custom-painted widgets drop their cached pixmaps that hold
   colours or fonts (display frame, LED atlas, nameplate) and rebuild them before the
   paint; the nameplate rebuilds with the new skin's `type.wordmark` (3.1);
   `MainWindow` shows the display widget and the group-slot widget the skin names (each
   slot is a `QStackedWidget` at the fixed rect, so nothing moves).

The style sheet text for each skin is read from resources once and kept; nothing is
built per switch beyond the polish and the cache rebuild. No timer is added.

---

## 3. Typography

Inter (DESIGN.md 3.1), the same two committed files, sets everything except the
nameplate. The seven-segment digits are painted from geometry (4.2), which is crisper at
2x than a segment font and adds no file (decision 15); the originals' panel lettering is
a Helvetica-like grotesque that Inter already stands in for (decision 22). **One
exception** (owner amendment 2026-10-26, decisions 23 to 26): the `PATCH SWITCHER`
nameplate is set per skin in Archivo, two subset files of 7.6 KB each (3.1).

New roles (`base.type`; same conventions as DESIGN.md 3.2):

| Role | Face | Size | Spacing | Case | Colour | Line height | Used for |
|---|---|---|---|---|---|---|---|
| `type.displayName3` | Medium | 28 | 0 | As stored | `labelPrimary` | 34 | LED skins: name in the display slot, step p = 3 |
| `type.displayName4` | Medium | 37 | 0 | As stored | `labelPrimary` | 45 | same, p = 4 |
| `type.displayName5` | Medium | 46 | 0 | As stored | `labelPrimary` | 56 | same, p = 5 |
| `type.displayContext3` | Medium | 13 | 1.0 | Caps | per state (4.2) | 16 | LED skins: context line, p = 3 |
| `type.displayContext4` | Medium | 17 | 1.5 | Caps | per state | 21 | same, p = 4 |
| `type.displayContext5` | Medium | 21 | 2.0 | Caps | per state | 26 | same, p = 5 |
| `type.ruleTitle` | Medium | 10 | 1.0 | Caps | `ruleTitleText` / `ruleBandText` / `ruleBand2Text` | 12 | Title of a `doubleBar` or `band` section rule |
| `type.switcherTab` | Medium | 10 | 1.0 | Caps | per state (4.4) | 12 | Device names in the switcher |

Measured widths (Inter 4.1 subset, kerned, with spacing; the mockup script prints them):
`Alpha Omega Ensemble` (the longest factory name, 20 characters) 319 / 421 / 524 px at
p = 3 / 4 / 5; `FOUND JU-06A ON USB: OPEN SETTINGS`, the longest context, 298 / 396 /
494 px. Cap heights (`qRound(QFontMetricsF::capHeight())`): names 20 / 27 / 33, contexts
9 / 12 / 15.

### 3.1 Nameplate face per skin (`type.wordmark`)

`type.wordmark` resolves per skin (tokens `skins.<id>.type.wordmark`, `extraFonts`). Only
the wordmark `PATCH SWITCHER` changes: its position, the divider, the sub `FOR ROLAND
<MODEL>` and the device tabs stay exactly as 4.4, and the sub and the tabs stay in Inter
(`type.wordmarkSub`, `type.switcherTab`), so "Roland" and the model names appear only as
plain text in the UI face. Evidence and the trademark reasoning: REFERENCES.md section 14,
decisions 23 to 26.

| Skin | Family, file in `assets/fonts/` | Weight | Size | Spacing | Case | Colour (resolved) | Line height | Cap height | Advance: mockup / Qt |
|---|---|---|---|---|---|---|---|---|---|
| `d05` | Inter, `Inter-Medium.ttf` (unchanged) | 500 Medium | 15 | 3.0 | Caps | `labelAccent` `#86A6D4` | 19 | 11 | 174.4 / 174 |
| `jp08` | Archivo, `Archivo-SemiBold-nameplate.ttf` | 600 SemiBold | 16 | 2.0 | Caps | `labelAccent` `#F07A4A` | 17 | 11 | 169.3 / 169 |
| `ju06a-60` | Archivo, `Archivo-Bold-nameplate.ttf` | 700 Bold | 16 | 1.0 | Caps | `labelPrimary` `#F0EDE2` | 17 | 11 | 158.4 / 158 |
| `ju06a-106` | Archivo, `Archivo-Bold-nameplate.ttf` | 700 Bold | 16 | 1.0 | Caps | `labelPrimary` `#F4F3EE` | 17 | 11 | 158.4 / 158 |

Advances include the spacing after every character, as `QFontMetricsF::horizontalAdvance`
reports it. "Mockup" is the unhinted, kerned value `build2.py` measures (Pillow and
libraqm); "Qt" is Qt 6.4.2 offscreen on Linux as the T8.1 build renders it (divider at
plate right + 14 in the 1x and 2x screenshots, D8 round 1; the design-time harness had
reported 177 / 172 / 160 under a different fontconfig). The code never hard-codes either:
it measures (step 5 below).

Why these numbers. Archivo at 16 px has the cap height of Inter at 15 px (10.97 and
10.91 px by `QFontMetricsF::capHeight`), so every skin's nameplate stands equally tall on
the shared baseline `y + 20` and the row is not re-laid out by a skin switch. Spacing is
whole pixels so that 1x glyph origins stay on whole pixels, as version 1's wordmark. The
JP-08 is SemiBold with open tracking (the JUPITER-8's section titles, decision 24); the
JUNOs are Bold, tightly set, in the panel white (the JUNO descriptor line, decision 25).

**Implementation** (`ui/Theme` and `ui/DeviceSwitcher`; the About box uses the same):

1. Resources `:/fonts/Archivo-SemiBold-nameplate.ttf` and
   `:/fonts/Archivo-Bold-nameplate.ttf`, registered once at start-up with
   `QFontDatabase::addApplicationFont`, beside Inter. Checked with Qt 6.4.2 on a
   fontconfig without system fonts: the SemiBold file reports families `Archivo` and
   `Archivo SemiBold`, the Bold file `Archivo`.
2. Font for `jp08`: `QFont f(QStringLiteral("Archivo")); f.setPixelSize(16);
   f.setWeight(QFont::DemiBold); f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);` For the
   JUNO skins: `QFont::Bold` and spacing `1.0`. Kerning stays on (Qt's default; the files
   keep `kern`), hinting preference stays the default, as for Inter. Checked: `QFontInfo`
   reports family `Archivo`, style `SemiBold`, weight 600, and `Bold`, weight 700.
3. Fallbacks, in this order. If `QFontInfo(f)` reports another family, or a weight more
   than 50 away from the role's, try the family `Archivo SemiBold` (the SemiBold file's
   legacy name, which a GDI font database lists separately) at `QFont::Normal`. If that
   also fails, or a file did not load, draw the base role (Inter Medium 15, spacing 3) in
   the skin's resolved wordmark colour. Never fall back to a system font. Log the
   failure once (`qWarning`), no status message.
4. Archivo is used for nothing else: the files hold only `A C E H I P R S T W` and the
   space (`extraFonts.Archivo.coverage`), and the wordmark is the constant string
   `PATCH SWITCHER`. A test should assert that both families resolve as in step 2, like
   the Inter check recorded in `assets/fonts/README.md`.
5. The plate's right edge is `x + qRound(QFontMetricsF(f).horizontalAdvance(text))`; the
   divider at `+ 14` and everything after it follow from that (4.4).
6. Painting and footprint: the wordmark is drawn with `QPainter::drawText` into the
   nameplate cache that 2.6 names (a pixmap at the device pixel ratio, cached as
   DESIGN.md 4.5 caches the logo), rebuilt only on a skin change, a DPR change or a
   device-set change. No layout per
   paint. The fonts are crisp at 1x (the files keep Archivo's ttfautohint instructions)
   and at 2x (checked in the Qt harness and in the mockups).

Size cost: 7,664 + 7,592 bytes of fonts and 4,388 bytes of licence text, 19,644 bytes in
the repository; the binary grows by the two font files (15,256 bytes, before any resource
compression). SPEC-2 §5's 2.5 MB is unaffected in practice.

---

## 4. Main window

### 4.1 What stays and what swaps

DESIGN.md 4.1 to 4.4 hold on every skin: minimum 800 x 480, default 960 x 560, margins,
the display step `p` by window width, `B = 46 + bezelH + 14`, left column at x 16 width
394, right column at x 434 width `W - 450`, status bar. The LED display slot reports the
same size as the LCD at the same `p`, DPR and window width (4.2), so `B` and every rect
below it are identical on all skins. Window title on every skin:
`Boutique Patch Switcher` (SPEC-2 §8).

| Slot | Rect | `d05` | `jp08` | `ju06a-60` / `ju06a-106` |
|---|---|---|---|---|
| Nameplate and switcher | `(16, 10, W - 32, 28)` | 4.4 | 4.4 | 4.4 |
| Display | `(16, 46, bezelW, bezelH)` | `DisplayWidget` (DESIGN.md 5) | `LedDisplayWidget`, 2 cells | `LedDisplayWidget`, 3 cells |
| Group slot | `(16, B + 12, 394, 68)` | `GroupBar` with its `PATCH GROUP` rule | empty | `EngineControl` |
| Grid, PREV / NEXT, control row, search, list, status bar | DESIGN.md 4.3 | as DESIGN.md | skin parts (5), rows (6) | skin parts (5), rows (6) |

### 4.2 LED display (`ui/LedDisplayWidget`, JP-08 and JU-06A)

Shows the three things of FR-12: the address as a seven-segment readout in the unit's
style (S12, S16), the name and the context line in panel lettering beside it.

**Widget size.** Exactly `DisplayWidget`'s: the same `setStep(p, windowWidth)` call, the
same choice of the device pitch `P` and gap from DESIGN.md 5.1, the same `sizeHint()`
`bezelW x bezelH`. The widget paints its full rect; everything outside the LED window
is `panel`.

**LED window** (logical px, relative to the widget):

| Part | Rect | Fill |
|---|---|---|
| Window | `(0, 0, fieldW + 22, bezelH)` | `bezel` |
| Frame | 1 px inside the window edge: top row `bezelRimTop`, left, right and bottom `bezelRim` | |
| Field | `(11, 11, fieldW, fieldH)`, `fieldW = ceil(F * P / dpr)`, `fieldH = ceil(19 * P / dpr)` (the LCD's `fieldH`) | `ledBack` |

`F` is 27 for two cells (JP-08) and 38 for three (JU-06A). At 1x and 2x:

| p | P (1x / 2x) | Window, two cells | Window, three cells | Widget (= LCD bezel) |
|---|---|---|---|---|
| 3 | 3 / 6 | 103 x 79 | 136 x 79 | 767 x 79 |
| 4 | 4 / 8 | 130 x 98 | 174 x 98 | 1010 x 98 |
| 5 | 5 / 10 | 157 x 117 | 212 x 117 | 1253 x 117 |

Corners square. The window frame follows the LCD bezel's (S12 and S16 show a thin
lighter edge on a black window).

**Digits.** All geometry is in units of `P` (device px), from the field's top-left, in
`geometry` of the token file:

| Quantity | Units of P |
|---|---|
| Digit box (unslanted) | 8 wide, 15 tall |
| Segment thickness `t` | 2 (half-thickness `h` = 1) |
| Gap at each segment end `e` | 0.25 |
| Cell pitch | 11 (digit 8, slant 1.5, decimal point to 10.5, 0.5 clear) |
| Cell `k` origin (0-based) | `(3 + 11 k, 2)`; padding 3 left, 2 top, 2 bottom (2 + 15 + 2 = 19) |
| Slant | every point `(x, y)` of the unslanted digit is drawn at `(x + 0.1 * (15 - y), y)`: the top leans 1.5 to the right, about 5.7 degrees (S16 measured about 6) |

Segments are hexagons with 45-degree points, given unslanted in digit-box units (apply
the slant, multiply by `P`, add the cell origin and the field origin in device px):

| Segment | Points |
|---|---|
| a (top) | `(1.25, 1) (2.25, 0) (5.75, 0) (6.75, 1) (5.75, 2) (2.25, 2)` |
| g (middle) | `(1.25, 7.5) (2.25, 6.5) (5.75, 6.5) (6.75, 7.5) (5.75, 8.5) (2.25, 8.5)` |
| d (bottom) | `(1.25, 14) (2.25, 13) (5.75, 13) (6.75, 14) (5.75, 15) (2.25, 15)` |
| f (upper left) | `(1, 1.25) (2, 2.25) (2, 6.25) (1, 7.25) (0, 6.25) (0, 2.25)` |
| b (upper right) | `(7, 1.25) (8, 2.25) (8, 6.25) (7, 7.25) (6, 6.25) (6, 2.25)` |
| e (lower left) | `(1, 7.75) (2, 8.75) (2, 12.75) (1, 13.75) (0, 12.75) (0, 8.75)` |
| c (lower right) | `(7, 7.75) (8, 8.75) (8, 12.75) (7, 13.75) (6, 12.75) (6, 8.75)` |
| decimal point | square `(8.5, 13) (10.5, 13) (10.5, 15) (8.5, 15)` |

Characters (the only ones the readout draws):

| Char | Segments | Char | Segments |
|---|---|---|---|
| `1` | b c | `6` | a c d e f g |
| `2` | a b d e g | `7` | a b c |
| `3` | a b c d g | `8` | a b c d e f g |
| `4` | b c f g | `-` | g |
| `5` | a c d f g | `_` | d |
| ` ` (blank) | none | `0`, `9` | a b c d e f / a b c d f g (not used by the app; drawn for completeness) |

Lit segments `ledSegOn`; every unlit segment and the unlit decimal point `ledSegGhost`
when `ledShowGhost` is true, nothing when false. **No halo, no core, no blur**
(decision 15; the faint ghost is the only "glow"). The decimal point is specified (the
JU-06A lights it while a patch is edited, S17) but the app never lights it: it cannot
know.

**Readout text** (`cells` characters, left to right):

| State | JP-08 (2) | JU-06A (3) |
|---|---|---|
| Current patch bank `b`, number `n` | `bn` (`71` for `7-1`; decision 14) | `b-n` (`7-1`) |
| No patch on the current engine (SPEC-2 §6) | (does not occur) | `---` |
| Entry echo, awaiting bank (`Ctrl+G`; switch `displayEntryEcho`) | `__` | `_-_` |
| Entry echo, awaiting number (bank digit or bank click) | `b_` | `b-_` |

**Lettering beside the window** (logical px, relative to the widget; `winW` the window
width above):

| Part | x | Baseline | Role | Content |
|---|---|---|---|---|
| Name | `winW + 24` | `11 + qRound(2P / dpr) + capHeight(name role)` (cap tops align with the digit tops) | `type.displayName<p>`, `labelPrimary` | the current patch's display name: the user name if set, else the factory name, as stored; blank for no patch and during an entry echo; elided right at `bezelW - winW - 24 - 8` |
| Context | `winW + 24` | `11 + qRound(17P / dpr)` (aligns with the digit bottoms) | `type.displayContext<p>`, colour by state below | one context string, caps, elided right at the same width |

At 1x, p = 3: name x 160 (three cells) or 127 (two), name baseline 37, context
baseline 62. p = 4: x 198 / 154, baselines 46 / 79. p = 5: x 236 / 181, baselines
54 / 96. At p = 3 the widest name is 319 px and the widest context 298 px, against at
least 599 px available.

**Context line** (FR-12; first that applies; strings exact, `CTRL` is `CMD` on macOS,
chosen at compile time as in DESIGN.md 5.2):

| Priority | Condition | Text | Colour |
|---|---|---|---|
| 1a | This device is enabled but has no output port saved | `OPEN SETTINGS: CTRL+,` | `labelPrimary` |
| 1b | Output port saved but not open | `NO MIDI OUTPUT` | `labelRed` |
| 2 | JU-06A armed for engine X (FR-19) | `FLIP SWITCH TO X` when the device's input is open, follow is on and `remembered[X]` is known; otherwise `FLIP TO X, THEN CTRL+E` | `ledSegOn` (the readout's colour: the prompt belongs to the unit) |
| 3 | 400 ms after a send | `SENT` | `labelPrimary` |
| 4 | JU-06A hint for engine X | `FLIPPED TO X? CTRL+E` | `labelPrimary` |
| 5 | Search field non-empty | `NO MATCHES`, `1 MATCH`, `<n> MATCHES` | `labelSecondary` |
| 6 | Otherwise | `strings.idleContext`: `JP-08`, `JU-06A  JUNO-60`, `JU-06A  JUNO-106` | `labelSecondary` |

X is `60` or `106`. **No string needs shortening**: the context line is proportional
type with at least 599 px, and the longest string is 298 px at p = 3. The D-05's LCD line
2 never shows the JU-06A strings (they belong to the JU-06A's display).

**States** (every one is in `docs/design/mockup/led-states.png`):

| State | Readout | Name | Context |
|---|---|---|---|
| Normal, connected | current | current | idle |
| Sent flash | new current | new current | `SENT`, 400 ms (one timer with the status LED, as DESIGN.md 5.3) |
| Search active | current | current | count |
| Armed (JU-06A) | current (the unit has not changed) | current | prompt, `ledSegOn` |
| Hint (JU-06A) | current | current | hint |
| No patch (JU-06A) | `---` | blank | as the priority table |
| Disconnected | current | current | `NO MIDI OUTPUT`, `labelRed` |
| Entry pending | echo | blank | unchanged |
| Enabled, no port saved | current or `---` | current or blank | `OPEN SETTINGS: CTRL+,` |

No hover, press, focus or disabled state; not focusable; ignores clicks.

**Rendering and footprint** (same contract as DESIGN.md 5.4):

- Glyph atlas: one `QImage` (`Format_RGB32`) per (`P`, `dpr`, skin, `ledShowGhost`) holding
  the 13 characters above, each with and without the lit decimal point, as `11P x 15P`
  device-px cells drawn on `ledBack` with the ghost segments under the lit ones. Polygons
  are filled with antialiasing **when the atlas is built** (an exception to DESIGN.md 11.2,
  like the star: slanted edges need it); blits are unscaled. Rebuilt only when `P`, the
  DPR or the skin changes.
- Frame: one `QPixmap` (device px, `setDevicePixelRatio(dpr)`) of the whole widget:
  `panel`, the window, the field, the cells, the name and the context drawn with
  `QPainter::drawText` into it. `setContent(readout, name, context, contextColourRole)`
  compares with the stored values and returns if equal; otherwise it redraws the changed
  parts into the frame (cells from the atlas; the name or context strip cleared to
  `panel` and drawn again) and calls `update()`. Text layout happens only there.
- `paintEvent` draws the frame pixmap at `QPoint(0, 0)`. No allocation, no text layout.

### 4.3 Group-bar slot

**`d05`: `GroupBar`** with its `PATCH GROUP` rule, unchanged (DESIGN.md 4.3, 6.3).

**`jp08`: empty.** The rect `(16, B + 12, 394, 68)` shows the band and nothing else; not
a focus stop. The grid does not move, so bank and number buttons are in the same place
on all three devices (principle 1; the owner switches devices mid-set and aims with the
mouse). Owner alternative, rendered as `docs/design/mockup/jp08-grid-up.png`: the grid,
its rules and PREV / NEXT move up 80 px on the JP-08 only (bank rule at `B + 12`);
decision 13.3.1.

**`ju06a-60`, `ju06a-106`: `EngineControl`** (FR-19), in the slot rect, coordinates
relative to it:

| Part | Rect | Content |
|---|---|---|
| Rule | `(0, 0, 394, 12)` | section rule in the skin's style (5.1) with `ruleBand2` / `ruleBand2Text`, title `ENGINE` |
| Button `60` | face `(0, 16, 44, 24)` | panel button kind B, face set `button`, lens LED, legend `60` (5.2) |
| Button `106` | face `(50, 16, 44, 24)` | same, legend `106` |
| Caption | x 112, baselines 25 and 40 | `type.note`, `labelDim`, two lines: `The app's belief about the 60/106 switch.` and `<key> changes the belief, never the unit.` where `<key>` is `QKeySequence(Ctrl+E).toString(NativeText)` (`Ctrl+E`; `⌘E` on macOS). Measured 235 and 231 px; 282 available |
| Rest | `(0, 44, 394, 24)` | empty band |

The two buttons borrow the idea of the JU-06A's two marked positions (S16, decision 21);
they are ordinary panel buttons, the caption says what they are, and there is no picture
of a switch.

| State | `60` button | `106` button | Other |
|---|---|---|---|
| Engine 60 believed | lens lit | lens off | skin `ju06a-60` |
| Engine 106 believed | lens off | lens lit | skin `ju06a-106` |
| Armed for the other engine | the armed engine's lens **armed** (5.2): a 2 px `ledOn` ring around a dark centre | believed lens lit | display prompt (4.2) |
| Hint for the other engine | the hinted engine's lens armed | believed lens lit | display hint |
| Hover, pressed, focus | as DESIGN.md 6.2 | | |

Interaction: a click on the unlit button does what `Ctrl+E` does (SPEC-2 §6); a click on
the lit one does nothing. One focus stop, in the tab order where the group bar is
(DESIGN.md 9); `Left` / `Right` move the cursor ring, `Enter` / `Space` press. Tooltips:
`Engine 60: what the app believes the switch is on. Ctrl+E toggles.` (and `106`), with
the native key text. Never disabled.

### 4.4 Nameplate and device switcher (`ui/DeviceSwitcher`, FR-16)

Replaces the version 1 nameplate (DESIGN.md 4.5) in the row `(16, 10, W - 32, 28)`; all
baselines at row `y + 20`. Parts from the left:

| Part | x | Content |
|---|---|---|
| Plate | 16 | `logo.png` if present, fitted and cached exactly as DESIGN.md 4.5 (max 360 x 28); else the wordmark `PATCH SWITCHER` in the current skin's `type.wordmark` and its colour (3.1): 174 px on `d05`, 169 on `jp08`, 158 on the JUNO skins (mockup values; the code measures) |
| Divider | plate right + 14 | 1 px rule, row `y + 9` to `y + 21`, `ruleSecondary`. Not drawn when a logo is shown and only one device is enabled (version 1 shows the logo alone) |
| Sub or tabs | divider + 15 | see below |

**One enabled device** (and the logo is absent): `strings.wordmarkSub` in
`type.wordmarkSub`, `labelSecondary`. On the D-05 this is version 1's `FOR ROLAND D-05`
exactly. With a logo and one device: the logo alone. **No enabled device**: the
wordmark alone (the display shows the first-run hint, 13.2.3).

**Two or three enabled devices: tabs**, in the fixed order D-05, JP-08, JU-06A (enabled
ones only), starting at `tx` = divider + 15:

| Part | Geometry (from the tab's `tx`) | Content |
|---|---|---|
| LED | `(tx, y + 14, 10, 4)` | slit LED as DESIGN.md 6.2: `ledOn` with the `ledCore` core `(tx + 2, y + 15, 6, 2)` for the current device; `ledOff` for the others |
| Name | x `tx + 16`, baseline `y + 20` | `strings.modelName` of that device, `type.switcherTab`. Colour: `labelRed` when that device's output is configured but not open (the `NO MIDI OUTPUT` condition) or has no port; else `labelPrimary` for the current device, `labelSecondary` for the others; hover over a non-current tab `labelPrimary` |
| Hit rect | `(tx - 6, 10, 16 + nameAdvance + 12, 28)` | click switches to that device on release; the current tab ignores clicks |
| Next tab | `tx + 16 + nameAdvance + 18` | |

Measured name advances: `D-05` 28, `JP-08` 34, `JU-06A` 44 px, so three tabs end at
x 409 without a logo on the D-05 skin (5 px earlier on `jp08`, 16 px earlier on the JUNO
skins, whose wordmarks are narrower) and at most x 595 with a full-width logo, inside the
800 px minimum. Tabs use the current skin's colours (the switcher is part of the panel). The
red name is the panel's "not found" red (DESIGN.md 8.1) and is the only connection mark;
the disconnected current device also shows `NO MIDI OUTPUT` on its display and `NOT
FOUND` in the status bar. The JU-06A tab never shows the engine (the skin does).

Not a focus stop: the keyboard path is `Ctrl+Tab` / `Ctrl+Shift+Tab` (FR-13; macOS
13.2.1). Tooltip per tab: `<model>: <output port>, channel <n> (Ctrl+Tab)`, or
`<model>: no MIDI output (Ctrl+Tab)`. Mouse cursor stays the arrow. Repaints only when
the device set, the current device, a connection state or the hover changes.

---

## 5. Panel parts per skin

### 5.1 Section rule styles (`ui/SectionRule`)

Every style is 12 px tall, so rows do not move. **Accent** rules (over the group slot,
bank and number rows) take the skin's `components.sectionRule`; **secondary** rules (in
dialogs) are the version 1 secondary hairline on every skin (`labelSecondary` title,
`ruleSecondary` rule).

| Style | Drawing (relative to the 12 px row, width `w`) | Title |
|---|---|---|
| `hairline` | DESIGN.md 6.1: rule `ruleAccent` at row `y + 8`, interrupted 6 px around the title | `type.panelLabel`, `labelAccent`, baseline `y + 10` |
| `doubleBar` (S12 drawing of S10's bar, decision 17) | two bars across the full width, `(0, 2, w, 3)` and `(0, 7, w, 3)`, `ruleAccent`; a plate from `textLeft - 8` to `textRight + 8`, `y` 0 to 12, radius 2 (antialiased), `ruleTitleBack`, drawn over the bars | `type.ruleTitle`, `ruleTitleText`, centred, baseline `y + 10` |
| `band` (S13, S14, S16) | one band `(0, 0, w, 12)` in `ruleBand` (bank and number rules) or `ruleBand2` (the engine rule) | `type.ruleTitle`, `ruleBandText` or `ruleBand2Text`, centred, baseline `y + 10` |

Rule titles per skin are `strings.*Title` (2.4). Numerals `1` to `8` over the grid use
`labelNumeral` (white on the 64-patch skins, as on the JUPITER-8 and the JUNOs; blue on the
D-05 as version 1).

### 5.2 Panel button (`ui/PanelButton`)

Geometry, kinds A to D, the slot, the top highlight and the states are DESIGN.md 6.2 on
every skin. Two things vary: which **face set** colours a button, and the **LED style**.

Face sets (each is five tokens: `<set>Face`, `<set>FaceHover`, `<set>Pressed`,
`<set>Highlight`, `<set>FaceLit`; the version 1 names `buttonFace`, `buttonFaceHover`,
`buttonPressed`, `buttonHighlight`, `buttonFaceLit` are the set `button`):

| Set | Buttons | `jp08` (decision 17) | `ju06a-60` (decision 18) | `ju06a-106` (decision 19) |
|---|---|---|---|---|
| `button` | number row, group bar, engine buttons | white | cream-white | teal |
| `bank` | bank row | white | tan | teal |
| `view` | ALL, FAVOURITES, RECENTS | blue | peach | cream |
| `commit` | AUTO SEND | red | orange | orange |
| `transport` | PREV, NEXT, SYSTEM | yellow | tan | cream |

On the D-05 all five sets resolve to the version 1 button colours. Unchanged on every
skin: `buttonSlot` (the 1 px recess), `buttonDisabled`, the legend `buttonLegend` (kind
B), the wedge `wedge` (kind D). Label colours above the control row: `labelPrimary`,
except `AUTO SEND` in `labelCommit`.

LED styles (`components.buttonLed`):

| Style | Kind A | Kind B | Lit | Unlit | Armed |
|---|---|---|---|---|---|
| `slit` (D-05) | DESIGN.md 6.2 | DESIGN.md 6.2 | as version 1 | as version 1 | (not used on the D-05) |
| `lens` (64-patch skins, decision 20) | ring: circle centre `(22, 12)` radius 5 in `lensRim`; lens: same centre radius 4 | ring centre `(10, 12)`, radius 5; lens radius 4; legend `type.panelLabel` `buttonLegend` centred on x 28, baseline 16 | lens `ledOn` | lens `ledOff` | ring and lens replaced by a circle radius 5 in `ledOn` with a circle radius 3 in `ledOff` (a 2 px red ring, dark centre) |

Circles are antialiased (like the face corners); everything shifts 1 px down when
pressed. On lens skins `<set>FaceLit` equals `<set>Face`: the face colour carries the
skin and the LED alone says "lit". At 2x every number doubles.

### 5.3 Control row, search field, status bar, menus, tooltips, scrollbar

As DESIGN.md 6.5, 7.1, 7.6, 8 with the skin's tokens (QSS per skin, 2.5). The search
placeholder is `strings.searchPlaceholder`. The status bar shows the current device's
output, channel and input. The scrollbar handle uses `controlFace*`. The status bar's
activity LED is a slit on every skin.

---

## 6. List rows (`ui/PatchDelegate`)

Row height, columns, selection, hover, match highlight and star are DESIGN.md 7.2 to
7.6. Changes by model:

### 6.1 Address column

| Model | Cells (9 px each, from x 28) | Text |
|---|---|---|
| D-05 | 5 | `P1-11` (version 1) |
| JP-08 | 3 | `4-5` |
| JU-06A | 3 | `4-5` (the engine is the tag, 6.3; the canonical `106:4-5` appears only in the library file and tooltips) |

The name stays at x 84 on every model. Pre-built `QStaticText` for `1` to `8`, `-`, `U`,
`P`.

### 6.2 LED column `(10, 12, 10, 4)`

First that applies:

| Row | Drawing |
|---|---|
| The armed address (JU-06A) | the LED rect in `ledOff` inside a 1 px `ledOn` frame drawn **outside** it, `(9, 11, 12, 6)`: an empty lit outline, "selected, waiting for the flip" |
| The current patch | lit, as version 1 (`ledOn` with the `ledCore` core) |
| The other engine's remembered patch (JU-06A, `remembered[other]` known) | `ledOff`, no core: a dark slot that says "the unit returns to this when flipped" |
| Any other row | nothing |

The address cells use `labelPrimary` on the current row only (version 1).

### 6.3 Tag slot

| `rowTag` | Rect | Drawing |
|---|---|---|
| `keyMode` (D-05) | `(Wl - 52, 6, 16, 16)` | DESIGN.md 7.2 |
| `none` (JP-08) | | nothing |
| `engine` (JU-06A), row of the believed engine | `(Wl - 58, 6, 28, 16)` | 1 px `tagBorder` frame; `60` or `106` in `type.listMeta`, `labelSecondary`, centred (`106` is 18 px) |
| `engine`, row of the other engine | same rect | no frame; the number in `labelDim`. The row itself is not dimmed: name, address and star as any row (available, one flip away) |

The recents count column `(Wl - 100, 0, 40, 28)` ends at `Wl - 60`, before the engine
tag; the star is unchanged.

### 6.4 Renamed slots (JP-08, JU-06A)

A slot with a user name (SPEC-2 §3.3) shows **both** names, so a search hit on either is
visible:

| Part | x | Style |
|---|---|---|
| User name | 84 | `type.listName`, `labelPrimary` |
| Factory name | user name end + 12 | `type.listName`, `labelDim` |

Both are drawn from one width budget (x 84 to `Wl - 100`): if the two do not fit, the
factory name is elided right; if the user name alone does not fit, it is elided right
and the factory name is not drawn. Match ranges (`matchBack`, DESIGN.md 7.3) are drawn
under whichever name they fall in; `SearchQuery` reports for each range which of the two
names it indexes (13.2.4). Tooltip on the row: `Factory name: <name>`. The display shows
the user name only. Rows without a user name are version 1 rows.

### 6.5 Inline editor on the 64-patch models

`F2` or a double-click opens the version 1 editor (DESIGN.md 7.4, same rect, max 18,
counter) on **any** JP-08 or JU-06A row. Its placeholder is the factory name (`labelDim`,
QLineEdit placeholder colour from the palette). While editing, the factory name is not
drawn. Committing an empty text removes the user name (the factory name returns).

### 6.6 Empty states

DESIGN.md 7.5, the same texts on every model.

---

## 7. Settings (`ui/SettingsDialog`, FR-9)

### 7.1 Window

Fixed width **760** (content 720 at margins 20), height from the tallest page (the
JU-06A's) so the dialog does not resize between pages: client **760 x 610** at 1x.
Window title `Settings`. Skinned with the current skin (section 2.1). Two columns: the
**device list** at content x 0, width 140; the **page** at content x 160, width 560. Below
both, the **global section** across 720. Mockup: `docs/design/mockup-settings-devices.png`.

### 7.2 Device list

A well `(0, 0, 140, 134)`: `well` fill, version 1 well edges (DESIGN.md 7.2), 1 px
`focusRing` border when focused. Three rows of 44 inside the 1 px border, always all
three models in the order D-05, JP-08, JU-06A. Row-relative geometry (x from the inner
left edge):

| Part | Geometry | Content |
|---|---|---|
| Enabled checkbox | indicator `(10, 10, 16, 6)` | the version 1 LED checkbox look (DESIGN.md 10.2): `ledOn` checked, `ledOff` with `fieldBorderHover` border unchecked; focus border `focusRing` |
| Name | x 36, baseline 19 | `type.body`; `labelPrimary` when enabled, `labelSecondary` when not |
| Connection LED | `(36, 29, 10, 4)` | lit (`ledOn`, no core) when connected, else `ledOff`; not drawn when disabled |
| Connection text | x 52, baseline 34 (x 36 when disabled) | `type.note`: `Connected` (`labelSecondary`), `Port not found` (`labelRed`), `Error` (`labelRed`, full text in the tooltip), `No output port` (`labelDim`), `Off` when disabled (`labelDim`). Widest 83 px of 86 |
| Selected row | full row | `selection` fill and the 3 px bar (`focusRing` when the list has focus, else `selectionBarIdle`) |
| Hover | full row | `rowHover` |

Painted by the dialog (a small custom widget or a delegate); the QSS does not style it.
Keyboard: `Up` / `Down` select a row (the page follows), `Space` toggles enabled. Mouse:
a click on x 0 to 32 toggles enabled; elsewhere it selects. The page shown first is the
current device's. Enabling or disabling takes effect on OK, like everything else.

### 7.3 Device page

Coordinates relative to the page's top-left (content x 160). Form labels as DESIGN.md
10.1 (`type.panelLabel` caps, `labelPrimary`, right edge at page x 100).

| y | Row |
|---|---|
| 0 | Secondary rule `MIDI OUTPUT` (560 wide) |
| 20 | `PORT` · combo `(112, y, 260, 26)` · `USE` `(380, y, 72, 26)`, object name `useOutputButton` · `REFRESH` `(460, y, 100, 26)` |
| 54 | `CHANNEL` · spin box `(112, y, 72, 26)` · conflict text `(196, y, 364, 26)`, object name `channelConflictText` |
| 88 | Connection line, as DESIGN.md 10.1, for this device |
| 124 | Secondary rule `MIDI INPUT` |
| 144 | `PORT` · combo `(112, y, 260, 26)`, first item `None` · `USE` `(380, y, 72, 26)`, `useInputButton` · input problem line `(460, y, 100, 26)` as DESIGN.md 10.1 |
| 178 | Checkbox `Follow the panel` (x 112) |
| 206 | Checkbox `Resend the current patch on connect` (x 112; per device now) |
| 242 | JU-06A only: secondary rule `ENGINE`, rows below; other models: the `ON THE <model>` rule here and the note at 262 |
| 262 | JU-06A: `BELIEVED` · value `60` or `106` at x 112 (`QLabel[role="engineValue"]`, Medium 13) · note at x 160, `type.note` `labelDim`: `Ctrl+E or the 60 / 106 buttons change it` (native key text) |
| 288 | JU-06A: `ON 60` · `4-5` at x 112 and the patch name at x 160 (`type.body`), or `unknown` at x 112 (`QLabel[role="engineUnknown"]`) |
| 314 | JU-06A: `ON 106` · same |
| 350 | JU-06A: secondary rule `ON THE JU-06A` |
| 370 | JU-06A: the note, `type.note`, `labelSecondary`, wrapped at 560 (3 lines, line height 15) |

Notes (wrapped at 560 px; D-05 and JP-08 two lines, JU-06A three):

| Model | Note |
|---|---|
| D-05 | `The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also has a Bank.S switch, turn it ON too.` (version 1) |
| JP-08 | `Set the JP-08's MIDI channel with [MANUAL] + [2], then [1] to [16]. The JP-08 has no program change switch: the channel is the only setting that matters.` |
| JU-06A | `Set the JU-06A's MIDI channel: hold ARPEGGIO [ON/OFF], press [2], turn [VALUE]. The app cannot read the 60/106 switch. When you flip it, the app follows if it sees the flip on the input; otherwise press Ctrl+E.` (`Cmd+E` on macOS) |

**Detected ports** (FR-18). In both combos, a port that `Identify` attributes to this
page's model gets the display text `<port name> · detected` (U+00B7, in the font subset);
the stored key is the plain port name in the item data. The closed combo shows the same
text. `USE` is enabled when exactly one port is detected for this model and the combo does
not already hold it; a click selects it. Tooltip `Use <port name>`. Disabled otherwise
(version 1 disabled button look). Both `USE` buttons are 72 x 26 overall: QSS
`#useOutputButton`, `#useInputButton` in `app.qss.in`.

**Port and channel conflict** (SPEC-2 §3.4). While two enabled devices would share an
output port and a channel, the page of the device that is not first in the list order
shows in `channelConflictText` (`type.note` 12 px, `labelRed`, vertically centred,
elided right, full text in the tooltip): `Channel 4 on Hapax MIDI 1 is used by the D-05.`
and OK is disabled. The same for inputs with `input` in place of nothing:
`Channel 4 on JU-06A (input) is used by the D-05.` The text disappears and OK enables as
soon as the conflict is gone.

### 7.4 Global section and buttons

Content-relative (x 0 is the dialog's content left):

| y | Row |
|---|---|
| 439 | Secondary rule `ALL DEVICES`, 720 wide |
| 459 | Checkbox `Auto-send on highlight` at x 272 |
| 487 | Checkbox `Follow switches device` at x 272; under it, at x 298 with baseline 519, `type.note` `labelDim`: `An incoming patch change from another device switches the window to it.` (420 px) |
| 542 | Button row, height 28: `TEST` at x 0 (tests the device whose page is shown; disabled while that device is disconnected or not enabled); `CANCEL` at x 512 and `OK` at x 620, both 100 wide; `OK` default |

The global section starts 24 px below the tallest page (415). Initial focus: the shown
page's output combo. Tab order: device list, page (top to bottom), global checkboxes,
TEST, CANCEL, OK. Escape = Cancel. Every control style is DESIGN.md 10.2 with the skin's
tokens.

---

## 8. Shortcuts, About, menus, titles

### 8.1 Shortcuts sheet (DESIGN.md 10.3)

A new section `DEVICE` at the end of the **left** column, after `LIST`:

| Keys | Action |
|---|---|
| `Ctrl+Tab` | Next device |
| `Ctrl+Shift+Tab` | Previous device |
| `Ctrl+E` | Toggle JU-06A engine |

Measured caps 59, 90 and 47 px (macOS `⌘E` 31); actions at most 140 px; both inside the
version 1 columns. On macOS the device keys are the Control key (13.2.1); their caps read
`Ctrl+Tab` and `Ctrl+Shift+Tab` literally (the font subset has no `⌃` or `⇥`). Left
column: 256 + 16 + 12 + 4 + 3 x 26 = **366**; right column 340. Content height 366 + 16 +
16 = 398; client **680 x 438** at 1x. Everything else as version 1. Shown on every
device (the rows explain themselves).

### 8.2 About box (DESIGN.md 10.4)

Width 440, title `About`. Top: the wordmark `PATCH SWITCHER` in the current skin's
`type.wordmark` (3.1), divider, and the sub `D-05 · JP-08 · JU-06A` (`type.wordmarkSub`,
129 px; total 332 px on `d05`, 327 on `jp08`, 316 on the JUNO skins, inside the 400 px
content width). Paragraphs, 8 px
apart:

1. `Version <x.y.z>` (`type.body`)
2. `Selects patches on Roland Boutique D-05, JP-08 and JU-06A synths over MIDI.` (`type.body`, two lines at 400)
3. Link (`type.note`, `labelAccent` of the current skin): `github.com/MarcAstr0/boutique-patch-switcher` to `https://github.com/MarcAstr0/boutique-patch-switcher` (T9.1 renames the repository; GitHub redirects the old URL meanwhile). The rich-text colour is the resolved `labelAccent`, not a literal.
4. `MIT licence. Copyright 2026 Mario Castro Squella.` (`type.note`)
5. `Roland, Boutique, D-05, D-50, JP-08, JUPITER-8, JU-06A, JUNO-60 and JUNO-106 are trademarks of Roland Corporation. This app is not affiliated with or endorsed by Roland.` (`type.note`, three lines)
6. `Inter and Archivo typefaces, SIL Open Font License 1.1.` (`type.note`; 310 px, one line)

### 8.3 Menus and titles

Main window title `Boutique Patch Switcher`. System menu as DESIGN.md 8.2 with the item
`About Boutique Patch Switcher`. Window icon: section 9.

---

## 9. Icon (`assets/icons/boutique-patch-switcher.*`)

Drawn for this project; no Roland mark, logotype or artwork. Source written by
`assets/icons/make-bps-icons.py`, commands in `assets/icons/README.md`.

- Tile `(8, 8, 240, 240)` radius 44, `panel` `#202326`; band below y 168 in `band`
  `#26292B` with a 2 px `bandEdge` `#3A3E41` line (the D-05 icon's family).
- Window `(32, 48, 192, 96)`: 2 px `bezelRim` `#2E3134`, `bezel` `#050606` inside; field
  `(44, 60, 168, 72)` `ledBack` `#0A0302`.
- Readout `1-1`: three cells of section 4.2 at `P` = 3.4, centred in the field, lit
  `#FF3A2A`, ghost `#1C0806`.
- Three faces `(40, 192, 48, 24)`, `(104, 192, 48, 24)`, `(168, 192, 48, 24)`, radius 3:
  D-05 grey `#3F4244` with an unlit slit `(56, 201, 16, 6)` `#2A1A1A`; JUPITER-8 white
  `#CFCBC2` with a lens (ring radius 7.5 `#141414`, lens radius 6) lit `#FF3B30`;
  JUNO-106 teal `#80A4A0` with an unlit lens (`#2A2B2D` ring, `#4A2B28` lens).
- 16, 24, 32 px: hand placed on whole pixels by the same script (`1-1` as bars).
- Files: `.svg`, `.png` (256), `-16` to `-128` PNGs, `.ico` (7 sizes), `.icns` (10 entries).
  The version 1 `d05-patch-switcher.*` files stay until T9.1.

---

## 10. Mockups

Source: `docs/design/mockup/build2.py` (reuses `build.py`'s helpers without changing it,
reads `tokens.json` (version 2), renders with Inkscape at 1x and 2x with a private `FONTCONFIG_FILE`
in a temporary directory whose only font directory is `assets/fonts`, so Inkscape sees the
committed Inter and Archivo subsets and no system font). Run
`python3 docs/design/mockup/build2.py`.

| File (1x; `@2x` twice the size) | Size | State |
|---|---|---|
| `docs/design/mockup-jp08.png` | 960 x 560 | JP-08, current `3-5` *Lo Brass*, list focused on the renamed `3-4` (*CN Strings Lift*, factory *Mellow Strings*), favourite `3-8`, connected through the Hapax on channel 5 with follow |
| `docs/design/mockup-ju06a-60.png` | 960 x 560 | JU-06A on 60, current `4-5` *LD Silo Lead*; search `pad` focused, `10 MATCHES` across both engines (60 tags framed, 106 tags dim), 106's remembered `7-1` marked |
| `docs/design/mockup-ju06a-106.png` | 960 x 560 | JU-06A on 106, current `1-1` *Brass*; search `bass`, list focused, **armed** on `60:3-6` *BS Ouch Bass*: armed row LED, `60` button armed, `FLIP SWITCH TO 60`; a renamed 106 slot |
| `docs/design/mockup-devices.png` | 960 x 560 | D-05 skin with three devices enabled: `D-05` current, `JP-08`, `JU-06A` missing (red); everything else version 1 |
| `docs/design/mockup-settings-devices.png` | 760 x 610 | Settings on the JU-06A page: detected port in both combos, `USE` disabled (already used), engine information, JP-08 disabled in the list |
| `docs/design/mockup/led-states.png` | 799 x 1372 | every LED display state of 4.2 on the JU-06A and JP-08 skins |
| `docs/design/mockup/engine-states.png` | 426 x 400 | engine control: 60, 106, armed, keyboard focus |
| `docs/design/mockup/jp08-grid-up.png` | 960 x 560 | owner alternative for the JP-08 group slot (13.3.1) |
| `docs/design/mockup/nameplates.png` | 440 x 264 | the four nameplates of 3.1 stacked, each on its skin's panel, logo absent, one device enabled (for the owner, 13.4) |

---

## 11. Skin palettes

Every token not listed for a skin resolves to the D-05 value (DESIGN.md 2 and 2.3 here).
Contrast ratios checked (WCAG relative luminance) for the pairs that carry text:
`labelPrimary` on `panel` 15.9 (jp08), 15.0 (60), 7.1 (106); `labelSecondary` on `band`
7.9, 6.9, 5.8; `labelDim` on `band` 3.6, 3.4, 3.6 (placeholders and hints only, as
version 1); `labelRed` on `band` 5.9, 5.6, 4.7; `ruleTitleText` on `ruleTitleBack` 12.4
(jp08); `ruleBandText` on `ruleBand` 6.0 (60), 5.2 (106); `ruleBand2Text` on `ruleBand2`
4.4 (60), 6.6 (106); `buttonLegend` on `buttonFace` 10.9, 12.3, 6.2; `ledSegOn` on
`ledBack` 7.2, 5.8, 5.8; `focusRing` on `band` 9.1, 8.4, 6.5.

### 11.1 `d05`

DESIGN.md sections 2 to 10, unchanged. The new tokens resolve as in 2.3.

### 11.2 `jp08` (JUPITER-8 panel, JP-08 readout)

Near-black cool panel; red-orange double bars with dark title plates; white grid buttons
with round red LEDs; the JUPITER-8's function colours on the other buttons (blue views,
red AUTO SEND, yellow PREV / NEXT / SYSTEM); orange-red readout; red-orange wordmark in
Archivo SemiBold (3.1) and red-orange link; white numerals.

| Token | Hex | D-05 value |
|---|---|---|
| `panel` | `#121314` | `#202326` |
| `band` | `#17181A` | `#26292B` |
| `bandEdge` | `#3E4044` | `#3A3E41` |
| `well` | `#0B0C0D` | `#141617` |
| `wellEdgeDark` | `#050506` | `#0B0C0D` |
| `wellEdgeLight` | `#2C2E31` | `#34383B` |
| `fieldBack` | `#08090A` | `#0E0F10` |
| `fieldBorder` | `#36383C` | `#3A3D40` |
| `fieldBorderHover` | `#4A4D52` | `#4C5053` |
| `rowHover` | `#1A1B1D` | `#1B1D1F` |
| `selection` | `#2C2E33` | `#33373B` |
| `selectionBarIdle` | `#6E7177` | `#6B6E70` |
| `matchBack` | `#5A2616` | `#2E4366` |
| `bezel` | `#050505` | `#050606` |
| `bezelRim` | `#2A2C2F` | `#2E3134` |
| `bezelRimTop` | `#4A4D52` | `#45494D` |
| `labelPrimary` | `#ECEDEE` | `#E2E0D4` |
| `labelSecondary` | `#A9ADB3` | `#A3A293` |
| `labelDim` | `#6C7076` | `#6E6D64` |
| `labelDisabled` | `#55585D` | `#5F5F58` |
| `labelAccent` | `#F07A4A` | `#86A6D4` |
| `ruleAccent` | `#C8441F` | `#6E8EBA` |
| `ruleSecondary` | `#80848A` | `#8C8B7E` |
| `labelRed` | `#FF5E4D` | `#E8565C` |
| `starOff` | `#55585D` | `#5C5E5A` |
| `tagBorder` | `#45484C` | `#4A4C48` |
| `buttonSlot` | `#050505` | `#121415` |
| `buttonFace` | `#CFCBC2` | `#3F4244` |
| `buttonFaceHover` | `#DCD8CF` | `#4A4D50` |
| `buttonPressed` | `#ABA8A0` | `#2F3133` |
| `buttonHighlight` | `#E6E3DC` | `#56595C` |
| `buttonFaceLit` | `#CFCBC2` | `#4B3B3C` |
| `buttonDisabled` | `#3A3B3E` | `#2C2E30` |
| `ledOn` | `#FF3B30` | `#F0383F` |
| `ledOff` | `#4A2420` | `#2A1A1A` |
| `ledBack` | `#0A0402` | `#0A0302` |
| `ledSegOn` | `#FF6B1A` | `#FF3A2A` |
| `ledSegGhost` | `#1F0D05` | `#1C0806` |
| `bankFace` | `#CFCBC2` | `#3F4244` |
| `bankFaceHover` | `#DCD8CF` | `#4A4D50` |
| `bankPressed` | `#ABA8A0` | `#2F3133` |
| `bankHighlight` | `#E6E3DC` | `#56595C` |
| `bankFaceLit` | `#CFCBC2` | `#4B3B3C` |
| `viewFace` | `#2F74B5` | `#3F4244` |
| `viewFaceHover` | `#3A80C0` | `#4A4D50` |
| `viewPressed` | `#275F96` | `#2F3133` |
| `viewHighlight` | `#4D8DCA` | `#56595C` |
| `viewFaceLit` | `#2F74B5` | `#4B3B3C` |
| `commitFace` | `#C2341F` | `#3F4244` |
| `commitFaceHover` | `#CE422C` | `#4A4D50` |
| `commitPressed` | `#A02A18` | `#2F3133` |
| `commitHighlight` | `#D65A46` | `#56595C` |
| `commitFaceLit` | `#C2341F` | `#4B3B3C` |
| `transportFace` | `#C9982C` | `#3F4244` |
| `transportFaceHover` | `#D5A53B` | `#4A4D50` |
| `transportPressed` | `#A67D22` | `#2F3133` |
| `transportHighlight` | `#DDB45A` | `#56595C` |
| `transportFaceLit` | `#C9982C` | `#4B3B3C` |
| `buttonLegend` | `#18191B` | `#E2E0D4` |
| `wedge` | `#1A1A1A` | `#BDB9A0` |
| `lensRim` | `#141414` | `#121415` |
| `controlFace` | `#34363A` | `#3F4244` |
| `controlFaceHover` | `#404247` | `#4A4D50` |
| `controlPressed` | `#26282B` | `#2F3133` |
| `labelNumeral` | `#ECEDEE` | `#86A6D4` |
| `labelCommit` | `#FF6B4A` | `#E8565C` |
| `ruleTitleBack` | `#2B2D33` | `#202326` |
| `ruleTitleText` | `#F2F3F4` | `#E2E0D4` |

### 11.3 `ju06a-60` (JUNO-60 panel, JU-06A readout)

Black panel; blue bands over bank and number, a brick band over the engine control;
cream-white number buttons, tan bank, PREV / NEXT / SYSTEM buttons, peach views, orange
AUTO SEND; red readout; cream-white wordmark in Archivo Bold (3.1); blue accent lettering.

| Token | Hex | D-05 value |
|---|---|---|
| `panel` | `#18191A` | `#202326` |
| `band` | `#1E1F21` | `#26292B` |
| `bandEdge` | `#46484A` | `#3A3E41` |
| `well` | `#101112` | `#141617` |
| `wellEdgeDark` | `#070808` | `#0B0C0D` |
| `wellEdgeLight` | `#323436` | `#34383B` |
| `fieldBack` | `#0B0C0C` | `#0E0F10` |
| `fieldBorder` | `#3B3D3F` | `#3A3D40` |
| `fieldBorderHover` | `#4E5052` | `#4C5053` |
| `rowHover` | `#1D1E20` | `#1B1D1F` |
| `selection` | `#303235` | `#33373B` |
| `selectionBarIdle` | `#6E6C66` | `#6B6E70` |
| `matchBack` | `#2B4270` | `#2E4366` |
| `bezel` | `#050505` | `#050606` |
| `bezelRim` | `#2C2D2F` | `#2E3134` |
| `bezelRimTop` | `#48494B` | `#45494D` |
| `labelPrimary` | `#F0EDE2` | `#E2E0D4` |
| `labelSecondary` | `#ABA79B` | `#A3A293` |
| `labelDim` | `#75726B` | `#6E6D64` |
| `labelDisabled` | `#5E5C58` | `#5F5F58` |
| `labelAccent` | `#86A9E6` | `#86A6D4` |
| `ruleAccent` | `#2D5BA3` | `#6E8EBA` |
| `ruleSecondary` | `#8E8A7E` | `#8C8B7E` |
| `labelRed` | `#FF6157` | `#E8565C` |
| `starOff` | `#5E5D59` | `#5C5E5A` |
| `tagBorder` | `#4C4B47` | `#4A4C48` |
| `buttonSlot` | `#0B0B0B` | `#121415` |
| `buttonFace` | `#E3DDCC` | `#3F4244` |
| `buttonFaceHover` | `#EEE9DA` | `#4A4D50` |
| `buttonPressed` | `#C2BCAC` | `#2F3133` |
| `buttonHighlight` | `#F4F0E4` | `#56595C` |
| `buttonFaceLit` | `#E3DDCC` | `#4B3B3C` |
| `buttonDisabled` | `#3A3936` | `#2C2E30` |
| `ledOn` | `#F2433A` | `#F0383F` |
| `ledOff` | `#4A2A26` | `#2A1A1A` |
| `ledBack` | `#0A0202` | `#0A0302` |
| `ledSegOn` | `#FF3A2A` | `#FF3A2A` |
| `ledSegGhost` | `#1E0705` | `#1C0806` |
| `bankFace` | `#BCA48A` | `#3F4244` |
| `bankFaceHover` | `#C9B297` | `#4A4D50` |
| `bankPressed` | `#9E8971` | `#2F3133` |
| `bankHighlight` | `#D2BFA8` | `#56595C` |
| `bankFaceLit` | `#BCA48A` | `#4B3B3C` |
| `viewFace` | `#D8B391` | `#3F4244` |
| `viewFaceHover` | `#E3BF9F` | `#4A4D50` |
| `viewPressed` | `#B89678` | `#2F3133` |
| `viewHighlight` | `#E8CBB0` | `#56595C` |
| `viewFaceLit` | `#D8B391` | `#4B3B3C` |
| `commitFace` | `#E07E3C` | `#3F4244` |
| `commitFaceHover` | `#EA8B4A` | `#4A4D50` |
| `commitPressed` | `#BE6830` | `#2F3133` |
| `commitHighlight` | `#EE9F66` | `#56595C` |
| `commitFaceLit` | `#E07E3C` | `#4B3B3C` |
| `transportFace` | `#BCA48A` | `#3F4244` |
| `transportFaceHover` | `#C9B297` | `#4A4D50` |
| `transportPressed` | `#9E8971` | `#2F3133` |
| `transportHighlight` | `#D2BFA8` | `#56595C` |
| `transportFaceLit` | `#BCA48A` | `#4B3B3C` |
| `buttonLegend` | `#1E1E1E` | `#E2E0D4` |
| `wedge` | `#2A2724` | `#BDB9A0` |
| `lensRim` | `#2A2826` | `#121415` |
| `controlFace` | `#393B3E` | `#3F4244` |
| `controlFaceHover` | `#45474A` | `#4A4D50` |
| `controlPressed` | `#2B2D2F` | `#2F3133` |
| `labelNumeral` | `#F0EDE2` | `#86A6D4` |
| `labelCommit` | `#F29A5A` | `#E8565C` |
| `ruleBand` | `#2D5BA3` | `#6E8EBA` |
| `ruleBandText` | `#F4F2EA` | `#E2E0D4` |
| `ruleBand2` | `#B9502E` | `#6E8EBA` |
| `ruleBand2Text` | `#F7F1EA` | `#E2E0D4` |

### 11.4 `ju06a-106` (JUNO-106 panel, JU-06A readout)

Mid-grey panel (the lightest skin); light-blue bands over bank and number, a crimson band
over the engine control; teal bank and number buttons, cream views and PREV / NEXT /
SYSTEM, orange AUTO SEND; red readout; white wordmark in Archivo Bold (3.1); pale blue
accent lettering; a lighter focus ring for the grey.

| Token | Hex | D-05 value |
|---|---|---|
| `panel` | `#505155` | `#202326` |
| `band` | `#47484C` | `#26292B` |
| `bandEdge` | `#626367` | `#3A3E41` |
| `well` | `#2A2B2E` | `#141617` |
| `wellEdgeDark` | `#1C1D1F` | `#0B0C0D` |
| `wellEdgeLight` | `#5E5F63` | `#34383B` |
| `fieldBack` | `#232427` | `#0E0F10` |
| `fieldBorder` | `#6A6B70` | `#3A3D40` |
| `fieldBorderHover` | `#7E7F84` | `#4C5053` |
| `rowHover` | `#333437` | `#1B1D1F` |
| `selection` | `#46484D` | `#33373B` |
| `selectionBarIdle` | `#8E9095` | `#6B6E70` |
| `matchBack` | `#33507A` | `#2E4366` |
| `bezel` | `#151517` | `#050606` |
| `bezelRim` | `#3C3D41` | `#2E3134` |
| `bezelRimTop` | `#77787D` | `#45494D` |
| `labelPrimary` | `#F4F3EE` | `#E2E0D4` |
| `labelSecondary` | `#CFCEC7` | `#A3A293` |
| `labelDim` | `#A3A39E` | `#6E6D64` |
| `labelDisabled` | `#8C8C88` | `#5F5F58` |
| `labelAccent` | `#B4CDEE` | `#86A6D4` |
| `ruleAccent` | `#3F6896` | `#6E8EBA` |
| `ruleSecondary` | `#AFAEA6` | `#8C8B7E` |
| `labelRed` | `#FFA096` | `#E8565C` |
| `starOff` | `#7C7D81` | `#5C5E5A` |
| `tagBorder` | `#7C7D81` | `#4A4C48` |
| `buttonSlot` | `#2A2B2E` | `#121415` |
| `buttonFace` | `#80A4A0` | `#3F4244` |
| `buttonFaceHover` | `#8EB1AD` | `#4A4D50` |
| `buttonPressed` | `#6A8B87` | `#2F3133` |
| `buttonHighlight` | `#9DBDB9` | `#56595C` |
| `buttonFaceLit` | `#80A4A0` | `#4B3B3C` |
| `buttonDisabled` | `#5C5D61` | `#2C2E30` |
| `ledOn` | `#FF3B2F` | `#F0383F` |
| `ledOff` | `#4A2B28` | `#2A1A1A` |
| `focusRing` | `#C4DCFA` | `#9DBBE8` |
| `ledBack` | `#0B0203` | `#0A0302` |
| `ledSegOn` | `#FF3A2A` | `#FF3A2A` |
| `ledSegGhost` | `#240A08` | `#1C0806` |
| `bankFace` | `#80A4A0` | `#3F4244` |
| `bankFaceHover` | `#8EB1AD` | `#4A4D50` |
| `bankPressed` | `#6A8B87` | `#2F3133` |
| `bankHighlight` | `#9DBDB9` | `#56595C` |
| `bankFaceLit` | `#80A4A0` | `#4B3B3C` |
| `viewFace` | `#D3CDB2` | `#3F4244` |
| `viewFaceHover` | `#DED9C0` | `#4A4D50` |
| `viewPressed` | `#B5AF96` | `#2F3133` |
| `viewHighlight` | `#E6E2CE` | `#56595C` |
| `viewFaceLit` | `#D3CDB2` | `#4B3B3C` |
| `commitFace` | `#EE9F3C` | `#3F4244` |
| `commitFaceHover` | `#F5AB4C` | `#4A4D50` |
| `commitPressed` | `#CC8530` | `#2F3133` |
| `commitHighlight` | `#F7BB6E` | `#56595C` |
| `commitFaceLit` | `#EE9F3C` | `#4B3B3C` |
| `transportFace` | `#D3CDB2` | `#3F4244` |
| `transportFaceHover` | `#DED9C0` | `#4A4D50` |
| `transportPressed` | `#B5AF96` | `#2F3133` |
| `transportHighlight` | `#E6E2CE` | `#56595C` |
| `transportFaceLit` | `#D3CDB2` | `#4B3B3C` |
| `buttonLegend` | `#1C1D1F` | `#E2E0D4` |
| `wedge` | `#222426` | `#BDB9A0` |
| `lensRim` | `#2A2B2D` | `#121415` |
| `controlFace` | `#36373B` | `#3F4244` |
| `controlFaceHover` | `#424347` | `#4A4D50` |
| `controlPressed` | `#2C2D30` | `#2F3133` |
| `labelNumeral` | `#F4F3EE` | `#86A6D4` |
| `labelCommit` | `#FFC173` | `#E8565C` |
| `ruleBand` | `#3F6896` | `#6E8EBA` |
| `ruleBandText` | `#F4F4F0` | `#E2E0D4` |
| `ruleBand2` | `#A3262D` | `#6E8EBA` |
| `ruleBand2Text` | `#F7F2EC` | `#E2E0D4` |

---

## 12. Implementation map

| Part | Drawn by | Section |
|---|---|---|
| Skin resolution, palette, style sheet switch | `ui/Theme` | 2.5, 2.6 |
| LED display | `ui/LedDisplayWidget` (QPainter, atlas + frame) | 4.2 |
| Group slot | `QStackedWidget` holding `GroupBar`, an empty `QWidget`, `ui/EngineControl` | 4.3 |
| Nameplate and switcher | `ui/DeviceSwitcher` (replaces `ui/Nameplate`; keeps its logo code) | 4.4 |
| Nameplate faces (registering Archivo, resolving `type.wordmark` per skin, fallbacks) | `ui/Theme` | 3.1 |
| Section rule styles | `ui/SectionRule` | 5.1 |
| Face sets, lens LEDs, armed lens | `ui/PanelButton` | 5.2 |
| List rows | `ui/PatchDelegate` | 6 |
| Settings device list and pages | `ui/SettingsDialog` | 7 |
| Everything QSS styles | `assets/theme/<skin>.qss` from `app.qss.in` | 2.5 |

New object names and properties the QSS relies on: `QPushButton#useOutputButton`,
`#useInputButton`, `QLabel#channelConflictText`, `QLabel[role="engineValue"]`,
`QLabel[role="engineUnknown"]`. All version 1 names stay (DESIGN.md 12).

Screenshot states the designer reviews (FR-15 `--screenshot-state`), each at 1x and 2x:
`jp08`, `ju06a-60`, `ju06a-106`, `flip-prompt`, `flip-hint`, `devices`,
`settings-devices`, plus the version 1 D-05 states, which must match the version 1.0.0
references pixel for pixel. Compare against section 10's mockups.

---

## 13. Deviations, readings and owner decisions

### 13.1 Deviations

1. **D-05 dialogs change; the D-05 main window does not.** With only the D-05 enabled,
   the main window is pixel-identical to version 1.0.0. Accepted differences for T8.1's
   pixel comparison: the window title; the Settings dialog (7); the Shortcuts sheet's
   `DEVICE` section (8.1); the About box (8.2); the system menu's About item text; the
   nameplate when two or more devices are enabled (4.4).
2. **The readouts are the Boutiques', the panels are the originals'** (decision 13).
3. **Button faces stay 44 x 24 on every skin**, although the JUPITER-8's patch caps are
   portrait (S10) and the JU-06A's square (S16): the layout is one layout.
4. **LEDs are inside the faces** on the 64-patch skins (decision 20); the JUNOs and the
   JU-06A put them above the buttons, where the grid has its numerals.
5. **The engine control has no picture of a switch** (decision 21).
6. **Antialiasing** in the LED atlas (slanted polygons) and on lens circles, in addition
   to DESIGN.md 11.2's list.
7. **The JP-08 group slot is empty** rather than moving the grid (13.3.1).

### 13.2 Readings for the foreman

1. **`Ctrl+Tab` on macOS** is Control+Tab (`Qt::MetaModifier | Qt::Key_Tab` on macOS,
   where Qt maps Meta to the Control key), because Command+Tab belongs to the system.
   `Ctrl+E` follows the app's convention and is Command+E on macOS (`CMD+E` in the
   display strings, as the brief says).
2. **Prompt variant.** `FLIP SWITCH TO X` needs the input open, follow on and
   `remembered[X]` known; any one missing gives `FLIP TO X, THEN CTRL+E` (SPEC-2 FR-19
   and §6 read together).
3. **First run with no device enabled**: the window uses the `d05` skin (the LCD) and
   line 2 shows `FOUND <MODEL> ON USB: OPEN SETTINGS` when `Identify` finds exactly one
   model, `FOUND <n> UNITS ON USB: OPEN SETTINGS` for more than one, else version 1's
   `OPEN SETTINGS: CTRL+,` (`CMD+,`). All fit 40 columns (34 at most).
4. **Two names, one search.** `SearchQuery` must return, for each match range, which of
   the two names (user or factory) it indexes, so the delegate can draw it (6.4).
5. **Settings refuses with OK disabled** while a port-and-channel conflict exists, and
   names the other device in place (7.3).
6. **Remembered-patch marker** in the JU-06A list (6.2) is a design addition: static
   information that the state machine already holds.
7. **Dialog size** is constant across pages (the JU-06A page's height).

### 13.3 For the owner at G7

1. **JP-08 group slot.** Recommended: empty, the grid stays where it is on every device
   (`mockup-jp08.png`). Alternative: the grid moves up 80 px on the JP-08 only
   (`mockup/jp08-grid-up.png`): tidier on its own, but the bank and number buttons jump
   when you switch devices.
2. **JP-08 readout digits** (REFERENCES 13.1). Designed as two digits, `71` for `7-1`, as
   Roland's specification and photos show. If your unit shows a dash, it becomes three
   cells (`ledCells` 3, one token).
3. **JP-08 readout colour** (REFERENCES 13.2). Designed orange-red like Roland's photos
   of the JP-08; plain red is one token.
4. **JP-08 button colours.** Recommended: the JUPITER-8's function colours (blue views,
   red AUTO SEND, yellow PREV / NEXT / SYSTEM) around white patch buttons. Alternative:
   everything white, calmer and less JUPITER.
5. **Display choice.** Recommended: the seven-segment readout with the name and context
   in panel lettering, as designed. The alternative the brief allows, a two-line dot
   matrix in the readout's colour, would read as a D-05 in disguise.
6. **Context strings.** None needs shortening (4.2); please confirm the wording, in
   particular `FLIP TO 106, THEN CTRL+E` and `FLIPPED TO 60? CTRL+E`.
7. **Engine caption.** `The app's belief about the 60/106 switch.` / `Ctrl+E changes the
   belief, never the unit.` Shorter alternative: drop the caption and keep the tooltip.
8. **Ghost segments** in the readout (`ledShowGhost`, default on), as the LCD's ghost
   dots.

### 13.4 For the owner: nameplate lettering (amendment, 2026-10-26)

Look at `docs/design/mockup/nameplates@2x.png` (the four nameplates) and the three
re-rendered skin mockups.

1. **Faces.** Recommended as designed (3.1): JP-08 in Archivo SemiBold, open tracking,
   red-orange; both JUNO skins in Archivo Bold, tight, panel white. The D-05 stays Inter
   Medium (decision 26).
2. **The two JUNO skins share one face.** The JUNO-60 and the JUNO-106 print their
   descriptor line in the same bold grotesque (REFERENCES 14.2), so the evidence gives no
   second face; the sub-skins already differ in panel and band colours. If you want the
   nameplates themselves to differ, the cheapest honest option is colour (for example
   the 60 keeps blue `labelAccent`), not another family.
3. **JUNO nameplate colour.** Designed panel white, as the descriptor line. Alternative:
   keep the approved blue (60) and pale blue (106) of G7 (one field per skin:
   `colour: labelAccent`).
4. **A wider JP-08 nameplate?** Archivo Expanded SemiBold (same licence, one file swap)
   gives a broader, more emphatic stance, but the JUPITER-8's own panel lettering is
   normal width (REFERENCES 14.1), so the width would come from nowhere on the panel. Not
   recommended; say if you want to see it rendered.
