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
| `C0 24` | Program Change 36 = (4−1)×8 + (5−1) |

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

## The owner's cable path

```
D-05  ──DIN MIDI──▶  Squarp Hapax  ──USB──▶  computer
```

The Hapax is configured as a MIDI thru: what the computer sends to the Hapax's USB
input is forwarded to the DIN output the D-05 hangs on. From the computer's point of
view the port to open is the **Hapax's** USB MIDI port, not a D-05 port, which is why
the app never hard-codes a port name and always offers a picker.

The reverse direction (D-05 DIN out → Hapax DIN in → USB) only exists if that cable
is connected and the Hapax forwards it; follow-the-panel is therefore optional and off
by default.

The Hapax is a class-compliant USB MIDI device, so the same path works on Linux with
no driver. That is how the builder tests against real hardware on the Linux machine:
move the Hapax's USB cable from the Mac to the Linux box.

The D-05's own micro-USB port needs Roland's driver on macOS and Windows for
"vendor" mode and has no Linux driver at all; the app does not depend on it.

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
2. Hapax on, thru routing from USB device in to the D-05's DIN out enabled.
3. App: Settings → output port = the Hapax; channel as above; "Test" sends the current
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
