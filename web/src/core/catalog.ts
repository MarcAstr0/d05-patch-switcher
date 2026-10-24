// The 896-entry catalogue: 384 preset names from data/d05-presets.tsv (bundled at build
// time, no fetch) plus the user-slot name overlay. No DOM.
import tsv from '../../../data/d05-presets.tsv?raw';
import { CATALOG_SIZE, allAddresses, fromParts, isUser, type PatchAddress } from './address';

export const MAX_USER_NAME = 18;

export type KeyMode = 'W' | 'D' | 'S';

export interface PresetRow {
  readonly address: PatchAddress;
  readonly name: string;
  readonly mode: KeyMode;
}

export function parsePresetTable(text: string): PresetRow[] {
  const rows: PresetRow[] = [];
  const seen = new Set<number>();
  const lines = text.split(/\r?\n/);
  for (let i = 1; i < lines.length; i++) {
    const line = lines[i]!;
    if (line === '') continue;
    const c = line.split('\t');
    if (c.length < 5) continue;
    const g = c[0]!;
    if (g.length !== 2 || g[0] !== 'P' || g[1]! < '1' || g[1]! > '6') continue;
    if (!/^\d$/.test(c[1]!) || !/^\d$/.test(c[2]!)) continue;
    const address = fromParts(7 + Number(g[1]), Number(c[1]), Number(c[2]));
    const name = c[3]!;
    const mode = c[4]!;
    if (!address || name === '' || (mode !== 'W' && mode !== 'D' && mode !== 'S')) continue;
    if (seen.has(address.index)) continue;
    seen.add(address.index);
    rows.push({ address, name, mode });
  }
  return rows;
}

/** Printable ASCII only, whitespace runs become one space, at most 18 characters, trimmed. */
export function sanitizeUserName(raw: string): string {
  let clean = '';
  for (let i = 0; i < raw.length; i++) {
    const u = raw.charCodeAt(i);
    if (u === 9 || u === 10 || u === 11 || u === 12 || u === 13) clean += ' ';
    else if (u >= 0x20 && u <= 0x7e) clean += raw[i];
  }
  return clean.replace(/ +/g, ' ').trim().slice(0, MAX_USER_NAME).trim();
}

const presetNames: string[] = new Array<string>(CATALOG_SIZE).fill('');
const presetModes: (KeyMode | '')[] = new Array<KeyMode | ''>(CATALOG_SIZE).fill('');
for (const r of parsePresetTable(tsv)) {
  presetNames[r.address.index] = r.name;
  presetModes[r.address.index] = r.mode;
}

export class Catalog {
  private readonly userNames = new Map<number, string>();

  /** Catalogue name: preset name, user name, or empty. */
  name(a: PatchAddress): string {
    return isUser(a) ? (this.userNames.get(a.index) ?? '') : presetNames[a.index]!;
  }

  /** Key mode of a preset; empty for user slots. */
  mode(a: PatchAddress): KeyMode | '' {
    return presetModes[a.index]!;
  }

  hasUserName(a: PatchAddress): boolean {
    return this.userNames.has(a.index);
  }

  /** Returns false for preset slots. An empty result removes the name. */
  setUserName(a: PatchAddress, raw: string): boolean {
    if (!isUser(a)) return false;
    const clean = sanitizeUserName(raw);
    if (clean === '') this.userNames.delete(a.index);
    else this.userNames.set(a.index, clean);
    return true;
  }

  clearUserNames(): void {
    this.userNames.clear();
  }

  /** User names in catalogue order. */
  userNameEntries(): [PatchAddress, string][] {
    return allAddresses()
      .filter((a) => this.userNames.has(a.index))
      .map((a) => [a, this.userNames.get(a.index)!]);
  }

  presetCount(): number {
    return presetNames.reduce((n, s) => n + (s === '' ? 0 : 1), 0);
  }
}
