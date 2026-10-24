# References: what the D-50 and D-05 panels look like

Task T2.1. This file records what was observed in photographs and official pages,
which source each observation came from, how sure it is, and the design decisions
taken from it. `docs/design/DESIGN.md` (T2.2) turns the decisions into pixels and hex
values; where the two disagree, DESIGN.md wins and must say why.

Research date: 2026-10-24. No reference image is committed to this repository; the
photos are third-party works (licences listed below) and are only linked.

## 1. Sources

| ID | Source | What it is | Used for |
|---|---|---|---|
| S1 | Roland, D-05 product page, <https://www.roland.com/global/products/d-05/> and specifications, <https://www.roland.com/global/products/d-05/specifications/> | Official text | D-05 display spec, control names |
| S2 | Roland, D-05 front image, <https://static.roland.com/assets/images/products/gallery/d-05_gal.jpg> (1680 x 713 px, from the S1 gallery) | Official top-down product image, evenly lit; may be retouched | D-05 display, colours, buttons, LEDs, labels, rules |
| S3 | Wikimedia Commons, "Roland D-50 Front.JPG", <https://commons.wikimedia.org/wiki/File:Roland_D-50_Front.JPG> (public domain, user Iixorbiusii, Nikon D40, 2010, 1805 x 1200 px) | Whole original D-50, top-down, display not visibly lit | D-50 layout, label colours, rule colours, LED position |
| S4 | Wikimedia Commons, "P1010903 (49964950062).jpg", <https://commons.wikimedia.org/wiki/File:P1010903_(49964950062).jpg> (CC BY-SA 2.0, from Flickr, described "Roland D-50 EX Musitronics"; read at 1280 px) | Frontal close-up of the left half of the panel with the display lit | D-50 display geometry and colour, buttons, labels, LEDs, rules |
| S5 | Wikimedia Commons, "P1010909 (49964163478).jpg", <https://commons.wikimedia.org/wiki/File:P1010909_(49964163478).jpg> (same photographer and unit as S4) | Close-up of the display showing a 40-character boot message | Column count, dot matrix, lowercase shapes |
| S6 | Wikimedia Commons, Category:Roland D-50, <https://commons.wikimedia.org/wiki/Category:Roland_D-50> | Index | Finding S3 to S5; other photos in the set (rear, angles) gave nothing new |
| S7 | Lit-display D-50 photos from Reverb listings embedded on <https://soundprogramming.net/synthesizers/roland/roland-d-50>, read at 400 x 400 px: [a](https://rvb-img.reverb.com/i/s--DL9gGS1b--/quality=medium-low,height=400,width=400,fit=cover,gravity=bottom/addb55b4-f59f-4eb9-8ffa-632dca2447b1.jpg), [b](https://rvb-img.reverb.com/i/s---I7esqM8--/quality=medium-low,height=400,width=400,fit=cover,gravity=bottom/9c4d0579-aea8-4f81-bda5-13498b06b4c9.jpg), [c](https://rvb-img.reverb.com/i/s--NKNbw1eY--/quality=medium-low,height=400,width=400,fit=cover,gravity=bottom/bcduwwsoqyjkeqzlo2qt.jpg), [d](https://rvb-img.reverb.com/i/s--hWaexGJh--/quality=medium-low,height=400,width=400,fit=cover,gravity=bottom/ioo7xamqkod489f9l5jx.jpg), [e](https://rvb-img.reverb.com/i/s--yXJ-1HwT--/quality=medium-low,height=400,width=400,fit=cover,gravity=bottom/atrto74as5vv20qaydky.jpg) | Used-gear listing photos; marketplace links may expire | Backlight colour across several units |
| S8 | Roland support, "D-50 Technical Specifications", <https://support.roland.com/hc/en-us/articles/201966399-D-50-Technical-Specifications>; Sweetwater mirror <https://www.sweetwater.com/sweetcare/articles/roland-d-50-technical-specifications/> | Official spec sheet | Display spec. Both pages returned HTTP 403 to the fetch tool; the wording below comes from the search engine's extract of them, not from reading the page |
| S9 | Vintage Synth Explorer, <https://www.vintagesynth.com/roland/d-50>, and its D-550 image <https://www.vintagesynth.com/sites/default/files/2017-05/roland_d550_lg.jpg> | Encyclopedia page | Nothing about the panel in the text; D-50 image only 600 x 169 px, too small to read. D-550 image shows the rack's display unlit. Looked at, not relied on |

Colour samples were taken from the downloaded images with Python/PIL: the median of a
small patch for surfaces, and the mean of the brightest 3 to 20 % of pixels for thin
lettering and lit dots. Cameras, JPEG and retouching shift colour, so the numbers are
evidence of hue and relative lightness, not of the paint. Hue is the most stable
quantity across sources and is quoted where it agrees.

## 2. The display

### 2.1 Character grid

| | D-50 | D-05 |
|---|---|---|
| Grid | **2 lines x 40 characters**. S8 (via search extract): "2 x 40 LCD backlit". Verified independently in S5: the boot message `*** Expanded Linear Synthesizer D-50 ***` is exactly 40 characters and fills one line edge to edge; in S4 the right-hand text `MB Init1-2` ends on column 40 when column 1 is the first character of `Y-11` | **2 lines x 16 characters**. S1 specifications: "16 characters 2 line LCD". Consistent with S2: the 16 column pitches fit the lit area |
| Cell | 5 x 7 dot matrix, verified in S4 and S5 at 1280 px: individual dots and the gaps between them are visible | 5 x 7 dot matrix, verified in S2: dots visible in `DUAL` |
| Lowercase | Drawn inside the 7-row cell with no descenders: `p` in "Expanded" and `y` in "Synthesizer" sit on the baseline (S5) | Same: no descenders in "Fantasia" (no descending letters to test; unverified) |
| Zero | `0` in `2020` appears to carry a diagonal stroke (S4). Low confidence at this resolution | Not shown |
| Controller | The look matches the common HD44780-style character ROM. **Unverified**: no source names the D-50's LCD module or controller | Unverified |

Measured geometry (pixels in the source image, +/- 1 px):

| | D-50 (S4) | D-05 (S2) |
|---|---|---|
| Glyph width (5 dots) | 13 | 17 |
| Glyph height (7 dots) | 21 | 27 |
| Character pitch | 15 | 20 |
| Line pitch | 24 | 33 |
| Horizontal dot pitch | 2.6 | 3.4 |
| Vertical dot pitch | 3.0 | 3.9 |
| Horizontal / vertical dot pitch | 0.87 | 0.88 |
| Gap between characters, in horizontal dot pitches | about 0.8 | about 0.9 |
| Gap between lines, in vertical dot pitches | about 1 | about 1.5 |

Both units have slightly tall cells (dots about 13 % narrower than tall in pitch). The
D-05's lettering is set in that same 5 x 7 vocabulary, only fewer and larger cells.

### 2.2 What the display shows

- D-50 (S3, S4, S7a): line 1 is the patch address and name at the left, then
  `U: <upper tone name>` at the right; line 2 is the key mode at the left
  (`DUAL`), split point (`SP C4`), balance (`Bal 63`), then `L: <lower tone name>` at
  the right. Address form on the original unit in S3: `I-37` (letter, hyphen, bank
  digit, number digit). S4 shows `Y-11`, a bank letter that is not part of the stock
  D-50; that unit is modified (see 2.4).
- D-05 (S2): line 1 `U1-11:Fantasia` (the separators between `U1`, `11` and the name
  are drawn narrower than a full cell, which suggests custom glyphs; mechanism
  unverified); line 2 `DUAL` and `SP C4`.
- Note against SPEC FR-12: the real units put the key mode on line 2 at the left; the
  spec puts it on line 1 at the right. Not changed here (behaviour is the spec's call),
  listed as an owner question in section 6.

### 2.3 Colour

| | D-50 | D-05 |
|---|---|---|
| Lit dots | Yellow-green. S4 bright-dot mean `#5A771B`, S5 `#516726` (both underexposed), S7a `#525C2A`. Hue 71 to 80 degrees | Yellow-green. S2 bright-dot mean `#8CC509`, brightest `#A1D624`. Hue 77 to 78 degrees |
| Background of the lit area | Near-black with a faint green cast: S4 `#0A140C` | Near-black with a faint green cast: S2 `#020E02` to `#041700` |
| Unlit dots | Not visible in any source at the available resolution | Not visible in S2 |
| Mode | Bright characters on a dark field (negative-type look), seen in S4, S5, S7a, S7b, S7c | Same, S2 |

Backlight variation across units (S7): of five lit displays, three are yellow-green
(S7a, S7b, S7c), one is blue (S7d) and one is pale amber/white (S7e). The original
backlight colour is therefore **verified only as "the majority of surviving units"**.
The blue and amber units are probably aftermarket backlight replacements: the
electroluminescent backlights of this era wear out and are commonly replaced (general
forum discussion, not D-50 specific: <https://gearspace.com/threads/lcds-and-lcd-backlights.983393/>);
**unverified for these specific units**, and the D-50's original backlight type is
itself unverified. The S4/S5 unit is a Musitronics-modified D-50 and may also have a replaced
backlight; its colour agrees with S7a to S7c and S2 anyway.

### 2.4 Bezel and window

- D-50 (S4): the display sits in a recessed rectangular black window cut into the
  panel, with a thin slightly lighter rim at the top edge of the cut-out. The lit area
  is inset with generous black margins: in S4 the window is about 698 x 120 px and the
  lit area about 612 x 59 px, so margins of about 3 character pitches left and right
  and about 1.3 line pitches above and below. The window corners are square. No
  silkscreen label on or around the window.
- D-05 (S2): same arrangement scaled down: black window (about 407 x 130 px in S2)
  with a hairline lighter frame, square corners, lit area about 325 x 82 px, margins
  of about 2 character pitches left and right.
- Window and bezel measurements are by eye from the image, +/- 5 px.

## 3. Panel colours

| Element | D-50 | D-05 |
|---|---|---|
| Panel | Black with a very slight cool or neutral cast. S3 (flash-lit) median `#26272B`; S4 (underexposed) `#050601`. The true value cannot be read from photos | Charcoal black. S2 `#212528` (upper area), `#262827` (lower area) |
| Panel structure | The panel has a horizontal step: the PATCH BANK / NUMBER row and the bottom-left buttons sit on a band that reads lighter than the upper panel in S4 (`#2A2A28` against `#050601` in the same photo) | Same idea: a separate lower band holding PATCH BANK / NUMBER, slightly different shade (S2) |
| Section and button labels | White, slightly warm. S4 brightest `#FEFDF7` (overexposed) | Off-white. S2 `#9C9C90` (anti-aliased small text, so darker than the ink) |
| Accent lettering | Red: `DATA TRANSFER`, `WRITE`, `SHIFT`, `ENTER` and the digits on the numeric keys (S3) | Red: `SEQUENCER`, `WRITE`, `SHIFT`, `ENTER` and the digits on the numeric keys (S2), S2 samples `#864146`, `#822C30`, hue 355 to 357 degrees |
| Blue | The `D-50` logotype and the `PATCH BANK` / `PATCH NUMBER` rule lines (S3: rule `#98ACD5`, logotype `#4D8ED1`, hue 210 to 220 degrees). In S4 the rule is overexposed and reads near-white | `PATCH BANK` / `PATCH NUMBER` titles, their 1 to 8 numerals and their rules, `FUNCTION`, `PRESET/USER`, the logotype (S2: title `#819CC9`, rule `#6E8EBA`, `FUNCTION` `#7FA1CC`, hue 213 to 217 degrees) |
| Secondary rules and text | Cream/white: `PARTIAL SELECT`, `PARTIAL MUTE`, the brackets around `PARTIAL BALANCE` and `EDIT` (S3, S4: `#EEF0DE`) | Pale tan: `PARTIAL SELECT`, `PARTIAL MUTE` and the structure/envelope chart (S2: `#ABAB9D`, hue 60 to 66 degrees) |
| Lit LED | Red. Core blown out to white in S4; edge `#A92942` | Red to red-orange. S2 lit bank button core `#D82938`, the F1 to F2 cursor buttons `#DF3D24` |
| Unlit LED | Dark red-brown slot, S4 `#170909` | Dark lens, no visible red |

## 4. Typography

- **Face.** Both panels use an upright neo-grotesque sans, all capitals, medium to
  regular weight, normal to slightly open tracking (S3, S4, S2). The exact typeface is
  **not identified** and is not needed; the logotypes (`D-50`, `D-05`, "LINEAR
  SYNTHESIZER") are separate wide geometric drawn lettering and are trademarked trade
  dress, so they are out of bounds.
- **Size.** Button labels are small relative to buttons: in S4 cap height is about a
  quarter of the button height.
- **Two-line labels** stack centred (`TONE` / `DETUNE`, `L-TONE` / `EDIT`) above the
  button (S4).
- **Section titles set into the rule.** `PATCH BANK` and `PATCH NUMBER` sit centred in a
  gap in a long horizontal hairline that spans the eight buttons, above the numerals
  (S3, S4, S2). `PARTIAL SELECT` / `PARTIAL MUTE` sit the same way in a rule below the
  buttons, with `1 LOWER 2   1 UPPER 2` between the buttons and that rule (S4, S2).
- **Numerals** `1` to `8` are centred above each bank/number button, in white on the
  D-50 (S4) and in blue on the D-05 (S2).
- **Brackets.** On the D-50, `PARTIAL BALANCE` and `EDIT` are joined to their buttons by
  thin right-angled brackets in the label colour (S4).
- **Arrows.** Cursor and joystick arrows are solid, flat, light grey/cream wedges
  (S4: `#BDB9A0`).

## 5. Buttons, LEDs and rules

- **D-50 buttons (S3, S4).** Flat rectangular caps, wide (about 3.5 : 1 width to height
  in S4), sharp or barely rounded corners, warm mid-grey `#43443E` in S4, a faint
  highlight on the top edge, set in a slot slightly darker than the panel. **No LED in
  any patch bank or number button**: the D-50 shows the current patch only on the
  display. Separate small rectangular red LEDs sit beside `UPPER`, `LOWER`, `VALUE`,
  `LOCAL` (S4: `VALUE` lit).
- **D-05 buttons (S2).** Smaller rectangles with rounded corners, dark grey face
  (`#424546`) in a darker recessed surround, with a small window in the face through
  which the LED shows. The bank and number buttons are lit buttons: S1 lists "PATCH
  BANK buttons and indicators (1-8)" and "PATCH NUMBER buttons and indicators (1-8)",
  and S2 shows bank 1 and number 1 lit red. One bank and one number are lit at a time
  in S2.
- **Rules.** Hairlines, one stroke weight throughout, no boxes around sections except
  the silkscreened charts at the right (D-50) or bottom (D-05). Groups are made by
  rules with titles set in, by brackets, and by empty black space.
- **Restraint.** Most of both panels is empty black. Colour is used in exactly three
  roles: white or off-white for labels, blue for the patch selection section and
  logotype, red for "commit" actions and the numeric digits. The LCD green is the only
  green.

## 6. Decisions taken from the references

Each decision is carried into DESIGN.md (T2.2), which sets the exact pixel and hex
values.

1. **Display grid: 2 x 40, as the D-50** (S5, S8), not the D-05's 2 x 16 (S1). It
   matches SPEC FR-12 and leaves room for the full 18-character name, address and key
   mode on one line.
2. **5 x 7 dot matrix drawn from a glyph table**, not a font. The real displays are dot
   matrices with descender-less lowercase (S5); a bitmap table reproduces that exactly,
   lets the builder render the glyph atlas once per size (footprint rule), and allows
   unlit dots if wanted. DESIGN.md will carry the table (ASCII 0x20 to 0x7E plus the
   characters the app needs) in an HD44780-like style drawn for this project, with
   descender-less lowercase and a plain (unslashed) zero, since the slashed zero is not
   verified.
3. **Dot geometry: square dot pitch** in the app, with a one-dot gap between characters
   and a one-dot gap between lines. The real cells are about 13 % taller than wide; at
   app sizes that difference costs integer-pixel crispness at 1x, so it is dropped on
   purpose. Dots are drawn as squares with a visible gap, as in S4 and S5.
4. **Display colour: yellow-green lit dots, hue about 78 degrees, on a near-black field
   with a faint green cast.** Every source that agrees (S2, S4, S5, S7a to S7c) sits at
   hue 71 to 80 degrees. The D-05's brighter rendering (S2, `#8CC509` to `#A1D624`) is
   the target lightness, because the S4/S5 photos are underexposed. Not blue, not
   amber (S7d, S7e are treated as replaced backlights).
5. **Unlit (ghost) dots: barely visible**, a step above the field colour. No source
   shows them, so they are a stylisation that makes the grid legible on a laptop screen;
   DESIGN.md keeps their contrast low enough that a screenshot reads like S2. Owner may
   veto (question 2).
6. **Bezel: recessed square-cornered black window with a hairline lighter rim at the top
   edge, lit area inset by generous black margins** (S4, S2). No label on the bezel.
7. **Panel black: near-neutral charcoal, slightly cool,** in the range of S2 `#212528`;
   a lower band one step lighter holds the group bar and the bank/number grid, as both
   panels put PATCH BANK / NUMBER on a band (S2, S4).
8. **Three-role palette:** off-white labels; blue (hue about 215 degrees) for the patch
   selection section (titles set into rules, numerals) as on the D-05 (S2) and the D-50
   rules (S3); red (hue about 355 degrees) only for lit LEDs and the one commit-like
   accent (the `SENT` state and the auto-send toggle when on). Pale tan/cream for
   secondary rules. No other colours.
9. **Bank and number buttons with LEDs, D-05 style** (S1, S2), because the app must show
   the current bank and number (SPEC FR-4) and the D-50 has no such indicators. Flat
   rectangular faces with a small red LED window, one bank and one number lit.
10. **Section titles set into hairline rules**: `PATCH BANK` over the bank row and
    `PATCH NUMBER` over the number row, numerals 1 to 8 above each button, all as in
    S2 and S4. Same treatment for any other section title in the app.
11. **Typography: an OFL neo-grotesque, all capitals, regular or medium weight, slightly
    open tracking** for labels; the exact face is chosen in T2.3. No attempt to copy the
    logotype lettering. "D-05" and "Roland" appear only as plain text in the label face.
12. **No animation, no glow.** The real lit LED in S2 has a bloom; the app renders a flat
    LED with at most a one-pixel lighter core, because blur is out of budget.

## 7. Open questions for the owner

1. **Key mode position.** The real D-50 and D-05 show the key mode (`DUAL` in every
   photo looked at) at the left of line 2; SPEC FR-12 puts it at the right of line 1 and uses
   line 2 for status. Keep the spec, or move key mode to line 2 when there is no status
   to show?
2. **Ghost dots.** Show unlit dots very faintly (decision 5), or a pure black field with
   only lit dots, as the photos show?
3. **Backlight colour.** Yellow-green is what most units and the D-05 show. If your own
   memory of a D-50, or your D-05 in the room, looks different, say so before the
   mockups.
4. **Your D-05.** A phone photo of your D-05's display lit, straight on, would let the
   colour be matched to the unit you actually play next to the laptop. Optional; put it
   in `assets/local/` (git-ignored) if you send one.
