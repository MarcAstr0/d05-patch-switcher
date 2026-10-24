// The page: display, group strip, bank and number pads, transport, views, search, list, chips,
// status bar and access panel (docs/design/DESIGN.md 4 to 8, DESIGN-WEB 4 to 8).
// One DOM for the three layouts; CSS positions it from `data-layout` and custom properties.
import type { PatchAddress } from '../core/address';
import { formatAddress, groupLabel } from '../core/address';
import { capText, comboOf } from '../logic/keymap';
import type { Metrics } from '../logic/layout';
import type { View } from '../logic/views';
import { Display } from './display';
import { esc, q } from './dom';
import { PatchList, type ListHandlers } from './list';
import { AccessPanel, StatusBar, type AccessHandlers } from './status';

export interface PanelHandlers extends ListHandlers, AccessHandlers {
  group(g: number): void;
  bank(b: number): void;
  number(n: number): void;
  step(d: 1 | -1): void;
  view(v: View): void;
  autoSend(): void;
  system(anchor: HTMLElement): void;
  chip(a: PatchAddress): void;
  query(text: string): void;
  openSearch(): void;
  closeSearch(): void;
}

export interface Chip {
  address: PatchAddress;
  name: string;
}

export interface PanelState {
  current: PatchAddress;
  entry: { phase: 'idle' | 'bank' | 'number'; group: number; bank: number };
  accessGranted: boolean;
  autoSend: boolean;
  view: View;
  query: string;
  overlay: boolean;
  chips: readonly Chip[];
}

const range = (a: number, b: number): number[] => Array.from({ length: b - a }, (_, i) => a + i);

export class Panel {
  readonly stage: HTMLElement;
  readonly display: Display;
  readonly list: PatchList;
  readonly status: StatusBar;
  readonly access: AccessPanel;
  readonly search: HTMLInputElement;
  readonly groups: HTMLElement;
  readonly grid: HTMLElement;
  metrics: Metrics | null = null;
  private readonly groupBtns: HTMLButtonElement[];
  private readonly bankBtns: HTMLButtonElement[];
  private readonly numBtns: HTMLButtonElement[];
  private readonly viewBtns: Record<View, HTMLButtonElement>;
  private readonly autoBtn: HTMLButtonElement;
  private readonly sysBtn: HTMLButtonElement;
  private readonly prevBtn: HTMLButtonElement;
  private readonly nextBtn: HTMLButtonElement;
  private readonly searchBtn: HTMLButtonElement;
  private readonly searchText: HTMLElement;
  private readonly chipsEl: HTMLElement;
  private readonly banksEl: HTMLElement;
  private readonly numsEl: HTMLElement;
  private groupCursor = 0;
  private gridCursor = { row: 0, col: 0 };
  private lastChips = '';
  private lastGroup = -1;
  private placeholder = '';

  constructor(root: HTMLElement, private readonly h: PanelHandlers, mac: boolean) {
    const cap = (a: Parameters<typeof comboOf>[0]): string => capText(comboOf(a), mac);
    const gbtn = (g: number): string =>
      `<button type="button" class="pad g${g === 0 ? ' firstU' : ''}" data-act="group" data-g="${g}" style="--r:${g < 8 ? 1 : 2};--c:${(g < 8 ? g : g - 8) + 1}" tabindex="-1" title="Group ${groupLabel(g)}"><span class="face"><span class="lg">${groupLabel(g)}</span><i class="led"></i></span></button>`;
    const nbtn = (act: string, n: number, what: string): string =>
      `<button type="button" class="pad ${act === 'bank' ? 'b' : 'n'}" data-act="${act}" data-n="${n}" tabindex="-1" title="${what} ${n}"><span class="face"><span class="num">${n}</span><i class="led"></i></span></button>`;
    const vbtn = (v: View, label: string, tip: string): string =>
      `<button type="button" class="pad v" data-act="view" data-v="${v}" title="${tip}"><span class="face"><span class="num">${label}</span><i class="led"></i></span></button>`;
    const rule = (cls: string, text: string): string => `<div class="rule ${cls}"><i></i><b class="type-panelLabel">${text}</b><i></i></div>`;
    root.innerHTML =
      `<div class="stage" id="stage">` +
      `<div class="nameplate"><span class="wm type-wordmark">PATCH SWITCHER</span><span class="wmr"></span><span class="wms type-wordmarkSub">FOR ROLAND D-05</span></div>` +
      `<div class="bezel" id="bezel"></div>` +
      `<div class="rightbg"></div>` +
      `<div class="band"><div class="bandScroll"><div class="bandInner">` +
      rule('accent r-group', 'PATCH GROUP') +
      `<div class="groups" tabindex="0" role="group" aria-label="Patch group">${[...range(8, 14), ...range(0, 8)].map(gbtn).join('')}</div>` +
      `<div class="grid" tabindex="0" role="group" aria-label="Patch bank and number">` +
      rule('accent r-bank', 'PATCH BANK') +
      `<div class="pads banks">${range(1, 9).map((n) => nbtn('bank', n, 'Bank')).join('')}</div>` +
      rule('accent r-num', 'PATCH NUMBER') +
      `<div class="pads numbers">${range(1, 9).map((n) => nbtn('number', n, 'Number')).join('')}</div>` +
      `</div>` +
      `<div class="transport">` +
      `<button type="button" class="pad t prev" data-act="prev" title="Previous patch (${cap('prev')})"><span class="face"><svg class="wedge" viewBox="0 0 7 8" width="7" height="8" aria-hidden="true"><polygon points="7,0 7,8 0,4"/></svg><span class="num">PREV</span></span></button>` +
      `<button type="button" class="pad t next" data-act="next" title="Next patch (${cap('next')})"><span class="face"><span class="num">NEXT</span><svg class="wedge" viewBox="0 0 7 8" width="7" height="8" aria-hidden="true"><polygon points="0,0 0,8 7,4"/></svg></span></button>` +
      `</div>` +
      rule('secondary r-fav', 'FAVOURITES') +
      `<div class="chips" role="group" aria-label="Favourites"></div>` +
      `</div></div></div>` +
      `<div class="overlayBg"></div>` +
      `<div class="views" role="radiogroup" aria-label="View">` +
      vbtn('all', 'ALL', `All patches (${cap('viewAll')})`) +
      vbtn('favorites', 'FAVOURITES', `Favourites (${cap('viewFavorites')})`) +
      vbtn('recents', 'RECENTS', `Recently sent (${cap('viewRecents')})`) +
      `</div>` +
      `<button type="button" class="pad auto" data-act="auto" title="Auto-send on highlight (${cap('autoSend')})"><span class="face"><span class="num">AUTO SEND</span><i class="led"></i></span></button>` +
      `<button type="button" class="pad sys" data-act="sys" aria-haspopup="menu" title="System menu"><span class="face"><span class="num">SYSTEM</span></span></button>` +
      `<input class="search type-field" type="text" inputmode="search" enterkeyhint="search" autocomplete="off" autocapitalize="off" spellcheck="false" aria-label="Search patches">` +
      `<button type="button" class="searchBtn type-field" data-act="openSearch"><span class="sbt"></span></button>` +
      `<button type="button" class="dbtn def type-padLabel done" data-act="done">DONE</button>` +
      `<div class="list" tabindex="0" role="listbox" aria-label="Patches"></div>` +
      `<div class="access" hidden></div>` +
      `<div class="status"></div>` +
      `</div>`;
    this.stage = q(root, '.stage');
    this.display = new Display(q(root, '.bezel'));
    this.list = new PatchList(q(root, '.list'), h);
    this.status = new StatusBar(q(root, '.status'));
    this.access = new AccessPanel(q(root, '.access'), h);
    this.search = q<HTMLInputElement>(root, '.search');
    this.groups = q(root, '.groups');
    this.grid = q(root, '.grid');
    this.banksEl = q(root, '.banks');
    this.numsEl = q(root, '.numbers');
    this.groupBtns = [...root.querySelectorAll<HTMLButtonElement>('.groups .pad')];
    this.bankBtns = [...root.querySelectorAll<HTMLButtonElement>('.banks .pad')];
    this.numBtns = [...root.querySelectorAll<HTMLButtonElement>('.numbers .pad')];
    this.viewBtns = {
      all: q<HTMLButtonElement>(root, '[data-v=all]'),
      favorites: q<HTMLButtonElement>(root, '[data-v=favorites]'),
      recents: q<HTMLButtonElement>(root, '[data-v=recents]'),
    };
    this.autoBtn = q<HTMLButtonElement>(root, '.auto');
    this.sysBtn = q<HTMLButtonElement>(root, '.sys');
    this.prevBtn = q<HTMLButtonElement>(root, '.prev');
    this.nextBtn = q<HTMLButtonElement>(root, '.next');
    this.searchBtn = q<HTMLButtonElement>(root, '.searchBtn');
    this.searchText = q(root, '.searchBtn .sbt');
    this.chipsEl = q(root, '.chips');

    this.stage.addEventListener('click', (e) => this.onClick(e));
    this.search.addEventListener('input', () => h.query(this.search.value));
    this.groups.addEventListener('keydown', (e) => this.onGroupKey(e));
    this.grid.addEventListener('keydown', (e) => this.onGridKey(e));
    this.groups.addEventListener('focus', () => {
      this.groupCursor = Math.max(0, this.groupBtns.findIndex((b) => b.classList.contains('lit')));
      this.paintCursors();
    });
    this.grid.addEventListener('focus', () => {
      const b = this.bankBtns.findIndex((x) => x.classList.contains('lit'));
      this.gridCursor = { row: 0, col: Math.max(0, b) };
      this.paintCursors();
    });
  }

  // ---------------------------------------------------------------- layout

  applyLayout(m: Metrics): void {
    this.metrics = m;
    const s = this.stage;
    const kind = m.choice.kind;
    // On <html>, so dialogs and menus (which live in <body>) can use it too.
    document.documentElement.dataset['layout'] = kind;
    document.documentElement.dataset['density'] = m.choice.compact ? 'compact' : 'normal';
    for (const [k, v] of Object.entries(m.vars)) s.style.setProperty(`--${k}`, `${v}px`);
    s.style.setProperty('--W', `${m.W}px`);
    s.style.setProperty('--H', `${m.H}px`);
    if (kind === 'portrait') {
      const hp = m.vars['hp']!;
      s.style.setProperty('--lt', `${Math.floor((hp - 8 - 23) / 2)}px`);
    }
    this.display.setGeometry(m.display, m.dpr);
    this.list.setMetrics(kind === 'desktop' ? { rowH: 28, starTarget: 30, contextRename: false } : kind === 'portrait' ? { rowH: 52, starTarget: 52, contextRename: true } : { rowH: 44, starTarget: 44, contextRename: true });
    this.list.host.style.setProperty('--ry', `${((kind === 'desktop' ? 28 : kind === 'portrait' ? 52 : 44) - 28) / 2}px`);
    this.setPlaceholder(kind === 'desktop' ? 'Search names or addresses: brass, P3-45' : 'Search patches');
    this.paintCursors();
    this.scrollGroupIntoView(this.lastGroup);
  }

  private setPlaceholder(t: string): void {
    if (t === this.placeholder) return;
    this.placeholder = t;
    this.search.placeholder = t;
  }

  /** Phone strip: scroll the least amount that shows the group's whole cell (DESIGN-WEB 7.2). */
  scrollGroupIntoView(g: number): void {
    if (g < 0 || !this.metrics || this.metrics.choice.kind === 'desktop') return;
    const b = this.groupBtns.find((x) => x.dataset['g'] === String(g));
    const sc = this.groups;
    if (!b) return;
    const left = b.offsetLeft;
    const right = left + b.offsetWidth;
    if (left < sc.scrollLeft) sc.scrollLeft = left;
    else if (right > sc.scrollLeft + sc.clientWidth) sc.scrollLeft = right - sc.clientWidth;
  }

  // ---------------------------------------------------------------- state

  update(s: PanelState): void {
    const idle = s.entry.phase === 'idle';
    const litGroup = idle ? s.current.group : s.entry.group;
    for (const b of this.groupBtns) b.classList.toggle('lit', Number(b.dataset['g']) === litGroup);
    const litBank = idle ? s.current.bank : s.entry.phase === 'number' ? s.entry.bank : 0;
    const litNum = idle ? s.current.number : 0;
    this.bankBtns.forEach((b, i) => b.classList.toggle('lit', i + 1 === litBank));
    this.numBtns.forEach((b, i) => b.classList.toggle('lit', i + 1 === litNum));
    this.banksEl.classList.toggle('awaiting', s.entry.phase === 'bank');
    this.numsEl.classList.toggle('awaiting', s.entry.phase === 'number');
    this.stage.classList.toggle('entry', !idle);
    (Object.keys(this.viewBtns) as View[]).forEach((v) => {
      this.viewBtns[v].classList.toggle('lit', v === s.view);
      this.viewBtns[v].setAttribute('aria-checked', String(v === s.view));
    });
    this.autoBtn.classList.toggle('lit', s.autoSend);
    this.autoBtn.setAttribute('aria-pressed', String(s.autoSend));
    const off = !s.accessGranted;
    for (const b of [...this.groupBtns, ...this.bankBtns, ...this.numBtns, ...Object.values(this.viewBtns), this.autoBtn, this.prevBtn, this.nextBtn, this.searchBtn]) b.disabled = off;
    this.search.disabled = off;
    this.stage.classList.toggle('noaccess', off);
    this.stage.classList.toggle('overlay', s.overlay);
    if (this.search.value !== s.query) this.search.value = s.query;
    const st = s.query === '' ? 'Search patches' : s.query;
    if (this.searchText.textContent !== st) this.searchText.textContent = st;
    this.searchBtn.classList.toggle('kept', s.query !== '');
    if (litGroup !== this.lastGroup && idle) {
      this.lastGroup = litGroup;
      this.scrollGroupIntoView(litGroup);
    }
    this.groupCursor = Math.min(this.groupCursor, this.groupBtns.length - 1);
    this.renderChips(s);
    this.paintCursors();
  }

  private renderChips(s: PanelState): void {
    const key = JSON.stringify([s.chips.map((c) => [c.address.index, c.name]), s.current.index, s.accessGranted]);
    if (key === this.lastChips) return;
    this.lastChips = key;
    if (s.chips.length === 0) {
      this.chipsEl.innerHTML = '<div class="nochips"><span class="type-emptyTitle">NO FAVOURITES</span><span class="type-note dim">Star a patch in the list</span></div>';
      return;
    }
    this.chipsEl.innerHTML = s.chips
      .map((c) => {
        const t = formatAddress(c.address);
        const cur = c.address.index === s.current.index;
        return (
          `<button type="button" class="pad chip${cur ? ' lit' : ''}${c.name === '' ? ' noname' : ''}" data-act="chip" data-i="${c.address.index}" title="${esc(c.name || t)}" ${s.accessGranted ? '' : 'disabled'}>` +
          `<span class="face"><i class="led"></i><span class="ad">${[...t].map((ch) => `<s>${ch}</s>`).join('')}</span>${c.name === '' ? '' : `<span class="nm">${esc(c.name)}</span>`}</span></button>`
        );
      })
      .join('');
  }

  // ---------------------------------------------------------------- events

  private onClick(e: MouseEvent): void {
    const t = (e.target as Element).closest<HTMLElement>('[data-act]');
    if (!t || (t instanceof HTMLButtonElement && t.disabled)) return;
    const h = this.h;
    switch (t.dataset['act']) {
      case 'group': {
        const g = Number(t.dataset['g']);
        this.groupCursor = this.groupBtns.indexOf(t as HTMLButtonElement);
        h.group(g);
        break;
      }
      case 'bank':
        this.gridCursor = { row: 0, col: Number(t.dataset['n']) - 1 };
        h.bank(Number(t.dataset['n']));
        break;
      case 'number':
        this.gridCursor = { row: 1, col: Number(t.dataset['n']) - 1 };
        h.number(Number(t.dataset['n']));
        break;
      case 'prev':
        h.step(-1);
        break;
      case 'next':
        h.step(1);
        break;
      case 'view':
        h.view(t.dataset['v'] as View);
        break;
      case 'auto':
        h.autoSend();
        break;
      case 'sys':
        h.system(this.sysBtn);
        break;
      case 'chip': {
        const idx = Number(t.dataset['i']);
        h.chip({ index: idx, group: idx >> 6, bank: ((idx >> 3) & 7) + 1, number: (idx & 7) + 1 });
        break;
      }
      case 'openSearch':
        h.openSearch();
        break;
      case 'done':
        h.closeSearch();
        break;
    }
  }

  private paintCursors(): void {
    this.groupBtns.forEach((b, i) => b.classList.toggle('cursor', i === this.groupCursor));
    this.bankBtns.forEach((b, i) => b.classList.toggle('cursor', this.gridCursor.row === 0 && i === this.gridCursor.col));
    this.numBtns.forEach((b, i) => b.classList.toggle('cursor', this.gridCursor.row === 1 && i === this.gridCursor.col));
  }

  private groupPos(i: number): { r: number; c: number } {
    if (this.metrics?.choice.kind === 'desktop') {
      const g = Number(this.groupBtns[i]!.dataset['g']);
      return { r: g < 8 ? 0 : 1, c: g < 8 ? g : g - 8 };
    }
    return { r: 0, c: i };
  }

  private onGroupKey(e: KeyboardEvent): void {
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.groupBtns[this.groupCursor]?.click();
      return;
    }
    const d = { ArrowLeft: [0, -1], ArrowRight: [0, 1], ArrowUp: [-1, 0], ArrowDown: [1, 0] }[e.key as 'ArrowLeft'];
    if (!d) return;
    e.preventDefault();
    const cur = this.groupPos(this.groupCursor);
    const all = this.groupBtns.map((_, i) => ({ i, ...this.groupPos(i) }));
    let best: { i: number; r: number; c: number } | undefined;
    if (d[0] === 0) best = all.find((p) => p.r === cur.r && p.c === cur.c + d[1]!);
    else {
      const rowItems = all.filter((p) => p.r === cur.r + d[0]!);
      best = rowItems.sort((a, b) => Math.abs(a.c - cur.c) - Math.abs(b.c - cur.c))[0];
    }
    if (best) {
      this.groupCursor = best.i;
      this.paintCursors();
      this.scrollGroupIntoView(Number(this.groupBtns[best.i]!.dataset['g']));
    }
  }

  private onGridKey(e: KeyboardEvent): void {
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    const cur = this.gridCursor;
    if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      (cur.row === 0 ? this.bankBtns : this.numBtns)[cur.col]?.click();
      return;
    }
    if (e.key === 'ArrowLeft') cur.col = Math.max(0, cur.col - 1);
    else if (e.key === 'ArrowRight') cur.col = Math.min(7, cur.col + 1);
    else if (e.key === 'ArrowUp') cur.row = 0;
    else if (e.key === 'ArrowDown') cur.row = 1;
    else return;
    e.preventDefault();
    this.paintCursors();
  }

  /** A digit key moves the grid cursor to that button (DESIGN.md 6.4). */
  setGridCursor(row: 0 | 1, digit: number): void {
    this.gridCursor = { row, col: digit - 1 };
    this.paintCursors();
  }

  /** True when the element is part of the group strip or the pad grid. */
  inEntryArea(t: EventTarget | null): boolean {
    return t instanceof Node && (this.groups.contains(t) || this.grid.contains(t));
  }
}
