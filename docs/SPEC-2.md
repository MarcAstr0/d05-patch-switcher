# Boutique Patch Switcher: specification, version 2

Owner: Mario Castro Squella. Written 2026-10-26 by Fable from the owner's brief and
the measurements in `docs/IDENTIFY.md`, `docs/MIDI-JP08.md` and `docs/MIDI-JU06A.md`.

This document is a **delta over `docs/SPEC.md`** (version 1, the D-05 app, shipped as
v1.0.0 on 2026-10-24). Everything in SPEC.md stays in force unless a section here says
otherwise. Section numbers below match SPEC.md where they amend it; new requirements
continue its FR numbering from FR-16. Where the two disagree, this file wins.

## 1. Purpose

One app, three Roland Boutique synths: the **D-05** (896 patches in 14 groups, Bank
Select plus Program Change), the **JP-08** (64 patches, Program Change only) and the
**JU-06A** (two engines, JUNO-60 and JUNO-106, 64 patches each, Program Change only,
and a physical engine switch the app cannot read). The user picks which synth the
window is driving, or the app follows whichever synth was last touched; the window
re-skins to that synth's panel. Each synth has its own MIDI port and channel, so the
three can be driven independently in one session, including over one shared port
through a forwarding device.

The app's name becomes **Boutique Patch Switcher**. The repository is renamed at the
end of this version (§8). It stays a selector: it never writes to any unit's memory.

### Non-goals for version 2

- The web version. It stays the D-05-only app it is today, with one maintenance change
  so it can still import the desktop's library file (W5 in the plan). Web MIDI exposes
  no USB identity and the engine logic would be a third implementation; separate web
  apps per synth are a possible later version.
- Reading the JU-06A's engine from the unit (impossible: no SysEx) or reading any
  patch data.
- The JP-08's eight patch presets and its lower part in Dual mode (no documented MIDI).
- More than one unit of the same model (chain mode makes two units one instrument).
- Hapax definition export, editors, MIDI learn (still §10 of SPEC.md).

## 2. Platforms and stack

Unchanged: C++17, Qt 6 Widgets with the 6.4 floor, RtMidi 6.0.0, CMake, Qt Test,
Linux/macOS/Windows packages, MIT. No new dependencies. The Linux-only USB serial lookup
(`docs/IDENTIFY.md` rule 2) uses alsa-lib, which the ALSA backend of RtMidi already
links, and reads files under `/proc` and `/sys`; it is compiled only on Linux.

## 3. Domain

### 3.1 Models

| Model id | Name in the UI | Addresses | Address scheme | Engines |
|---|---|---|---|---|
| `d05` | D-05 | 896 | group U1…U8, P1…P6; bank 1–8; number 1–8 | none |
| `jp08` | JP-08 | 64 | bank 1–8; number 1–8 | none |
| `ju06a` | JU-06A | 128 | engine 60 or 106; bank 1–8; number 1–8 | 60, 106 |

A model id is the stable key used in settings, the library file, the CLI and the
data files. The three are fixed; there is no plugin mechanism.

### 3.2 Addresses

An address is `(model, engine, group, bank, number)` with the parts the model uses.
Canonical text forms, which are also what the list, the display and the library file
show:

| Model | Canonical | Also parsed | Catalogue order and index |
|---|---|---|---|
| D-05 | `P3-45` | as SPEC.md FR-3 | unchanged, 0…895 |
| JP-08 | `4-5` | `45` | bank, then number: `1-1` = 0 … `8-8` = 63 |
| JU-06A | `106:4-5` | `106:45`, `60:4-5`; bare `4-5` / `45` means the device's current engine | 60 `1-1` = 0 … 60 `8-8` = 63, 106 `1-1` = 64 … 106 `8-8` = 127 |

Digits are 1–8 only in every scheme. `PatchAddress::parse(text, model)` takes the
model; a bare JU-06A form takes the current engine as a second argument or fails.

MIDI messages per address, on channel *n*:

| Model | Messages, in order |
|---|---|
| D-05 | `B<n-1> 00 57`, `B<n-1> 20 <group index>`, `C<n-1> <program>` (unchanged) |
| JP-08 | `C<n-1> <program>` |
| JU-06A | `C<n-1> <program>`, and only when the address's engine equals the device's engine state (§6) |

`program = (bank − 1) × 8 + (number − 1)` in every scheme. The byte tables and the
unit settings are in `docs/MIDI.md`, `docs/MIDI-JP08.md` and `docs/MIDI-JU06A.md`.

### 3.3 Patches and names

| | D-05 | JP-08 | JU-06A |
|---|---|---|---|
| Factory names | 384 presets, `data/d05-presets.tsv` | 64, `data/jp08-patches.tsv` | 128, `data/ju06a-patches.tsv` (column `engine` = `60` or `106`) |
| Slots without a factory name | U1…U8 (512) | none | none |
| User names | U slots only, as today | any slot: shown instead of the factory name when set | any slot, same |
| Key mode tag (W/D/S) | as today | none | none; the row shows the engine tag `60` or `106` in that slot instead |

A user name on a JP-08 or JU-06A slot means "I overwrote this slot"; the factory name
stays available in a tooltip and in search (both names are searched). The 18-character
limit and the printable-ASCII rule from the Decisions log stay. Factory names in the
new tables may contain `’` and `&` (`Louis’ Weapon`, `Brass & Strings`): the tables keep
them as printed, and the display's normaliser maps `’` to `'` as it does for the D-05.

### 3.4 Devices

A **device** is one model with its connection settings. There are exactly three, one per
model, each **enabled** or not. Only enabled devices appear in the device switcher and
have ports opened. Per device, in `QSettings` under `devices/<model id>/`:

| Key | Meaning |
|---|---|
| `enabled` | bool, default false; the v1 migration enables `d05` |
| `outputPort`, `inputPort` | port keys as today (empty = none) |
| `channel` | 1–16 |
| `follow`, `resendOnConnect` | as today, per device |
| `lastPatch` | canonical address, restored at start without sending |
| `engine` (JU-06A only) | `60` or `106`, the app's last known engine state |
| `remembered60`, `remembered106` (JU-06A only) | program 0–63 or absent, see §6 |

Global keys: `currentDevice` (model id), `autoSend`, `followSwitchesDevice` (bool,
default true), window geometry.

Two enabled devices may share an output port (a forwarder) but not a port *and* a
channel; Settings refuses the second with a message naming the first. Input ports may
be shared on the same terms.

### 3.5 Library (per-user state), file version 2

```json
{
  "version": 2,
  "devices": {
    "d05":   { "favorites": ["P1-11"], "recents": [{ "address": "P1-11", "count": 12, "last": "2026-10-24T18:02:11Z" }], "userNames": { "U1-11": "Warm Pad Mk2" } },
    "jp08":  { "favorites": ["4-5"], "recents": [], "userNames": { "1-1": "CN Intro Lead" } },
    "ju06a": { "favorites": ["106:1-1", "60:2-3"], "recents": [], "userNames": { "106:1-2": "CN Brass" } }
  }
}
```

Each device block has exactly the v1 shape and rules (recents cap 50, sorted
favourites, sanitised names). A **version 1 file** is read as `devices.d05`, a copy is
kept once as `library.json.v1`, and the file is written as version 2 at the next save.
Unknown device ids are preserved untouched. The web app's importer learns to read
`devices.d05` from a version 2 file (plan W5); nothing else in the web changes.

Settings and the library move to a new data directory, `marcastr0/boutique-patch-switcher/`
(same platform roots as SPEC.md §3.3). On start, if that directory does not exist and
`marcastr0/d05-patch-switcher/` does, its `library.json` and settings file are copied
over, then the v1 rules above apply. The old directory is never modified or deleted.
`--config-dir` bypasses all of this.

## 4. Functional requirements

Amendments to SPEC.md FR-1 … FR-15, then new requirements.

**FR-1 Catalogue**, amended: per device. The JU-06A list holds both engines in catalogue
order (60 then 106), each row tagged with its engine.

**FR-2 Select and send**, amended: a selection on the JU-06A for an address of the other
engine does not send; it arms the selection (FR-19). Everything else as today, on the
current device's link.

**FR-3 Search**, amended: the grammar is per model. The D-05 grammar is unchanged. For
the JP-08 and the JU-06A:

| Token looks like | Interpreted as | Examples |
|---|---|---|
| one digit 1–8 | bank filter | `4` lists `4-1` … `4-8` |
| two digits 1–8 each, with or without a dash | exact address | `45`, `4-5` |
| `60`, `106` (JU-06A only) | engine filter | `106 brass` |
| `60:…`, `106:…` (JU-06A only) with either form above | engine plus bank or address | `60:4`, `106:45` |
| anything else, including digits with 0 or 9 and three or more digits | text, as today | `1982`, `rock`, `85s` |

Consequence recorded for the owner: on these two models a bare `85` is the address
`8-5`, not the name *SY Rock 85*; `rock` finds it. A bare `45` on the JU-06A matches
`60:4-5` and `106:4-5`, two rows. All tokens AND, catalogue order, highlighted match,
as today. Text tokens match the factory name and the user name.

**FR-4 Views**, amended: the grid shows the group bar only for the D-05. For the JU-06A
the group bar's place holds the engine control (FR-19). Favourites and Recents are per
device.

**FR-5 Prev / next**, amended: within the device; on the JU-06A within the current
engine, clamped at `1-1` and `8-8` of that engine (crossing would need a flip).

**FR-6 … FR-8**, unchanged, per device. `F2` renames any slot on the JP-08 and JU-06A.

**FR-9 Settings dialog**, amended: a device list on the left (D-05, JP-08, JU-06A, each
with an enabled checkbox) and the device's page on the right with the existing fields
(output, input, channel, follow, resend-on-connect, Test, the connection line, the
unit-settings reminder for that model) plus, for the JU-06A, the engine state and the
two remembered programs as read-only information. Next to the port combos, ports that
`docs/IDENTIFY.md` identifies as this device's model are marked `detected`; a `Use`
button fills the port when exactly one such port exists. Global: auto-send, "follow
switches device". OK applies and persists all devices; Cancel reverts.

**FR-10 Connection handling**, amended: per device. The display's `NO MIDI OUTPUT`
refers to the current device. The 2 s poll runs while any enabled device is
disconnected and stops when all are connected (still the only timer).

**FR-11 Follow the panel**, amended: per device and per model. The D-05 needs the
CC 0 / CC 32 / PC triplet as today; the JP-08 and JU-06A need a lone Program Change on
the device's channel. Echo suppression 300 ms per link. A Program Change 64–127 is
ignored on the 64-patch models.

**FR-12 Display**, amended: every skin shows the three things the LCD shows today, in
the skin's own form (DESIGN-2.md decides the form): the current address, the current
name, and a context line. Context priority, highest first: `NO MIDI OUTPUT`; the
JU-06A flip prompt while armed (FR-19); `SENT` for 400 ms; the JU-06A flip hint
(FR-19); `<n> MATCHES` while a search is active; otherwise the model's idle text (the
long group name on the D-05; the device name and, on the JU-06A, the engine name on
the others). Exact strings are in DESIGN-2.md.

**FR-13 Keyboard**, amended, additions:

| Keys | Action |
|---|---|
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | next / previous enabled device |
| `Ctrl+E` | JU-06A only: toggle the engine state (FR-19) |
| `Esc` while armed | clear the armed selection (then as today) |

All existing shortcuts keep their meaning on every device.

**FR-14 Optional logo**, unchanged; one `logo.png` for the app, shown on every skin.

**FR-15 Command line**, amended:

```
boutique-patch-switcher [options]
  --device MODEL            d05 | jp08 | ju06a; default: the saved current device
  --engine 60|106           JU-06A only; overrides the saved engine state for this run
  --identify                list ports with the detected model and confidence, exit 0
  --send ADDR               in the device's address form (bare JU-06A forms use --engine or the saved engine)
                            exit codes as today, plus 5 = engine mismatch (nothing sent)
  --screenshot-state S      adds: jp08 | ju06a-60 | ju06a-106 | flip-prompt | flip-hint | devices | settings-devices
```

`--port`, `--in-port`, `--channel` apply to the device named by `--device` for that run.

**FR-16 Device switcher.** A visible control (DESIGN-2.md places it) lists the enabled
devices, marks the current one and each one's connection state, and switches on click.
Switching changes the skin, the catalogue, the views, the display, the grid and the
status bar together, synchronously, with no transient of the old device's content.
Nothing is sent on a switch. With one enabled device the control still shows it (it is
the device's nameplate); with none, the first-run hint says to open Settings.

**FR-17 Skins.** Each model has a skin: `d05` (the current design, unchanged), `jp08`,
and for the JU-06A two sub-skins `ju06a-60` and `ju06a-106` switched by the engine
state. A skin is a complete token set (colours, type roles, switches) plus the
component choices DESIGN-2.md makes for it (which display widget, grid layout, the
engine control). The token file `docs/design/tokens.json` becomes version 2 with one
set per skin and a shared base; `docs/design/gen-tokens.py` generates one QSS per skin
from a template and keeps the `--check` cross-checks. `Theme` becomes a runtime palette
(the `inline const QColor` constants go) and the window re-polishes on switch. No
animation on switch; it is a repaint.

**FR-18 Detection.** Per `docs/IDENTIFY.md`: port-name rules on every platform, the
USB-serial refinement on Linux. Used by FR-9's `detected` marks and `Use` button, by
`--identify`, and by the first-run hint, which names the detected model when nothing is
enabled yet (`FOUND JU-06A ON USB: OPEN SETTINGS`). Detection never opens, sends or
switches anything by itself.

**FR-19 JU-06A engine state.** The app keeps an engine state for the JU-06A (§6). The
engine control shows it (two lit-able buttons, `60` and `106`); `Ctrl+E` or a click
toggles it. Selecting an address of the other engine arms it instead of sending and
shows the prompt (`FLIP SWITCH TO 106` when the device has an input port and follow is
on, otherwise `FLIP TO 106, THEN CTRL+E`). The arm is cleared by `Esc`, by any other
selection, by a device switch, or by being fulfilled. An unprompted Program Change that
matches the other engine's remembered program produces, when armed for that engine,
the flip confirmation and the send; when not armed, a hint (`FLIPPED TO 60? CTRL+E`)
that stays until the next action. The app never changes the engine state by itself
except through the armed confirmation.

**FR-20 Follow switches device.** With `followSwitchesDevice` on and all enabled
devices' input links open, a follow event for a device other than the current one
switches the window to that device and applies the event. Off, the event is still
applied to that device's state (its current patch, its JU-06A engine logic) but the
window stays. Default on. The JU-06A hint and the armed confirmation work whether or
not the JU-06A is the current device.

## 5. Non-functional requirements, amended

| | Budget (replaces SPEC.md §5 where named) | Notes |
|---|---|---|
| Idle CPU | ≤ 1 % of one core averaged over 30 s with three units connected and sending clock; 0.0 % with one unit not sending clock | clock from three units is about 150 callbacks/s at 120 BPM; the callbacks must drop realtime bytes before any allocation |
| Memory | ≤ 90 MB RSS at 1×, ≤ 130 MB at 2× | one extra glyph atlas per display widget size at most |
| Binary | ≤ 2.5 MB without Qt | two TSVs and the new widgets |
| Threads | main + one RtMidi input thread per open input port, at most three | replaces "nothing else" |
| Timers | unchanged: the 2 s poll only while some enabled device is disconnected; single-shot 400 ms and 500 ms | a skin switch adds no timer |
| Repaints | a device or engine switch repaints the window once | code review |

Startup, HiDPI, robustness and dependencies as before.

## 6. JU-06A engine state machine

State per JU-06A device, persisted: `engine` (60 or 106), `remembered[60]` and
`remembered[106]` (program or unknown), and in memory: `armed` (address or none),
`hint` (engine or none). After start the engine is the saved one and is shown as such;
the app cannot verify it. Let `other` be the engine that is not `engine`.

| Event | Action |
|---|---|
| App sends address A (A.engine == engine) | `remembered[engine] = A.program`; current = A; recents as today |
| User selects A with A.engine == other | do not send; `armed = A`; show the prompt; highlight stays on A |
| `Esc`, another selection, device switch | `armed = none`, prompt cleared |
| `Ctrl+E` or engine control click | `engine = other`; `hint = none`; if `armed` and `armed.engine == engine`: send it, `armed = none`; current = `remembered[engine]` if known, else "no patch" (display blank address, name blank) |
| Incoming PC `p` on the channel, not an echo, while `armed` and `remembered[armed.engine] == p` | this is the flip re-sending that engine's old patch: `engine = armed.engine`; send `armed`; `armed = none`; prompt cleared |
| Incoming PC `p`, not armed, `remembered[other] == p`, `remembered[engine] != p` | follow as a panel press on the current engine (current = (engine, p), `remembered[engine] = p`) **and** `hint = other`; if the user then presses `Ctrl+E` within the hint, the follow is undone: `remembered[engine]` returns to its previous value and current = (other, p) |
| Incoming PC `p`, any other case | follow as a panel press: current = (engine, p), `remembered[engine] = p`; `hint = none` |
| Incoming PC 64–127 | ignored |

`remembered` unknown on one side means the unit may show `---` there and a flip into
it sends nothing; the prompt then can only be answered with `Ctrl+E`, and the prompt
text says so when the input path exists but that engine's program is unknown
(`FLIP TO 106, THEN CTRL+E`). Test vectors for every row are in `data/test-vectors.json`
(`models.ju06a.engine`), as event sequences with the expected state and sent bytes.

## 7. Architecture, amended

```
src/
  core/Model.*             enum Model {D05, JP08, JU06A}; ModelInfo (id, name, catalogue size, scheme, engines)
  core/PatchAddress.*      generalised: model, engine, group, bank, number; parse(text, model[, engine]);
                           toString(); catalogueIndex(); midiMessages(channel); midiTriplet() kept for the D-05
  core/PatchCatalog.*      constructed per model; loads the model's TSV; factory + user names; prev/next per model
  core/SearchQuery.*       constructed per model; grammar per FR-3
  core/Library.*           version 2; per-device blocks; v1 migration; same signals, with a model argument
  core/EngineTracker.*     §6 as a pure QObject with signals (engineChanged, armedChanged, hintChanged, sendRequested)
  core/Identify.*          docs/IDENTIFY.md rule 1 (pure function on port names) and the Linux rule 2 behind #ifdef
  midi/PortHub.*           opens each OS port once (in and out), reference-counted, demultiplexes input to links
  midi/MidiLink.*          one per device; model-aware message building and follow parsing; unchanged API otherwise
  midi/DeviceRouter.*      owns the three links and trackers; current device; FR-20 dispatch; status aggregation
  app/AppSettings.*        per-device sections (§3.4); data-dir migration
  ui/Theme.*               runtime palette per skin; loads assets/theme/<skin>.qss; re-polish on switch
  ui/LedDisplayWidget.*    the 64-patch models' display per DESIGN-2.md (atlas-cached like DisplayWidget)
  ui/DeviceSwitcher.*      FR-16
  ui/EngineControl.*       FR-19's two buttons with LEDs
  ui/MainWindow.*          swaps display widget and group bar / engine control per model; everything else shared
  ui/SettingsDialog.*      FR-9 device list + pages
```

`d05core` / `d05ui` / the `d05` namespace keep their names until the rename task
(plan T9.1), which may rename them if the whole suite stays green in the same commit.
Data flow as today, with `DeviceRouter` between `MainWindow` and the links. The UI never
calls a backend directly.

## 8. Packaging and rename

- Binary `boutique-patch-switcher`; macOS bundle `Boutique Patch Switcher.app`; desktop
  file, icon name and AppImage name follow; window title and About box say Boutique
  Patch Switcher. The CLI help's first line names the three models.
- GitHub repository renamed to `boutique-patch-switcher` by the owner at T9.1; GitHub
  redirects the old URL. The web app's Pages URL changes with it; the README and the
  web's manifest get the new URL in the same task.
- Version `2.0.0`; `CHANGELOG.md` gets a 2.0.0 entry listing the models and the rename.
- Notarisation stays deferred as logged on 2026-10-24.

## 9. Trademarks

"Roland", "Boutique", "D-05", "D-50", "JP-08", "JUPITER-8", "JU-06A", "JUNO-60" and
"JUNO-106" are trademarks of Roland Corporation. The app names the products in plain
text to say what it works with, carries the non-affiliation notice in the README and
About box, and ships none of Roland's artwork or logotypes. Skins borrow colours,
proportions and lettering style from the panels, not logos, wordmarks or drawn artwork
(DESIGN-BRIEF-2.md).

## 10. Testing strategy, amended

| Layer | How |
|---|---|
| `core/` | vectors per model in `data/test-vectors.json` under a new top-level `models` key (addresses, parse, invalid, search, midi, and `ju06a.engine` sequences); the existing top-level sections stay byte-for-byte for the web suite; library v2 round-trip and v1 migration; data-dir migration in a temp dir |
| `midi/` | fake backend: one message for the 64-patch models, three for the D-05; shared-port demultiplexing by channel; echo suppression per link; engine tracker driven through the link; FR-20 dispatch |
| `ui/` | `--screenshot` for every new state at 1× and 2×; the designer reviews |
| Linux loopback | `scripts/midi-smoke.sh` gains one case per model and the engine-mismatch exit code |
| Hardware | the checklists in `docs/MIDI.md`, `docs/MIDI-JP08.md`, `docs/MIDI-JU06A.md`, at the gates in `docs/PLAN.md` |
| CI | unchanged; the web job stays green (W5) |

## 11. Later (not scheduled)

Per-synth web apps; the JP-08 lower part if its MIDI behaviour is ever measured; a
`--names` import for the owner's overwritten slots; everything in SPEC.md §10.
