# Web version

A browser app that does exactly what the desktop app does, over the Web MIDI API.
Proposed 2026-10-24 by the owner after noticing that the M-VAVE FM-1's web configurator
drives the pedal from Chrome on the Mac with no install. Status: **specified, not
scheduled**; see §9 for when it should start relative to `docs/PLAN.md`.

## 1. Why build it, and what it cannot do

What a web version adds over the desktop app:

- **No install, no Gatekeeper.** A URL. For anyone else who owns a D-05, this is the
  version they would actually use; nobody downloads an unsigned `.dmg` from a stranger.
- **A phone as a patch pad.** Chrome on Android has Web MIDI, and a USB OTG cable to the
  synth is enough. The owner already drives a Quad Cortex from a Redmi 10A over OTG; the
  same phone can sit on the D-05 with a big touch grid.
- **Hot-plug done right.** Web MIDI has `onstatechange`; no polling.
- **Zero packaging work.** No AppImage, no universal binary, no `windeployqt`.

What it cannot do:

- **Safari, and therefore every browser on iOS**, has no Web MIDI. iPhone and iPad are
  out unless through a third-party "Web MIDI browser" app. On the Mac it means Chrome,
  Edge, Brave or Firefox, not Safari.
- **Footprint.** A browser tab is tens to a few hundred MB regardless of how small the
  page is. Idle CPU is still zero, and the page itself is tiny, but the desktop app's
  memory budget is not a target here.
- **It is a second renderer of the same spec.** The core logic is a few hundred lines
  and ports in an afternoon; the UI is the bulk and has to be written again. The design
  is shared through the same tokens, so it is the same panel, not a second design.

## 2. Browser facts (verify at build time; browsers move)

| Browser | Web MIDI | Notes |
|---|---|---|
| Chrome, Edge, Brave, Opera (desktop) | yes | one permission prompt per origin, even without SysEx |
| Chrome on Android | yes | USB OTG; the device must be class-compliant (Android has no vendor drivers) |
| Firefox ≥ 108 (desktop) | yes | asks to install a per-site permission add-on the first time |
| Safari (macOS, iOS, iPadOS), any iOS browser | no | WebKit has not shipped it |

Requirements that follow: the page must be served over **HTTPS** (or `localhost`),
the app must **feature-detect** `navigator.requestMIDIAccess` and explain the matrix
above when it is absent, and it must handle a denied permission as a normal state, not
an error.

## 3. Scope

Feature parity with `docs/SPEC.md` §4 **FR-1 to FR-14**, with these substitutions:

| Desktop | Web |
|---|---|
| FR-10 poll every 2 s while disconnected | `MIDIAccess.onstatechange`; reconnect by port name when it reappears |
| FR-15 command line, `--screenshot` | none; Playwright screenshots in tests instead |
| `QSettings` + `library.json` on disk | `localStorage` for settings, `IndexedDB` or `localStorage` for the library, **same JSON schema as `library.json` v1**, with Import and Export buttons so favourites and user names move between desktop and web |
| Window geometry | responsive layout: desktop ≥ 800 px wide, phone portrait and landscape (§6) |
| About box | an About panel plus the browser-support matrix |

Additions:

- **PWA**: web manifest and a service worker that caches the app shell so it loads
  offline and installs to the Android home screen / the macOS dock. No network after
  the first load; no analytics, no cookies, no backend.
- **Touch targets** sized for a phone on the grid, the group strip and the favourites row.

## 4. Architecture

Lives in this repository under `web/`, so it shares `data/d05-presets.tsv`, the spec,
the design tokens and the test vectors. Separate CI job, separate deploy.

```
web/
  index.html
  src/
    app.ts                 wiring, keyboard map (same table as SPEC FR-13), views
    core/address.ts        PatchAddress: parse/format, catalogue index, MIDI triplet
    core/catalog.ts        896 entries from the TSV (imported at build time), user-name overlay
    core/search.ts         SPEC FR-3 grammar → predicate + match ranges
    core/library.ts        favourites, recents, user names; storage; import/export
    midi/webmidi.ts        requestMIDIAccess, port list, open by name, send triplet,
                           onmidimessage → follow mode with 300 ms echo suppression,
                           onstatechange → reconnect
    midi/fake.ts           same interface, records bytes; used by tests and by a ?fake=1 query flag
    ui/display.ts          the LCD on a <canvas>: glyph atlas drawn once per size,
                           redraw only when text changes
    ui/grid.ts             14 group buttons + 8 bank + 8 number with LEDs; two-digit entry
    ui/list.ts             the patch list: DOM rows, rendered on demand, star, mode tag, highlight
    ui/settings.ts         port picker, channel, auto-send, follow, test, import/export
    ui/theme.css           CSS custom properties generated from the design tokens
  public/fonts/            the same OFL fonts as assets/fonts/
  public/manifest.webmanifest, sw.js
  tests/                   Vitest (core) + Playwright (UI with the fake MIDI shim)
  vite.config.ts, tsconfig.json, package.json
```

Rules:

- **Vite + TypeScript, no UI framework.** The UI is a display, a grid, a list and a
  dialog; a framework buys nothing here. If the builder makes a case for Preact, the
  cap still applies.
- **Budget:** ≤ 60 KB gzipped for all JS and CSS excluding fonts; no runtime
  dependencies in `package.json` (dev dependencies only). Enforced by a size check in CI.
- **Same semantics as the desktop core**, proven by shared test vectors (§7), not by
  reading the C++.
- **Tokens, not copies:** `ui/theme.css` is generated from the design document's token
  list (a small script reads the token block at the top of `assets/theme/app.qss` or a
  `docs/design/tokens.json` the designer adds) so that a colour change happens once.

## 5. MIDI layer

```ts
const access = await navigator.requestMIDIAccess({ sysex: false });
// ports: access.outputs / access.inputs (Maps keyed by an opaque, browser-specific id)
output.send([0xB0 | ch, 0x00, 87]);
output.send([0xB0 | ch, 0x20, lsb]);
output.send([0xC0 | ch, program]);
```

- Persist the chosen port by `name` (plus `manufacturer` when present), never by `id`:
  ids are opaque and not promised to survive across browsers or reboots.
- `onstatechange`: when the saved name connects, open it and clear the disconnected
  state; when it disconnects, show `NO MIDI OUTPUT` exactly like the desktop display.
- Permission states: `granted` → proceed; `prompt` → the page shows a single "Connect
  MIDI" button (browsers require a user gesture for the prompt in practice); `denied` →
  explain how to re-enable it in the site settings, with the browser named.
- Follow mode: `input.onmidimessage` parses CC 0 / CC 32 / Program Change on the channel,
  with the desktop's 300 ms echo suppression.
- No SysEx request, ever, in this app: it changes the permission prompt and is not needed.

## 6. Phone layout

Portrait: display on top (two lines, full width), the group strip scrolling
horizontally under it, the 8 + 8 grid as large buttons filling the middle, favourites
as a row of chips, search collapsed into a button that expands into a full-screen list.
Landscape: display and grid side by side, list on the right. Touch targets ≥ 44 px.
The designer specifies this in a `docs/design/DESIGN-WEB.md` addendum from the same
tokens and typography; the LCD, buttons and LEDs are the same drawings in CSS/canvas.

## 7. Shared test vectors

`data/test-vectors.json`, hand-written from the spec, consumed by both the C++ tests and
Vitest, so that both implementations are proven against the same cases:

```json
{
  "addresses": [
    { "text": "P3-45", "group": 10, "bank": 4, "number": 5, "index": 668, "msb": 87, "lsb": 10, "program": 28 }
  ],
  "invalid": ["P1-19", "P1-90", "P7-11", "U9-11", "X1-11", "P1-1"],
  "search": [
    { "query": "p345",        "expect": ["P3-45"] },
    { "query": "3",           "expectContains": ["P3-66"], "expectNot": ["P3-11"] },
    { "query": "brass p1",    "expectContains": ["P1-33", "P1-35"] }
  ],
  "formats": [ { "input": " p345 ", "canonical": "P3-45" } ]
}
```

Adding the file and a C++ test that reads it is a small task for the desktop builder
(it slots into M1 if the web work starts early, otherwise into M5).

## 8. Hosting and deployment

**GitHub Pages from this repository**: a `web` job in `.github/workflows/build.yml`
runs `npm ci && npm test && npm run build` on every push (path-filtered to `web/**`,
`data/**`) and deploys `web/dist` to Pages on pushes to `main`. URL:
`https://marcastr0.github.io/d05-patch-switcher/`. HTTPS comes with it, which Web MIDI
requires. Netlify is the alternative if a custom domain is wanted; either way there is
no server and nothing to operate.

## 9. Plan

Start **after desktop M3** (so `docs/design/DESIGN.md`, the tokens and the fonts exist)
and run in a git worktree in parallel with M4/M5, or after v1.0.0 if the owner prefers
one thing at a time.

- **W0 Setup** (foreman): add `.claude/agents/web-builder.md` from the appendix; add the
  `web` CI job; create `web/` with Vite + TypeScript + Vitest + Playwright and a green
  empty test; add `data/test-vectors.json` and the C++ test that reads it.
- **W1 Core** (web-builder): `address`, `catalog`, `search`, `library` passing the shared
  vectors and the spec's own cases; import/export round-trip with a `library.json`
  written by the desktop app.
- **W2 Design addendum** (designer): `DESIGN-WEB.md` with the phone layouts, token
  generation into `theme.css`, the fonts served from `web/public`.
- **W3 UI** (web-builder): display on canvas, grid, list, settings, keyboard map, views,
  PWA. Playwright screenshots at desktop and phone sizes reviewed by the designer.
- **W4 MIDI and deploy** (web-builder): Web MIDI layer, permission flow, hot-plug, follow
  mode; Pages deployment live; size budget check in CI.
- **Gates (owner):** GW1 Chrome on the MacBook against the rig; GW2 Chrome on the Redmi
  10A over OTG, straight into the D-05 (generic USB mode) or through the Hapax; GW3 one
  pass in Firefox to confirm the permission add-on flow reads sensibly.

## Appendix: `web-builder` agent (to be added at W0)

```markdown
---
name: web-builder
description: Implements one task of the web version (docs/WEB.md) in TypeScript with Vite, Vitest and Playwright, and verifies tests and the size budget before reporting. Use for every web/ coding task.
model: sonnet
tools: Read, Edit, Write, Bash, Grep, Glob
---

You are the web builder on d05-patch-switcher. Read CLAUDE.md, docs/WEB.md, the task's
section of docs/PLAN.md, and the parts of docs/SPEC.md the task cites. Behaviour is
defined by docs/SPEC.md; docs/WEB.md only says what differs in the browser.

Rules: TypeScript strict; Vite; no UI framework and no runtime dependencies without the
foreman's written approval; ≤ 60 KB gzipped JS+CSS; the core must pass
data/test-vectors.json; never request SysEx; persist ports by name, not id; the display
redraws only when its text changes; touch targets ≥ 44 px on phone layouts; no network
calls after load; no analytics. Run `npm test` and `npm run build` and report their
output. Do not commit.
```
