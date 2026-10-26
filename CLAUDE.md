# d05-patch-switcher

A small C++17 / Qt 6 Widgets desktop app that finds and selects patches on Roland
Boutique synths over MIDI. Linux, macOS, Windows. One window, a search box, a list, a
button grid and a display. Version 1 (shipped as v1.0.0) drives the D-05 (Bank Select +
Program Change) with a green two-line display modelled on the D-50's. Version 2, in
progress, becomes **Boutique Patch Switcher**: it adds the JP-08 and the JU-06A
(Program Change only), per-device ports and channels, a window that re-skins to the
synth it drives, and app-side tracking of the JU-06A's engine switch. No editor, no
SysEx.

## Read in this order

1. `docs/SPEC.md`: what it does and how it is built. The authority for behaviour.
2. `docs/SPEC-2.md`: version 2 as a delta over SPEC.md; where the two disagree, it wins.
3. `docs/MIDI.md`: the D-05's bytes and settings, the owner's cable path, the test
   recipe without hardware. `docs/MIDI-JP08.md` and `docs/MIDI-JU06A.md`: the same for
   the other two models, with their hardware checklists.
4. `docs/IDENTIFY.md`: how each unit shows up over USB and how the app tells them apart
   (measured, not assumed).
5. `docs/PLAN.md`: milestones, tasks, acceptance criteria, gates. The foreman ticks it.
6. `docs/DESIGN-BRIEF.md` then `docs/design/DESIGN.md`; for version 2
   `docs/DESIGN-BRIEF-2.md` then `docs/design/DESIGN-2.md` (once it exists):
   everything visual. The builder does not invent visuals.
7. `docs/FOREMAN.md`: how the coordinating session works.

## Build, test, run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release     # first time fetches RtMidi 6.0.0
cmake --build build -j
ctest --test-dir build --output-on-failure          # headless; tests set QT_QPA_PLATFORM=offscreen
./build/d05-patch-switcher --list-ports
./build/d05-patch-switcher                          # the GUI
QT_QPA_PLATFORM=offscreen ./build/d05-patch-switcher --screenshot /tmp/main.png   # once M3 lands
```

Linux box: Ubuntu 24.04, Qt 6.4.2 (`qt6-base-dev`), GCC 13, ALSA. `aseqdump` (from
`alsa-utils`) and the ALSA `Midi Through` port are the hardware-free MIDI test path.

## Conventions

- C++17, Qt 6.4 API floor, Qt Widgets only. No QML. No new dependencies. No network.
- One class per `.h/.cpp` pair under `src/<module>/` (`app`, `core`, `midi`, `ui`).
  `QStringLiteral` for literals; `QT_NO_CAST_FROM_ASCII` is on.
- Every `core/` and `midi/` behaviour has a Qt Test in `tests/`. Add tests with
  `d05_add_test(name sources…)` in `tests/CMakeLists.txt`.
- MIDI input callbacks arrive on RtMidi's thread: marshal with a queued signal, never
  touch widgets there. Port open and send can throw `RtMidiError`: catch, report in
  status, never crash.
- The app sends MIDI only on an explicit selection (Enter, click, prev/next, `--send`)
  or when auto-send is on. Never on a list highlight by default.
- Footprint rules: no timer under 2 s and that one only while disconnected; no repaint
  without a state change; no allocations in paint paths.
- `data/d05-presets.tsv` is generated from the guide by `scripts/extract-presets.py`;
  edit the script, not the file.
- Nothing trademarked under `assets/`. `assets/local/` is git-ignored and may hold the
  owner's own artwork.
- Commit messages: `M<milestone>.<task>: <what>`; the foreman commits, agents do not.
- No em dashes in user-facing strings or docs.

## Hardware facts you must not get wrong

D-05 (version 1 behaviour, must not change):

- Bank Select MSB = 87 always. LSB 0–7 = U1–U8, 8–13 = P1–P6. Program = (bank−1)·8 + (number−1), zero-based. Send MSB, LSB, PC in that order on the configured channel.
- Panel addresses use digits 1–8 only (`11`…`88`): there is no `P1-19`.
- The D-05 ignores all of it unless `SYSTEM → MIDI → Prog.C` is ON. The owner's manual also lists a `Bank.S` switch; the owner's unit has none, and bank select reception is not gated (`docs/MIDI.md`).

JP-08 and JU-06A (details and sources in their MIDI docs):

- Program Change only, no Bank Select. 64 patches each as `<bank>-<number>`, digits 1–8,
  program = (bank−1)·8 + (number−1), 0–63; values 64–127 are ignored by the units.
- The JU-06A has two engines (`[60/106]` switch), 64 patches each. A Program Change
  lands on whichever engine the switch is on; the engine cannot be selected or read
  over MIDI. Never send an address of the engine the app does not believe the unit is
  on; SPEC-2 §6 defines the prompt-and-arm flow. A flip re-sends the destination
  engine's current patch (nothing if that engine shows `---`).
- No Program Change on/off switch on either; a wrong channel is the only "nothing
  happens" cause found.
- On USB the ports are named `Boutique` (D-05), `Boutiq` (JP-08, shared with the other
  first-generation Boutiques) and `JU-06A`; `docs/IDENTIFY.md` has the rules.

All models:

- On the owner's MacBook the app talks to the **Hapax's** USB MIDI port; the Hapax forwards to the units over DIN. During hardware tests on the Linux box a unit may be plugged in over USB directly, in which case the port is named after it. Through the Hapax the channel plan is the identity (D-05 3, JU-06A 4, JP-08 5). Never hard-code a port name; always offer the picker.
- A web version is specified in `docs/WEB.md` (Web MIDI, same repo under `web/`) and is live. It stays D-05 only in version 2, apart from reading the version 2 library file (plan W5).

## Agents and reviewers

`.claude/agents/builder.md` (Sonnet, code), `designer.md` (Opus, visuals),
`reviewer.md` (Sonnet, read-only review). Dispatch one task at a time per the plan.

Every task also gets an **independent review by OpenAI Codex** through
`scripts/codex-review.sh` (`--commit <sha>`, `--base origin/main` or `--uncommitted`,
`-o` to save the report). The foreman reconciles the two reviews; see `docs/FOREMAN.md`.
Machine notes for the owner's Linux box: Codex lives under nvm's Node 22 and
`~/.codex/config.toml` pins a model the account rejects, so the script sets `PATH` and
passes the model explicitly (`CODEX_MODEL`, default `gpt-5.6-sol`; the accepted slugs are
in `~/.codex/models_cache.json`). It also unsets `OPENAI_API_KEY` so the run bills to the
ChatGPT login. Call the script, not the Claude Codex plugin's subagent.
Copy `docs/claude-settings.example.json` to `.claude/settings.json` once so the routine
build, test and git commands run without permission prompts.
