# D-05 Patch Switcher

Find and select patches on a Roland Boutique D-05 from your computer, over MIDI.

The D-05 keeps 896 patches behind an 8 × 8 button grid and a two-line display, with no
search. This app gives you one: type a few letters of a name, press Enter, and the synth
switches. Browse by group and bank with a grid that mirrors the panel, star favourites,
and see what you used recently, all on a black panel with a green display that looks
like it belongs next to the synth.

**Status: in development.** See `docs/PLAN.md` for where it stands.

## What you need

- A Roland Boutique D-05 reachable from your computer over MIDI, directly or through
  another device that forwards MIDI (the author's D-05 hangs off a Squarp Hapax that
  acts as a USB-to-DIN thru).
- On the D-05, in `FUNCTION` → `SYSTEM` → `MIDI`: `Prog.C` **ON** and `CH` set to the
  channel you pick in the app (if your firmware also shows `Bank.S`, turn it on). With
  `Prog.C` off the synth ignores the app in silence.
- Linux, macOS 12 or later, or Windows 10 or later.

## Install

Builds for the three platforms are attached to each GitHub release.

| Platform | File | Notes |
|---|---|---|
| Linux x86_64 | `d05-patch-switcher-x86_64.AppImage` | `chmod +x`, run |
| macOS (Intel and Apple Silicon) | `d05-patch-switcher-macos-universal.dmg` | Not notarised: the first time, right-click the app and choose *Open* |
| Windows x64 | `d05-patch-switcher-windows-x64.zip` | unzip anywhere, run the `.exe` |

## Use

1. Open Settings (`Ctrl+,`), pick the MIDI output port your D-05 is behind, set the
   channel, press *Test*. The D-05's display should jump to the patch shown in the app.
2. Type in the search box: `brass`, `e-piano`, `p3-45`, `345`, `u2`. Arrow keys move,
   Enter sends. `PgUp` / `PgDn` step through the catalogue.
3. `Ctrl+D` stars the current patch; `Ctrl+2` shows favourites, `Ctrl+3` recents.
4. `F1` lists every shortcut.

User banks U1–U8 show their address until you name them (`F2`). Names, favourites and
recents live in one JSON file in your user data directory, which the About box shows.

If a `logo.png` is placed in that directory it is shown in the panel.

## Build from source

Requires CMake 3.22+, a C++17 compiler and Qt 6.4+ (Widgets). RtMidi is fetched at
configure time.

```sh
# Ubuntu/Debian: sudo apt install cmake g++ qt6-base-dev libasound2-dev
# macOS:         brew install cmake qt
# Windows:       Qt 6 (MSVC 2022) from the Qt installer or aqtinstall, CMake, VS 2022
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/d05-patch-switcher
```

Command-line options (`--help`): `--list-ports`, `--port`, `--channel`, `--send P1-11`
for scripting, `--fake-midi` for trying it with no hardware.

## Trademarks

Roland, Boutique, D-05 and D-50 are trademarks of Roland Corporation. This project is
not affiliated with or endorsed by Roland, and ships none of its artwork. The patch
names in `data/` are reproduced from Roland's published Parameter Guide for
interoperability.

## Licence

MIT. See `LICENSE`.
