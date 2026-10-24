# D-05 Patch Switcher: specification

Version 1 target. Owner: Mario Castro Squella. Written 2026-10-24.

## 1. Purpose

The Roland Boutique D-05 holds 896 patches behind a `PATCH BANK` / `NUMBER` button
grid and a two-line display, with no way to search. This app runs on the computer the
D-05 is already cabled to and selects patches over MIDI with a proper finder: type a
few letters, press Enter, the synth switches. It also remembers favourites and what was
used recently.

It is a *selector*, not an editor or a librarian. It never writes to the synth's memory.

### Non-goals for v1

- Editing patch parameters, reading or writing patch data over SysEx.
- Reading user-bank names from the unit (recorded in `docs/MIDI.md` for later).
- Exporting to the Hapax instrument-definition format.
- Any network feature, update checker, telemetry, or account.
- Light theme. The app is the black panel; there is one theme.

## 2. Platforms and stack

| | |
|---|---|
| Language | C++17 |
| Toolkit | Qt 6 Widgets, floor 6.4 (the Linux dev box has 6.4.2; CI uses 6.8). No QML, no Qt Quick |
| MIDI | RtMidi 6.0.0, vendored at configure time via CMake `FetchContent`; ALSA on Linux, CoreMIDI on macOS, WinMM on Windows |
| Build | CMake ≥ 3.22, Ninja or Make, GCC 13 / AppleClang / MSVC 2022 |
| Tests | Qt Test, run by `ctest`, headless (`QT_QPA_PLATFORM=offscreen`) |
| Targets | Linux x86_64 (AppImage), macOS 12+ universal x86_64 + arm64 (.dmg, ad-hoc signed), Windows 10+ x64 (.zip) |
| Licence | MIT |

Rationale: the owner's other desktop tools are C++/Qt 6 Widgets with CMake, a Widgets
app idles at zero CPU and a few tens of MB, and RtMidi is the smallest cross-platform
MIDI layer that is still maintained.

## 3. Domain

### 3.1 Addresses

An address is `(group, bank, number)`: group ∈ {U1…U8, P1…P6} (index 0…13), bank ∈ 1…8,
number ∈ 1…8. Canonical text form `P3-45`; display form on the LCD `P3-45`. 896 addresses.
Catalogue order is group index, then bank, then number: `U1-11 … U8-88, P1-11 … P6-88`.

MIDI triplet: `(msb = 87, lsb = group index, program = (bank−1)·8 + (number−1))`.
See `docs/MIDI.md` for the bytes and the unit's settings.

### 3.2 Patches

| Field | Preset groups (P1–P6) | User groups (U1–U8) |
|---|---|---|
| Name | from `data/d05-presets.tsv`, read-only | from the library file; empty by default, editable |
| Key mode | W / D / S from the table | unknown; shown blank |
| Display name when unnamed | n/a | the address, e.g. `U3-45` |

### 3.3 Library (per-user state)

One JSON file, `library.json`, in `QStandardPaths::AppDataLocation`
(`~/.local/share/marcastr0/d05-patch-switcher/` on Linux,
`~/Library/Application Support/marcastr0/d05-patch-switcher/` on macOS,
`%APPDATA%\marcastr0\d05-patch-switcher\` on Windows), overridable with `--config-dir`.

```json
{
  "version": 1,
  "favorites": ["P1-11", "P2-57"],
  "recents": [{ "address": "P1-11", "count": 12, "last": "2026-10-24T18:02:11Z" }],
  "userNames": { "U1-11": "Warm Pad Mk2" }
}
```

- Favourites: a set, unlimited, shown in catalogue order.
- Recents: the last 50 addresses *sent*, most recent first, with a send count; a
  resend moves the entry to the front and increments the count. Clearing is a menu action.
- Written atomically (`QSaveFile`) after every change, debounced 500 ms. A malformed
  file is renamed to `library.json.broken-<timestamp>` and a fresh one started; the
  user is told once in the status bar.

Settings (ports, channel, toggles, window geometry) use `QSettings` in INI format so
they are readable on every platform.

## 4. Functional requirements

**FR-1 Catalogue.** All 896 addresses are browsable; the 384 preset names come from the
embedded table; user slots show their user-given name or the address.

**FR-2 Select and send.** Selecting a patch sends the three messages on the configured
channel to the configured output. Selection happens on: Enter on a list row, click or
Enter on a bank/number button, prev/next, a favourite/recents row activation, and
`--send`. Highlighting a row with the arrow keys does *not* send unless **auto-send** is
on (toggle in the toolbar and Settings, default off; intended for auditioning).

**FR-3 Search.** A single text field filters the list live. Each whitespace-separated
token is interpreted independently:

| Token looks like | Interpreted as | Examples |
|---|---|---|
| `[UP]<g>` | group filter | `p1`, `U3` |
| `[UP]<g>-<b>` or `[UP]<g><b>` (2 digits) | group + bank | `p3-4`, `P34` |
| `[UP]<g>-<b><n>`, `[UP]<g><b><n>`, `<g>-<b><n>`, `<g><b><n>` (3 digits) | exact address; a missing letter means P | `P3-45`, `p345`, `3-45`, `345` |
| anything else | case- and accent-insensitive substring of the name | `brass`, `e-piano` |

All tokens must match (AND). A bare one- or two-digit token with no letter is text, not
an address (`3` finds *Synth Lead 3*; `45` finds nothing unless a name contains it).
Group digits above 6 with `P` and above 8 with `U` are text, not addresses. Results are
in catalogue order. The match is highlighted in the row.

**FR-4 Views.** One list with three filters: *All*, *Favourites*, *Recents* (most recent
first). The search applies within the active filter. The bank/number grid always shows
the group of the current patch and lights the current bank and number, like the D-05.

**FR-5 Prev / next.** Moves one step in catalogue order across group boundaries
(`P1-88` → `P2-11`), stops at the ends, and sends.

**FR-6 Favourites.** Toggle with a star control on the row, `Ctrl+D`, or Space when the
list has focus. Persisted immediately.

**FR-7 Recents.** Automatic, per FR-3.3. A *Clear recents* action with confirmation.

**FR-8 User-slot names.** `F2` or double-click on a U-group row opens an inline editor;
names are up to 18 characters (the D-05's own limit) of printable text. Persisted.

**FR-9 Settings dialog.** Output port (combo + Refresh), input port (combo with *None*),
channel 1–16, auto-send, follow-the-panel (enabled only when an input port is set),
resend-current-on-connect, a *Test* button that sends the current patch, a connection
status line (`Connected to <port>` / `Port not found` / `Error: <text>`), and a short
reminder that the D-05 needs `Prog.C` ON (see `docs/MIDI.md` about `Bank.S`). OK applies
and persists; Cancel reverts.

**FR-10 Connection handling.** On start the app opens the saved output port by name.
If it is missing or vanishes, the display shows `NO MIDI OUTPUT` on its second line and
the app polls the port list every 2 s until the name reappears, then reconnects and
stops polling. Opening and sending never block the GUI; RtMidi exceptions become status
text, never crashes.

**FR-11 Follow the panel.** With an input port set and the toggle on, incoming
CC 0 / CC 32 / Program Change on the channel move the current patch and the display
without sending, with 300 ms echo suppression (see `docs/MIDI.md`).

**FR-12 Display.** A custom-painted character LCD, two rows, 40 columns, modelled on the
D-50's display (the designer verifies the real geometry). Row 1: address and name, key
mode at the right. Row 2: context, in priority order: `NO MIDI OUTPUT` when
disconnected; `SENT` for 400 ms after a send; `<n> MATCHES` while a search is active;
otherwise the long group name (`PRESET 1: ORIGINAL D-50`). The second line is the only
place status text appears besides the status bar.

**FR-13 Keyboard.** The whole app is usable without a mouse:

| Keys | Action |
|---|---|
| `/`, `Ctrl+F` | focus the search field |
| `Esc` | clear the search; second press returns focus to the list |
| `Up` / `Down`, `Home` / `End` | move the highlight |
| `Enter` | send the highlighted patch |
| `Space` (list focused), `Ctrl+D` | toggle favourite |
| `PgUp` / `PgDn` | previous / next patch (sends) |
| `Ctrl+G` then `1`–`8`, `1`–`8` | focus the grid and enter bank then number, like the panel; sends on the second digit |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | All / Favourites / Recents |
| `F2` | rename the highlighted user slot |
| `Ctrl+Shift+A` | toggle auto-send |
| `Ctrl+,` | Settings |
| `F1` | shortcut sheet |
| `Ctrl+Q` | quit |

**FR-14 Optional logo.** If `<data dir>/logo.png` exists it is shown in the slot the
design reserves, scaled to fit; otherwise the slot shows the app's own wordmark. No
logo file is ever shipped.

**FR-15 Command line.**

```
d05-patch-switcher [options]
  --list-ports              print MIDI inputs and outputs, exit 0
  --port NAME               output port (substring match, first hit); overrides the saved one for this run
  --in-port NAME            input port, same rules
  --channel N               1–16
  --send ADDR               headless: open the port, send ADDR, print the bytes in hex, exit
                            (0 sent, 1 bad address, 2 port not found, 3 MIDI error)
  --fake-midi               use the fake backend: ports "Fake D-05 In/Out", bytes printed to stderr
  --config-dir DIR          where library.json and settings live (tests use a temp dir)
  --screenshot FILE.png     render the main window offscreen and exit
  --screenshot-state S      default | search | favorites | recents | disconnected | settings
```

`--screenshot` exists so agents can look at the UI on a headless machine; it runs with
the fake backend and a canned library unless told otherwise.

## 5. Non-functional requirements

| | Budget | How it is measured |
|---|---|---|
| Idle CPU | 0.0 % over 30 s when connected; one wake per 2 s when disconnected | `scripts/measure-footprint.sh` (`ps -o %cpu`, `/proc/<pid>/stat` ticks) |
| Memory | ≤ 80 MB RSS on Linux X11 at 1× DPR after 1 minute idle; ≤ 120 MB at 2× | same script |
| Startup | ≤ 1 s from launch to first paint on the dev box | script, `--screenshot` timing |
| Binary | ≤ 2 MB without Qt | `ls -l` |
| Repaints | display repaints only on text change; list repaints only on model change | code review |
| Timers | none faster than 2 s; the 2 s poll only while disconnected; the 400 ms `SENT` and 500 ms debounce are single-shot | code review |
| Threads | main + RtMidi's input thread; nothing else | code review |
| Dependencies | Qt6::Widgets, Qt6::Test (tests only), RtMidi | `CMakeLists.txt` |
| HiDPI | crisp at 1× and 2×; no blurry rasters | screenshots at `QT_SCALE_FACTOR=2` |
| Robustness | no crash on any MIDI error, missing port, bad JSON, or missing config dir | tests + review |

## 6. Architecture

```
src/
  main.cpp                 wires Cli → AppSettings → Library → MidiLink → MainWindow
  app/Cli.*                QCommandLineParser; headless paths (--list-ports, --send, --screenshot)
  app/AppSettings.*        QSettings wrapper with typed getters/setters and defaults
  app/Paths.*              data dir, library path, logo path; honours --config-dir
  core/PatchAddress.*      value type; parse/format; catalogueIndex(); midi triplet
  core/PatchCatalog.*      896 entries; preset names from :/data/d05-presets.tsv; user names overlay
  core/SearchQuery.*       FR-3 grammar → predicate; match ranges for highlighting
  core/Library.*           favourites, recents, user names; atomic JSON persistence; signals
  midi/MidiBackend.h       abstract: ports, open/close, send, received(bytes) callback
  midi/RtMidiBackend.*     RtMidi implementation; callback marshalled to the main thread
  midi/FakeMidiBackend.*   records sent bytes; can inject received bytes
  midi/MidiLink.*          QObject façade: current ports, channel, connect/reconnect/poll,
                           sendPatch(address), follow-mode parsing, echo suppression, status
  ui/Theme.*               colour tokens, fonts, loads assets/theme/app.qss
  ui/DisplayWidget.*       the LCD (FR-12)
  ui/GroupBar.*            14 group buttons
  ui/BankGrid.*            8 bank + 8 number buttons with LEDs; two-digit entry
  ui/PatchListModel.*      QAbstractListModel over the catalogue + filter + search
  ui/PatchDelegate.*       row painting: address, name, mode, star, recent count, highlight
  ui/MainWindow.*          layout, actions, shortcuts, status bar
  ui/SettingsDialog.*      FR-9
  ui/ShortcutsDialog.*     F1
```

CMake splits this into `d05core` (core + midi + app, no Widgets dependency beyond
QtCore/QtGui), `d05ui` (ui), and the executable, so tests link only what they test.

Data flow: user action → `MainWindow` → `MidiLink::sendPatch(addr)` → backend →
`MidiLink::sent(addr)` → `Library::recordSend`, `DisplayWidget::flashSent`. Incoming
bytes → backend callback (RtMidi thread) → queued signal → `MidiLink` parser →
`currentChanged(addr)` → UI. The UI never calls the backend directly.

## 7. Testing strategy

| Layer | How |
|---|---|
| `core/` | Qt Test: address parse/format round-trip over all 896; the formula at the group, bank and number boundaries; search grammar table from FR-3; library JSON round-trip, corruption handling, debounce |
| `midi/` | Qt Test on `FakeMidiBackend`: exact bytes per channel; reconnect state machine; follow-mode parsing and echo suppression |
| `ui/` | construction + `--screenshot` per state under `offscreen`; the designer reviews the PNGs |
| Linux loopback | `scripts/midi-smoke.sh` against ALSA `Midi Through` |
| Hardware | the checklist in `docs/MIDI.md`, run by the owner at the gates in `docs/PLAN.md` |
| CI | `.github/workflows/build.yml`: build + tests on Linux, macOS, Windows on every push; packages on tags |

## 8. Packaging

Linux: `linuxdeploy` + Qt plugin → AppImage; also `cmake --install` for a plain prefix.
macOS: `macdeployqt`, universal, ad-hoc `codesign`, `hdiutil` DMG. Without notarisation
the first launch needs right-click → Open (documented in the README).
Windows: `windeployqt` folder zipped. No installer.

Version comes from `project(VERSION)` in CMake; tags are `vMAJOR.MINOR.PATCH`.

## 9. Trademarks

"Roland", "Boutique", "D-05" and "D-50" are trademarks of Roland Corporation. The app
and repository name the product in plain text to say what it works with, carry a
non-affiliation notice in the README and About box, and ship none of Roland's artwork.

## 10. Later (not scheduled)

Read user-bank names over SysEx; a patch editor (a separate project the owner has in
mind); Hapax `[PC]` export of the favourites; MIDI learn for a foot controller.
