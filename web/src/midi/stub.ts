// Placeholder for the real Web MIDI layer (W4): detects the unsupported and insecure states for
// real, offers no ports, and sends nothing. Replaced by webmidi.ts in W4.
import type { PatchAddress } from '../core/address';
import { BasePort, type AccessState, type LinkStatus, type MidiPort, type PortInfo } from './port';

export function detectAccess(env: { isSecureContext?: boolean; requestMIDIAccess?: unknown }): AccessState {
  if (env.isSecureContext === false) return 'insecure';
  if (typeof env.requestMIDIAccess !== 'function') return 'unsupported';
  return 'prompt';
}

export class StubMidi extends BasePort implements MidiPort {
  access: AccessState;
  readonly output: LinkStatus = { state: 'none' };
  readonly input: LinkStatus = { state: 'none' };

  constructor(initial: AccessState) {
    super();
    this.access = initial;
  }
  requestAccess(): void {
    if (this.access !== 'prompt' && this.access !== 'denied') return;
    this.access = 'granted';
    this.emitChange();
  }
  outputs(): readonly PortInfo[] {
    return [];
  }
  inputs(): readonly PortInfo[] {
    return [];
  }
  setOutput(): void {}
  setInput(): void {}
  sendPatch(_a: PatchAddress, _channel: number): string | null {
    return 'MIDI is not available yet';
  }
}
