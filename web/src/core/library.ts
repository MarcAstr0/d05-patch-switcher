// Per-user state: favourites, recents, user names. Same JSON schema as the desktop
// library.json v1 (docs/SPEC.md 3.3). No DOM; storage is injected.
import { formatAddress, isUser, parseAddress, type PatchAddress } from './address';
import type { Catalog } from './catalog';

export const FILE_VERSION = 1;
export const MAX_RECENTS = 50;
export const DEBOUNCE_MS = 500;
export const RETRY_MS = 2000;
const MAX_COUNT = 2147483647;

export interface Recent {
  readonly address: PatchAddress;
  /** Send count, saturating at 2147483647. */
  readonly count: number;
  /** UTC, `YYYY-MM-DDTHH:MM:SSZ`. */
  readonly last: string;
}

export interface LibraryStorage {
  /** The stored text, or null when nothing is stored. */
  read(): string | null;
  /** May throw (quota, private mode); the library keeps the data and retries. */
  write(text: string): void;
  /** Keep a copy of an unreadable file out of the way (desktop: library.json.broken-<stamp>). */
  quarantine?(text: string): void;
}

export class MemoryStorage implements LibraryStorage {
  text: string | null;
  broken: string[] = [];
  writes = 0;
  failWrites = false;
  constructor(text: string | null = null) {
    this.text = text;
  }
  read(): string | null {
    return this.text;
  }
  write(text: string): void {
    if (this.failWrites) throw new Error('write failed');
    this.text = text;
    this.writes++;
  }
  quarantine(text: string): void {
    this.broken.push(text);
  }
}

export class LocalLibraryStorage implements LibraryStorage {
  constructor(
    private readonly key = 'd05.library',
    private readonly store: Pick<Storage, 'getItem' | 'setItem'> = localStorage,
  ) {}
  read(): string | null {
    try {
      return this.store.getItem(this.key);
    } catch {
      return null;
    }
  }
  write(text: string): void {
    this.store.setItem(this.key, text);
  }
  quarantine(text: string): void {
    try {
      const stamp = new Date().toISOString().replace(/[-:]/g, '').replace(/\.\d+Z$/, '');
      this.store.setItem(`${this.key}.broken-${stamp}`, text);
    } catch {
      /* nothing more to do */
    }
  }
}

export interface ParsedLibrary {
  favorites: PatchAddress[];
  recents: Recent[];
  userNames: [PatchAddress, string][];
  /** Entries dropped because they were not valid (bad address, wrong type, duplicate, beyond the cap). */
  skipped: number;
}

const ISO = /^(\d{4})-(\d\d)-(\d\d)(?:T(\d\d):(\d\d)(?::(\d\d)(?:[.,]\d+)?)?)?(Z|[+-]\d\d(?::?\d\d)?)?$/;

/** ISO 8601; text without a zone is UTC. Returns null when it is not a date. */
export function parseIsoUtc(text: string): number | null {
  const m = ISO.exec(text.trim());
  if (!m) return null;
  const [y, mo, d, h = 0, mi = 0, s = 0] = [m[1], m[2], m[3], m[4], m[5], m[6]].map((v) => (v === undefined ? 0 : Number(v)));
  const ms = Date.UTC(y!, mo! - 1, d!, h, mi, s);
  const chk = new Date(ms);
  if (chk.getUTCMonth() !== mo! - 1 || chk.getUTCDate() !== d! || h > 23 || mi > 59 || s > 59) return null;
  let offset = 0;
  const z = m[7];
  if (z && z !== 'Z') {
    const sign = z[0] === '-' ? -1 : 1;
    const digits = z.slice(1).replace(':', '');
    offset = sign * (Number(digits.slice(0, 2)) * 60 + Number(digits.slice(2) || '0'));
  }
  return ms - offset * 60000;
}

export function formatIsoUtc(ms: number): string {
  return new Date(ms).toISOString().replace(/\.\d+Z$/, 'Z');
}

const isObject = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v);

/**
 * Validates like the desktop: null when the text is not a usable library (not JSON, not an
 * object, version is not 1, or a section has the wrong type); otherwise the good entries,
 * with bad ones skipped.
 */
export function parseLibraryJson(text: string): ParsedLibrary | null {
  let doc: unknown;
  try {
    doc = JSON.parse(text);
  } catch {
    return null;
  }
  if (!isObject(doc)) return null;
  const { version, favorites: fv, recents: rv, userNames: nv } = doc;
  if (version !== FILE_VERSION) return null;
  if (!(Array.isArray(fv) || fv === undefined)) return null;
  if (!(Array.isArray(rv) || rv === undefined)) return null;
  if (!(isObject(nv) || nv === undefined)) return null;

  let skipped = 0;
  const seenFav = new Set<number>();
  const favorites: PatchAddress[] = [];
  for (const v of fv ?? []) {
    const a = typeof v === 'string' ? parseAddress(v) : null;
    if (!a || seenFav.has(a.index)) {
      skipped++;
      continue;
    }
    seenFav.add(a.index);
    favorites.push(a);
  }
  favorites.sort((x, y) => x.index - y.index);

  const parsed: { r: Recent; ms: number }[] = [];
  for (const v of rv ?? []) {
    const e = isObject(v) ? v : {};
    const a = typeof e.address === 'string' ? parseAddress(e.address) : null;
    if (!a) {
      skipped++;
      continue;
    }
    const c = typeof e.count === 'number' && Number.isFinite(e.count) ? e.count : 1;
    const count = c >= MAX_COUNT ? MAX_COUNT : c < 1 ? 1 : Math.trunc(c);
    const ms = typeof e.last === 'string' ? parseIsoUtc(e.last) : null;
    parsed.push({ r: { address: a, count, last: formatIsoUtc(ms ?? 0) }, ms: ms ?? 0 });
  }
  parsed.sort((x, y) => y.ms - x.ms); // stable: ties keep file order
  const recents: Recent[] = [];
  const seenRecent = new Set<number>();
  for (const { r } of parsed) {
    if (recents.length >= MAX_RECENTS || seenRecent.has(r.address.index)) {
      skipped++;
      continue;
    }
    seenRecent.add(r.address.index);
    recents.push(r);
  }

  const userNames: [PatchAddress, string][] = [];
  for (const [k, v] of Object.entries(nv ?? {})) {
    const a = parseAddress(k);
    if (a && isUser(a) && typeof v === 'string') userNames.push([a, v]);
    else skipped++;
  }
  return { favorites, recents, userNames, skipped };
}

export type LoadResult = 'fresh' | 'loaded' | 'recovered';

export type ImportResult =
  | { ok: true; favorites: number; recents: number; userNames: number; skipped: number }
  | { ok: false; error: string };

export class Library {
  private favs: PatchAddress[] = [];
  private recs: Recent[] = [];
  private dirty = false;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private failing = false;
  private blocked = false;
  private listeners = new Set<() => void>();
  /** Set while a save keeps failing (storage full or unavailable); null otherwise. */
  saveError: string | null = null;

  constructor(
    private readonly catalog: Catalog,
    private readonly storage: LibraryStorage,
  ) {}

  /** Reads storage. A corrupt file is quarantined and a fresh library started ('recovered'). */
  load(): LoadResult {
    this.cancelTimer();
    this.dirty = false;
    this.failing = false;
    this.blocked = false;
    this.favs = [];
    this.recs = [];
    this.catalog.clearUserNames();
    let text: string | null;
    try {
      text = this.storage.read();
    } catch {
      text = null;
    }
    if (text === null) {
      this.emit();
      return 'fresh';
    }
    const p = parseLibraryJson(text);
    if (!p) {
      try {
        this.storage.quarantine?.(text);
      } catch {
        this.blocked = true; // could not set it aside: never overwrite it
      }
      this.emit();
      return 'recovered';
    }
    this.apply(p);
    this.emit();
    return 'loaded';
  }

  private apply(p: ParsedLibrary): void {
    this.favs = p.favorites;
    this.recs = p.recents;
    this.catalog.clearUserNames();
    for (const [a, n] of p.userNames) this.catalog.setUserName(a, n);
  }

  onChange(fn: () => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit(): void {
    for (const fn of this.listeners) fn();
  }

  /** Favourites in catalogue order. */
  favorites(): readonly PatchAddress[] {
    return this.favs;
  }

  isFavorite(a: PatchAddress): boolean {
    return this.favs.some((f) => f.index === a.index);
  }

  setFavorite(a: PatchAddress, on: boolean): void {
    const i = this.favs.findIndex((f) => f.index >= a.index);
    const has = i >= 0 && this.favs[i]!.index === a.index;
    if (on === has) return;
    if (on) this.favs.splice(i < 0 ? this.favs.length : i, 0, a);
    else this.favs.splice(i, 1);
    this.changed();
  }

  toggleFavorite(a: PatchAddress): boolean {
    const now = !this.isFavorite(a);
    this.setFavorite(a, now);
    return now;
  }

  /** Most recent first. */
  recents(): readonly Recent[] {
    return this.recs;
  }

  /** A send: moves the entry to the front, increments its count (saturating), caps at 50. */
  recordSend(a: PatchAddress, when: Date = new Date()): void {
    let count = 1;
    const i = this.recs.findIndex((r) => r.address.index === a.index);
    if (i >= 0) {
      const old = this.recs[i]!.count;
      count = old < MAX_COUNT ? old + 1 : old;
      this.recs.splice(i, 1);
    }
    this.recs.unshift({ address: a, count, last: formatIsoUtc(when.getTime()) });
    if (this.recs.length > MAX_RECENTS) this.recs.length = MAX_RECENTS;
    this.changed();
  }

  clearRecents(): void {
    if (this.recs.length === 0) return;
    this.recs = [];
    this.changed();
  }

  /** Sanitises (see Catalog.setUserName). False for preset slots. */
  setUserName(a: PatchAddress, name: string): boolean {
    if (!isUser(a)) return false;
    const before = this.catalog.name(a);
    this.catalog.setUserName(a, name);
    if (this.catalog.name(a) !== before) this.changed();
    return true;
  }

  /** The library.json v1 text (2-space indent, trailing newline). */
  exportJson(): string {
    const userNames: Record<string, string> = {};
    for (const [a, n] of this.catalog.userNameEntries()) userNames[formatAddress(a)] = n;
    const doc = {
      version: FILE_VERSION,
      favorites: this.favs.map(formatAddress),
      recents: this.recs.map((r) => ({ address: formatAddress(r.address), count: r.count, last: r.last })),
      userNames,
    };
    return JSON.stringify(doc, null, 2) + '\n';
  }

  /**
   * Replaces the whole library with the file's contents (bad entries skipped). Never throws;
   * on garbage the library is left untouched and `{ ok: false }` says why.
   */
  importJson(text: string): ImportResult {
    let p: ParsedLibrary | null = null;
    try {
      p = parseLibraryJson(String(text));
    } catch {
      p = null;
    }
    if (!p) return { ok: false, error: 'Not a D-05 Patch Switcher library file (library.json version 1).' };
    this.apply(p);
    this.blocked = false;
    this.changed();
    return { ok: true, favorites: p.favorites.length, recents: p.recents.length, userNames: p.userNames.length, skipped: p.skipped };
  }

  private changed(): void {
    this.dirty = true;
    this.schedule(this.failing ? RETRY_MS : DEBOUNCE_MS);
    this.emit();
  }

  private schedule(ms: number): void {
    if (this.blocked) return;
    this.cancelTimer();
    this.timer = setTimeout(() => this.flush(), ms);
  }

  private cancelTimer(): void {
    if (this.timer !== null) clearTimeout(this.timer);
    this.timer = null;
  }

  /** Writes now if there is anything unsaved. Returns false when the write failed. */
  flush(): boolean {
    this.cancelTimer();
    if (!this.dirty) return true;
    if (this.blocked) {
      this.saveError = 'The stored library could not be set aside; not overwriting it.';
      return false;
    }
    try {
      this.storage.write(this.exportJson());
    } catch (e) {
      this.failing = true;
      this.saveError = e instanceof Error ? e.message : String(e);
      this.schedule(RETRY_MS);
      return false;
    }
    this.dirty = false;
    this.failing = false;
    this.saveError = null;
    return true;
  }
}
