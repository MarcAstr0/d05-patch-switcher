import { describe, expect, test } from 'vitest';
import { formatAddress, fromIndex, midiTriplet, parseAddress } from '../../src/core/address';
import { Catalog } from '../../src/core/catalog';
import { Library, MemoryStorage } from '../../src/core/library';
import { runSearch } from '../../src/core/search';
import { readRepoFile } from './helpers';

interface SearchCase {
  query: string;
  expect?: string[];
  expectCount?: number;
  expectFirst?: string;
  expectLast?: string;
  expectContains?: string[];
  expectNot?: string[];
  expectOnlyPresets?: boolean;
}
interface LibExpect {
  favorites: string[];
  recents: (string | { address: string; count: number; last: string })[];
  userNames: Record<string, string>;
  names?: Record<string, string>;
  searches?: SearchCase[];
}
const vectors = JSON.parse(readRepoFile('data/test-vectors.json')) as {
  addresses: { text: string; group: number; bank: number; number: number; index: number; msb: number; lsb: number; program: number }[];
  parse: { input: string; canonical: string }[];
  invalid: string[];
  search: SearchCase[];
  library: { file: unknown; expect: LibExpect; invalidEntries: { file: unknown; expect: LibExpect } };
};

function check(c: SearchCase, hits: string[]): void {
  if (c.expect) expect(hits).toEqual(c.expect);
  if (c.expectCount !== undefined) expect(hits).toHaveLength(c.expectCount);
  if (c.expectFirst) expect(hits[0]).toBe(c.expectFirst);
  if (c.expectLast) expect(hits[hits.length - 1]).toBe(c.expectLast);
  for (const a of c.expectContains ?? []) expect(hits).toContain(a);
  for (const a of c.expectNot ?? []) expect(hits).not.toContain(a);
  if (c.expectOnlyPresets) expect(hits.filter((h) => h.startsWith('U'))).toEqual([]);
}

describe('shared vectors', () => {
  test('addresses', () => {
    for (const v of vectors.addresses) {
      const a = parseAddress(v.text)!;
      expect(a, v.text).not.toBeNull();
      expect([a.group, a.bank, a.number, a.index]).toEqual([v.group, v.bank, v.number, v.index]);
      expect(formatAddress(a)).toBe(v.text);
      expect(midiTriplet(a)).toEqual({ msb: v.msb, lsb: v.lsb, program: v.program });
      expect(fromIndex(v.index)).toBe(a);
    }
  });

  test('parse', () => {
    for (const v of vectors.parse) expect(formatAddress(parseAddress(v.input)!), JSON.stringify(v.input)).toBe(v.canonical);
  });

  test('invalid', () => {
    for (const s of vectors.invalid) expect(parseAddress(s), JSON.stringify(s)).toBeNull();
  });

  test('search', () => {
    const cat = new Catalog();
    expect(cat.presetCount()).toBe(384);
    for (const c of vectors.search) {
      check(c, runSearch(c.query, (a) => cat.name(a)).map(formatAddress));
    }
  });

  function loadLib(file: unknown): { lib: Library; cat: Catalog } {
    const cat = new Catalog();
    const lib = new Library(cat, new MemoryStorage(JSON.stringify(file)));
    expect(lib.load()).toBe('loaded');
    return { lib, cat };
  }

  test('library', () => {
    const { file, expect: ex } = vectors.library;
    const { lib, cat } = loadLib(file);
    expect(lib.favorites().map(formatAddress)).toEqual(ex.favorites);
    expect(lib.recents().map((r) => ({ address: formatAddress(r.address), count: r.count, last: r.last }))).toEqual(ex.recents);
    expect(Object.fromEntries(cat.userNameEntries().map(([a, n]) => [formatAddress(a), n]))).toEqual(ex.userNames);
    for (const [addr, name] of Object.entries(ex.names ?? {})) expect(cat.name(parseAddress(addr)!), addr).toBe(name);
    for (const c of ex.searches ?? []) check(c, runSearch(c.query, (a) => cat.name(a)).map(formatAddress));
  });

  test('library invalidEntries', () => {
    const { file, expect: ex } = vectors.library.invalidEntries;
    const { lib, cat } = loadLib(file);
    expect(lib.favorites().map(formatAddress)).toEqual(ex.favorites);
    expect(lib.recents().map((r) => formatAddress(r.address))).toEqual(ex.recents);
    expect(Object.fromEntries(cat.userNameEntries().map(([a, n]) => [formatAddress(a), n]))).toEqual(ex.userNames);
  });
});
