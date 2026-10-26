# MIDI facts for the Roland Boutique JP-08

Roland publishes no MIDI implementation chart for the JP-08 (2015). The online
"JUPITER-08" chart is a different product, the October 2026 reissue; nothing in it
applies here. Everything below comes from the *JP-08 Owner's Manual* (`JP-08_e02_w.pdf`)
and from measurements on the owner's unit over USB on 2026-10-26. Where the manual is
silent, the measurement is the authority, and the measurement date is given.

## Patches

64 patches in 8 banks of 8, addressed on the panel with the PATCH NUMBER `[1]`–`[8]`
buttons as `<bank>-<number>` with both digits 1–8. The unit's display reads e.g. `7-1`.
There are no groups and no preset/user split: every slot is writable, and every slot
ships with a factory sound (`data/jp08-patches.tsv`, from the *JP-08 Sound List*,
`JP-08_Sound_List_e01_W.pdf`, 1-1 *NEG Sync* … 8-8 *Space Boomerang*).

Separately, eight **patch presets** (PATCH PRESET `[1]`–`[8]`) store the state of the
two parts (upper and lower) together. They are not the same thing as patches and no
MIDI message for them is documented or was found. Out of scope.

## Selecting a patch

Program Change only. No Bank Select (sending CC 0 / CC 32 does no harm but selects
nothing).

```
program = (bank − 1) × 8 + (number − 1)        1-1 → 0,  2-3 → 10,  4-5 → 28,  7-1 → 48,  8-8 → 63
```

To select `4-5` on channel *n* send one message: `0xC0 | (n−1)`, `0x1C`.

Verified 2026-10-26 (receive): PC 48 showed `7-1` on the unit, PC 28 showed `4-5`.

## What the unit transmits

Pressing a PATCH NUMBER pair on the panel transmits the same Program Change on the
unit's channel. Verified 2026-10-26: `1-1` sent PC 0, `2-3` sent PC 10. Follow-the-panel
therefore works exactly as on the D-05, minus the Bank Select pair: a lone Program
Change on the channel is the event.

The unit also sends MIDI clock continuously over USB (24 messages per beat at its
tempo), and notes when a keyboard is attached. The input parser must keep ignoring
realtime and channel-voice messages other than Program Change.

## Dual mode and parts

The JP-08 has an upper and a lower part. `[DUAL]` layers them; in Dual mode
`[UPPER/LOWER]` chooses which part the panel edits, and the lower part can be selected
only in Dual mode. Which part a received Program Change applies to in Dual mode was
**not measured**. The app treats the JP-08 as one part on one channel. If the owner's
rig later maps the lower part to its own channel (the live-rig plan reserves channel 6
for the "dual upper" part), that is a second device entry with another channel, which
the app allows but nothing in this version depends on it.

## Settings on the JP-08 that gate this

System settings are entered by holding `[MANUAL]` and pressing a numeric button
(manual, "System Settings"):

| Setting | Buttons | Must be |
|---|---|---|
| MIDI channel (transmit and receive) | `[MANUAL]` + `[2]`, then `[1]`–`[16]` | the channel the device entry uses. The owner's unit was on **13** on 2026-10-26; the live-rig plan moves it to **5** |
| MIDI clock source | `[MANUAL]` + `[3]` | irrelevant to patch selection |

There is no Program Change on/off switch documented for this unit (unlike the D-05's
`Prog.C`). Sending on the wrong channel is the only "nothing happens" cause found.

## Connecting the JP-08

Same three paths as `docs/MIDI.md` "Connecting the D-05". On the unit's own USB port
(path 1) Linux needs no driver; the port is named **`Boutiq`** (see `docs/IDENTIFY.md`
for why, and for how the app tells it from a JU-06 or JX-03). macOS and Windows need
Roland's Boutique driver for path 1 and were not tested. Path 1 carries both
directions, so follow-the-panel works over it.

## Testing without hardware (Linux)

```sh
aseqdump -p "Midi Through" &
./build/boutique-patch-switcher --device jp08 --port "Midi Through" --channel 5 --send 4-5
# expect exactly one message:
#   Program change  4, program 28
```

(`aseqdump` prints channels zero-based: `4` is channel 5.)

## Hardware checklist (the owner runs this)

1. JP-08 on, MIDI channel set as above and matching the device entry.
2. On USB directly, `Boutiq` appears in `--list-ports` and `--identify` says JP-08
   (on Linux, via the serial; elsewhere it says "first generation, JP-08 assumed").
3. App: the JP-08 device selected, Settings → output port and channel; *Test* sends the
   current patch. The unit's display must change to it.
4. From the app select `1-1` *NEG Sync*, then `4-5` *Sine Glass*, then `8-8`
   *Space Boomerang*: each must show on the unit within a blink.
5. Prev/next from `1-8` must land on `2-1` on the unit.
6. With follow-the-panel on and the USB path: press `2-3` on the unit; the app's
   display must show `2-3` *Echo Piano* without the unit re-selecting.
7. If the D-05 is also connected, switching the app's device back to the D-05 and
   sending `P1-11` must change the D-05, not the JP-08.
