import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, test } from 'vitest';
import { fromParts, formatAddress, parseAddress } from '../../src/core/address';
import { Catalog } from '../../src/core/catalog';
import { Library, MemoryStorage } from '../../src/core/library';
import { accessContent, browserKind, type Env } from '../../src/logic/access';
import { Entry } from '../../src/logic/entry';
import { BINDINGS, capText, matchKey, sheetSections, type KeyLike } from '../../src/logic/keymap';
import { chooseLayout, computeMetrics, desktopDisplay, phoneDisplay, portraitMetrics, portraitRows } from '../../src/logic/layout';
import { lcdLines, type LcdInput } from '../../src/logic/lcd';
import { findPort, parseSettings } from '../../src/logic/settings';
import { buildRows, emptyText, rowOf } from '../../src/logic/views';
import { FakeMidi } from '../../src/midi/fake';
import { FollowParser } from '../../src/midi/follow';
import { glyphRows } from '../../src/ui/glyphs';

const A = (t: string) => parseAddress(t)!;

describe('entry state machine', () => {
  test('group, bank, number commits', () => {
    const e = new Entry();
    const cur = A('P1-11');
    e.begin(10);
    expect(e.text()).toBe('P3-__');
    e.pressBank(4, cur);
    expect(e.text()).toBe('P3-4_');
    expect(formatAddress(e.pressNumber(5, cur)!)).toBe('P3-45');
    expect(e.phase).toBe('idle');
    expect(e.text()).toBe('');
  });
  test('number while idle sends in the current group and bank; while awaiting a bank it is ignored', () => {
    const e = new Entry();
    expect(formatAddress(e.pressNumber(7, A('P2-35'))!)).toBe('P2-37');
    e.begin(3);
    expect(e.pressNumber(2, A('P2-35'))).toBeNull();
    expect(e.phase).toBe('bank');
  });
  test('bank while idle starts at awaiting number in the current group', () => {
    const e = new Entry();
    e.pressBank(6, A('U3-11'));
    expect(e.phase).toBe('number');
    expect(e.text()).toBe('U3-6_');
  });
  test('digits: idle starts an entry, second digit commits, cancel returns to idle', () => {
    const e = new Entry();
    expect(e.digit(3, A('P1-11'))).toBeNull();
    expect(e.phase).toBe('number');
    expect(formatAddress(e.digit(8, A('P1-11'))!)).toBe('P1-38');
    e.begin(0);
    e.cancel();
    expect(e.pending).toBe(false);
    expect(e.digit(9, A('P1-11'))).toBeNull();
  });
});

describe('views and search', () => {
  const catalog = new Catalog();
  const lib = new Library(catalog, new MemoryStorage());
  lib.load();
  test('all has 896 rows, search narrows, ranges are reported', () => {
    expect(buildRows('all', '', catalog, lib)).toHaveLength(896);
    const r = buildRows('all', 'brass', catalog, lib);
    expect(r.length).toBeGreaterThan(5);
    expect(r[0]!.ranges.length).toBe(1);
    expect(buildRows('all', 'p345', catalog, lib).map((x) => formatAddress(x.address))).toEqual(['P3-45']);
  });
  test('favourites and recents views filter and keep recents order and counts', () => {
    lib.setFavorite(A('P2-11'), true);
    lib.setFavorite(A('P1-11'), true);
    expect(buildRows('favorites', '', catalog, lib).map((x) => formatAddress(x.address))).toEqual(['P1-11', 'P2-11']);
    lib.recordSend(A('P1-11'));
    lib.recordSend(A('P3-45'));
    lib.recordSend(A('P1-11'));
    const rec = buildRows('recents', '', catalog, lib);
    expect(rec.map((x) => [formatAddress(x.address), x.count])).toEqual([['P1-11', 2], ['P3-45', 1]]);
    expect(buildRows('recents', 'fantasia', catalog, lib)).toHaveLength(1);
    expect(rowOf(rec, A('P3-45'))).toBe(1);
  });
  test('unnamed user slots show their address and do not match text', () => {
    const rows = buildRows('all', 'u1', catalog, lib);
    expect(rows[0]!.shown).toBe('U1-11');
    expect(emptyText('favorites', '', false).title).toBe('NO FAVOURITES');
    expect(emptyText('all', 'zzzz', false).title).toBe('NO MATCHES');
  });
});

describe('keyboard map', () => {
  const k = (key: string, o: Partial<KeyLike> = {}): KeyLike => ({ key, ctrlKey: false, metaKey: false, shiftKey: false, altKey: false, ...o });
  test('FR-13 keys', () => {
    expect(matchKey(k('/'), false, 'other')).toBe('focusSearch');
    expect(matchKey(k('f', { ctrlKey: true }), false, 'other')).toBe('focusSearch');
    expect(matchKey(k('Escape'), false, 'search')).toBe('escape');
    expect(matchKey(k('ArrowDown'), false, 'list')).toBe('down');
    expect(matchKey(k('ArrowDown'), false, 'other')).toBeNull();
    expect(matchKey(k('Home'), false, 'search')).toBeNull();
    expect(matchKey(k('Enter'), false, 'search')).toBe('send');
    expect(matchKey(k(' '), false, 'list')).toBe('toggleFavoriteSpace');
    expect(matchKey(k(' '), false, 'search')).toBeNull();
    expect(matchKey(k('d', { ctrlKey: true }), false, 'other')).toBe('toggleFavorite');
    expect(matchKey(k('PageUp'), false, 'other')).toBe('prev');
    expect(matchKey(k('PageDown'), false, 'other')).toBe('next');
    expect(matchKey(k('g', { ctrlKey: true }), false, 'other')).toBe('gridEntry');
    expect(matchKey(k('1', { ctrlKey: true }), false, 'other')).toBe('viewAll');
    expect(matchKey(k('3', { ctrlKey: true }), false, 'other')).toBe('viewRecents');
    expect(matchKey(k('F2'), false, 'other')).toBe('rename');
    expect(matchKey(k('A', { ctrlKey: true, shiftKey: true }), false, 'other')).toBe('autoSend');
    expect(matchKey(k('a', { ctrlKey: true }), false, 'other')).toBeNull();
    expect(matchKey(k(','), false, 'other')).toBeNull();
    expect(matchKey(k(',', { ctrlKey: true }), false, 'other')).toBe('settings');
    expect(matchKey(k('F1'), false, 'other')).toBe('shortcuts');
    expect(matchKey(k('q', { ctrlKey: true }), false, 'other')).toBeNull();
  });
  test('on a Mac Cmd is the modifier and Ctrl is ignored', () => {
    expect(matchKey(k('d', { metaKey: true }), true, 'other')).toBe('toggleFavorite');
    expect(matchKey(k('d', { ctrlKey: true }), true, 'other')).toBeNull();
    expect(matchKey(k('d', { metaKey: true }), false, 'other')).toBeNull();
  });
  test('no two bindings share a key, scope-aware', () => {
    const seen = new Set<string>();
    for (const b of BINDINGS) {
      const id = `${b.combo}|${b.scope}`;
      expect(seen.has(id), id).toBe(false);
      seen.add(id);
    }
  });
  test('every browser-reserved or shared key is documented', () => {
    for (const b of BINDINGS) if (b.combo.startsWith('Mod+') || b.combo === '/' || b.combo === 'F1') expect(b.browser, b.combo).toBeTruthy();
  });
  test('caps and the sheet', () => {
    expect(capText('Mod+Shift+A', false)).toBe('Ctrl+Shift+A');
    expect(capText('Mod+Shift+A', true)).toBe('⇧⌘A');
    expect(capText('PageDown', false)).toBe('PgDn');
    expect(capText('PageUp', true)).toBe('fn ↑');
    const rows = (mac: boolean) => sheetSections(mac).flatMap((s) => s.rows.map((r) => r.text));
    expect(rows(false)).toContain('Settings');
    expect(rows(true)).not.toContain('Settings'); // Cmd+, belongs to the browser on macOS
    expect(rows(false)).not.toContain('Quit');
  });
});

describe('layout', () => {
  test('which layout', () => {
    expect(chooseLayout(960, 560).kind).toBe('desktop');
    expect(chooseLayout(800, 479).kind).toBe('landscape');
    expect(chooseLayout(844, 390)).toEqual({ kind: 'landscape', compact: false });
    expect(chooseLayout(800, 300).compact).toBe(true);
    expect(chooseLayout(390, 844).kind).toBe('portrait');
    expect(chooseLayout(700, 800).kind).toBe('portrait');
  });
  test('desktop pitch table of DESIGN.md 5.1', () => {
    const g = (W: number, dpr: number) => desktopDisplay(W, dpr);
    expect([g(960, 1).P, g(960, 1).bezelW, g(960, 1).bezelH]).toEqual([3, 767, 79]);
    expect([g(960, 2).P, g(960, 2).g]).toEqual([6, 2]);
    expect([g(960, 1.25).P, g(960, 1.25).bezelW]).toEqual([4, 816]);
    expect(g(800, 1.25).P).toBe(3);
    expect(g(1100, 1).bezelW).toBe(1010);
    expect(g(1400, 1).bezelW).toBe(1253);
    expect(g(960, 1.5).P).toBe(4);
  });
  test('phone pitch table of DESIGN-WEB 5.1', () => {
    const p = (W: number, dpr: number) => { const m = portraitMetrics(W); return phoneDisplay('portrait', false, m.spanW, dpr); };
    expect([p(390, 3).P, p(390, 3).fieldW, p(390, 3).bezelH]).toEqual([8, 328, 65]);
    expect([p(360, 2).P, p(360, 2).fieldW, p(360, 2).bezelW]).toEqual([5, 308, 328]);
    expect(p(412, 2.625).P).toBe(7);
    expect(p(393, 2.75).P).toBe(7);
    expect(p(768, 2).P).toBe(8);
    expect(phoneDisplay('landscape', false, 8 * 49 - 4, 3).P).toBe(8);
    expect(phoneDisplay('landscape', true, 8 * 47 - 4, 2).P).toBe(4);
  });
  test('portrait metrics and rows', () => {
    const m = portraitMetrics(390);
    expect([m.cw, m.gx, m.spanX, m.spanW]).toEqual([91, 13, 16, 358]);
    expect(portraitRows(844, 65)).toEqual({ B: 83, hp: 88 });
    expect(portraitRows(640, 62)).toEqual({ B: 80, hp: 52 });
    expect(portraitRows(668, 62).hp).toBe(59);
  });
  test('computeMetrics lands on the design numbers', () => {
    const m = computeMetrics(960, 560, 1);
    expect(m.vars['B']).toBe(139);
    expect(computeMetrics(844, 390, 3).vars['L']).toBe(416);
  });
});

describe('display text', () => {
  const base = (o: Partial<LcdInput> = {}): LcdInput => ({ columns: 40, current: A('P1-11'), name: 'Fantasia', mode: 'D', entryText: '', access: 'granted', outputConfigured: true, outputOpen: true, sentFlash: false, showCount: false, matches: 0, ...o });
  test('40 columns', () => {
    const [a, b] = lcdLines(base());
    expect(a).toBe('P1-11:Fantasia' + ' '.repeat(22) + 'DUAL');
    expect(a.length).toBe(40);
    expect(b).toBe('PRESET 1: ORIGINAL D-50');
    expect(lcdLines(base({ sentFlash: true }))[1]).toBe('SENT');
    expect(lcdLines(base({ showCount: true, matches: 1 }))[1]).toBe('1 MATCH');
    expect(lcdLines(base({ showCount: true, matches: 0 }))[1]).toBe('NO MATCHES');
    expect(lcdLines(base({ outputOpen: false, sentFlash: true }))[1]).toBe('NO MIDI OUTPUT');
    expect(lcdLines(base({ outputConfigured: false }))[1]).toBe('CHOOSE A PORT IN SETTINGS');
    expect(lcdLines(base({ access: 'prompt' }))[1]).toBe('PRESS CONNECT MIDI');
    expect(lcdLines(base({ access: 'denied' }))[1]).toBe('MIDI ACCESS BLOCKED');
    expect(lcdLines(base({ access: 'unsupported' }))[1]).toBe('NO WEB MIDI IN THIS BROWSER');
    expect(lcdLines(base({ entryText: 'P3-4_' }))[0]).toBe('P3-4_');
    expect(lcdLines(base({ current: A('U3-45'), name: '', mode: '' }))).toEqual(['U3-45:', 'USER 3']);
  });
  test('20 columns', () => {
    const n = (o: Partial<LcdInput> = {}) => lcdLines(base({ columns: 20, ...o }));
    expect(n()).toEqual(['Fantasia', 'P1-11 DUAL']);
    expect(n({ sentFlash: true })[1]).toBe('P1-11 SENT');
    expect(n({ outputOpen: false })[1]).toBe('P1-11 NO MIDI OUTPUT');
    expect(n({ access: 'prompt' })[1]).toBe('P1-11 CONNECT MIDI');
    expect(n({ entryText: 'P3-__' })).toEqual(['', 'P3-__ DUAL']);
    expect(n({ current: A('U3-45'), name: '', mode: '' })).toEqual(['', 'U3-45']);
    expect(n({ showCount: true, matches: 17 })[1]).toBe('P1-11 17 MATCHES');
    for (const s of ['MIDI BLOCKED', 'NO WEB MIDI', 'CHOOSE A PORT']) expect(s.length).toBeLessThanOrEqual(14);
  });
});

describe('access panel texts', () => {
  const env = (o: Partial<Env> = {}): Env => ({ userAgent: 'Mozilla/5.0 Chrome/130 Safari/537', brands: ['Google Chrome'], mobile: false, maxTouchPoints: 0, brave: false, permissionChange: true, ...o });
  test('states', () => {
    expect(accessContent('granted', env())).toBeNull();
    expect(accessContent('prompt', env())!.button).toBe('CONNECT MIDI');
    expect(accessContent('waiting', env())!.button).toBe('WAITING');
    expect(accessContent('insecure', env())!.title).toBe('MIDI NEEDS A SECURE PAGE');
    expect(accessContent('unsupported', env({ userAgent: 'iPhone Safari/604', maxTouchPoints: 5 }))!.title).toBe('NO WEB MIDI ON IPHONE OR IPAD');
    expect(accessContent('unsupported', env({ userAgent: 'Macintosh Version/17 Safari/605', brands: [] }))!.title).toBe('NO WEB MIDI IN SAFARI');
    expect(accessContent('unsupported', env({ userAgent: 'Macintosh Chrome/130 Safari/537' }))!.title).toBe('NO WEB MIDI IN THIS BROWSER');
  });
  test('denied steps follow the browser', () => {
    const d = accessContent('denied', env())!;
    expect(d.steps).toHaveLength(4);
    expect(d.body[0]).toBe('Chrome is blocking MIDI for this page. To allow it:');
    expect(accessContent('denied', env({ permissionChange: false }))!.steps[3]).toBe('Come back here and press TRY AGAIN.');
    expect(accessContent('denied', env({ userAgent: 'Gecko/20100101 Firefox/130.0', brands: [] }))!.steps).toHaveLength(3);
    expect(accessContent('denied', env({ mobile: true, userAgent: 'Android Chrome/130' }))!.steps[1]).toBe('Tap Permissions.');
    expect(browserKind(env({ brands: ['Microsoft Edge'] }))).toBe('edge');
  });
  test('no em dashes in any text', () => {
    for (const s of ['insecure', 'unsupported', 'denied', 'prompt'] as const) expect(JSON.stringify(accessContent(s, env()))).not.toContain('—');
  });
});

describe('follow parser', () => {
  test('triplet moves the patch; own sends within 300 ms are ignored', () => {
    let t = 0;
    const f = new FollowParser(() => t);
    const feed = (b: number[], cur = A('P1-11')) => f.feed(b, 3, cur);
    expect(feed([0xb2, 0, 87])).toBeNull();
    expect(feed([0xb2, 32, 10])).toBeNull();
    expect(formatAddress(feed([0xc2, 28])!)).toBe('P3-45');
    expect(feed([0xb0, 32, 10])).toBeNull(); // wrong channel
    f.noteSent(A('P2-11'));
    feed([0xb2, 0, 87]);
    feed([0xb2, 32, 9]);
    expect(feed([0xc2, 0])).toBeNull(); // echo of our own send
    t = 400;
    feed([0xb2, 0, 87]);
    feed([0xb2, 32, 9]);
    expect(formatAddress(feed([0xc2, 0])!)).toBe('P2-11');
    expect(formatAddress(feed([0xc2, 9], A('P2-11'))!)).toBe('P2-22'); // no bank: stays in the group
    expect(feed([0xf8])).toBeNull();
  });
});

describe('fake MIDI and settings', () => {
  test('records the three messages', () => {
    const m = new FakeMidi();
    m.setOutput(m.outputs()[0]!);
    expect(m.sendPatch(A('P3-45'), 3)).toBeNull();
    expect(m.hex()).toEqual(['B2 00 57', 'B2 20 0A', 'C2 1C']);
    m.unplug('out', m.outputs()[0]!.name);
    expect(m.output.state).toBe('missing');
    expect(m.sendPatch(A('P3-45'), 3)).toBe('Port not found');
  });
  test('settings are read defensively and ports are by name', () => {
    expect(parseSettings('{"output":{"name":"X","id":"abc"},"channel":99,"autoSend":true}')).toMatchObject({ output: { name: 'X' }, channel: 1, autoSend: true });
    expect(parseSettings('nonsense').channel).toBe(1);
    expect(JSON.stringify(parseSettings('{"output":{"name":"X","id":"abc"}}'))).not.toContain('abc');
  });
  test('glyph table: A is 0E 11 11 11 1F 11 11', () => {
    expect([...glyphRows('A')]).toEqual([0x0e, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11]);
    expect([...glyphRows('é')]).toEqual([...glyphRows('?')]);
    expect(fromParts(8, 1, 1)).toBeTruthy();
  });
});

describe('web rules', () => {
  const src = fileURLToPath(new URL('../../src', import.meta.url));
  const walk = (d: string): string[] => readdirSync(d).flatMap((n) => (statSync(join(d, n)).isDirectory() ? walk(join(d, n)) : [join(d, n)]));
  test('no colour literals outside theme.css (DESIGN-WEB 2)', () => {
    for (const f of walk(src)) {
      if (f.endsWith('theme.css') || !/\.(ts|css)$/.test(f)) continue;
      const text = readFileSync(f, 'utf8').replace(/&#\d+;/g, '');
      expect(text.match(/#[0-9a-fA-F]{3,8}\b|\brgba?\(|\bhsla?\(/g), f).toBeNull();
    }
  });
  test('no network calls, no SysEx request, no analytics in the source', () => {
    for (const f of walk(src)) {
      if (!f.endsWith('.ts')) continue;
      const text = readFileSync(f, 'utf8');
      expect(text, f).not.toMatch(/\bfetch\(|XMLHttpRequest|sendBeacon|WebSocket|EventSource/);
      expect(text, f).not.toMatch(/sysex\s*:\s*true/i);
    }
  });
  test('no em dashes in source strings', () => {
    for (const f of walk(src)) expect(readFileSync(f, 'utf8'), f).not.toContain('—');
  });
  test('catalog loads 384 presets', () => expect(new Catalog().presetCount()).toBe(384));
});

describe('findPort', () => {
  const a = { name: 'USB MIDI', manufacturer: 'Acme' };
  const b = { name: 'USB MIDI', manufacturer: 'Other' };
  const bare = { name: 'USB MIDI' };
  test('exact match wins; a different maker is never a fallback', () => {
    expect(findPort([a, b], b)).toBe(b);
    expect(findPort([b], a)).toBeUndefined();
  });
  test('name alone matches when either side has no manufacturer', () => {
    expect(findPort([a], bare)).toBe(a);
    expect(findPort([bare], a)).toBe(bare);
    expect(findPort([], a)).toBeUndefined();
  });
});
