# D-05 Patch Switcher

Find and select patches on a Roland Boutique D-05 from your computer, over MIDI.

The D-05 keeps 896 patches behind an 8 × 8 button grid and a two-line display, with no
search. This app gives you one: type a few letters of a name, press Enter, and the synth
switches. Browse by group and bank with a grid that mirrors the panel, star favourites,
and see what you used recently, all on a black panel with a green display that looks
like it belongs next to the synth.

<p align="center">
  <img src="docs/images/screenshot-macos.png" width="800"
       alt="D-05 Patch Switcher on macOS: a green two-line display reading P1-21 DigitalNativeDance, the patch group, bank and number buttons with the current patch lit, a search field, and the patch list.">
</p>

**Status: [v1.0.0](https://github.com/MarcAstr0/d05-patch-switcher/releases/tag/v1.0.0).** See `CHANGELOG.md` for what changed.

## Web version

**[Open D-05 Patch Switcher in your browser](https://marcastr0.github.io/d05-patch-switcher/)**,
with nothing to install. It does the same as the desktop app, over Web MIDI, and also fits
a phone screen.

- Works in Chrome, Edge, Brave and Opera on desktop (one permission prompt) and in
  Firefox 108 or later (asks once to install a per-site permission add-on).
- Chrome on Android needs a USB MIDI interface or forwarding device between the phone
  and the D-05 (see [Connecting the D-05](#connecting-the-d-05)); this path is not yet
  tested on hardware.
- Does not work in Safari or in any browser on iPhone or iPad: they have no Web MIDI.
- After the first visit it works offline and can be installed to the home screen or
  the dock.
- Names, favourites and recents stay in the browser. Use *Export Library* and
  *Import Library* in the menu to move them to or from the desktop app's `library.json`.

The source is in `web/`; see `docs/WEB.md`.

## What you need

- A Roland Boutique D-05 reachable from your computer over MIDI. See
  [Connecting the D-05](#connecting-the-d-05).
- On the D-05, in `FUNCTION` → `SYSTEM` → `MIDI`: `Prog.C` **ON** and `CH` set to the
  channel you pick in the app (if your firmware also shows `Bank.S`, turn it on). With
  `Prog.C` off the synth ignores the app in silence.
- Linux, macOS 12 or later, or Windows 10 or later.

### Connecting the D-05

The app sends Bank Select and Program Change to whichever MIDI output you pick in
Settings. It never assumes a port name, so any of these works:

- **The D-05's own USB port.** On Linux it works with no driver and shows up as
  `Boutique`. On macOS and Windows it needs Roland's USB driver for the D-05. On Android
  (USB OTG) the D-05's own port is not recognised as a MIDI device; use one of the
  options below.
- **A USB MIDI interface** with its MIDI OUT cabled to the D-05's MIDI IN. Class-compliant
  interfaces need no driver on any of the three systems.
- **A hardware sequencer, hub or other device that forwards MIDI** from its USB port to
  the D-05. Check two things on that device:
  - Many of them expose several USB ports. If the D-05 does not react, try each port in
    turn with *Test*.
  - Some route incoming MIDI through a track or a channel map and send it on their own
    channel, so the app's channel setting may not be the one that reaches the D-05. Make
    sure the device passes Program Change and Bank Select (CC 0 and CC 32) through.

## Install

Builds for the three platforms are attached to each GitHub release.

| Platform | File | Notes |
|---|---|---|
| Linux x86_64 | `d05-patch-switcher-x86_64.AppImage` | `chmod +x`, run |
| macOS (Intel and Apple Silicon) | `d05-patch-switcher-macos-universal.dmg` | Not notarised: see [Opening the macOS app](#opening-the-macos-app) |
| Windows x64 | `d05-patch-switcher-windows-x64.zip` | unzip anywhere, run the `.exe`. The exe is unsigned: SmartScreen may warn; click *More info*, then *Run anyway* |

### macOS notes

Needs macOS 12 or later. Open the `.dmg` and drag *D-05 Patch Switcher* onto the
*Applications* shortcut in the same window.

The app is signed ad hoc but not notarised by Apple, so Gatekeeper blocks the first
launch. Pick the steps for your macOS version.

#### Opening the macOS app

**macOS 12 to 14**

1. In Finder, open *Applications*.
2. Right-click (or Control-click) *D-05 Patch Switcher* and choose *Open*.
3. In the dialog, click *Open*. Later launches work normally.

**macOS 15 (Sequoia) and later** removed the right-click bypass for unnotarised apps.

1. Open the app once (double-click) and dismiss the warning with *Done*.
2. Open *System Settings* → *Privacy & Security* and scroll down to *Security*.
3. Next to "D-05 Patch Switcher was blocked", click *Open Anyway*, then confirm with
   your password or Touch ID and click *Open Anyway* again. Later launches work normally.

**Terminal alternative** (any version):

```sh
xattr -dr com.apple.quarantine "/Applications/D-05 Patch Switcher.app"
```

This removes the quarantine flag macOS attached to the download, so Gatekeeper no longer
stops the app.

#### MIDI on macOS

- A class-compliant USB MIDI interface or forwarding device needs no driver on macOS.
- The D-05's own USB port does need Roland's USB driver on macOS. Connecting through
  another device's MIDI output avoids that.
- Names, favourites, recents and settings live in
  `~/Library/Application Support/marcastr0/d05-patch-switcher/`.

## Use

1. Open Settings (`Ctrl+,`, `Cmd+,` on macOS), pick the MIDI output that reaches your
   D-05, set the channel, press *Test*. The D-05's display should jump to the patch shown
   in the app. If nothing happens, see [Connecting the D-05](#connecting-the-d-05).
2. Type in the search box: `brass`, `e-piano`, `p3-45`, `345`, `u2`. Arrow keys move,
   Enter sends. `PgUp` / `PgDn` step through the catalogue.
3. `Ctrl+D` toggles the favourite of the highlighted row; `Ctrl+2` shows favourites, `Ctrl+3` recents.
4. `F1` lists every shortcut.

User banks U1–U8 show their address until you name them (`F2`). Names, favourites and
recents live in `library.json`, and the settings in `settings.ini`, both in your user data
directory:

| Platform | Directory |
|---|---|
| Linux | `~/.local/share/marcastr0/d05-patch-switcher/` |
| macOS | `~/Library/Application Support/marcastr0/d05-patch-switcher/` |
| Windows | `%APPDATA%\marcastr0\d05-patch-switcher\` |

If a `logo.png` is placed in that directory it is shown in the panel instead of the
wordmark. `--config-dir DIR` points the app at another directory.

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

Command-line options (`--help`): `--list-ports`, `--port`, `--in-port`, `--channel`,
`--send P1-11` for scripting (exit 0 sent, 1 bad address, 2 port not found, 3 MIDI
error), `--fake-midi` for trying it with no hardware, `--config-dir`.

### Developer: testing MIDI without hardware

Unit tests run on a fake backend (`ctest`). On Linux, `scripts/midi-smoke.sh` drives the
real RtMidi backend against the ALSA `Midi Through` port and checks the bytes with
`aseqdump` (`sudo apt install alsa-utils`):

```sh
scripts/midi-smoke.sh [path/to/d05-patch-switcher]   # default: build/d05-patch-switcher
```

### Developer: CI and packaging

`.github/workflows/build.yml` builds, tests, packages and verifies on Linux, macOS and
Windows. Qt, linuxdeploy, its Qt plugin and every action are pinned to exact versions
(actions by commit SHA); bump a version and its sha256 together. Each package step is
followed by a step that runs the packaged binary (`--version`, `--list-ports`) and
checks the 2 MB binary limit. Packages are downloadable from each run's Artifacts.

## Trademarks

Roland, Boutique, D-05 and D-50 are trademarks of Roland Corporation. This project is
not affiliated with or endorsed by Roland, and ships none of its artwork. The patch
names in `data/` are reproduced from Roland's published Parameter Guide for
interoperability.

## Licence

MIT. See `LICENSE`. Third-party components (Qt under the LGPLv3, RtMidi, the Inter
typeface) and how to replace Qt are in `THIRD-PARTY-NOTICES.md`, which ships in every
package.
