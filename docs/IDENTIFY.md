# Identifying which Boutique is on a port

Facts measured on the owner's Linux box (Ubuntu 24.04, kernel 7.0, ALSA) on 2026-10-24
(D-05) and 2026-10-26 (JP-08, JU-06A), each unit plugged into the computer over its own
USB port. This file is the authority for the detection rules in `docs/SPEC-2.md` §5.
What was not measured is marked as such; do not promote a guess to a fact.

## What each unit reports over USB

| | D-05 | JP-08 | JU-06A |
|---|---|---|---|
| USB vendor:product | `0582:01ff` | `0582:01b5` | `0582:023a` |
| usb.ids name | Boutique D-05 | Boutique Series Synthesizer (Normal mode) | JU-06A |
| Product ID unique to the model | yes | **no**, shared by the first generation (JP-08, JU-06, JX-03) | yes |
| USB product string | `Boutique` | `Boutiq` (sic, six characters) | `JU-06A` |
| USB manufacturer string | `Roland` | `Roland` | `Roland` |
| USB serial, first four characters | `BQ_D` | `BQJP` | `BQJU` |
| ALSA card name (`/proc/asound/cardN/id`) | `Boutique` | `Boutiq` | `JU06A` |
| ALSA sequencer client name | `Boutique` | `Boutiq` | `JU-06A` |
| RtMidi port name on Linux | `Boutique:Boutique MIDI 1 24:0` | `Boutiq:Boutiq MIDI 1 28:0` | `JU-06A:JU-06A MIDI 1 28:0` |
| Driver | none, kernel auto-detects Roland's vendor-specific interfaces | same | same |
| USB interfaces | 4 (1 MIDI, 3 audio) | 4 | 4 |
| Sends MIDI clock continuously | yes | yes | yes |

The trailing `client:port` numbers change across replug; `MidiLink::portKey` already
strips them. The serial follows `BQ` plus a two-character model code for every unit
measured, then digits that look like a date code and a unit number. The full serials
are not recorded here on purpose.

`0582:01b6` is the first generation's "storage mode" (the unit mounted as a USB drive
for backups); it has no MIDI port and the app ignores it.

## Detection rules

The app never needs detection to work: a device entry (model, port, channel) can
always be configured by hand, and through a forwarding device such as the Hapax nothing
below applies because the port belongs to the forwarder. Detection exists to pre-fill
the model when a Boutique is on the computer's own USB.

**Rule 1, by port name (all platforms).** Match the port's name, case-insensitively,
against this table, longest match first:

| Name contains | Model | Confidence |
|---|---|---|
| `JU-06A` | JU-06A | certain |
| `JP-08` | JP-08 | certain (not seen on Linux; Roland's macOS and Windows drivers may name the port this way, unverified) |
| `D-05` | D-05 | certain (same caveat) |
| `Boutique` | D-05 | likely: the only Boutique that reports `Boutique` on Linux is the D-05, but the word may appear in other Roland product strings |
| `Boutiq` (and not `Boutique`) | first generation | ambiguous between JP-08, JU-06 and JX-03; of those the app supports only the JP-08 |

"Certain" pre-selects the model without asking. "Likely" and "ambiguous" pre-select the
best candidate and the Settings dialog says so next to the model combo, so the user can
correct it.

**Rule 2, by USB serial (Linux only, optional refinement).** Resolve the RtMidi port to
its ALSA client number (the trailing `N:0`), the client to its sound card with
`snd_seq_client_info_get_card()` (alsa-lib, already linked through RtMidi's ALSA
backend, so no new dependency), then read:

```
/proc/asound/card<N>/usbid                       0582:01b5
/sys/class/sound/card<N>/device/../serial        BQJP…   (the card's USB device directory)
/sys/class/sound/card<N>/device/../product       Boutiq
```

The serial prefix decides, together with the measured product ID: `0582:01b5` + `BQJP` = JP-08, `0582:023a` + `BQJU` = JU-06A, `0582:01ff` + `BQ_D` = D-05. Any other pairing (for example a first-generation JU-06 on `01b5`, whose serial was not measured) falls back to rule 1. Any other
`BQ..` prefix is a Boutique the app does not support; the user still gets the manual
model choice. If any file is missing or unreadable, fall back to rule 1 silently. This
rule lifts the `Boutiq` case from "ambiguous" to "certain" on Linux. Not implemented on
macOS and Windows in this version (CoreMIDI and WinMM would need their own lookups).

**What detection must not do.** It never opens a port by itself, never changes the
device the user is working on, never sends anything, and never runs on a timer: it runs
when the port list is refreshed (Settings, the 2 s poll while disconnected, and
`--identify`).

## Through a forwarding device

On the owner's live rig all three units hang off the Hapax (`docs/MIDI.md`,
"Connecting the D-05", path 3) and the computer sees only the Hapax's ports. There the
channel plan is the identity: the user configures three device entries on the same
port with different channels. The channel plan as of 2026-10-25 (the units themselves
were still on their old channels on 2026-10-26):

| Device | Planned channel | Measured on the unit 2026-10-26 |
|---|---|---|
| D-05 | 3 | 3 |
| JU-06A | 4 | 12 |
| JP-08 | 5 (dual upper part 6) | 13 |

## Measuring it yourself

```sh
lsusb | grep 0582
for c in /proc/asound/card*; do echo "$c $(cat $c/usbid 2>/dev/null) $(cat $c/id)"; done
for d in /sys/bus/usb/devices/*; do
  [ -f "$d/idVendor" ] && [ "$(cat $d/idVendor)" = 0582 ] &&
  echo "$(cat $d/idProduct) product='$(cat $d/product)' serial='$(cat $d/serial)'"
done
aconnect -l        # shows card=N per client
amidi -l
```

`--identify` in the app prints the same conclusion per port (SPEC-2 FR-15).
