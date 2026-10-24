// FR-3 search grammar: whitespace-separated tokens, all must match. No DOM.
import { allAddresses, type PatchAddress } from './address';

export interface Range {
  /** UTF-16 index into the original name. */
  readonly start: number;
  readonly length: number;
}

type Token =
  | { kind: 'group'; group: number }
  | { kind: 'groupBank'; group: number; bank: number }
  | { kind: 'exact'; group: number; bank: number; number: number }
  | { kind: 'text'; folded: string };

const MARK = /\p{M}/u;

/** Folds one UTF-16 unit: lowercase, first unit of its canonical decomposition; combining marks vanish (''). */
function foldUnit(c: string): string {
  const u = c.charCodeAt(0);
  if (u < 0x80) return u >= 65 && u <= 90 ? String.fromCharCode(u + 32) : c;
  if (u >= 0xd800 && u <= 0xdfff) return c;
  if (MARK.test(c)) return '';
  const d = c.normalize('NFD');
  return d === '' ? '' : d[0]!.toLowerCase();
}

function foldString(s: string): string {
  let out = '';
  for (let i = 0; i < s.length; i++) out += foldUnit(s[i]!);
  return out;
}

const digit = (c: string | undefined): number => (c !== undefined && c >= '0' && c <= '9' ? c.charCodeAt(0) - 48 : -1);

function parseToken(raw: string): Token | null {
  let i = 0;
  let hasLetter = false;
  let user = false;
  const first = raw[0]!.toLowerCase();
  if (first === 'u' || first === 'p') {
    hasLetter = true;
    user = first === 'u';
    i = 1;
  }
  const g = i < raw.length ? digit(raw[i]) : -1;
  if (g >= 0) {
    i++;
    let dash = false;
    if (raw[i] === '-') {
      dash = true;
      i++;
    }
    const rest: number[] = [];
    let ok = true;
    for (; i < raw.length; i++) {
      const d = digit(raw[i]);
      if (d < 1 || d > 8 || rest.length === 2) {
        ok = false;
        break;
      }
      rest.push(d);
    }
    if (ok && g >= 1 && g <= (user ? 8 : 6)) {
      const group = user ? g - 1 : g + 7;
      if (rest.length === 0 && !dash && hasLetter) return { kind: 'group', group };
      if (rest.length === 1 && hasLetter) return { kind: 'groupBank', group, bank: rest[0]! };
      if (rest.length === 2) return { kind: 'exact', group, bank: rest[0]!, number: rest[1]! };
    }
  }
  const folded = foldString(raw);
  return folded === '' ? null : { kind: 'text', folded };
}

export class SearchQuery {
  private readonly tokens: Token[] = [];

  constructor(text: string) {
    for (const raw of text.trim().split(/\s+/)) {
      if (raw === '') continue;
      const t = parseToken(raw);
      if (t) this.tokens.push(t);
    }
  }

  /** True when the query has no tokens (everything matches). */
  get isEmpty(): boolean {
    return this.tokens.length === 0;
  }

  /** True when at least one token is a text token. */
  get hasText(): boolean {
    return this.tokens.some((t) => t.kind === 'text');
  }

  /**
   * Predicate. When `ranges` is given it is filled with merged, sorted match ranges
   * (UTF-16 indices into `name`) for the text tokens.
   */
  matches(a: PatchAddress, name: string, ranges?: Range[]): boolean {
    if (ranges) ranges.length = 0;
    let needText = false;
    for (const t of this.tokens) {
      switch (t.kind) {
        case 'group':
          if (a.group !== t.group) return false;
          break;
        case 'groupBank':
          if (a.group !== t.group || a.bank !== t.bank) return false;
          break;
        case 'exact':
          if (a.group !== t.group || a.bank !== t.bank || a.number !== t.number) return false;
          break;
        case 'text':
          needText = true;
          break;
      }
    }
    if (!needText) return true;

    let folded = '';
    const map: number[] = [];
    for (let i = 0; i < name.length; i++) {
      const f = foldUnit(name[i]!);
      if (f !== '') {
        folded += f;
        map.push(i);
      }
    }
    const found: Range[] = [];
    for (const t of this.tokens) {
      if (t.kind !== 'text') continue;
      const fi = folded.indexOf(t.folded);
      if (fi < 0) return false;
      const start = map[fi]!;
      let end = map[fi + t.folded.length - 1]! + 1;
      while (end < name.length && foldUnit(name[end]!) === '') end++; // keep trailing combining marks
      found.push({ start, length: end - start });
    }
    if (ranges && found.length > 0) {
      found.sort((x, y) => x.start - y.start);
      let cur = found[0]!;
      for (let i = 1; i < found.length; i++) {
        const r = found[i]!;
        if (r.start <= cur.start + cur.length) {
          cur = { start: cur.start, length: Math.max(cur.length, r.start + r.length - cur.start) };
        } else {
          ranges.push(cur);
          cur = r;
        }
      }
      ranges.push(cur);
    }
    return true;
  }
}

/** All matching addresses in catalogue order. `nameOf` supplies the (user-overlaid) name. */
export function runSearch(query: string | SearchQuery, nameOf: (a: PatchAddress) => string): PatchAddress[] {
  const q = typeof query === 'string' ? new SearchQuery(query) : query;
  return allAddresses().filter((a) => q.matches(a, nameOf(a)));
}
