// The keyboard map (SPEC FR-13) as one table. `matchKey` and the Shortcuts sheet both read it.
// Web differences: `Mod` is Ctrl, or Cmd on macOS; there is no Quit.

export type Action =
  | 'focusSearch'
  | 'escape'
  | 'up'
  | 'down'
  | 'home'
  | 'end'
  | 'send'
  | 'toggleFavorite'
  | 'toggleFavoriteSpace'
  | 'prev'
  | 'next'
  | 'gridEntry'
  | 'viewAll'
  | 'viewFavorites'
  | 'viewRecents'
  | 'rename'
  | 'autoSend'
  | 'settings'
  | 'shortcuts';

/** Where a binding applies: anywhere on the page, only with the list focused, or list or search field. */
export type Scope = 'global' | 'list' | 'listOrSearch';

export interface Binding {
  readonly action: Action;
  /** `Mod+Shift+A`, `/`, `PageUp`, `F2`: modifiers first, then the KeyboardEvent.key (letters upper case). */
  readonly combo: string;
  readonly scope: Scope;
  /** What the browser does with this key, as far as is known (docs/WEB.md 11 and the report; verify on the real browsers). */
  readonly browser?: string;
  /** Browsers that keep this combo on macOS (page never sees it): not shown in the sheet there. */
  readonly macReserved?: boolean;
}

export const BINDINGS: readonly Binding[] = [
  { action: 'focusSearch', combo: '/', scope: 'global', browser: 'Firefox Quick Find opens on `/` outside fields; preventDefault stops it' },
  { action: 'focusSearch', combo: 'Mod+F', scope: 'global', browser: 'Find in page in both; page keydown may preventDefault' },
  { action: 'escape', combo: 'Escape', scope: 'global' },
  { action: 'up', combo: 'ArrowUp', scope: 'listOrSearch' },
  { action: 'down', combo: 'ArrowDown', scope: 'listOrSearch' },
  { action: 'home', combo: 'Home', scope: 'list' },
  { action: 'end', combo: 'End', scope: 'list' },
  { action: 'send', combo: 'Enter', scope: 'listOrSearch' },
  { action: 'toggleFavoriteSpace', combo: ' ', scope: 'list' },
  { action: 'toggleFavorite', combo: 'Mod+D', scope: 'global', browser: 'Add bookmark in both; page keydown may preventDefault' },
  { action: 'prev', combo: 'PageUp', scope: 'global' },
  { action: 'next', combo: 'PageDown', scope: 'global' },
  { action: 'gridEntry', combo: 'Mod+G', scope: 'global', browser: 'Find next in both; page keydown may preventDefault' },
  { action: 'viewAll', combo: 'Mod+1', scope: 'global', browser: 'Chrome and Firefox (macOS, Windows) select tab 1; Firefox on Linux uses Alt+1; page keydown may preventDefault' },
  { action: 'viewFavorites', combo: 'Mod+2', scope: 'global', browser: 'as Mod+1' },
  { action: 'viewRecents', combo: 'Mod+3', scope: 'global', browser: 'as Mod+1' },
  { action: 'rename', combo: 'F2', scope: 'global' },
  { action: 'autoSend', combo: 'Mod+Shift+A', scope: 'global', browser: 'Chrome: Search tabs; Firefox: Add-ons manager; Firefox may refuse to let the page override it' },
  { action: 'settings', combo: 'Mod+,', scope: 'global', browser: 'Chrome and Firefox on macOS keep Cmd+, for their own settings (page never sees it)', macReserved: true },
  { action: 'shortcuts', combo: 'F1', scope: 'global', browser: 'Opens the browser help; page keydown may preventDefault' },
];

interface Parsed {
  key: string;
  mod: boolean;
  shift: boolean;
}

const parsed = new Map<Binding, Parsed>();
function parse(b: Binding): Parsed {
  let p = parsed.get(b);
  if (!p) {
    const parts = b.combo === '+' ? ['+'] : b.combo.split('+');
    const last = parts[parts.length - 1] === '' ? '+' : parts[parts.length - 1]!;
    const key = last.length === 1 ? last.toUpperCase() : last;
    p = { key, mod: parts.includes('Mod'), shift: parts.includes('Shift') };
    parsed.set(b, p);
  }
  return p;
}

export interface KeyLike {
  key: string;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
  altKey: boolean;
}

export type KeyContext = 'search' | 'list' | 'other';

/** The action a key press means in a context, or null. Alt combinations never match. */
export function matchKey(e: KeyLike, mac: boolean, context: KeyContext): Action | null {
  if (e.altKey) return null;
  const mod = mac ? e.metaKey : e.ctrlKey;
  // The other modifier (Ctrl on a Mac, Cmd elsewhere) is never part of a binding.
  if (mac ? e.ctrlKey : e.metaKey) return null;
  const key = e.key.length === 1 ? e.key.toUpperCase() : e.key;
  for (const b of BINDINGS) {
    const p = parse(b);
    if (p.key !== key || p.mod !== mod) continue;
    // Shift is ignored for punctuation, which needs it on some layouts.
    const punctuation = p.key === '/' || p.key === ',' || p.key === ' ';
    if (!punctuation && p.shift !== e.shiftKey) continue;
    if (b.scope === 'list' && context !== 'list') continue;
    if (b.scope === 'listOrSearch' && context === 'other') continue;
    return b.action;
  }
  return null;
}

export const isMac = (nav: { userAgentData?: { platform?: string }; platform?: string }): boolean =>
  /^Mac/i.test(nav.userAgentData?.platform ?? nav.platform ?? '');

/** The text on a key cap (DESIGN.md 10.3 caps, DESIGN-WEB 11.4). */
export function capText(combo: string, mac: boolean): string {
  const parts = combo === '+' ? ['+'] : combo.split('+');
  const key = parts[parts.length - 1]!;
  const names: Record<string, string> = { Escape: 'Esc', PageUp: mac ? 'fn ↑' : 'PgUp', PageDown: mac ? 'fn ↓' : 'PgDn', ' ': 'Space', ArrowUp: 'Up', ArrowDown: 'Down' };
  const base = names[key] ?? (key.length === 1 ? key.toUpperCase() : key);
  const mods: string[] = [];
  if (parts.includes('Mod')) mods.push(mac ? '⌘' : 'Ctrl');
  if (parts.includes('Shift')) mods.push(mac ? '⇧' : 'Shift');
  if (mac) return (mods.includes('⇧') ? '⇧' : '') + (mods.includes('⌘') ? '⌘' : '') + base;
  return [...mods, base].join('+');
}

/** The first binding's combo for an action. */
export function comboOf(action: Action): string {
  return BINDINGS.find((b) => b.action === action)!.combo;
}

export type SheetPart = { kind: 'key'; text: string; combo?: string } | { kind: 'word'; text: string };
export interface SheetRow {
  parts: SheetPart[];
  text: string;
}
export interface SheetSection {
  title: string;
  column: 0 | 1;
  rows: SheetRow[];
}

/** The Shortcuts sheet of DESIGN.md 10.3 (without Quit), as data. */
export function sheetSections(mac: boolean): SheetSection[] {
  const key = (c: string): SheetPart => ({ kind: 'key', text: capText(c, mac), combo: c });
  const lit = (t: string): SheetPart => ({ kind: 'key', text: t });
  const word = (t: string): SheetPart => ({ kind: 'word', text: t });
  const reserved = (c: string): boolean => mac && BINDINGS.some((b) => b.combo === c && b.macReserved);
  const app: SheetRow[] = [];
  if (!reserved('Mod+,')) app.push({ parts: [key('Mod+,')], text: 'Settings' });
  app.push({ parts: [key('F1')], text: 'This sheet' });
  return [
    { title: 'FIND', column: 0, rows: [
      { parts: [key('/'), word('or'), key('Mod+F')], text: 'Focus the search field' },
      { parts: [key('Escape')], text: 'Clear the search' },
      { parts: [key('Escape'), word('again')], text: 'Back to the list' },
    ] },
    { title: 'LIST', column: 0, rows: [
      { parts: [lit('Up'), lit('Down')], text: 'Move the highlight' },
      { parts: [lit('Home'), lit('End')], text: 'First or last row' },
      { parts: [lit('Enter')], text: 'Send highlighted patch' },
      { parts: [lit('Space'), word('or'), key('Mod+D')], text: 'Toggle favourite' },
      { parts: [key('F2')], text: 'Rename a user slot' },
    ] },
    { title: 'PLAY', column: 1, rows: [
      { parts: [key('PageUp')], text: 'Previous patch' },
      { parts: [key('PageDown')], text: 'Next patch' },
      { parts: [key('Mod+G'), word('then'), lit('1-8'), lit('1-8')], text: 'Enter bank and number' },
      { parts: [key('Mod+Shift+A')], text: 'Auto-send on or off' },
    ] },
    { title: 'VIEW', column: 1, rows: [
      { parts: [key('Mod+1')], text: 'All' },
      { parts: [key('Mod+2')], text: 'Favourites' },
      { parts: [key('Mod+3')], text: 'Recents' },
    ] },
    { title: 'APP', column: 1, rows: app },
  ];
}
