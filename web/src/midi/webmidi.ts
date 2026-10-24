// The real Web MIDI layer (docs/WEB.md section 5). Never requests SysEx. Ports are chosen and
// remembered by name (plus manufacturer), never by id; hot-plug comes from `onstatechange`.
import type { PatchAddress } from '../core/address';
import { findPort } from '../logic/settings';
import { BasePort, patchMessages, type AccessState, type LinkStatus, type MidiPort, type PortInfo } from './port';

export interface WebMidiEnv {
  isSecureContext?: boolean;
  navigator: Navigator;
}

export function detectAccess(env: { isSecureContext?: boolean; requestMIDIAccess?: unknown }): AccessState {
  if (env.isSecureContext === false) return 'insecure';
  if (typeof env.requestMIDIAccess !== 'function') return 'unsupported';
  return 'prompt';
}

interface Entry<T> extends PortInfo {
  readonly port: T;
}

function list<T extends MIDIPort>(map: ReadonlyMap<string, T> | undefined): Entry<T>[] {
  const out: Entry<T>[] = [];
  map?.forEach((p) => {
    if (p.state === 'disconnected') return;
    out.push({ name: p.name ?? '', manufacturer: p.manufacturer ? p.manufacturer : undefined, port: p });
  });
  return out;
}

const errText = (e: unknown): string => (e instanceof Error && e.message ? e.message : String(e));

export class WebMidi extends BasePort implements MidiPort {
  access: AccessState;
  output: LinkStatus = { state: 'none' };
  input: LinkStatus = { state: 'none' };

  private midi: MIDIAccess | null = null;
  private wantOut: PortInfo | null = null;
  private wantIn: PortInfo | null = null;
  private outPort: MIDIOutput | null = null;
  private inPort: MIDIInput | null = null;

  constructor(private readonly env: WebMidiEnv) {
    super();
    this.access = detectAccess({ isSecureContext: env.isSecureContext, requestMIDIAccess: (env.navigator as { requestMIDIAccess?: unknown }).requestMIDIAccess });
    if (this.access === 'prompt') void this.watchPermission();
  }

  /** Reads the stored permission where the browser can tell. A granted permission needs no click. */
  private async watchPermission(): Promise<void> {
    try {
      const status = await this.env.navigator.permissions.query({ name: 'midi' as PermissionName });
      status.onchange = () => this.onPermission(status.state);
      this.onPermission(status.state);
    } catch {
      // Not supported (Firefox, Safari): the button is the only way.
    }
  }

  private onPermission(state: PermissionState): void {
    if (state === 'granted') {
      if (this.access === 'prompt' || this.access === 'denied') this.requestAccess();
    } else if (state === 'denied') {
      if (this.access !== 'denied' && this.access !== 'insecure' && this.access !== 'unsupported') {
        this.detach();
        this.access = 'denied';
        this.refresh();
      }
    } else if (this.access === 'denied') {
      this.access = 'prompt';
      this.emitChange();
    }
  }

  requestAccess(): void {
    if (this.access !== 'prompt' && this.access !== 'denied') return;
    this.access = 'waiting';
    this.emitChange();
    let p: Promise<MIDIAccess>;
    try {
      p = this.env.navigator.requestMIDIAccess({ sysex: false });
    } catch {
      this.access = 'denied';
      this.emitChange();
      return;
    }
    p.then(
      (a) => {
        this.midi = a;
        a.onstatechange = () => this.refresh();
        this.access = 'granted';
        this.refresh();
      },
      () => {
        this.access = 'denied';
        this.refresh();
      },
    );
  }

  outputs(): readonly PortInfo[] {
    return this.access === 'granted' ? list(this.midi?.outputs) : [];
  }

  inputs(): readonly PortInfo[] {
    return this.access === 'granted' ? list(this.midi?.inputs) : [];
  }

  setOutput(port: PortInfo | null): void {
    this.wantOut = port;
    this.detachOutput();
    this.refresh();
  }

  setInput(port: PortInfo | null): void {
    this.wantIn = port;
    this.detachInput();
    this.refresh();
  }

  sendPatch(a: PatchAddress, channel: number): string | null {
    const out = this.outPort;
    if (this.output.state !== 'open' || !out) return this.output.state === 'none' ? 'No output port chosen' : (this.output.text ?? 'Port not found');
    const msgs = patchMessages(a, channel);
    if (typeof msgs === 'string') return msgs;
    try {
      for (const m of msgs) out.send(m);
    } catch (e) {
      const text = errText(e);
      this.output = { state: 'error', text };
      this.emitChange();
      return text;
    }
    return null;
  }

  // ---------------------------------------------------------------- internals

  private detachOutput(): void {
    this.outPort = null;
    this.output = { state: 'none' };
  }

  private detachInput(): void {
    if (this.inPort) this.inPort.onmidimessage = null;
    this.inPort = null;
    this.input = { state: 'none' };
  }

  private detach(): void {
    this.detachOutput();
    this.detachInput();
    if (this.midi) this.midi.onstatechange = null;
    this.midi = null;
  }

  /** Re-reads the port lists and brings both links in line with the saved names. */
  private refresh(): void {
    if (this.access !== 'granted' || !this.midi) {
      this.outPort = null;
      this.output = { state: 'none' };
      if (this.inPort) this.inPort.onmidimessage = null;
      this.inPort = null;
      this.input = { state: 'none' };
      this.emitChange();
      return;
    }
    this.refreshOutput();
    this.refreshInput();
    this.emitChange();
  }

  private refreshOutput(): void {
    if (!this.wantOut) return this.detachOutput();
    const found = findPort(list(this.midi?.outputs), this.wantOut);
    if (!found) {
      this.outPort = null;
      this.output = { state: 'missing' };
      return;
    }
    if (found.port === this.outPort && this.output.state !== 'none' && this.output.state !== 'missing') return;
    this.outPort = found.port;
    this.output = { state: 'open' };
    found.port.open().catch((e: unknown) => {
      if (this.outPort !== found.port) return;
      this.output = { state: 'error', text: errText(e) };
      this.emitChange();
    });
  }

  private refreshInput(): void {
    if (!this.wantIn) return this.detachInput();
    const found = findPort(list(this.midi?.inputs), this.wantIn);
    if (!found) {
      if (this.inPort) this.inPort.onmidimessage = null;
      this.inPort = null;
      this.input = { state: 'missing' };
      return;
    }
    if (found.port === this.inPort && this.input.state === 'open') return;
    if (this.inPort) this.inPort.onmidimessage = null;
    this.inPort = found.port;
    found.port.onmidimessage = (e) => this.onMessageEvent(e);
    this.input = { state: 'open' };
    found.port.open().catch((e: unknown) => {
      if (this.inPort !== found.port) return;
      this.input = { state: 'error', text: errText(e) };
      this.emitChange();
    });
  }

  /** The D-05 sends clock continuously: drop realtime and system messages before anything else. */
  private onMessageEvent(e: MIDIMessageEvent): void {
    const d = e.data;
    if (!d || d.length < 2 || d[0]! >= 0xf0) return;
    this.emitMessage(d as unknown as readonly number[]);
  }
}
