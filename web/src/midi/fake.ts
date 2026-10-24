// A MidiPort that records bytes. Used by the Playwright tests and by the `?fake=1` page flag.
// `?fake=1&access=prompt|denied|unsupported|insecure|waiting` starts in that access state;
// `&ports=none` starts with no ports; `&grant=deny` makes CONNECT MIDI end in `denied`.
import type { PatchAddress } from '../core/address';
import { findPort } from '../logic/settings';
import { BasePort, patchMessages, samePort, type AccessState, type LinkStatus, type MidiPort, type PortInfo } from './port';

export interface FakeOptions {
  access?: AccessState;
  outputs?: PortInfo[];
  inputs?: PortInfo[];
  /** What a permission request ends in. */
  grant?: 'granted' | 'denied';
}

export const FAKE_OUT: PortInfo = { name: 'Fake D-05 Out', manufacturer: 'Fake' };
export const FAKE_IN: PortInfo = { name: 'Fake D-05 In', manufacturer: 'Fake' };

export class FakeMidi extends BasePort implements MidiPort {
  access: AccessState;
  /** Every message sent, in order. */
  readonly sent: number[][] = [];
  /** Pending request outcome (set by tests or the `grant` option). */
  grant: 'granted' | 'denied';
  private outs: PortInfo[];
  private ins: PortInfo[];
  private wantOut: PortInfo | null = null;
  private wantIn: PortInfo | null = null;
  private outStatus: LinkStatus = { state: 'none' };
  private inStatus: LinkStatus = { state: 'none' };
  private sendError: string | null = null;

  constructor(opts: FakeOptions = {}) {
    super();
    this.access = opts.access ?? 'granted';
    this.grant = opts.grant ?? 'granted';
    this.outs = opts.outputs ?? [FAKE_OUT];
    this.ins = opts.inputs ?? [FAKE_IN];
  }

  get output(): LinkStatus {
    return this.outStatus;
  }
  get input(): LinkStatus {
    return this.inStatus;
  }
  outputs(): readonly PortInfo[] {
    return this.access === 'granted' ? this.outs : [];
  }
  inputs(): readonly PortInfo[] {
    return this.access === 'granted' ? this.ins : [];
  }

  requestAccess(): void {
    if (this.access === 'insecure' || this.access === 'unsupported' || this.access === 'waiting') return;
    this.access = 'waiting';
    this.emitChange();
    queueMicrotask(() => {
      this.access = this.grant;
      this.refresh();
    });
  }

  setOutput(port: PortInfo | null): void {
    this.wantOut = port;
    this.outStatus = { state: 'none' };
    this.refresh();
  }

  setInput(port: PortInfo | null): void {
    this.wantIn = port;
    this.refresh();
  }

  sendPatch(a: PatchAddress, channel: number): string | null {
    if (this.outStatus.state !== 'open') return this.outStatus.state === 'none' ? 'No output port chosen' : (this.outStatus.text ?? 'Port not found');
    const msgs = patchMessages(a, channel);
    if (typeof msgs === 'string') return msgs;
    if (this.sendError) {
      const e = this.sendError;
      this.sendError = null;
      this.outStatus = { state: 'error', text: e };
      this.emitChange();
      return e;
    }
    for (const m of msgs) this.sent.push(m);
    return null;
  }

  // ---- test controls (also reachable as window.__fakeMidi when the page runs with ?fake=1)

  /** Hot-plug: a port appears. */
  plug(kind: 'out' | 'in', port: PortInfo): void {
    const list = kind === 'out' ? this.outs : this.ins;
    if (!list.some((p) => samePort(p, port))) list.push(port);
    this.refresh();
  }

  /** Hot-unplug: a port disappears. */
  unplug(kind: 'out' | 'in', name: string): void {
    if (kind === 'out') this.outs = this.outs.filter((p) => p.name !== name);
    else this.ins = this.ins.filter((p) => p.name !== name);
    this.refresh();
  }

  /** A message arrives on the open input. */
  inject(bytes: readonly number[]): void {
    if (this.inStatus.state === 'open') this.emitMessage(bytes);
  }

  /** The next send fails with this text. */
  failNextSend(text: string): void {
    this.sendError = text;
  }

  setAccess(state: AccessState): void {
    this.access = state;
    this.refresh();
  }

  /** The sent messages as hex strings, three per patch: `B0 00 57`. */
  hex(): string[] {
    return this.sent.map((m) => m.map((b) => b.toString(16).toUpperCase().padStart(2, '0')).join(' '));
  }

  clearSent(): void {
    this.sent.length = 0;
  }

  private refresh(): void {
    this.outStatus = this.link(this.wantOut, this.outputs(), this.outStatus);
    this.inStatus = this.link(this.wantIn, this.inputs(), this.inStatus);
    this.emitChange();
  }

  private link(want: PortInfo | null, list: readonly PortInfo[], prev: LinkStatus): LinkStatus {
    if (!want) return { state: 'none' };
    if (this.access !== 'granted') return { state: 'none' };
    if (!findPort(list, want)) return { state: 'missing' };
    return prev.state === 'error' && prev.text ? prev : { state: 'open' };
  }
}

/** Reads the page's query string; null when `?fake=1` is absent. */
export function fakeFromQuery(search: string): FakeMidi | null {
  const q = new URLSearchParams(search);
  if (q.get('fake') !== '1') return null;
  const access = q.get('access');
  const states: AccessState[] = ['insecure', 'unsupported', 'prompt', 'waiting', 'denied', 'granted'];
  return new FakeMidi({
    access: states.find((s) => s === access) ?? 'granted',
    outputs: q.get('ports') === 'none' ? [] : undefined,
    inputs: q.get('ports') === 'none' ? [] : undefined,
    grant: q.get('grant') === 'deny' ? 'denied' : 'granted',
  });
}
