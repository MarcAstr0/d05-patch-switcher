# Design brief, version 2: three panels in one window

For the designer agent (Opus). Builds on `docs/DESIGN-BRIEF.md` and the existing
`docs/design/DESIGN.md`, which stays the D-05 skin and must not change in look. Output
goes to `docs/design/` and `assets/`; the builder implements from it, so precision
beats prose. Functional detail: `docs/SPEC-2.md` §4 (FR-12, FR-16 to FR-20) and §6.

## What changes

The window now drives one of three synths at a time and looks like that synth's panel:

| Device | Skin | Modelled on |
|---|---|---|
| D-05 | `d05` | the 1987 D-50 panel, as designed; unchanged |
| JP-08 | `jp08` | the 1981 Jupiter-8 panel, which the JP-08 Boutique recreates |
| JU-06A, engine 60 | `ju06a-60` | the 1982 JUNO-60 panel |
| JU-06A, engine 106 | `ju06a-106` | the 1984 JUNO-106 panel |

The owner's words: "the UI switches to match the current synth; for the JP-08 the
colours and look of the classic Jupiter-8, and the same for the JU-06A with the look
and feel of the classic Junos." The two JU-06A sub-skins share one layout and differ in
palette and lettering, so the engine state is visible from across the room, which is
the point: the app cannot read the switch, the user keeps it honest, and the colour is
the reminder.

The layout stays one layout. The display, the grid, the search field, the list, the
views, the status bar and the dialogs are in the same places on every skin; a device
switch is a repaint, not a different app. Skins differ in tokens (colours, type roles,
switches) and in three component choices named below.

## Research first

Record in `docs/design/REFERENCES.md` (extend it) what you verified from photographs
of the Jupiter-8, the JP-08, the JUNO-60, the JUNO-106 and the JU-06A, with sources:

- panel colours: the Jupiter-8's dark panel and its coloured section markings and
  slider caps; the JUNO-60's panel and the colour bands over its sections; the
  JUNO-106's lighter grey with its accents; how the two Boutiques translate them;
- the Boutiques' displays: the JP-08 and JU-06A each have a small LED readout that
  shows `7-1`, `4-5`, `---` (verified on the units 2026-10-26; the JU-06A also shows a
  decimal point when a patch is edited). Verify the digit count, the segment style and
  the colour, and the original synths' patch-number displays for comparison;
- lettering: how section titles and button labels are set on each panel, the PATCH
  NUMBER / BANK row labels, and how `60` / `106` are marked on the JU-06A;
- the shapes of the patch buttons and their LEDs on the two Boutiques.

Where photos disagree or are too small, say so and pick one.

## The three component choices per skin

1. **Display.** The D-05 keeps the 2 × 40 character LCD. For the JP-08 and the JU-06A
   design what the window shows in that slot. The requirement (SPEC-2 FR-12) is the
   same three things: the address, the name, and one context line with the exact
   strings below. The expected answer is an LED-style readout for the address, in the
   unit's style (verify colour and digit count), beside a lettered area for the name
   and the context line, in the panel's typography rather than a dot-matrix font;
   but if a two-line dot-matrix in the model's colour reads better, argue it. Whatever
   you choose must be paintable from a glyph atlas with no per-frame work, like
   `DisplayWidget`. The builder implements a new `LedDisplayWidget` from your spec, so
   give segment geometry, digit pitch, the dash and the dot, the off-segment ghost
   (or none), and the lit colour with its halo if any.
2. **Group bar slot.** The D-05 has 14 group buttons there. The JP-08 has nothing to
   put there: decide what the row becomes (a thinner rule, the nameplate, or nothing
   and the grid moves up). The JU-06A puts the **engine control** there: two buttons,
   `60` and `106`, one lit, in the style of the unit's own `[60/106]` marking, with the
   lit one matching the active sub-skin. It must read as "the app's belief", not as a
   control that changes the synth; a short caption may say so.
3. **Device switcher** (SPEC-2 FR-16). A visible control that lists the enabled devices
   (up to three), marks the current one, shows each one's connection state (the
   `NO MIDI OUTPUT` condition), and switches on click. Place it where the panel
   vocabulary allows (the nameplate area is the natural candidate: the current device's
   name becomes the nameplate and the others sit beside it as unlit tabs). With one
   enabled device it is just the nameplate. Keyboard: `Ctrl+Tab`.

## Context strings (FR-12) that every display must fit

Exact, uppercase, in priority order: `NO MIDI OUTPUT` · `FLIP SWITCH TO 106` /
`FLIP SWITCH TO 60` · `FLIP TO 106, THEN CTRL+E` (and `60`) · `SENT` ·
`FLIPPED TO 60? CTRL+E` (and `106`) · `NO MATCHES` / `1 MATCH` / `<n> MATCHES` · idle
text. On macOS the key reads `CMD+E`. Idle text per skin: the D-05's long group name
as today; `JP-08` on the JP-08; `JU-06A  JUNO-60` / `JU-06A  JUNO-106` on the JU-06A.
If your display has fewer than 24 characters on the context line, propose shorter
strings and list them; the builder uses yours.

The JU-06A has one more display state: no patch on the current engine (after a flip
into an engine with nothing selected, or when the remembered program is unknown). The
unit shows `---`; show the same.

## List rows

The D-05 row has a key-mode tag (W/D/S). On the JP-08 that slot is empty; on the JU-06A
it shows the engine tag `60` or `106`, and rows of the engine that is not active should
read as available but "one flip away" (a dimmer tag, not a dimmer row: the name must
stay searchable-looking). A user-named slot on the JP-08 and JU-06A shows the user name
with the factory name available in the tooltip; design the row for both.

## Settings and dialogs

The Settings dialog gains a device list on the left (three rows, each with an enabled
checkbox and a connection dot) and a page per device on the right (SPEC-2 FR-9),
including the JU-06A's read-only engine information and the `detected` marks and `Use`
button next to the port combos. Dialogs are skinned with the current device's tokens
(they are part of the panel). The Shortcuts sheet gains the three new rows. The About
box lists the three models and the extended trademark line.

## Tokens and assets

- `docs/design/tokens.json` becomes version 2: a `base` set and one override set per
  skin (`d05`, `jp08`, `ju06a-60`, `ju06a-106`), every colour and type role resolvable
  per skin. `d05` resolves to exactly today's values (the generator's `--check` must
  pass unchanged for the D-05 before and after). Document the schema at the top of the
  file as today.
- `docs/design/gen-tokens.py` (the builder updates it to your schema; you specify) emits
  `assets/theme/<skin>.qss` from one template `assets/theme/app.qss.in`, plus the web
  CSS for the D-05 only, unchanged.
- Fonts: OFL only, committed with licences. If the LED readout wants a seven-segment
  face, prefer painting segments from geometry over a font (crisper at 2×, no new
  file); if you specify a font, say why.
- Icon: a new app icon for "Boutique Patch Switcher" (SVG, 256 px PNG, `.icns`/`.ico`),
  not derived from any Roland mark.
- **No Roland trademarks in artwork.** No Roland logo, no Jupiter-8 / JUNO logotypes or
  their stylised lettering, no copying of the Boutiques' silkscreen artwork. Plain-text
  `JP-08`, `JU-06A`, `JUNO-60`, `JUNO-106`, `JUPITER-8` in the UI typeface is fine.
  Colours, proportions and lettering *style* are what you borrow.

## Constraints that still hold

Qt Widgets, QSS plus custom `QPainter` widgets, no QML, no raster chrome without a
documented reason, crisp at 1× and 2×, one 400 ms flash as the only transient, no
animation on a skin switch (it is a repaint), window minimum 800 × 480 and default
960 × 560, the footprint rules in SPEC-2 §5.

## Deliverables

1. `docs/design/REFERENCES.md` extended as above.
2. `docs/design/DESIGN-2.md`: the skin system (how a skin is defined, the token schema),
   the three component choices per skin with dimensions and every state (including
   armed, hint, no-patch, disconnected, and the switcher's states), the list row
   variants, the Settings device list and pages, the Shortcuts and About changes, and
   a per-skin section with its palette and type roles. The D-05 sections of DESIGN.md
   are referenced, not duplicated.
3. `docs/design/tokens.json` version 2, `assets/theme/app.qss.in` (with the builder:
   you own the values and the template's structure, the builder owns the generator).
4. Fonts and icon as above.
5. Mockups from `docs/design/mockup/` (SVG rendered with Inkscape, or HTML through the
   foreman's Playwright tool; never the system Chrome): `mockup-jp08.png`,
   `mockup-ju06a-60.png`, `mockup-ju06a-106.png` (the last one in the armed state with
   the prompt showing), `mockup-devices.png` (the switcher with three devices, one
   disconnected), `mockup-settings-devices.png`. 1×, and 2× if cheap.

## Review passes

After the builder implements, you receive screenshot paths for every state on every
skin at 1× and 2×. Compare against DESIGN-2.md and return numbered deviations, most
visible first, each with the exact fix. Two rounds per milestone.
