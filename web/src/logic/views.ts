// The list model: view (All, Favourites, Recents) filtered by the search (SPEC FR-3, FR-4).
import { allAddresses, formatAddress, type PatchAddress } from '../core/address';
import type { Catalog, KeyMode } from '../core/catalog';
import type { Library } from '../core/library';
import { SearchQuery, type Range } from '../core/search';

export type View = 'all' | 'favorites' | 'recents';

export interface Row {
  readonly address: PatchAddress;
  /** Catalogue name; empty for an unnamed user slot. */
  readonly name: string;
  /** Key mode of a preset; empty for user slots. */
  readonly mode: KeyMode | '';
  /** Name or, for an unnamed user slot, the address. */
  readonly shown: string;
  readonly ranges: readonly Range[];
  /** Send count; Recents view only. */
  readonly count: number;
}

export function buildRows(view: View, queryText: string, catalog: Catalog, library: Library): Row[] {
  const q = new SearchQuery(queryText);
  const source: { a: PatchAddress; count: number }[] =
    view === 'all'
      ? allAddresses().map((a) => ({ a, count: 0 }))
      : view === 'favorites'
        ? library.favorites().map((a) => ({ a, count: 0 }))
        : library.recents().map((r) => ({ a: r.address, count: r.count }));
  const rows: Row[] = [];
  const ranges: Range[] = [];
  for (const { a, count } of source) {
    const name = catalog.name(a);
    if (!q.matches(a, name, ranges)) continue;
    rows.push({ address: a, name, mode: catalog.mode(a), shown: name === '' ? formatAddress(a) : name, ranges: ranges.slice(), count });
  }
  return rows;
}

export const searchActive = (queryText: string): boolean => !new SearchQuery(queryText).isEmpty;

export function rowOf(rows: readonly Row[], a: PatchAddress | null): number {
  return a ? rows.findIndex((r) => r.address.index === a.index) : -1;
}

export function emptyText(view: View, queryText: string, mac: boolean): { title: string; note: string } {
  if (searchActive(queryText)) return { title: 'NO MATCHES', note: 'Esc clears the search' };
  if (view === 'favorites') return { title: 'NO FAVOURITES', note: `Space or ${mac ? '⌘' : 'Ctrl+'}D adds the highlighted patch` };
  if (view === 'recents') return { title: 'NOTHING SENT YET', note: 'Patches you send appear here' };
  return { title: '', note: '' };
}

/** The phone texts of DESIGN-WEB 7.8 (no keyboard on a phone). */
export function emptyTextPhone(view: View, queryText: string): { title: string; note: string } {
  if (searchActive(queryText)) return { title: 'NO MATCHES', note: 'Clear the search to see all patches' };
  if (view === 'favorites') return { title: 'NO FAVOURITES', note: 'Tap a star to add a favourite' };
  if (view === 'recents') return { title: 'NOTHING SENT YET', note: 'Patches you send appear here' };
  return { title: '', note: '' };
}
