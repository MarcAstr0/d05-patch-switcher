# MIDI facts for the Roland Boutique JU-06A

Sources: the *JU-06A Owner's Manual* (`JU-06A_eng02_W.pdf`), the *JU-06A MIDI
Implementation Chart* (Sep 2019, version 1.01, SOUND MODULE mode), the *JU-06A Sound
List* (`JU-06A_Sound_List_e01_W.pdf`), and measurements on the owner's unit over USB
on 2026-10-26. The chart is the authority for what the unit can do; the measurements
are the authority for how it behaves at the engine switch, which the chart does not
describe.

## Two engines, two banks of patches

The `[60/106]` switch on the panel selects the JUNO-60 or the JUNO-106 engine. Each
engine has its own 64 patches in 8 banks of 8, addressed with the PATCH `[1]`–`[8]`
buttons as `<bank>-<number>`, both digits 1–8. The display reads e.g. `1-6`, or `---`
when no patch has been selected on that engine since power-on (the panel's live
settings are sounding). Every slot is writable and ships with a factory sound
(`data/ju06a-patches.tsv`: JUNO-60 1-1 *LD Echolocator* … 8-8 *1982 Synth Drum*;
JUNO-106 1-1 *Brass* … 8-8 *Takeoff*). 128 addresses in total.

The engine is **not addressable over MIDI**: no message selects it and no message
reports it. System Exclusive is `x` both ways in the chart (no Identity Request, no
data request of any kind), there is no Bank Select, and Program Change values are
0–63 in both directions. The app therefore keeps its own idea of which engine the unit
is on (`docs/SPEC-2.md` §6) and uses the behaviour below to keep it honest.

## Selecting a patch

Program Change only, on the unit's channel, and it lands on **whichever engine the
switch is on**:

```
program = (bank − 1) × 8 + (number − 1)        1-1 → 0,  1-6 → 5,  2-3 → 10,  4-5 → 28,  8-8 → 63
```

Verified 2026-10-26: on 106, PC 28 showed `4-5`; on 60, PC 5 showed `1-6`. Values
64–127 are ignored (PC 70 left the display at `1-6`).

The app must never send a patch for an engine other than the one it believes the unit
is on: the message would select the same-numbered patch of the wrong engine, audibly,
in the middle of a set. See SPEC-2 FR-19 for the prompt-and-arm behaviour instead.

## What the unit transmits

- Pressing a PATCH pair on the panel transmits the Program Change on the unit's
  channel. Verified: `1-1` sent PC 0, `2-3` sent PC 10.
- **Flipping the `[60/106]` switch re-selects the destination engine's current patch
  and transmits its Program Change.** Measured sequence, all on channel 12:

  | Step | Unit display | Transmitted |
  |---|---|---|
  | on 106, app sent PC 28 | `4-5` | |
  | flip to 60 (no patch selected there yet) | `---` | nothing |
  | app sent PC 5 | `1-6` | |
  | flip to 106 | `4-5` | PC 28 |
  | flip to 60 | `1-6` | PC 5 |

  So each engine remembers its own current patch, a flip into an engine that has one
  re-sends it, and a flip into an engine showing `---` sends nothing.
- MIDI clock continuously over USB, and notes when a keyboard is attached (the K-25m
  sends fixed velocity 127). The input parser ignores everything but Program Change on
  the channel.

What this gives the app: a Program Change that arrives without the app having sent
anything and that equals the *other* engine's remembered patch is, in all likelihood, a
flip. It is not proof, because pressing that same number on the current engine's panel
produces the identical message. SPEC-2 §6 says how the app uses it: as a confirmation
when it is already waiting for a flip, and as a hint otherwise, never as a silent
switch.

## Chart summary (SOUND MODULE mode)

| Item | Transmitted | Recognised |
|---|---|---|
| Basic channel | 1–16 | 1–16 |
| Mode | Mode 3 (Omni off, Poly) | Mode 3; Omni/Mono/Poly mode messages recognised |
| Program Change | 0–63 | 0–63 |
| Bank Select | x | x |
| System Exclusive | x | x |
| Clock, Start, Stop | | recognised; Continue is treated as Start |
| Song Position | x | x |

The chart's second table, "Control Surface Mode", is a CC map for driving a plug-in
from the panel and sends no Program Change; irrelevant here.

## Settings on the JU-06A that gate this

System settings: hold ARPEGGIO `[ON/OFF]`, press a numeric button, turn `[VALUE]`
(manual, "System Settings"):

| Setting | Button | Must be |
|---|---|---|
| MIDI channel (transmit and receive) | `[2]`, 1–16, default 1 | the channel the device entry uses. The owner's unit was on **12** on 2026-10-26; the live-rig plan moves it to **4** |
| MIDI clock source | `[3]` | irrelevant to patch selection |

No Program Change on/off switch is documented. Wrong channel is the only "nothing
happens" cause found.

## Connecting the JU-06A

Same three paths as `docs/MIDI.md` "Connecting the D-05". On its own USB port (path 1)
Linux needs no driver and the port is named **`JU-06A`**, unambiguous
(`docs/IDENTIFY.md`). macOS and Windows need Roland's driver for path 1, untested.
Path 1 carries both directions, which the flip detection needs; through a forwarder it
needs the unit's MIDI OUT cabled back.

## Testing without hardware (Linux)

```sh
aseqdump -p "Midi Through" &
./build/boutique-patch-switcher --device ju06a --engine 106 --port "Midi Through" --channel 4 --send 4-5
# expect exactly one message:
#   Program change  3, program 28
./build/boutique-patch-switcher --device ju06a --engine 60 --port "Midi Through" --channel 4 --send 106:4-5
# expect exit code 5 (engine mismatch), nothing sent, a one-line explanation on stderr
```

## Hardware checklist (the owner runs this)

1. JU-06A on, MIDI channel set as above and matching the device entry.
2. On USB directly, `JU-06A` appears in `--list-ports` and `--identify` says JU-06A.
3. App: the JU-06A device selected; its engine indicator set to match the switch (the
   app cannot know it at start); Settings → port and channel; *Test* sends the current
   patch and the unit shows it.
4. On 60: from the app select `1-1` *LD Echolocator*, `4-5` *LD Silo Lead*, `8-8`
   *1982 Synth Drum*: each shows on the unit. Flip to 106, tell the app (engine toggle),
   select `1-1` *Brass*, `4-5` *Reso Up*, `8-8` *Takeoff*.
5. Prev/next from `1-8` lands on `2-1` on the unit; next from 60 `8-8` stops (it does
   not cross into 106).
6. Follow-the-panel on, USB path: press `2-3` on the unit; the app follows without the
   unit re-selecting.
7. **Flip flow.** On 60 with a patch selected on both engines (step 4 did that): in the
   app select a 106 patch. Nothing must be sent; the display must read the flip prompt.
   Flip the switch to 106. The app must see the re-sent Program Change, switch its
   engine indicator to 106, send the selected patch, and the unit must show it.
8. **Hint flow.** Follow on, not armed: flip to 60. The app must not switch by itself;
   it shows the "flipped to 60?" hint, and the engine toggle accepts it.
9. If another Boutique is connected, switching the app's device to it and sending must
   not touch the JU-06A.
