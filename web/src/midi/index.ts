import { fakeFromQuery } from './fake';
import type { MidiPort } from './port';
import { WebMidi } from './webmidi';

/** The page's MIDI port: the fake with `?fake=1`, otherwise Web MIDI. */
export function createPort(win: Window): MidiPort {
  const fake = fakeFromQuery(win.location.search);
  if (fake) {
    (win as unknown as { __fakeMidi?: unknown }).__fakeMidi = fake;
    return fake;
  }
  return new WebMidi({ isSecureContext: win.isSecureContext, navigator: win.navigator });
}
