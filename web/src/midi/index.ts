import { fakeFromQuery } from './fake';
import type { MidiPort } from './port';
import { StubMidi, detectAccess } from './stub';

/** The page's MIDI port: the fake with `?fake=1`, otherwise (until W4) the stub. */
export function createPort(win: Window): MidiPort {
  const fake = fakeFromQuery(win.location.search);
  if (fake) {
    (win as unknown as { __fakeMidi?: unknown }).__fakeMidi = fake;
    return fake;
  }
  return new StubMidi(detectAccess({ isSecureContext: win.isSecureContext, requestMIDIAccess: (win.navigator as { requestMIDIAccess?: unknown }).requestMIDIAccess }));
}
