# AGENTS.md

Instructions for OpenAI Codex (and any other agent that reads this file). Claude Code
agents read `CLAUDE.md`; the two say the same things.

## What this is

`d05-patch-switcher`: a C++17 / Qt 6 Widgets desktop app that finds and selects patches
on a Roland Boutique D-05 over MIDI (Bank Select + Program Change). Linux, macOS,
Windows. No editor, no SysEx in v1. A web version is specified in `docs/WEB.md`.

## Read first, in this order

1. `docs/SPEC.md`: behaviour, search grammar, footprint budget, architecture. The
   authority for what is correct.
2. `docs/MIDI.md`: the bytes, the synth's settings, the owner's cable path.
3. `docs/PLAN.md`: tasks with acceptance criteria. A change is judged against the
   criteria of the task it implements.

## Hardware facts that are not negotiable

- Bank Select MSB (CC 0) = 87 always. LSB (CC 32) 0–7 = U1–U8, 8–13 = P1–P6.
- Program Change = (bank − 1) × 8 + (number − 1), zero-based, 0–63.
- Send MSB, LSB, Program Change in that order on the configured channel; status bytes
  `0xB0 | (ch − 1)` and `0xC0 | (ch − 1)`.
- Panel addresses use digits 1–8 only: `P1-11` … `P6-88`, `U1-11` … `U8-88`. There is
  no `P1-19`, `P1-90`, `P7-11` or `U9-11`. 896 addresses in total.
- The D-05 ignores everything unless `SYSTEM → MIDI → Prog.C` is ON.

## When you are asked to review

You are the independent second reviewer next to a Claude-based one; you never see its
findings and it never sees yours. Report defects, not preferences. For each finding:
severity (blocker, major, minor), file and line, what is wrong, a concrete input or
sequence that triggers it, and the fix. Look hardest at:

- wrong MIDI bytes or ordering; address parsing edge cases (digits 0 and 9, groups
  above the range, lower case, spacing, `P3 45` vs `P3-45` vs `P345`);
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
