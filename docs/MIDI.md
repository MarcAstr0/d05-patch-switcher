# MIDI facts for the Roland Boutique D-05

Everything here comes from the *D-05 Parameter Guide* (Roland, 2018, "MIDI
Implementation", pages 33–44) unless marked otherwise. The guide is the authority; if
this file and the guide disagree, the guide wins and this file gets fixed.

## Selecting a patch

The D-05 has 14 patch groups of 64 patches each, 896 addresses in total:

| Group | Bank Select LSB (CC 32) | Content |
|---|---:|---|
| U1 … U8 | 0 … 7 | user banks, writable |
| P1 | 8 | Preset 1: Original D-50 |
| P2 | 9 | Preset 2: New for D-05 |
| P3 | 10 | Preset 3: PN-D50-01 |
| P4 | 11 | Preset 4: PN-D50-02 |
| P5 | 12 | Preset 5: PN-D50-03 |
| P6 | 13 | Preset 6: PN-D50-04 |

Bank Select MSB (CC 0) is **always 87** (`0x57`).

Within a group a patch is addressed on the panel as `<bank><number>` with both digits
1–8 (`11` … `88`); there is no 9 or 0. The Program Change value is zero-based:

```
program = (bank − 1) × 8 + (number − 1)        P1-11 → 0,  P1-21 → 8,  P1-44 → 27,  P6-88 → 63
```

To select `P3-45` on channel 1 send, in this order:

| Bytes (hex) | Meaning |
|---|---|
| `B0 00 57` | CC 0 (Bank Select MSB) = 87 |
| `B0 20 0A` | CC 32 (Bank Select LSB) = 10, group P3 |
| `C0 1C` | Program Change 28 = (4−1)×8 + (5−1) |

For channel *n* (1–16) the status bytes are `0xB0 | (n−1)` and `0xC0 | (n−1)`.
Send all three every time; do not assume the unit remembers the bank.

## Settings on the D-05 that gate this

`FUNCTION` → `SYSTEM` → `MIDI` screen (guide p. 3 and p. 31):

| Setting | Must be | Why |
|---|---|---|
| `CH` (basic channel) | the channel the app sends on | everything above is a channel message |
| `Prog.C` (Program Change) | ON | "transmitted and received" when ON; when OFF the unit silently ignores Program Change |
| `Bank.S` (Bank Select) | ON, **if your unit shows it** | the owner's manual documents it, but the owner's D-05 has no such entry (its MIDI menu runs `Prog.C` → `Excl` → `Sync` → `Thru`), and the Parameter Guide's implementation chart puts the "not received when the switch is OFF" footnote on Program Change only. Bank Select reception is not gated on that unit |
| `Excl` (Exclusive) | any | only matters for the SysEx features below |

`Prog.C` OFF produces a symptom identical to a wrong number. The README tells the user
to check it first.

## What the unit transmits

With `Prog.C` ON, pressing the panel buttons transmits the same three messages on
the transmit channel (`TxCH`, default = basic channel). The app's "follow
the panel" mode listens for CC 0 / CC 32 / Program Change on its channel and moves the
display without re-sending. Echo suppression: ignore an incoming triplet identical to
one the app sent in the previous 300 ms, because some MIDI paths loop the output back.

Per-patch `TxPC` (a Program Change number stored in each patch, p. 5) is a different
feature, for driving *other* gear from the D-05, and is irrelevant here.

## Connecting the D-05

The app opens whatever MIDI output the user picks; it never hard-codes a port name.
Three kinds of path work:

```
computer ──USB──▶ D-05                                   (1) the D-05's own USB port
computer ──USB──▶ MIDI interface ──DIN──▶ D-05 MIDI IN    (2) a USB MIDI interface
computer ──USB──▶ sequencer / hub ──DIN──▶ D-05 MIDI IN   (3) a device that forwards MIDI
```

**(1) The D-05's own USB port.** Verified on Linux (2026-10-24, gate G1, Ubuntu 24.04):
the unit enumerates with no driver.

| | Observed |
|---|---|
| USB ID | `0582:01ff` Roland Corp. Boutique D-05 (`lsusb`) |
| ALSA client | `Boutique` (kernel, one port) |
| Port name in the app | `Boutique:Boutique MIDI 1 24:0`; the trailing `24:0` changes across replug, the app matches without it |
| Select from the CLI | `--port Boutique` |
| Result | `P1-11`, `P2-11`, `P6-88`, `U1-11` sent with `--send` on the unit's channel each showed on the D-05 in order |
| Unit output | MIDI clock continuously on the USB port; the app ignores timing messages |

Roland's D-05 manual says the USB driver must be installed and documents no GENERIC /
class-compliant mode; on Linux the kernel's USB audio driver handles the unit
regardless. On macOS and Windows this path needs Roland's driver. Note the port is
named **Boutique**, not D-05.

Android does **not** recognise it (2026-10-24, gate GW2, Redmi 10A, Chrome, D-05
directly on a USB OTG adapter): neither Chrome's Web MIDI nor the MIDI Scope app lists
any device for it; the only port Chrome showed was MIDI Scope's own virtual `Scope`.
On Android use path (2) or (3).

**(2) A USB MIDI interface.** A class-compliant interface needs no driver on Linux,
macOS or Windows. The port name is the interface's; the app's channel must match the
D-05's `CH`.

**(3) A device that forwards MIDI** (a hardware sequencer, a MIDI hub, a synth with
USB-to-DIN thru). Two behaviours to expect, both seen on real hardware:

- **Several USB ports.** Such devices often expose many virtual ports. Only the one
  routed to the DIN output that reaches the D-05 works; try them in turn with *Test*.
- **Channel rewriting.** Some devices pass incoming MIDI through a track or channel map
  and send it on their own output channel. Then the app's channel does not matter; the
  device's output channel must match the D-05's `CH`. The device must also pass Bank
  Select (CC 0, CC 32) and Program Change through.

Example verified at gate G4 (2026-10-24, macOS, Squarp Hapax with hapaxOS 3.x): the
Hapax's USB port appears as 16 virtual ports; incoming MIDI goes into the selected
track (input ALL ACTIVE) and leaves on that track's DIN output on the track's channel;
the D-05 answered on the third port, and Bank Select and Program Change passed through.

**Follow the panel** needs the reverse direction too: the D-05's output reaching the
computer (path 1 carries both directions; paths 2 and 3 need a cable from the D-05's
MIDI OUT and a device that forwards it). That is why follow-the-panel is optional and
off by default.

**First step of any hardware test:** `./build/d05-patch-switcher --list-ports` (or
`aconnect -l` on Linux) with the D-05 on and connected, then check the unit's `CH`:
sending on another channel does nothing, exactly like `Prog.C` OFF.

## Testing without hardware (Linux)

ALSA ships a loopback client, `Midi Through`. Run a dump on it, send to it, assert:

```sh
aseqdump -p "Midi Through" &      # alsa-utils
./build/d05-patch-switcher --port "Midi Through" --channel 1 --send P1-11
# expect, in order:
#   Control change  0, controller 0,  value 87
#   Control change  0, controller 32, value 8
#   Program change  0, program 0
```

`scripts/midi-smoke.sh` automates this. `--fake-midi` goes one level lower and prints
the bytes without opening any port, which is what the unit tests use.

## Hardware checklist (the owner runs this)

1. D-05 on, `SYSTEM` → `MIDI`: `CH` matches the app, `Prog.C` ON (and `Bank.S` ON if the menu has it).
2. The D-05 is reachable (see "Connecting the D-05"): on USB directly, `Boutique` appears in `--list-ports`; through an interface or a forwarding device, that device is on and routes to the D-05's MIDI IN.
3. App: Settings → output port = the port that reaches the D-05 (`Boutique` on USB directly); channel as above; "Test" sends the current
   patch. The D-05 display must change to it.
4. From the app select `P1-11` *Fantasia*, then `P2-11` *Neo Horizon*, then `P6-88`
   *Big Waves*: each must show on the D-05 within a blink. Then `U1-11`.
5. Prev/next from `P1-88` must land on `P2-11` on the unit (group boundary).
6. Optional, if the reverse cable exists: enable "follow the panel", press patch
   buttons on the D-05, the app display must follow without the unit re-selecting.

## System Exclusive (not used in v1, recorded for the editor project)

Roland "type IV" format: `F0 41 <device id> <model id> <command> <address×3> … <checksum> F7`.
Device ID defaults to basic channel − 1. Commands `RQ1` (`0x11`, request data) and
`DT1` (`0x12`, data set) with 3-byte addresses. Checksum: the low 7 bits of the sum of
address, size (or data) and checksum must be zero.

| Area | Address | Notes |
|---|---|---|
| Temporary (the sounding patch) | `00 00 00` … `00 03 00` | Upper partial 1/2, upper common, lower partial 1/2, lower common, patch; 64 bytes each |
| Work area (the selected *bank*) | `02 00 00` = patch 1-1, stride `00 03 40` (448 bytes), … `03 5C 40` = patch 8-8 | followed by reverb types 17–32 |

The model ID is `14H`: the D-05 identifies as a D-50 (guide p. 37, RQ1/DT1 examples),
which is why D-50 editors and librarians can talk to it. Tone name = bytes 0–9 of a
common block; patch name is in the patch block (p. 42 of the guide gives the offsets).
Reading the user-bank names would mean selecting each of U1–U8 in turn and requesting
the work area, roughly 29 KB per bank. The full offset tables are on pages 36–44 of the
guide and are not transcribed here.
