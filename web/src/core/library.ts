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
  /** The stored text; null only when nothing is stored. Throws when storage cannot be read. */
  read(): string | null;
  /** May throw (quota, private mode); the library keeps the data and retries. */
  write(text: string): void;
  /**
   * Set an unreadable file aside, like the desktop's library.json.broken-<stamp>: keep a copy
   * AND remove the primary entry. Throws if either step fails (the library then refuses to write).
   */
  quarantine?(text: string): void;
}

export class MemoryStorage implements LibraryStorage {
  text: string | null;
  broken: string[] = [];
  writes = 0;
  failWrites = false;
  failReads = false;
  failQuarantine = false;
  constructor(text: string | null = null) {
    this.text = text;
  }
  read(): string | null {
    if (this.failReads) throw new Error('read failed');
    return this.text;
  }
  write(text: string): void {
    if (this.failWrites) throw new Error('write failed');
    this.text = text;
    this.writes++;
  }
  quarantine(text: string): void {
    if (this.failQuarantine) throw new Error('quarantine failed');
    this.broken.push(text);
    this.text = null;
  }
}

export const MAX_BROKEN_COPIES = 3;

type StoreLike = Pick<Storage, 'getItem' | 'setItem' | 'removeItem' | 'key' | 'length'>;

export class LocalLibraryStorage implements LibraryStorage {
  private readonly given: StoreLike | undefined;
  constructor(
    private readonly key = 'd05.library',
    store?: StoreLike,
  ) {
    this.given = store;
  }
  /** Resolved on use: merely touching `localStorage` throws when site storage is blocked. */
  private get store(): StoreLike {
    const s = this.given ?? (globalThis as { localStorage?: StoreLike }).localStorage;
    if (!s) throw new Error('localStorage is not available');
    return s;
  }
  read(): string | null {
    return this.store.getItem(this.key);
  }
  write(text: string): void {
    this.store.setItem(this.key, text);
  }
  quarantine(text: string): void {
    const st = this.store;
    const stamp = new Date().toISOString().replace(/[-:.]/g, '').replace(/Z$/, '');
    const base = `${this.key}.broken-${stamp}`;
    let copy = base;
    for (let n = 2; st.getItem(copy) !== null; n++) copy = `${base}-${n}`;
    st.setItem(copy, text);
    try {
      st.removeItem(this.key);
      if (st.getItem(this.key) !== null) throw new Error('could not remove the unreadable library');
    } catch (e) {
      try {
        st.removeItem(copy);
      } catch {
        /* best effort */
      }
      throw e;
    }
    this.pruneBroken(st);
  }
  private pruneBroken(st: StoreLike): void {
    const prefix = `${this.key}.broken-`;
    const keys: string[] = [];
    for (let i = 0; i < st.length; i++) {
      const k = st.key(i);
      if (k !== null && k.startsWith(prefix)) keys.push(k);
    }
    keys.sort();
    try {
      for (const k of keys.slice(0, Math.max(0, keys.length - MAX_BROKEN_COPIES))) st.removeItem(k);
    } catch {
      /* best effort */
    }
  }
}

/**
 * localStorage when it works, otherwise an in-memory store. `persistent: false` means
 * changes will not survive a reload (the UI should say so).
 */
export function createLibraryStorage(key = 'd05.library', getStore?: () => StoreLike): { storage: LibraryStorage; persistent: boolean } {
  try {
    const st = getStore ? getStore() : (globalThis as { localStorage?: StoreLike }).localStorage;
    if (!st) throw new Error('no localStorage');
    const probe = `${key}.probe`;
    st.setItem(probe, '1');
    st.removeItem(probe);
    return { storage: new LocalLibraryStorage(key, st), persistent: true };
  } catch {
    return { storage: new MemoryStorage(), persistent: false };
  }
}

export interface ParsedLibrary {
  favorites: PatchAddress[];
  recents: Recent[];
  userNames: [PatchAddress, string][];
  /** Entries dropped because they were not valid (bad address, wrong type, duplicate, beyond the cap). */
  skipped: number;
}

// What Qt::ISODate accepts for a date-time: date, optional [T t space] hh[:mm[:ss]] with a
// fraction (. or ,) on the last field given, optional Z/z/+hh[[:]mm]. Nothing is trimmed.
const ISO =
  /^(\d{4})-(\d\d)-(\d\d)(?:[Tt ](\d\d)(?::(\d\d)(?::(\d\d))?)?(?:[.,](\d+))?)?(Z|z|[+-]\d\d(?::?\d\d)?)?$/;

/** ISO 8601 as the desktop reads it; text without a zone is UTC. Milliseconds since the epoch, or null. */
export function parseIsoUtc(text: string): number | null {
  const m = ISO.exec(text);
  if (!m) return null;
  const y = Number(m[1]);
  const mo = Number(m[2]);
  const d = Number(m[3]);
  const hasH = m[4] !== undefined;
  let h = hasH ? Number(m[4]) : 0;
  const mi = m[5] !== undefined ? Number(m[5]) : 0;
  const sec = m[6] !== undefined ? Number(m[6]) : 0;
  if (y < 1) return null;
  const frac = m[7] !== undefined ? Number('0.' + m[7]) : 0;
  // The fraction belongs to the last field present.
  const unit = m[6] !== undefined ? 1000 : m[5] !== undefined ? 60000 : 3600000;
  const fracMs = Math.floor(frac * unit);
  let dayShift = 0;
  if (h === 24) {
    if (mi !== 0 || sec !== 0 || fracMs !== 0) return null;
    h = 0;
    dayShift = 1;
  }
  if (h > 23 || mi > 59 || sec > 59) return null;
  const dt = new Date(0);
  dt.setUTCFullYear(y, mo - 1, d); // setUTCFullYear: years 0..99 are not mapped to 19xx
  dt.setUTCHours(0, 0, 0, 0);
  if (dt.getUTCFullYear() !== y || dt.getUTCMonth() !== mo - 1 || dt.getUTCDate() !== d) return null;
  let ms = dt.getTime() + dayShift * 86400000 + ((h * 60 + mi) * 60 + sec) * 1000 + fracMs;
  const z = m[8];
  if (z && z !== 'Z' && z !== 'z') {
    const sign = z[0] === '-' ? -1 : 1;
    const digits = z.slice(1).replace(':', '');
    const oh = Number(digits.slice(0, 2));
    const om = Number(digits.slice(2) || '0');
    if (oh > 23 || om > 59) return null;
    ms -= sign * (oh * 60 + om) * 60000;
  }
  return ms;
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
  // QJsonObject iterates keys sorted by UTF-16 code unit, and a later duplicate (same address,
  // different spelling) overwrites an earlier one: do the same.
  for (const [k, v] of Object.entries(nv ?? {}).sort(([x], [y]) => (x < y ? -1 : x > y ? 1 : 0))) {
    const a = parseAddress(k);
    if (a && isUser(a) && typeof v === 'string') userNames.push([a, v]);
    else skipped++;
  }
  return { favorites, recents, userNames, skipped };
}

/**
 * fresh: nothing stored. loaded: ok. recovered: corrupt file set aside, fresh library.
 * unreadable / sidelineFailed: storage could not be read / the corrupt file could not be set
 * aside; the library is empty and refuses to write so the stored data is never overwritten.
 */
export type LoadResult = 'fresh' | 'loaded' | 'recovered' | 'unreadable' | 'sidelineFailed';

export type ImportResult =
  | { ok: true; favorites: number; recents: number; userNames: number; skipped: number }
  | { ok: false; error: string };

export class Library {
  private favs: PatchAddress[] = [];
  private recs: Recent[] = [];
  private dirty = false;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private failing = false;
  private blockedReason: string | null = null;
  private listeners = new Set<() => void>();
  private saveListeners = new Set<(error: string | null) => void>();
  private err: string | null = null;

  constructor(
    private readonly catalog: Catalog,
    private readonly storage: LibraryStorage,
  ) {}

  /** The reason the last save failed (or writes are blocked); null when saving works. */
  get saveError(): string | null {
    return this.err;
  }

  /** Called whenever `saveError` changes: when a save starts failing and when one succeeds again. */
  onSaveStatus(fn: (error: string | null) => void): () => void {
    this.saveListeners.add(fn);
    return () => this.saveListeners.delete(fn);
  }

  private setSaveError(e: string | null): void {
    if (e === this.err) return;
    this.err = e;
    for (const fn of this.saveListeners) fn(e);
  }

  /** Reads storage. A corrupt file is set aside and a fresh library started ('recovered'). */
  load(): LoadResult {
    this.cancelTimer();
    this.dirty = false;
    this.failing = false;
    this.blockedReason = null;
    this.favs = [];
    this.recs = [];
    this.catalog.clearUserNames();
    const result = this.loadInto();
    if (this.blockedReason) this.setSaveError(this.blockedReason);
    else this.setSaveError(null);
    this.emit();
    return result;
  }

  private readStored(): { text: string | null; failed: boolean } {
    try {
      return { text: this.storage.read(), failed: false };
    } catch {
      return { text: null, failed: true };
    }
  }

  private loadInto(): LoadResult {
    const { text, failed } = this.readStored();
    if (failed) {
      this.blockedReason = 'Stored library could not be read; not overwriting it.';
      return 'unreadable';
    }
    if (text === null) return 'fresh';
    const p = parseLibraryJson(text);
    if (p) {
      this.apply(p);
      return 'loaded';
    }
    return this.sideline(text);
  }

  /** Sets a corrupt stored text aside. If that fails, writes are blocked so the text is never overwritten. */
  private sideline(text: string): LoadResult {
    try {
      if (!this.storage.quarantine) throw new Error('no way to set it aside');
      this.storage.quarantine(text);
    } catch {
      this.blockedReason = 'The stored library is corrupt and could not be set aside; not overwriting it.';
      return 'sidelineFailed';
    }
    return 'recovered';
  }

  private block(result: LoadResult): LoadResult {
    if (this.blockedReason) {
      this.cancelTimer();
      this.setSaveError(this.blockedReason);
    }
    return result;
  }

  /**
   * For a `storage` event (another tab wrote the library). With no unsaved edits it is a plain
   * load(). With unsaved local edits it merges instead of losing either side: favourites are
   * the union, recents are the union keeping the newer entry per address (cap 50), and local
   * user names win over stored ones. The merged result is then saved as usual.
   */
  reload(): LoadResult {
    if (!this.dirty) return this.load();
    const { text, failed } = this.readStored();
    if (failed) {
      this.blockedReason = 'Stored library could not be read; not overwriting it.';
      return this.block('unreadable');
    }
    if (text === null) return 'fresh';
    const p = parseLibraryJson(text);
    // Keep the local edits, but set the corrupt value aside first (or block saving if that fails).
    if (!p) return this.block(this.sideline(text));
    const fav = new Map(this.favs.map((a) => [a.index, a] as const));
    for (const a of p.favorites) fav.set(a.index, a);
    this.favs = [...fav.values()].sort((x, y) => x.index - y.index);
    const rec = new Map(this.recs.map((r) => [r.address.index, r] as const));
    for (const r of p.recents) {
      const mine = rec.get(r.address.index);
      if (!mine || parseIsoUtc(r.last)! > parseIsoUtc(mine.last)!) rec.set(r.address.index, r);
    }
    this.recs = [...rec.values()].sort((x, y) => parseIsoUtc(y.last)! - parseIsoUtc(x.last)!).slice(0, MAX_RECENTS);
    for (const [a, n] of p.userNames) if (!this.catalog.hasUserName(a)) this.catalog.setUserName(a, n);
    this.changed();
    return 'loaded';
  }

  onChange(fn: () => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit(): void {
    for (const fn of this.listeners) fn();
  }

  private apply(p: ParsedLibrary): void {
    this.favs = p.favorites;
    this.recs = p.recents;
    this.catalog.clearUserNames();
    for (const [a, n] of p.userNames) this.catalog.setUserName(a, n);
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
    this.changed();
    return { ok: true, favorites: p.favorites.length, recents: p.recents.length, userNames: p.userNames.length, skipped: p.skipped };
  }

  private changed(): void {
    this.dirty = true;
    this.schedule(this.failing ? RETRY_MS : DEBOUNCE_MS);
    this.emit();
  }

  private schedule(ms: number): void {
    if (this.blockedReason) return;
    this.cancelTimer();
    this.timer = setTimeout(() => this.flush(), ms);
  }

  private cancelTimer(): void {
    if (this.timer !== null) clearTimeout(this.timer);
    this.timer = null;
  }

  /** Writes now if there is anything unsaved. Returns false when the write failed or is blocked. */
  flush(): boolean {
    this.cancelTimer();
    if (!this.dirty) return true;
    if (this.blockedReason) {
      this.setSaveError(this.blockedReason);
      return false;
    }
    try {
      this.storage.write(this.exportJson());
    } catch (e) {
      this.failing = true;
      this.setSaveError(e instanceof Error ? e.message : String(e));
      this.schedule(RETRY_MS);
      return false;
    }
    this.dirty = false;
    this.failing = false;
    this.setSaveError(null);
    return true;
  }
}
