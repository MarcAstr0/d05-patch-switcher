// Patch addresses (docs/SPEC.md 3.1, docs/MIDI.md). No DOM.

export interface PatchAddress {
  /** 0..7 = U1..U8, 8..13 = P1..P6. */
  readonly group: number;
  /** 1..8 */
  readonly bank: number;
  /** 1..8 */
  readonly number: number;
  /** Catalogue index 0..895: group, then bank, then number. */
  readonly index: number;
}

export const CATALOG_SIZE = 896;
export const USER_GROUPS = 8;
export const BANK_SELECT_MSB = 87;

const all: PatchAddress[] = [];
for (let i = 0; i < CATALOG_SIZE; i++) {
  all.push(Object.freeze({ group: i >> 6, bank: ((i >> 3) & 7) + 1, number: (i & 7) + 1, index: i }));
}

export function fromIndex(index: number): PatchAddress | null {
  if (!Number.isInteger(index) || index < 0 || index >= CATALOG_SIZE) return null;
  return all[index] ?? null;
}

export function fromParts(group: number, bank: number, number: number): PatchAddress | null {
  if (!Number.isInteger(group) || group < 0 || group > 13) return null;
  if (!Number.isInteger(bank) || bank < 1 || bank > 8) return null;
  if (!Number.isInteger(number) || number < 1 || number > 8) return null;
  return fromIndex(group * 64 + (bank - 1) * 8 + (number - 1));
}

export function allAddresses(): readonly PatchAddress[] {
  return all;
}

export const isUser = (a: PatchAddress): boolean => a.group < USER_GROUPS;
export const isPreset = (a: PatchAddress): boolean => a.group >= USER_GROUPS;

/** `U3` or `P1`. */
export function groupLabel(group: number): string {
  return group < USER_GROUPS ? `U${group + 1}` : `P${group - 7}`;
}

/** Canonical text, e.g. `P3-45`. */
export function formatAddress(a: PatchAddress): string {
  return `${groupLabel(a.group)}-${a.bank}${a.number}`;
}

const isDigit = (c: string | undefined): boolean => c !== undefined && c >= '0' && c <= '9';

/** Accepts `P3-45`, `p345`, `3-45`, `345` (no letter means P), surrounding whitespace. ASCII digits only. */
export function parseAddress(text: string): PatchAddress | null {
  const s = text.trim();
  let pos = 0;
  let user = false;
  const c0 = s[0];
  if (c0 === 'U' || c0 === 'u') {
    user = true;
    pos = 1;
  } else if (c0 === 'P' || c0 === 'p') {
    pos = 1;
  }
  const rest = s.slice(pos);
  let digits: string;
  if (rest.length === 4 && rest[1] === '-') digits = rest[0]! + rest.slice(2);
  else if (rest.length === 3) digits = rest;
  else return null;
  for (const c of digits) if (!isDigit(c)) return null;
  const g = Number(digits[0]);
  if (g < 1 || g > (user ? 8 : 6)) return null;
  return fromParts(user ? g - 1 : g + 7, Number(digits[1]), Number(digits[2]));
}

export interface MidiTriplet {
  readonly msb: number;
  readonly lsb: number;
  readonly program: number;
}

export function midiTriplet(a: PatchAddress): MidiTriplet {
  return { msb: BANK_SELECT_MSB, lsb: a.group, program: (a.bank - 1) * 8 + (a.number - 1) };
}

/** The three messages in send order (Bank MSB, Bank LSB, Program Change) on channel 1..16. */
export function midiBytes(a: PatchAddress, channel: number): number[][] {
  const ch = Math.min(16, Math.max(1, Math.trunc(channel))) - 1;
  const t = midiTriplet(a);
  return [
    [0xb0 | ch, 0x00, t.msb],
    [0xb0 | ch, 0x20, t.lsb],
    [0xc0 | ch, t.program],
  ];
}

/** Inverse of midiTriplet; null unless msb is 87 and lsb/program are in range. */
export function fromMidi(msb: number, lsb: number, program: number): PatchAddress | null {
  if (msb !== BANK_SELECT_MSB || program < 0 || program > 63) return null;
  return fromParts(lsb, (program >> 3) + 1, (program & 7) + 1);
}

/** One step in catalogue order; clamps at U1-11 and P6-88 (FR-5). */
export function step(a: PatchAddress, delta: 1 | -1): PatchAddress {
  return fromIndex(a.index + delta) ?? a;
}
export const next = (a: PatchAddress): PatchAddress => step(a, 1);
export const prev = (a: PatchAddress): PatchAddress => step(a, -1);
