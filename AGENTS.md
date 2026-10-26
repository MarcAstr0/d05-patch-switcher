# AGENTS.md

Instructions for OpenAI Codex (and any other agent that reads this file). Claude Code
agents read `CLAUDE.md`; the two say the same things.

## What this is

`d05-patch-switcher`: a C++17 / Qt 6 Widgets desktop app that finds and selects patches
on Roland Boutique synths over MIDI. Linux, macOS, Windows. Version 1 (v1.0.0) drives
the D-05 (Bank Select + Program Change). Version 2, in progress, becomes Boutique Patch
Switcher and adds the JP-08 and the JU-06A (Program Change only), per-device ports and
channels, per-synth skins and app-side tracking of the JU-06A's engine switch. No
editor, no SysEx. A web version (D-05 only) is specified in `docs/WEB.md`.

## Read first, in this order

1. `docs/SPEC.md`: behaviour, search grammar, footprint budget, architecture. The
   authority for what is correct.
2. `docs/SPEC-2.md`: version 2 as a delta over SPEC.md; where the two disagree, it
   wins.
3. `docs/MIDI.md`: the D-05's bytes and settings, the owner's cable path.
   `docs/MIDI-JP08.md`, `docs/MIDI-JU06A.md`: the same for the other two models.
   `docs/IDENTIFY.md`: how the units show up over USB and how they are told apart.
4. `docs/PLAN.md`: tasks with acceptance criteria. A change is judged against the
   criteria of the task it implements.

## Hardware facts that are not negotiable

D-05 (version 1 behaviour; it must not change in version 2):

- Bank Select MSB (CC 0) = 87 always. LSB (CC 32) 0–7 = U1–U8, 8–13 = P1–P6.
- Program Change = (bank − 1) × 8 + (number − 1), zero-based, 0–63.
- Send MSB, LSB, Program Change in that order on the configured channel; status bytes
  `0xB0 | (ch − 1)` and `0xC0 | (ch − 1)`.
- Panel addresses use digits 1–8 only: `P1-11` … `P6-88`, `U1-11` … `U8-88`. There is
  no `P1-19`, `P1-90`, `P7-11` or `U9-11`. 896 addresses in total.
- The D-05 ignores everything unless `SYSTEM → MIDI → Prog.C` is ON.

JP-08 and JU-06A:

- One Program Change `0xC0 | (ch − 1)`, program 0–63, no Bank Select. Addresses are
  `<bank>-<number>` with digits 1–8; 64 per model, 64 per engine on the JU-06A.
- The JU-06A's engine (`[60/106]` switch) cannot be selected or read over MIDI; a
  Program Change lands on whichever engine the switch is on. An address of the engine
  the app does not believe the unit is on must never be sent (`docs/SPEC-2.md` §6).

## When you are asked to review

You are the independent second reviewer next to a Claude-based one; you never see its
findings and it never sees yours. Report defects, not preferences. For each finding:
severity (blocker, major, minor), file and line, what is wrong, a concrete input or
sequence that triggers it, and the fix. Look hardest at:

- wrong MIDI bytes or ordering; address parsing edge cases (digits 0 and 9, groups
  above the range, lower case, spacing, `P3 45` vs `P3-45` vs `P345`; on the 64-patch
  models `45` vs `4-5` vs `106:45`, and bare JU-06A forms without an engine);
- the JU-06A engine state machine (`docs/SPEC-2.md` §6): no send for the other engine,
  arm, confirm, hint and undo; shared-port demultiplexing by channel;
- threading: RtMidi input callbacks arrive on RtMidi's thread and must reach Qt widgets
  only through a queued signal; nothing may block the GUI thread on MIDI;
- error paths: port vanishes while open, saved port name absent at startup, `RtMidiError`
  on open or send, malformed `library.json`, missing config directory;
- persistence: atomic writes (`QSaveFile`), the 500 ms debounce, the recents cap of 50;
- footprint discipline from `docs/SPEC.md` §5: no timer under 2 s and only while
  disconnected, no repaint without a state change, no allocation in paint paths;
- tests that prove something easier than the task's acceptance criteria.

Say "no defects found" for an area you checked and found clean. End with one line: ship
or do not ship, and why. No praise, no restating the diff.

## Conventions, if you are ever asked to change code

C++17, Qt 6.4 API floor, Qt Widgets only, no QML, no new dependencies, no network.
One class per `.h/.cpp` pair under `src/<module>/`. `QStringLiteral` for literals;
`QT_NO_CAST_FROM_ASCII` is on. Every `core/` and `midi/` behaviour has a Qt Test in
`tests/`. Build and test:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Nothing trademarked under `assets/`; `assets/local/` is never committed. No em dashes
in user-facing strings or docs.
