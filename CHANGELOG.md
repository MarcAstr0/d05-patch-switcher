# Changelog

All notable changes to D-05 Patch Switcher. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-10-24

### Added

- `F1` opens a sheet of every keyboard shortcut. On macOS it shows the Mac keys
  (`⌘`, `⇧`, `⌥`, and `fn ↑` / `fn ↓` for page up and down).
- About box with the version, licence, trademark notice and a link to the project.
- On first run, before any output port is set, the display's second line says
  `OPEN SETTINGS: CTRL+,` (`CMD+,` on macOS).
- Input port problems now show: an error next to the input port in Settings, and
  `NOT FOUND` after the input name in the status bar while follow-the-panel is on.
- Every package includes `LICENSE` and `THIRD-PARTY-NOTICES.md` (Qt, RtMidi, Inter).
- The Windows `.exe` carries the app icon and version details.

### Changed

- The app's own MIDI ports (and those of a second running copy) no longer appear in
  the port lists, so follow-the-panel cannot be pointed at the app's own output. On
  Linux the app now shows up in `aconnect -l` as "D-05 Patch Switcher".
- Sharper icon at 16, 24 and 32 px; the macOS icon now includes every size.
- Two identical MIDI devices are listed by their full names, so you can pick either.
- The README documents the Squarp Hapax path, where the data files live on each
  system, and the Windows SmartScreen step.

### Fixed

- A patch change made on the D-05 while `SENT` was showing no longer keeps `SENT` on
  the display.
- Moving the window to a screen with a different scale factor now re-lays out the
  panel instead of leaving a gap under the display.
- Holding Enter, Space or a digit key no longer sends the patch over and over.
- A patch sent for the first time shows up at once in an open Recents view.
- `--fake-midi` no longer saves its fake ports to your settings.

## [0.9.0] - 2026-10-24

First pre-release.

### Added

- Search the D-05's 896 patches by name or address (`brass`, `p3`, `P3-45`, `345`),
  accent-insensitive, with matches highlighted.
- Patch group, bank and number buttons that work like the panel, PREV and NEXT across
  groups, and a two-line display modelled on the D-50's.
- Favourites, Recents with send counts, and names for user slots (`F2`, up to 18
  characters).
- Settings: output and input port, MIDI channel, TEST, auto-send on highlight,
  follow the panel, and resend the current patch on connect.
- Reconnects by itself when the port comes back; never hard-codes a port name.
- Command line: `--list-ports`, `--port`, `--channel`, `--send`, `--fake-midi`,
  `--config-dir`, `--screenshot`.
- Builds for Linux (AppImage), macOS (universal `.dmg`, Intel and Apple Silicon) and
  Windows (zip).

### Verified on hardware

- Linux, D-05 over USB (shows up as `Boutique`): sending, prev/next across groups and
  follow-the-panel.
- macOS through a Squarp Hapax (port `HAPAX Puerto 3`): sending and prev/next.

[1.0.0]: https://github.com/MarcAstr0/d05-patch-switcher/releases/tag/v1.0.0
[0.9.0]: https://github.com/MarcAstr0/d05-patch-switcher/releases/tag/v0.9.0
