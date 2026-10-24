// What the display says (docs/design/DESIGN.md 5.2, DESIGN-WEB 5.3). Pure.
import { formatAddress, type PatchAddress } from '../core/address';
import type { KeyMode } from '../core/catalog';
import type { AccessState } from '../midi/port';

export interface LcdInput {
  columns: 20 | 40;
  current: PatchAddress;
  name: string;
  mode: KeyMode | '';
  /** The entry echo (`P3-4_`), empty when no entry is pending. */
  entryText: string;
  access: AccessState;
  /** Some output port has been saved. */
  outputConfigured: boolean;
  outputOpen: boolean;
  sentFlash: boolean;
  /** Show the match count: the search is active (portrait: and the overlay is open). */
  showCount: boolean;
  matches: number;
}

const MODE_WORD: Record<KeyMode, string> = { W: 'WHOLE', D: 'DUAL', S: 'SPLIT' };

const GROUP_LONG = ['PRESET 1: ORIGINAL D-50', 'PRESET 2: NEW FOR D-05', 'PRESET 3: PN-D50-01', 'PRESET 4: PN-D50-02', 'PRESET 5: PN-D50-03', 'PRESET 6: PN-D50-04'];

export const groupLongName = (group: number): string => (group < 8 ? `USER ${group + 1}` : GROUP_LONG[group - 8]!);

export const matchText = (n: number): string => (n === 0 ? 'NO MATCHES' : n === 1 ? '1 MATCH' : `${n} MATCHES`);

function context(i: LcdInput): { wide: string; narrow: string } | null {
  switch (i.access) {
    case 'insecure':
    case 'unsupported':
      return { wide: 'NO WEB MIDI IN THIS BROWSER', narrow: 'NO WEB MIDI' };
    case 'denied':
      return { wide: 'MIDI ACCESS BLOCKED', narrow: 'MIDI BLOCKED' };
    case 'prompt':
    case 'waiting':
      return { wide: 'PRESS CONNECT MIDI', narrow: 'CONNECT MIDI' };
    case 'granted':
      break;
  }
  if (!i.outputConfigured) return { wide: 'CHOOSE A PORT IN SETTINGS', narrow: 'CHOOSE A PORT' };
  if (!i.outputOpen) return { wide: 'NO MIDI OUTPUT', narrow: 'NO MIDI OUTPUT' };
  if (i.sentFlash) return { wide: 'SENT', narrow: 'SENT' };
  if (i.showCount) return { wide: matchText(i.matches), narrow: matchText(i.matches) };
  return null;
}

/** The two display lines. They may be shorter than the column count; the renderer pads. */
export function lcdLines(i: LcdInput): [string, string] {
  const ctx = context(i);
  if (i.columns === 40) {
    let l1: string;
    if (i.entryText) l1 = i.entryText;
    else {
      const head = `${formatAddress(i.current)}:${i.name.slice(0, 20)}`;
      const mode = i.mode ? MODE_WORD[i.mode] : '';
      l1 = mode ? head.padEnd(40 - mode.length) + mode : head;
    }
    return [l1, ctx ? ctx.wide : groupLongName(i.current.group)];
  }
  const l1 = i.entryText ? '' : i.name.slice(0, 20);
  const addr = i.entryText || formatAddress(i.current);
  const c = ctx ? ctx.narrow : i.mode ? MODE_WORD[i.mode] : '';
  return [l1, c ? `${addr} ${c.slice(0, 14)}` : addr];
}
