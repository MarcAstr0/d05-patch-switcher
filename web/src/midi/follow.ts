// Follow-the-panel parsing (SPEC FR-11, docs/MIDI.md): CC 0 / CC 32 / Program Change on the
// channel move the current patch without sending, ignoring our own sends looped back for 300 ms.
import { BANK_SELECT_MSB, fromParts, midiTriplet, type PatchAddress } from '../core/address';

export const ECHO_MS = 300;

export class FollowParser {
  private rxMsb = -1;
  private rxLsb = -1;
  private sent: { key: string; at: number }[] = [];

  constructor(private readonly clock: () => number = () => performance.now()) {}

  /** Call after every successful send. */
  noteSent(a: PatchAddress): void {
    const now = this.clock();
    this.prune(now);
    this.sent.push({ key: tripletKey(a), at: now });
  }

  /**
   * Feeds one MIDI message. Returns the address to move to, or null (not a complete selection on
   * this channel, our own echo, or already current).
   */
  feed(bytes: readonly number[], channel: number, current: PatchAddress | null): PatchAddress | null {
    if (bytes.length < 2) return null;
    const status = bytes[0]!;
    if ((status & 0x0f) !== channel - 1) return null;
    const type = status & 0xf0;
    const d1 = bytes[1]!;
    if (type === 0xb0 && bytes.length >= 3) {
      if (d1 === 0) this.rxMsb = bytes[2]!;
      else if (d1 === 32) this.rxLsb = bytes[2]!;
      return null;
    }
    if (type !== 0xc0) return null;
    // A Program Change completes the triplet and consumes the bank state. Without a prior Bank
    // Select the D-05 stays in its group, so fall back to the current group.
    const msb = this.rxMsb < 0 ? BANK_SELECT_MSB : this.rxMsb;
    let lsb = this.rxLsb;
    this.rxMsb = this.rxLsb = -1;
    if (lsb < 0 && current) lsb = current.group;
    if (msb !== BANK_SELECT_MSB || lsb < 0 || d1 > 63) return null;
    const a = fromParts(lsb, (d1 >> 3) + 1, (d1 & 7) + 1);
    if (!a) return null;
    this.prune(this.clock());
    if (this.sent.some((s) => s.key === tripletKey(a))) return null;
    if (current && current.index === a.index) return null;
    return a;
  }

  reset(): void {
    this.rxMsb = this.rxLsb = -1;
  }

  private prune(now: number): void {
    this.sent = this.sent.filter((s) => now - s.at < ECHO_MS);
  }
}

function tripletKey(a: PatchAddress): string {
  const t = midiTriplet(a);
  return `${t.msb}/${t.lsb}/${t.program}`;
}
