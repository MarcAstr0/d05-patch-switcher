import { describe, expect, test } from 'vitest';
import { fromParts } from '../../src/core/address';
import { FollowParser } from '../../src/midi/follow';
import { WebMidi } from '../../src/midi/webmidi';
import { MockPort, flush, mockNavigator } from './midi-mock';

const hex = (m: number[][]) => m.map((x) => x.map((b) => b.toString(16).padStart(2, '0')).join(' '));

async function granted(opts: Parameters<typeof mockNavigator>[0] = {}) {
  const m = mockNavigator(opts);
  const w = new WebMidi({ isSecureContext: true, navigator: m.nav });
  await flush();
  if (w.access === 'prompt') {
    w.requestAccess();
    m.settle(true);
    await flush();
  }
  return { m, w };
}

describe('access states', () => {
  test('insecure and unsupported', () => {
    const nav = mockNavigator().nav;
    expect(new WebMidi({ isSecureContext: false, navigator: nav }).access).toBe('insecure');
    expect(new WebMidi({ isSecureContext: true, navigator: {} as Navigator }).access).toBe('unsupported');
  });

  test('prompt -> waiting -> granted, without sysex', async () => {
    const m = mockNavigator();
    const w = new WebMidi({ isSecureContext: true, navigator: m.nav });
    await flush();
    expect(w.access).toBe('prompt');
    const seen: string[] = [];
    w.onChange(() => seen.push(w.access));
    w.requestAccess();
    expect(w.access).toBe('waiting');
    m.settle(true);
    await flush();
    expect(w.access).toBe('granted');
    expect(seen).toContain('waiting');
    expect(m.requests).toEqual([{ sysex: false }]);
  });

  test('refusal gives denied, and TRY AGAIN can ask once more', async () => {
    const m = mockNavigator();
    const w = new WebMidi({ isSecureContext: true, navigator: m.nav });
    w.requestAccess();
    m.settle(false);
    await flush();
    expect(w.access).toBe('denied');
    w.requestAccess();
    expect(w.access).toBe('waiting');
    m.settle(true);
    await flush();
    expect(w.access).toBe('granted');
  });

  test('stored permission granted connects without a click', async () => {
    const { w } = await granted({ permission: 'granted', auto: true });
    expect(w.access).toBe('granted');
  });

  test('stored permission denied starts denied; reset to prompt returns to prompt', async () => {
    const m = mockNavigator({ permission: 'denied' });
    const w = new WebMidi({ isSecureContext: true, navigator: m.nav });
    await flush();
    expect(w.access).toBe('denied');
    m.setPermission('prompt');
    expect(w.access).toBe('prompt');
    expect(m.requests).toHaveLength(0);
  });

  test('permission revoked while granted', async () => {
    const { m, w } = await granted({ permission: 'granted', auto: true });
    m.setPermission('denied');
    expect(w.access).toBe('denied');
    expect(w.outputs()).toEqual([]);
  });

  test('permissions API missing or throwing leaves the button path', async () => {
    const m = mockNavigator({ permission: 'unsupported' });
    const w = new WebMidi({ isSecureContext: true, navigator: m.nav });
    await flush();
    expect(w.access).toBe('prompt');
  });
});

describe('sending', () => {
  for (const ch of [1, 3, 10, 16]) {
    test(`exact bytes on channel ${ch}`, async () => {
      const { m, w } = await granted();
      const out = new MockPort('o1', 'Hapax', 'Squarp');
      m.access.add('out', out);
      w.setOutput({ name: 'Hapax', manufacturer: 'Squarp' });
      expect(w.output.state).toBe('open');
      expect(w.sendPatch(fromParts(8, 3, 5)!, ch)).toBeNull(); // P1-35: LSB 8, program 20 = 0x14
      const c = ch - 1;
      expect(hex(out.sent)).toEqual([(0xb0 | c).toString(16) + ' 00 57', (0xb0 | c).toString(16) + ' 20 08', (0xc0 | c).toString(16) + ' 14']);
    });
  }

  test('user bank U8-88 is LSB 7, program 63', async () => {
    const { m, w } = await granted();
    const out = new MockPort('o1', 'X', '');
    m.access.add('out', out);
    w.setOutput({ name: 'X' });
    w.sendPatch(fromParts(7, 8, 8)!, 1);
    expect(out.sent).toEqual([[0xb0, 0, 87], [0xb0, 32, 7], [0xc0, 63]]);
  });

  test('bad channel is reported, nothing sent', async () => {
    const { m, w } = await granted();
    const out = new MockPort('o1', 'X', '');
    m.access.add('out', out);
    w.setOutput({ name: 'X' });
    expect(typeof w.sendPatch(fromParts(0, 1, 1)!, 17)).toBe('string');
    expect(out.sent).toEqual([]);
  });

  test('send throwing becomes status text, not an exception', async () => {
    const { m, w } = await granted();
    const out = new MockPort('o1', 'X', '');
    m.access.add('out', out);
    w.setOutput({ name: 'X' });
    out.failSend = new Error('port closed');
    expect(w.sendPatch(fromParts(0, 1, 1)!, 1)).toBe('port closed');
    expect(w.output).toEqual({ state: 'error', text: 'port closed' });
  });

  test('no output chosen', async () => {
    const { w } = await granted();
    expect(w.sendPatch(fromParts(0, 1, 1)!, 1)).toBe('No output port chosen');
  });
});

describe('hot-plug', () => {
  test('saved port missing at start, appears, vanishes, returns by name (new id)', async () => {
    const { m, w } = await granted();
    w.setOutput({ name: 'Hapax', manufacturer: 'Squarp' });
    expect(w.output.state).toBe('missing');
    const opened: boolean[] = [];
    w.onChange(() => opened.push(w.output.state === 'open'));
    const a = new MockPort('id-1', 'Hapax', 'Squarp');
    m.access.add('out', a);
    expect(w.output.state).toBe('open');
    expect(a.opened).toBe(1);
    m.access.remove('out', 'id-1');
    expect(w.output.state).toBe('missing');
    expect(w.sendPatch(fromParts(0, 1, 1)!, 1)).toBe('Port not found');
    const b = new MockPort('id-2', 'Hapax', 'Squarp');
    m.access.add('out', b);
    expect(w.output.state).toBe('open');
    w.sendPatch(fromParts(0, 1, 1)!, 1);
    expect(b.sent).toHaveLength(3);
    expect(a.sent).toHaveLength(0);
  });

  test('unrelated port changes do not reopen or re-announce the open port', async () => {
    const { m, w } = await granted();
    const a = new MockPort('1', 'Hapax', 'Squarp');
    m.access.add('out', a);
    w.setOutput({ name: 'Hapax' });
    m.access.add('out', new MockPort('2', 'Other', ''));
    expect(a.opened).toBe(1);
  });

  test('a port that stays listed but reports disconnected counts as missing', async () => {
    const { m, w } = await granted();
    const a = new MockPort('1', 'Hapax', '');
    m.access.add('out', a);
    w.setOutput({ name: 'Hapax' });
    a.state = 'disconnected';
    m.access.onstatechange?.();
    expect(w.output.state).toBe('missing');
    expect(w.outputs()).toEqual([]);
  });

  test('same-named ports are told apart by manufacturer', async () => {
    const { m, w } = await granted();
    const a = new MockPort('1', 'USB MIDI', 'Acme');
    const b = new MockPort('2', 'USB MIDI', 'Other Co');
    m.access.add('out', a);
    m.access.add('out', b);
    expect(w.outputs().map((p) => p.manufacturer)).toEqual(['Acme', 'Other Co']);
    w.setOutput({ name: 'USB MIDI', manufacturer: 'Other Co' });
    w.sendPatch(fromParts(0, 1, 1)!, 1);
    expect(a.sent).toHaveLength(0);
    expect(b.sent).toHaveLength(3);
    // a saved port without a manufacturer matches by name alone
    w.setOutput({ name: 'USB MIDI' });
    expect(w.output.state).toBe('open');
  });
});

describe('input and follow mode', () => {
  test('clock bytes are dropped; a selection between clocks is followed once', async () => {
    const { m, w } = await granted();
    const inp = new MockPort('i', 'Hapax', 'Squarp');
    m.access.add('in', inp);
    w.setInput({ name: 'Hapax', manufacturer: 'Squarp' });
    expect(w.input.state).toBe('open');
    const got: number[][] = [];
    w.onMessage((b) => got.push([...b]));
    const f = new FollowParser();
    const moves: (number | null)[] = [];
    w.onMessage((b) => {
      const r = f.feed(b, 3, null);
      if (r) moves.push(r.index);
    });
    inp.receive(0xf8);
    inp.receive(0xb2, 0, 87);
    inp.receive(0xf8);
    inp.receive(0xfe);
    inp.receive(0xb2, 32, 8);
    inp.receive(0xf8);
    inp.receive(0xc2, 20);
    inp.receive(0xf8);
    expect(got).toEqual([[0xb2, 0, 87], [0xb2, 32, 8], [0xc2, 20]]);
    expect(moves).toEqual([fromParts(8, 3, 5)!.index]);
  });

  test('our own send looped back is not followed', async () => {
    const { m, w } = await granted();
    const inp = new MockPort('i', 'Loop', '');
    const out = new MockPort('o', 'Loop', '');
    m.access.add('in', inp);
    m.access.add('out', out);
    w.setInput({ name: 'Loop' });
    w.setOutput({ name: 'Loop' });
    const f = new FollowParser();
    const moves: number[] = [];
    w.onMessage((b) => {
      const r = f.feed(b, 1, null);
      if (r) moves.push(r.index);
    });
    const a = fromParts(8, 2, 2)!;
    w.sendPatch(a, 1);
    f.noteSent(a);
    for (const msg of out.sent) inp.receive(...msg);
    expect(moves).toEqual([]);
    // a different selection from the panel is followed
    inp.receive(0xb0, 0, 87);
    inp.receive(0xb0, 32, 9);
    inp.receive(0xc0, 0);
    expect(moves).toEqual([fromParts(9, 1, 1)!.index]);
  });

  test('switching the input detaches the old handler; input vanishing is missing', async () => {
    const { m, w } = await granted();
    const a = new MockPort('1', 'A', '');
    const b = new MockPort('2', 'B', '');
    m.access.add('in', a);
    m.access.add('in', b);
    w.setInput({ name: 'A' });
    w.setInput({ name: 'B' });
    expect(a.onmidimessage).toBeNull();
    expect(b.onmidimessage).not.toBeNull();
    m.access.remove('in', '2');
    expect(w.input.state).toBe('missing');
    expect(b.onmidimessage).toBeNull();
    w.setInput(null);
    expect(w.input.state).toBe('none');
  });
});
