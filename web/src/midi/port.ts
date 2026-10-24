// The MIDI interface the UI talks to. Web MIDI (W4) and the fake implement it.
// No SysEx, ever. Ports are identified by name (and manufacturer when present), never by id.
import { midiBytes, type PatchAddress } from '../core/address';

export const STALL_MS = 20_000;

export type AccessState = 'insecure' | 'unsupported' | 'prompt' | 'waiting' | 'denied' | 'granted';

export interface PortInfo {
  readonly name: string;
  readonly manufacturer?: string;
}

/** none: nothing chosen. open: ready. missing: chosen but not in the port list. error: could not open or lost. */
export type LinkState = 'none' | 'open' | 'missing' | 'error';

export interface LinkStatus {
  readonly state: LinkState;
  /** The error text when state is `error`. */
  readonly text?: string;
}

export interface MidiPort {
  readonly access: AccessState;
  /** In `waiting` for longer than STALL_MS: the browser never answered. One more requestAccess is then allowed. */
  readonly stalled: boolean;
  /** The user pressed CONNECT MIDI / TRY AGAIN. Moves `access` to `waiting`, then `granted` or `denied`. */
  requestAccess(): void;
  outputs(): readonly PortInfo[];
  inputs(): readonly PortInfo[];
  /** Choose the output by name; null = none. Opens it when it is present, and again when it reappears. */
  setOutput(port: PortInfo | null): void;
  setInput(port: PortInfo | null): void;
  readonly output: LinkStatus;
  readonly input: LinkStatus;
  /** Sends Bank Select MSB, Bank Select LSB and Program Change on channel 1..16. Null on success, else why not. */
  sendPatch(a: PatchAddress, channel: number): string | null;
  /** Access, port list or link status changed. */
  onChange(cb: () => void): () => void;
  /** Raw bytes of every message arriving on the open input. */
  onMessage(cb: (bytes: readonly number[]) => void): () => void;
}

export const samePort = (a: PortInfo | null, b: PortInfo | null): boolean =>
  a === b || (a !== null && b !== null && a.name === b.name && (a.manufacturer === undefined || b.manufacturer === undefined || a.manufacturer === b.manufacturer));

/** Listener plumbing shared by the implementations. */
export abstract class BasePort {
  private readonly changeListeners = new Set<() => void>();
  private readonly messageListeners = new Set<(bytes: readonly number[]) => void>();

  onChange(cb: () => void): () => void {
    this.changeListeners.add(cb);
    return () => this.changeListeners.delete(cb);
  }

  onMessage(cb: (bytes: readonly number[]) => void): () => void {
    this.messageListeners.add(cb);
    return () => this.messageListeners.delete(cb);
  }

  protected emitChange(): void {
    for (const cb of [...this.changeListeners]) cb();
  }

  protected emitMessage(bytes: readonly number[]): void {
    for (const cb of [...this.messageListeners]) cb(bytes);
  }
}

/** The three messages of a patch selection, or an error string for a bad channel. */
export function patchMessages(a: PatchAddress, channel: number): number[][] | string {
  try {
    return midiBytes(a, channel);
  } catch (e) {
    return e instanceof Error ? e.message : String(e);
  }
}
