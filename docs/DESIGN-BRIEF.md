# Design brief

For the designer agent (Opus). Output goes to `docs/design/` and `assets/`; the builder
implements from it, so precision beats prose.

## What the app is

A single-window desktop tool that selects patches on a Roland Boutique D-05 over MIDI:
a search field, a list of patches, a bank/number button grid that mirrors the synth's
panel, a two-line display that mirrors the synth's display, favourites and recents, and
a Settings dialog. Functional detail: `docs/SPEC.md` §4 (FR-3, FR-4, FR-9, FR-12,
FR-13, FR-14). The owner uses it live while playing, often one-handed, often at arm's
length, on a 16-inch laptop next to the synth.

## The look the owner wants

The front panel of the 1987 Roland D-50, the synth the D-05 recreates: matte black,
late-eighties Roland industrial design, a green character display in a bezel, white and
accent-coloured silkscreen typography, thin rule lines grouping sections, membrane
buttons with small LEDs. The app should feel like a piece of that panel, not like a
modern dark-mode utility with a retro font pasted on. Restraint is the point: the real
panel is mostly empty black with precise lettering.

Research before deciding. Look at photographs of the D-50 (and the D-05, which borrows
the same vocabulary) and record in `docs/design/REFERENCES.md` what you verified:

- the display: lines × characters, colour of the backlight and of the segments, bezel
  shape, whether characters are 5×7 dot matrix;
- panel colours: the exact black, the colour(s) used for section labels and for the
  accent lettering, the colour of the lit LEDs;
- typography: the weight and spacing of the panel labels, how section titles are set,
  how the "PATCH BANK" and "NUMBER" rows are labelled on the D-05;
- button shapes and the rule lines around them.

Cite each source. Where photos disagree or are too small to tell, say so and pick one.

## Constraints

- **Qt Widgets**, styled with QSS plus custom `QPainter` widgets (display, grid, list
  rows). No QML. No raster chrome unless you document why drawing it is impractical.
  Crisp at 1× and 2× device pixel ratio.
- **Fonts**: Open Font Licence (or equally permissive) only, committed with licence
  files. You need at least a panel-label face and a display face. A 5×7 dot-matrix-style
  face for the display is welcome if one exists under the OFL; otherwise specify how the
  builder renders dots from a glyph table.
- **No Roland trademarks.** No Roland logo, no Roland-drawn logotypes, no copying of
  the "D-50" or "Boutique" artwork. Plain text "D-05" in the UI typeface is fine.
  Reserve a slot (position and max size) where `logo.png` from the user's data
  directory is shown if present; design what the slot shows when it is absent (the app's
  own wordmark, which you draw).
- **Footprint.** The display repaints only when its text changes and uses a glyph atlas
  rendered once per size. Avoid designs that need animation, blur or per-frame effects.
  One flash (`SENT`, 400 ms) is the only transient.
- **One theme.** Black panel. No light mode.
- **Minimum window** 800 × 480; default 960 × 560; the list grows with the window, the
  display and grid keep their proportions.

## Deliverables

1. `docs/design/REFERENCES.md` as above.
2. `docs/design/DESIGN.md` with, at least:
   - window sizes and the layout grid in pixels at 1×, and the resize rules;
   - colour tokens as hex with names (`panel`, `bezel`, `lcdBack`, `lcdOn`, `lcdGhost`,
     `labelPrimary`, `labelAccent`, `ledOn`, `ledOff`, `buttonFace`, `buttonPressed`,
     `focusRing`, `selection`, …);
   - typography: files, sizes, letter-spacing, case rules for labels;
   - each component with dimensions and every state (normal, hover, pressed, focused,
     current, disabled; connected/disconnected; sent flash; empty search result): the
     display, the group bar (14 groups in two rows: U1–U8, P1–P6), the bank/number grid
     (8 + 8 buttons with LEDs, the current ones lit, two-digit entry feedback), the
     search field, the list row (address, name, key-mode tag W/D/S, star, recent count,
     search-match highlight, user-slot inline editor), the toolbar/filter controls (All,
     Favourites, Recents, auto-send), the status bar (port, channel, MIDI activity dot),
     the Settings dialog (SPEC FR-9), the Shortcuts sheet (F1), the About box;
   - the logo slot and the fallback wordmark;
   - keyboard focus visuals, since the app is keyboard-first.
3. `assets/fonts/…` with licences; `assets/icons/d05-patch-switcher.svg` and a 256 px
   PNG; `assets/theme/app.qss` carrying every QSS-expressible part of the design, with
   the tokens listed as a comment block at the top.
4. `docs/design/mockup-main.png` and `mockup-settings.png`, rendered from a source you
   keep in `docs/design/mockup/`. On the Linux box, SVG renders with Inkscape
   (`inkscape --export-type=png --export-width=1920 in.svg`) and an HTML mockup renders
   through the foreman's Playwright tool. Do not launch the system Google Chrome from a
   script on this machine: it has a fontconfig-cache problem that the owner's desktop
   launcher works around.

## Review passes

After the builder implements, you receive screenshot paths for every state at 1× and
2×. Compare against DESIGN.md and return numbered deviations, most visible first, each
with the exact fix (token, size, or file). Two rounds are budgeted per milestone.
