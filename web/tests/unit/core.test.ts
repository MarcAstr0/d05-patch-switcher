import { afterEach, beforeEach, describe, expect, test, vi } from 'vitest';
import {
  allAddresses,
  formatAddress as f,
  fromMidi,
  midiBytes,
  midiTriplet,
  next,
  parseAddress as p,
  prev,
} from '../../src/core/address';
import { Catalog, parsePresetTable, sanitizeUserName } from '../../src/core/catalog';
import { Library, LocalLibraryStorage, MemoryStorage, createLibraryStorage, formatIsoUtc, parseIsoUtc } from '../../src/core/library';
import { SearchQuery, runSearch } from '../../src/core/search';
import { readWebFile } from './helpers';

const A = (s: string) => p(s)!;

describe('address', () => {
  test('catalogue has 896 distinct addresses in order', () => {
    const all = allAddresses();
    expect(all).toHaveLength(896);
    expect(f(all[0]!)).toBe('U1-11');
    expect(f(all[511]!)).toBe('U8-88');
    expect(f(all[512]!)).toBe('P1-11');
    expect(f(all[895]!)).toBe('P6-88');
    expect(new Set(all.map(f)).size).toBe(896);
  });

  test('prev/next cross group boundaries and clamp at the ends', () => {
    expect(f(next(A('P1-88')))).toBe('P2-11');
    expect(f(prev(A('P2-11')))).toBe('P1-88');
    expect(f(next(A('U8-88')))).toBe('P1-11');
    expect(f(prev(A('P1-11')))).toBe('U8-88');
    expect(next(A('P6-88'))).toBe(A('P6-88'));
    expect(prev(A('U1-11'))).toBe(A('U1-11'));
  });

  test('MIDI bytes: MSB, LSB, PC on the channel (MIDI.md worked example P3-45)', () => {
    expect(midiBytes(A('P3-45'), 1)).toEqual([[0xb0, 0x00, 0x57], [0xb0, 0x20, 0x0a], [0xc0, 0x1c]]);
    expect(midiBytes(A('P6-88'), 16)).toEqual([[0xbf, 0x00, 87], [0xbf, 0x20, 13], [0xcf, 63]]);
    expect(midiBytes(A('U1-11'), 3)[2]).toEqual([0xc2, 0]);
  });

  test('inverse of the triplet for every address', () => {
    for (const a of allAddresses()) {
      const t = midiTriplet(a);
      expect(fromMidi(t.msb, t.lsb, t.program)).toBe(a);
    }
    expect(fromMidi(86, 0, 0)).toBeNull();
    expect(fromMidi(87, 14, 0)).toBeNull();
    expect(fromMidi(87, 0, 64)).toBeNull();
  });
});

describe('address strictness', () => {
  test('every address round-trips through every accepted spelling', () => {
    for (const a of allAddresses()) {
      const t = f(a);
      expect(p(t)).toBe(a);
      const [g, bn] = t.split('-') as [string, string];
      const forms = [t, t.toLowerCase(), g + bn, (g + bn).toLowerCase(), `  ${t}\t`];
      if (g.startsWith('P')) forms.push(`${g.slice(1)}-${bn}`, g.slice(1) + bn);
      for (const x of forms) expect(p(x), x).toBe(a);
    }
  });

  test('out-of-range digits are rejected in every position', () => {
    for (const letter of ['P', 'U', 'p', 'u', '']) {
      const user = letter.toLowerCase() === 'u';
      for (const dash of ['-', '']) {
        const groupBad = user ? ['0', '9'] : ['0', '7', '8', '9'];
        for (const g of groupBad) expect(p(`${letter}${g}${dash}11`), `${letter}${g}${dash}11`).toBeNull();
        for (const d of ['0', '9']) {
          expect(p(`${letter}1${dash}${d}1`)).toBeNull();
          expect(p(`${letter}1${dash}1${d}`)).toBeNull();
        }
      }
    }
    expect(p('U7-11')).not.toBeNull();
    expect(p('U8-11')).not.toBeNull();
    expect(p('P7-11')).toBeNull();
  });

  test('trimming follows Qt: U+0085 and NBSP trim, U+FEFF does not', () => {
    expect(p('\u0085P3-45\u00a0')).toBe(A('P3-45'));
    expect(p('\u3000P3-45')).toBe(A('P3-45'));
    expect(p('\ufeffP3-45')).toBeNull();
    expect(new SearchQuery('\u0085p1\u00a0\u2003').hasText).toBe(false);
    expect(new SearchQuery('\ufeff').isEmpty).toBe(false);
  });

  test('fromMidi rejects non-integers; midiBytes rejects bad channels', () => {
    expect(fromMidi(87, 8, 0.5)).toBeNull();
    expect(fromMidi(87, 1.5, 0)).toBeNull();
    expect(fromMidi(87, NaN, 0)).toBeNull();
    expect(fromMidi(87.5, 0, 0)).toBeNull();
    for (const ch of [0, 17, NaN, 1.5, -1, Infinity]) expect(() => midiBytes(A('P1-11'), ch), String(ch)).toThrow(RangeError);
  });
});

describe('catalog', () => {
  test('384 preset names, nothing for user slots, modes', () => {
    const c = new Catalog();
    expect(c.presetCount()).toBe(384);
    expect(c.name(A('P1-11'))).toBe('Fantasia');
    expect(c.mode(A('P1-11'))).toMatch(/^[WDS]$/);
    expect(c.name(A('U1-11'))).toBe('');
    expect(c.mode(A('U1-11'))).toBe('');
  });

  test('table parser needs plain digits for bank and number', () => {
    const rows = parsePresetTable('h\nP1\t1e0\t1\tA\tD\nP1\t 1\t1\tB\tD\nP1\t1\t1.0\tC\tD\nP1\t1\t1\tOk\tD');
    expect(rows.map((r) => r.name)).toEqual(['Ok']);
  });

  test('table parser skips bad lines and duplicates', () => {
    const rows = parsePresetTable(
      ['group\tbank\tnumber\tname\tmode', 'P1\t1\t1\tOne\tD', 'P1\t1\t1\tDup\tD', 'U1\t1\t1\tUser\tD', 'P1\t1\t9\tBad\tD', 'P1\t1\t2\t\tD', 'P1\t1\t3\tX\tQ', 'P1\t1', ''].join('\n'),
    );
    expect(rows.map((r) => r.name)).toEqual(['One']);
  });

  test('user names: printable ASCII, tabs to spaces, runs collapse, max 18, preset slots refuse', () => {
    expect(sanitizeUserName('  Tab\there  éx  ')).toBe('Tab here x');
    expect(sanitizeUserName('a\r\nb\u000bc\fd')).toBe('a b c d');
    expect(sanitizeUserName('123456789012345678901')).toBe('123456789012345678');
    expect(sanitizeUserName('12345678901234567 8')).toBe('12345678901234567'); // cut then trimmed
    expect(sanitizeUserName('あい')).toBe('');
    const c = new Catalog();
    expect(c.setUserName(A('P1-11'), 'Nope')).toBe(false);
    expect(c.name(A('P1-11'))).toBe('Fantasia');
    c.setUserName(A('U1-11'), 'Pad');
    expect(c.name(A('U1-11'))).toBe('Pad');
    c.setUserName(A('U1-11'), '   ');
    expect(c.hasUserName(A('U1-11'))).toBe(false);
  });
});

describe('search', () => {
  const c = new Catalog();
  const run = (q: string) => runSearch(q, (a) => c.name(a)).map(f);

  test('text tokens never match unnamed user slots, and match the name only', () => {
    expect(run('u1-11')).toEqual(['U1-11']);
    expect(run('u1')).toHaveLength(64);
    expect(run('u1 pad')).toEqual([]);
    expect(run('U1-11 u')).toEqual([]);
  });

  test('odd tokens fall back to text', () => {
    expect(new SearchQuery('p3-').hasText).toBe(true);
    expect(new SearchQuery('3-4').hasText).toBe(true);
    expect(new SearchQuery('p3-45').hasText).toBe(false);
    expect(new SearchQuery('').isEmpty).toBe(true);
  });

  test('ranges are UTF-16 indices into the original name, merged and sorted', () => {
    const q = new SearchQuery('lead synth');
    const r: { start: number; length: number }[] = [];
    expect(q.matches(A('P3-66'), 'Synth Lead 3', r)).toBe(true);
    expect(r).toEqual([{ start: 0, length: 5 }, { start: 6, length: 4 }]);
    expect(new SearchQuery('synth ynt').matches(A('P3-66'), 'Synth Lead 3', r)).toBe(true);
    expect(r).toEqual([{ start: 0, length: 5 }]);
    const q2 = new SearchQuery('d 3');
    expect(q2.matches(A('P3-66'), 'Synth Lead 3', r)).toBe(true);
    expect(r).toEqual([{ start: 9, length: 1 }, { start: 11, length: 1 }]);
  });

  test('accent-insensitive with decomposed input; ranges keep combining marks', () => {
    const r: { start: number; length: number }[] = [];
    expect(new SearchQuery('cafe').matches(A('P6-44'), 'Café Bar', r)).toBe(true);
    expect(r).toEqual([{ start: 0, length: 4 }]);
    expect(new SearchQuery('cafe').matches(A('P6-44'), 'Café Bar', r)).toBe(true);
    expect(r).toEqual([{ start: 0, length: 5 }]);
    expect(new SearchQuery('café').matches(A('P6-44'), 'Cafe', r)).toBe(true);
  });

  test('ranges cleared when nothing matches', () => {
    const r = [{ start: 1, length: 1 }];
    expect(new SearchQuery('zzz').matches(A('P1-11'), 'Fantasia', r)).toBe(false);
    expect(r).toEqual([]);
  });
});

describe('library', () => {
  let cat: Catalog;
  let st: MemoryStorage;
  let lib: Library;
  beforeEach(() => {
    vi.useFakeTimers();
    cat = new Catalog();
    st = new MemoryStorage();
    lib = new Library(cat, st);
    expect(lib.load()).toBe('fresh');
  });
  afterEach(() => vi.useRealTimers());

  test('favourites are a set kept in catalogue order', () => {
    lib.setFavorite(A('P2-57'), true);
    lib.setFavorite(A('U8-88'), true);
    lib.setFavorite(A('P1-11'), true);
    lib.setFavorite(A('P1-11'), true);
    expect(lib.favorites().map(f)).toEqual(['U8-88', 'P1-11', 'P2-57']);
    expect(lib.toggleFavorite(A('P1-11'))).toBe(false);
    expect(lib.isFavorite(A('P1-11'))).toBe(false);
    expect(lib.favorites().map(f)).toEqual(['U8-88', 'P2-57']);
  });

  test('recents: resend moves to front and counts; cap 50; clear', () => {
    lib.recordSend(A('P1-11'), new Date('2026-10-24T10:00:00Z'));
    lib.recordSend(A('P1-12'), new Date('2026-10-24T10:01:00Z'));
    lib.recordSend(A('P1-11'), new Date('2026-10-24T10:02:00Z'));
    expect(lib.recents().map((r) => [f(r.address), r.count, r.last])).toEqual([
      ['P1-11', 2, '2026-10-24T10:02:00Z'],
      ['P1-12', 1, '2026-10-24T10:01:00Z'],
    ]);
    for (let i = 0; i < 60; i++) lib.recordSend(allAddresses()[512 + i]!);
    expect(lib.recents()).toHaveLength(50);
    expect(f(lib.recents()[0]!.address)).toBe(f(allAddresses()[571]!));
    expect(lib.recents().some((r) => f(r.address) === 'P1-12')).toBe(false);
    lib.clearRecents();
    expect(lib.recents()).toEqual([]);
  });

  test('counts saturate at 2147483647', () => {
    const l2 = new Library(new Catalog(), new MemoryStorage('{"version":1,"recents":[{"address":"P1-11","count":2147483647,"last":"2026-10-24T10:00:00Z"},{"address":"P1-12","count":1e30,"last":"2026-10-24T09:00:00Z"}]}'));
    l2.load();
    expect(l2.recents().map((r) => r.count)).toEqual([2147483647, 2147483647]);
    l2.recordSend(A('P1-11'));
    expect(l2.recents()[0]!.count).toBe(2147483647);
  });

  test('user names go through the sanitiser; presets refuse', () => {
    expect(lib.setUserName(A('P1-11'), 'x')).toBe(false);
    lib.setUserName(A('U2-22'), ' a\tb ');
    expect(cat.name(A('U2-22'))).toBe('a b');
  });

  test('writes are debounced 500 ms and use the v1 schema', () => {
    lib.setFavorite(A('P1-11'), true);
    vi.advanceTimersByTime(499);
    lib.recordSend(A('P1-11'), new Date('2026-10-24T10:00:00Z'));
    vi.advanceTimersByTime(499);
    expect(st.writes).toBe(0);
    vi.advanceTimersByTime(1);
    expect(st.writes).toBe(1);
    expect(JSON.parse(st.text!)).toEqual({
      version: 1,
      favorites: ['P1-11'],
      recents: [{ address: 'P1-11', count: 1, last: '2026-10-24T10:00:00Z' }],
      userNames: {},
    });
    vi.advanceTimersByTime(5000);
    expect(st.writes).toBe(1);
  });

  test('flush writes at once; a failing store keeps the data and retries', () => {
    st.failWrites = true;
    lib.setFavorite(A('P1-11'), true);
    expect(lib.flush()).toBe(false);
    expect(lib.saveError).toBe('write failed');
    st.failWrites = false;
    vi.advanceTimersByTime(2000);
    expect(st.writes).toBe(1);
    expect(lib.saveError).toBeNull();
  });

  test('a corrupt stored file is quarantined and a fresh library starts', () => {
    for (const bad of ['{nope', '[]', '{"version":2}', '{"version":1,"favorites":{}}', '{"version":1,"userNames":[]}', 'null']) {
      const s = new MemoryStorage(bad);
      const l = new Library(new Catalog(), s);
      expect(l.load(), bad).toBe('recovered');
      expect(s.broken).toEqual([bad]);
      expect(s.text).toBeNull();
      expect(l.favorites()).toEqual([]);
    }
  });

  test('recents are ordered by timestamp, deduplicated and capped on load; bad dates become the epoch', () => {
    const recents = [{ address: 'P1-11', count: 1, last: 'garbage' }];
    for (let i = 0; i < 60; i++) recents.push({ address: f(allAddresses()[512 + 1 + i]!), count: 1, last: `2026-10-24T10:${String(i).padStart(2, '0')}:00Z` });
    recents.push({ address: 'p1-12', count: 5, last: '2020-01-01T00:00:00Z' });
    const l = new Library(new Catalog(), new MemoryStorage(JSON.stringify({ version: 1, recents })));
    l.load();
    expect(l.recents()).toHaveLength(50);
    expect(l.recents()[0]!.last).toBe('2026-10-24T10:59:00Z');
    const l2 = new Library(new Catalog(), new MemoryStorage(JSON.stringify({ version: 1, recents: [recents[0], recents[1]] })));
    l2.load();
    expect(l2.recents().map((r) => r.last)).toEqual(['2026-10-24T10:00:00Z', '1970-01-01T00:00:00Z']);
  });

  test('timestamps: Z, offsets and zone-less text', () => {
    expect(parseIsoUtc('2026-10-24T18:02:11Z')).toBe(Date.UTC(2026, 9, 24, 18, 2, 11));
    expect(parseIsoUtc('2026-10-24T18:02:11')).toBe(Date.UTC(2026, 9, 24, 18, 2, 11));
    expect(parseIsoUtc('2026-10-24T20:02:11+02:00')).toBe(Date.UTC(2026, 9, 24, 18, 2, 11));
    expect(parseIsoUtc('2026-13-24T00:00:00Z')).toBeNull();
    expect(parseIsoUtc('yesterday')).toBeNull();
  });

  test('importJson never throws and leaves the library alone on garbage', () => {
    lib.setFavorite(A('P1-11'), true);
    for (const bad of ['', 'x', '{', '[1]', '"s"', '{"version":"1"}', '{"version":1,"recents":3}']) {
      const r = lib.importJson(bad);
      expect(r.ok, bad).toBe(false);
    }
    expect(lib.importJson(undefined as unknown as string).ok).toBe(false);
    expect(lib.favorites().map(f)).toEqual(['P1-11']);
  });

  test('importJson replaces, skips bad entries and reports counts', () => {
    lib.setFavorite(A('P1-11'), true);
    lib.setUserName(A('U1-11'), 'Old');
    const r = lib.importJson(
      JSON.stringify({ version: 1, favorites: ['P3-45', 'P1-19', 7, 'P3-45'], recents: [{ address: 'P7-11' }, { address: 'U2-22', count: 'x', last: 'bad' }], userNames: { 'U1-12': 'New', 'P1-11': 'no', 'U9-11': 'no', 'U1-13': 5 } }),
    );
    expect(r).toEqual({ ok: true, favorites: 1, recents: 1, userNames: 1, skipped: 7 });
    expect(lib.favorites().map(f)).toEqual(['P3-45']);
    expect(cat.name(A('U1-11'))).toBe('');
    expect(cat.name(A('U1-12'))).toBe('New');
    expect(lib.recents()[0]).toMatchObject({ count: 1, last: '1970-01-01T00:00:00Z' });
    vi.advanceTimersByTime(500);
    expect(st.writes).toBe(1);
  });

  test('export then import into a fresh library is the identity', () => {
    lib.setFavorite(A('P2-57'), true);
    lib.recordSend(A('P1-11'));
    lib.setUserName(A('U3-33'), 'Mine');
    const text = lib.exportJson();
    const other = new Library(new Catalog(), new MemoryStorage());
    expect(other.importJson(text).ok).toBe(true);
    expect(JSON.parse(other.exportJson())).toEqual(JSON.parse(text));
  });

  test('save status is observable: a failure and the later success are both reported', () => {
    const seen: (string | null)[] = [];
    lib.onSaveStatus((e) => seen.push(e));
    st.failWrites = true;
    lib.setFavorite(A('P1-11'), true);
    vi.advanceTimersByTime(500);
    vi.advanceTimersByTime(2000);
    expect(lib.saveError).toBe('write failed');
    expect(seen).toEqual(['write failed']); // reported once, not on every retry
    st.failWrites = false;
    vi.advanceTimersByTime(2000);
    expect(seen).toEqual(['write failed', null]);
    expect(lib.saveError).toBeNull();
  });

  test('an unreadable store blocks writes instead of looking fresh', () => {
    const s = new MemoryStorage('{"version":1,"favorites":["P1-11"]}');
    s.failReads = true;
    const l = new Library(new Catalog(), s);
    expect(l.load()).toBe('unreadable');
    l.setFavorite(A('P2-11'), true);
    vi.advanceTimersByTime(5000);
    expect(l.flush()).toBe(false);
    expect(s.writes).toBe(0);
    expect(l.saveError).toMatch(/not overwriting/);
    s.failReads = false;
    expect(l.load()).toBe('loaded');
    expect(l.favorites().map(f)).toEqual(['P1-11']);
  });

  test('a corrupt file that cannot be set aside blocks writes', () => {
    const s = new MemoryStorage('{nope');
    s.failQuarantine = true;
    const l = new Library(new Catalog(), s);
    expect(l.load()).toBe('sidelineFailed');
    l.setFavorite(A('P2-11'), true);
    vi.advanceTimersByTime(5000);
    expect(l.flush()).toBe(false);
    expect(s.writes).toBe(0);
    expect(s.text).toBe('{nope');
    const noQ = new Library(new Catalog(), { read: () => '{nope', write: () => { throw new Error('should not write'); } });
    expect(noQ.load()).toBe('sidelineFailed');
  });

  test('reload: plain load when clean; merge when there are unsaved edits', () => {
    lib.setFavorite(A('P1-11'), true);
    lib.recordSend(A('P1-11'), new Date('2026-10-24T10:00:00Z'));
    lib.setUserName(A('U1-11'), 'Mine');
    vi.advanceTimersByTime(500);
    // another tab wrote
    st.text = JSON.stringify({
      version: 1,
      favorites: ['P2-11'],
      recents: [{ address: 'P1-11', count: 9, last: '2026-10-24T12:00:00Z' }, { address: 'P3-11', count: 1, last: '2026-10-24T11:00:00Z' }],
      userNames: { 'U1-11': 'Theirs', 'U2-22': 'Other' },
    });
    // unsaved local edit
    lib.setFavorite(A('P4-11'), true);
    expect(lib.reload()).toBe('loaded');
    expect(lib.favorites().map(f)).toEqual(['P1-11', 'P2-11', 'P4-11']);
    expect(lib.recents().map((r) => [f(r.address), r.count])).toEqual([['P1-11', 9], ['P3-11', 1]]);
    expect(cat.name(A('U1-11'))).toBe('Mine');
    expect(cat.name(A('U2-22'))).toBe('Other');
    vi.advanceTimersByTime(500);
    expect(JSON.parse(st.text!).favorites).toEqual(['P1-11', 'P2-11', 'P4-11']);
    // clean: replaced by what is stored
    st.text = '{"version":1,"favorites":["P6-88"]}';
    expect(lib.reload()).toBe('loaded');
    expect(lib.favorites().map(f)).toEqual(['P6-88']);
    expect(cat.name(A('U1-11'))).toBe('');
  });

  test('userNames: two spellings of one address apply in sorted key order (u111 after U1-11)', () => {
    for (const names of ['{"u111":"A","U1-11":"B"}', '{"U1-11":"B","u111":"A"}']) {
      const c = new Catalog();
      new Library(c, new MemoryStorage(`{"version":1,"userNames":${names}}`)).load();
      expect(c.name(A('U1-11'))).toBe('A');
    }
  });

  test('timestamps match what Qt::ISODate reads', () => {
    const u = (s: string) => parseIsoUtc(s);
    const base = Date.UTC(2026, 9, 24, 18, 2, 11);
    expect(u('2026-10-24 18:02:11')).toBe(base);
    expect(u('2026-10-24t18:02:11z')).toBe(base);
    expect(u('2026-10-24T18:02:11+0200')).toBe(base - 7200000);
    expect(u('2026-10-24T18:02:11-02')).toBe(base + 7200000);
    expect(u('2026-10-24T18')).toBe(Date.UTC(2026, 9, 24, 18));
    expect(u('2026-10-24T18:02')).toBe(Date.UTC(2026, 9, 24, 18, 2));
    expect(u('2026-10-24T18.5')).toBe(Date.UTC(2026, 9, 24, 18, 30));
    expect(u('2026-10-24T18:02.5')).toBe(Date.UTC(2026, 9, 24, 18, 2, 30));
    expect(u('2026-10-24T18:02:11.123Z')).toBe(base + 123);
    expect(u('2026-10-24T18:02:11,5')).toBe(base + 500);
    expect(u('2026-10-24T24:00:00')).toBe(Date.UTC(2026, 9, 25));
    expect(u('2026-10-24T24:00:01')).toBeNull();
    expect(u('2026-10-24')).toBe(Date.UTC(2026, 9, 24));
    expect(u('0050-01-02T03:04:05Z')).toBe(Date.UTC(2000, 0, 1) - Date.UTC(2000, 0, 1) + new Date('0050-01-02T03:04:05Z').getTime());
    expect(formatIsoUtc(u('0050-01-02T03:04:05Z')!)).toBe('0050-01-02T03:04:05Z');
    expect(u('0000-01-01T00:00:00Z')).toBeNull();
    for (const bad of [' 2026-10-24T18:02:11Z', '2026-10-24T18:02:11Z ', '2026-10-24T25:00:00Z', '2026-02-30T00:00:00Z', '2026-10-24T23:59:60Z', '20261024T180211Z', '2026-10-24T18:02:11 Z', '']) {
      expect(u(bad), JSON.stringify(bad)).toBeNull();
    }
  });

  test('milliseconds order recents even though the text drops them', () => {
    const l = new Library(
      new Catalog(),
      new MemoryStorage('{"version":1,"recents":[{"address":"P1-11","last":"2026-10-24T10:00:00.25Z"},{"address":"P1-12","last":"2026-10-24T10:00:00.75Z"}]}'),
    );
    l.load();
    expect(l.recents().map((r) => f(r.address))).toEqual(['P1-12', 'P1-11']);
    expect(l.recents()[0]!.last).toBe('2026-10-24T10:00:00Z');
  });
});

class FakeStore {
  m = new Map<string, string>();
  failSet: ((k: string) => boolean) | null = null;
  failRemove = false;
  get length() { return this.m.size; }
  key(i: number) { return [...this.m.keys()][i] ?? null; }
  getItem(k: string) { return this.m.get(k) ?? null; }
  setItem(k: string, v: string) { if (this.failSet?.(k)) throw new Error('quota'); this.m.set(k, v); }
  removeItem(k: string) { if (this.failRemove && k === 'k') throw new Error('locked'); this.m.delete(k); }
}
const brokenKeys = (s: FakeStore) => [...s.m.keys()].filter((k) => k.startsWith('k.broken-'));

describe('LocalLibraryStorage', () => {
  test('round-trips; absent is null', () => {
    const s = new FakeStore();
    const ls = new LocalLibraryStorage('k', s);
    expect(ls.read()).toBeNull();
    ls.write('abc');
    expect(ls.read()).toBe('abc');
  });

  test('read throws when the store fails (distinct from absent)', () => {
    const ls = new LocalLibraryStorage('k', { ...new FakeStore(), getItem: () => { throw new Error('denied'); } } as never);
    expect(() => ls.read()).toThrow();
  });

  test('quarantine copies to a unique .broken- key and removes the primary', () => {
    const s = new FakeStore();
    const ls = new LocalLibraryStorage('k', s);
    s.m.set('k', 'bad1');
    ls.quarantine('bad1');
    expect(s.m.has('k')).toBe(false);
    expect(brokenKeys(s)).toHaveLength(1);
    expect(brokenKeys(s)[0]).toMatch(/^k\.broken-\d{8}T\d{9}(-\d+)?$/);
    // same millisecond: never overwrite an existing copy
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-10-24T10:00:00.123Z'));
    s.m.clear();
    for (const t of ['a', 'b', 'c']) ls.quarantine(t);
    expect([...s.m.values()].sort()).toEqual(['a', 'b', 'c']);
    expect(brokenKeys(s).sort()).toEqual(['k.broken-20261024T100000123', 'k.broken-20261024T100000123-2', 'k.broken-20261024T100000123-3']);
    vi.useRealTimers();
  });

  test('keeps at most 3 broken copies, dropping the oldest', () => {
    vi.useFakeTimers();
    const s = new FakeStore();
    const ls = new LocalLibraryStorage('k', s);
    for (let i = 0; i < 6; i++) {
      vi.setSystemTime(new Date(Date.UTC(2026, 9, 24, 10, 0, i)));
      ls.quarantine(`bad${i}`);
    }
    expect(brokenKeys(s)).toHaveLength(3);
    expect([...s.m.values()].sort()).toEqual(['bad3', 'bad4', 'bad5']);
    vi.useRealTimers();
  });

  test('a failing copy or removal propagates; a failed removal drops the copy again', () => {
    const s = new FakeStore();
    const ls = new LocalLibraryStorage('k', s);
    s.m.set('k', 'bad');
    s.failSet = (k) => k.startsWith('k.broken-');
    expect(() => ls.quarantine('bad')).toThrow();
    expect(s.m.get('k')).toBe('bad');
    s.failSet = null;
    s.failRemove = true;
    expect(() => ls.quarantine('bad')).toThrow();
    expect(brokenKeys(s)).toEqual([]);
    expect(s.m.get('k')).toBe('bad');
    // through the library: writes stay blocked
    const l = new Library(new Catalog(), new LocalLibraryStorage('k', Object.assign(s, { m: new Map([['k', '{nope']]) })));
    expect(l.load()).toBe('sidelineFailed');
    expect(l.flush()).toBe(true); // nothing dirty
    l.setFavorite(A('P1-11'), true);
    expect(l.flush()).toBe(false);
    expect(s.m.get('k')).toBe('{nope');
  });

  test('a corrupt library is recovered once, not on every reload', () => {
    const s = new FakeStore();
    s.m.set('k', '{nope');
    for (let i = 0; i < 3; i++) {
      const l = new Library(new Catalog(), new LocalLibraryStorage('k', s));
      expect(l.load()).toBe(i === 0 ? 'recovered' : 'fresh');
    }
    expect(brokenKeys(s)).toHaveLength(1);
  });
});

describe('createLibraryStorage', () => {
  test('uses localStorage when it works', () => {
    const r = createLibraryStorage('k', () => new FakeStore());
    expect(r.persistent).toBe(true);
    expect(r.storage).toBeInstanceOf(LocalLibraryStorage);
  });

  test('falls back to memory when the store is unusable', () => {
    const s = new FakeStore();
    s.failSet = () => true;
    for (const get of [() => s, () => { throw new Error('SecurityError'); }]) {
      const r = createLibraryStorage('k', get);
      expect(r.persistent).toBe(false);
      expect(r.storage).toBeInstanceOf(MemoryStorage);
    }
  });

  test('falls back when the localStorage global has a getter that throws', () => {
    const g = globalThis as Record<string, unknown>;
    const saved = Object.getOwnPropertyDescriptor(g, 'localStorage');
    Object.defineProperty(g, 'localStorage', { configurable: true, get() { throw new Error('SecurityError'); } });
    try {
      expect(() => new LocalLibraryStorage()).not.toThrow();
      const r = createLibraryStorage();
      expect(r.persistent).toBe(false);
      expect(() => new LocalLibraryStorage().read()).toThrow();
    } finally {
      if (saved) Object.defineProperty(g, 'localStorage', saved);
      else delete g.localStorage;
    }
  });
});

describe('desktop library.json round trip', () => {
  const text = readWebFile('tests/fixtures/desktop-library.json');

  test('import then export is semantically identical to the desktop file', () => {
    const cat = new Catalog();
    const lib = new Library(cat, new MemoryStorage());
    lib.load();
    expect(lib.importJson(text)).toEqual({ ok: true, favorites: 4, recents: 2, userNames: 3, skipped: 0 });
    expect(JSON.parse(lib.exportJson())).toEqual(JSON.parse(text));
    expect(cat.name(A('U3-18'))).toBe('Tab here x');
    expect(lib.recents().map((r) => [f(r.address), r.count])).toEqual([['P1-11', 2], ['P3-45', 1]]);
  });

  test('loading it through storage gives the same state', () => {
    const lib = new Library(new Catalog(), new MemoryStorage(text));
    expect(lib.load()).toBe('loaded');
    expect(JSON.parse(lib.exportJson())).toEqual(JSON.parse(text));
  });
});
