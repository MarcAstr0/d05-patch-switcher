// Panel-style entry (DESIGN.md 6.4): group, then bank, then number; sends on the number.
import { fromParts, groupLabel, type PatchAddress } from '../core/address';

export type EntryPhase = 'idle' | 'bank' | 'number';

export class Entry {
  phase: EntryPhase = 'idle';
  group = 0;
  bank = 0;

  get pending(): boolean {
    return this.phase !== 'idle';
  }

  /** A group pad, or Ctrl+G: that group is chosen and a bank is awaited. */
  begin(group: number): void {
    this.phase = 'bank';
    this.group = group;
    this.bank = 0;
  }

  /** A bank pad. Idle: starts at "awaiting number" in the current group. */
  pressBank(bank: number, current: PatchAddress): void {
    if (bank < 1 || bank > 8) return;
    if (this.phase === 'idle') this.group = current.group;
    this.bank = bank;
    this.phase = 'number';
  }

  /**
   * A number pad. Idle: the address in the current group and bank. Awaiting number: commits.
   * Awaiting bank: ignored (null).
   */
  pressNumber(number: number, current: PatchAddress): PatchAddress | null {
    if (number < 1 || number > 8) return null;
    let a: PatchAddress | null = null;
    if (this.phase === 'idle') a = fromParts(current.group, current.bank, number);
    else if (this.phase === 'number') a = fromParts(this.group, this.bank, number);
    else return null;
    this.cancel();
    return a;
  }

  /** A digit key: the bank when idle or awaiting a bank, the number when awaiting one. */
  digit(d: number, current: PatchAddress): PatchAddress | null {
    if (d < 1 || d > 8) return null;
    if (this.phase === 'number') return this.pressNumber(d, current);
    this.pressBank(d, current);
    return null;
  }

  cancel(): void {
    this.phase = 'idle';
    this.bank = 0;
  }

  /** The echo for display line 1: `P3-__`, `P3-4_`; empty when idle. */
  text(): string {
    if (this.phase === 'idle') return '';
    return `${groupLabel(this.group)}-${this.bank > 0 ? this.bank : '_'}_`;
  }
}
