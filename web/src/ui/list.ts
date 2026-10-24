// The patch list (docs/design/DESIGN.md 7, DESIGN-WEB 9.5): fixed-height rows, only the visible
// ones (plus a few) are in the DOM, positioned absolutely; rows are repainted only when their
// content changes. Hit testing is by coordinates, so rows may be rebuilt between press and release.
import type { PatchAddress } from '../core/address';
import { formatAddress, isUser } from '../core/address';
import { MAX_USER_NAME } from '../core/catalog';
import type { Row } from '../logic/views';
import { esc, el, STAR_SVG } from './dom';

export interface ListHandlers {
  /** A tap or click on a row. */
  onRowClick(row: Row, index: number): void;
  onStar(a: PatchAddress): void;
  /** Double-click or long press on a user slot. */
  onRenameRequest(a: PatchAddress): void;
  /** The inline editor committed (Enter or clicking away) or cancelled (text null). */
  onRenameDone(a: PatchAddress, text: string | null): void;
}

export interface ListModel {
  rows: readonly Row[];
  current: PatchAddress | null;
  highlight: number;
  isFavorite(a: PatchAddress): boolean;
  showCounts: boolean;
  empty: { title: string; note: string };
}

export interface ListMetrics {
  rowH: number;
  /** The row's width of the right-hand star touch target. */
  starTarget: number;
  /** Long press (contextmenu) renames; off on the desktop layout. */
  contextRename: boolean;
}

const BUFFER = 3;

export class PatchList {
  private readonly sizer = el('div', 'sizer');
  private readonly empty = el('div', 'empty');
  private readonly pool = new Map<number, HTMLElement>();
  private readonly sigs = new Map<number, string>();
  private model: ListModel = { rows: [], current: null, highlight: -1, isFavorite: () => false, showCounts: false, empty: { title: '', note: '' } };
  private m: ListMetrics = { rowH: 28, starTarget: 30, contextRename: false };
  private editing: { address: PatchAddress; input: HTMLInputElement; counter: HTMLElement | null } | null = null;
  /** Rows built and updated, for tests. */
  stats = { rowBuilds: 0 };

  constructor(readonly host: HTMLElement, private readonly h: ListHandlers) {
    host.append(this.sizer, this.empty);
    host.addEventListener('scroll', () => this.render(), { passive: true });
    if (typeof ResizeObserver === 'function') new ResizeObserver(() => this.render()).observe(host);
    host.addEventListener('click', (e) => this.onClick(e));
    host.addEventListener('dblclick', (e) => {
      if (this.isEditor(e.target)) return;
      const hit = this.hit(e);
      if (hit && isUser(hit.row.address)) this.h.onRenameRequest(hit.row.address);
    });
    host.addEventListener('contextmenu', (e) => {
      if (!this.m.contextRename) return;
      e.preventDefault();
      const hit = this.hit(e);
      if (hit && isUser(hit.row.address)) this.h.onRenameRequest(hit.row.address);
    });
  }

  setMetrics(m: ListMetrics): void {
    if (m.rowH === this.m.rowH && m.starTarget === this.m.starTarget && m.contextRename === this.m.contextRename) return;
    this.m = m;
    this.host.style.setProperty('--rowH', `${m.rowH}px`);
    this.sizer.style.height = `${this.model.rows.length * m.rowH}px`;
    this.invalidate();
    this.render();
  }

  get rowHeight(): number {
    return this.m.rowH;
  }

  update(model: ListModel): void {
    const old = this.model;
    this.model = model;
    if (old.rows !== model.rows) this.sizer.style.height = `${model.rows.length * this.m.rowH}px`;
    const e = model.rows.length === 0 ? model.empty : { title: '', note: '' };
    const sig = `${e.title}|${e.note}`;
    if (this.empty.dataset['sig'] !== sig) {
      this.empty.dataset['sig'] = sig;
      this.empty.innerHTML = e.title ? `<div class="type-emptyTitle">${esc(e.title)}</div><div class="type-note dim">${esc(e.note)}</div>` : '';
    }
    if (this.editing) {
      const i = this.indexOf(this.editing.address);
      if (i < 0) this.cancelEdit(false);
    }
    this.render();
  }

  indexOf(a: PatchAddress): number {
    return this.model.rows.findIndex((r) => r.address.index === a.index);
  }

  /** Scrolls so the row is visible; `nearTop` puts it three rows below the top (DESIGN.md 14.2). */
  scrollToRow(i: number, nearTop = false): void {
    if (i < 0 || i >= this.model.rows.length) return;
    const h = this.m.rowH;
    const top = i * h;
    const view = this.host.clientHeight;
    if (nearTop) this.host.scrollTop = Math.max(0, top - 3 * h);
    else if (top < this.host.scrollTop) this.host.scrollTop = top;
    else if (top + h > this.host.scrollTop + view) this.host.scrollTop = top + h - view;
    this.render();
  }

  scrollToTop(): void {
    this.host.scrollTop = 0;
    this.render();
  }

  /** Opens the inline editor on a user row (DESIGN.md 7.4). */
  startEdit(a: PatchAddress, name: string): void {
    const i = this.indexOf(a);
    if (i < 0 || !isUser(a)) return;
    this.cancelEdit(true);
    this.scrollToRow(i);
    const input = el('input', 'nameEditor');
    input.type = 'text';
    input.value = name;
    input.autocomplete = 'off';
    input.spellcheck = false;
    input.setAttribute('aria-label', `Name of ${formatAddress(a)}`);
    input.addEventListener('input', () => {
      const v = clean(input.value);
      if (v !== input.value) input.value = v;
      this.updateCounter();
    });
    input.addEventListener('keydown', (e) => {
      e.stopPropagation();
      if (e.key === 'Enter') {
        e.preventDefault();
        this.finishEdit(true);
      } else if (e.key === 'Escape') {
        e.preventDefault();
        this.finishEdit(false);
      }
    });
    input.addEventListener('blur', () => this.finishEdit(true));
    this.editing = { address: a, input, counter: null };
    this.invalidate();
    this.render();
    input.focus();
    input.select();
  }

  get isEditing(): boolean {
    return this.editing !== null;
  }

  /** Commits the open editor (true) or drops it. */
  finishEdit(commit: boolean): void {
    const ed = this.editing;
    if (!ed) return;
    this.editing = null;
    const text = ed.input.value;
    const hadFocus = document.activeElement === ed.input;
    ed.input.remove();
    this.invalidate();
    this.render();
    if (hadFocus) this.host.focus({ preventScroll: true });
    this.h.onRenameDone(ed.address, commit ? text : null);
  }

  private cancelEdit(silent: boolean): void {
    if (!this.editing) return;
    if (silent) this.finishEdit(true);
    else {
      const ed = this.editing;
      this.editing = null;
      ed.input.remove();
      this.invalidate();
    }
  }

  private isEditor(t: EventTarget | null): boolean {
    return t instanceof HTMLElement && t.classList.contains('nameEditor');
  }

  private invalidate(): void {
    this.sigs.clear();
  }

  private hit(e: MouseEvent): { row: Row; index: number; star: boolean } | null {
    const r = this.host.getBoundingClientRect();
    const y = e.clientY - r.top - this.host.clientTop + this.host.scrollTop;
    const i = Math.floor(y / this.m.rowH);
    const row = this.model.rows[i];
    if (!row || y < 0) return null;
    const right = r.left + this.host.clientLeft + this.host.clientWidth;
    return { row, index: i, star: e.clientX >= right - this.m.starTarget };
  }

  private onClick(e: MouseEvent): void {
    if (this.isEditor(e.target)) return;
    const hit = this.hit(e);
    if (!hit) return;
    if (hit.star) this.h.onStar(hit.row.address);
    else if (e.detail <= 1) this.h.onRowClick(hit.row, hit.index); // a double-click renames, it does not send twice
  }

  private updateCounter(): void {
    const ed = this.editing;
    if (!ed?.counter) return;
    const n = ed.input.value.length;
    ed.counter.textContent = `${n}/${MAX_USER_NAME}`;
    ed.counter.classList.toggle('full', n >= MAX_USER_NAME);
  }

  render(): void {
    const { rows } = this.model;
    const h = this.m.rowH;
    const view = this.host.clientHeight || 0;
    const first = Math.max(0, Math.floor(this.host.scrollTop / h) - BUFFER);
    const last = Math.min(rows.length - 1, Math.ceil((this.host.scrollTop + view) / h) + BUFFER);
    for (const [i, node] of this.pool) {
      if (i < first || i > last) {
        node.remove();
        this.pool.delete(i);
        this.sigs.delete(i);
      }
    }
    for (let i = first; i <= last; i++) {
      const row = rows[i]!;
      let node = this.pool.get(i);
      if (!node) {
        node = el('div', 'row');
        node.style.top = `${i * h}px`;
        this.sizer.appendChild(node);
        this.pool.set(i, node);
      }
      const edit = this.editing && this.editing.address.index === row.address.index;
      const fav = this.model.isFavorite(row.address);
      const cur = this.model.current?.index === row.address.index;
      const sig = `${row.address.index}|${row.shown}|${row.name}|${row.ranges.map((r) => `${r.start}.${r.length}`).join(',')}|${fav ? 1 : 0}${cur ? 1 : 0}${i === this.model.highlight ? 1 : 0}${edit ? 1 : 0}|${this.model.showCounts ? row.count : ''}`;
      if (this.sigs.get(i) === sig) continue;
      this.sigs.set(i, sig);
      this.stats.rowBuilds++;
      this.fill(node, row, { fav, cur, hl: i === this.model.highlight, edit: !!edit });
    }
    this.host.dataset['rows'] = String(rows.length);
    this.host.dataset['rowBuilds'] = String(this.stats.rowBuilds);
  }

  private fill(node: HTMLElement, row: Row, s: { fav: boolean; cur: boolean; hl: boolean; edit: boolean }): void {
    const a = row.address;
    const text = formatAddress(a);
    node.className = `row${s.hl ? ' hl' : ''}${s.cur ? ' cur' : ''}${s.fav ? ' fav' : ''}${s.edit ? ' ed' : ''}`;
    node.setAttribute('role', 'option');
    node.setAttribute('aria-selected', s.hl ? 'true' : 'false');
    node.dataset['address'] = text;
    let name: string;
    if (row.name === '') name = `<span class="nm dim">${esc(row.shown)}</span>`;
    else {
      let out = '';
      let pos = 0;
      for (const r of row.ranges) {
        out += esc(row.name.slice(pos, r.start)) + `<mark>${esc(row.name.slice(r.start, r.start + r.length))}</mark>`;
        pos = r.start + r.length;
      }
      name = `<span class="nm">${out + esc(row.name.slice(pos))}</span>`;
    }
    const mode = row.mode;
    node.innerHTML =
      `<div class="rin">${s.cur ? '<i class="cur"></i>' : ''}<span class="ad">${[...text].map((c) => `<s>${c}</s>`).join('')}</span>${name}` +
      `${s.edit ? '<span class="ct"></span>' : this.model.showCounts ? `<span class="ct">×${row.count}</span>` : ''}` +
      `${mode ? `<span class="tg">${mode}</span>` : ''}${STAR_SVG}</div>`;
    if (s.edit && this.editing) {
      const rin = node.firstElementChild!;
      this.editing.counter = rin.querySelector('.ct');
      rin.appendChild(this.editing.input);
      this.updateCounter();
    }
  }
}

const clean = (s: string): string => {
  let out = '';
  for (const c of s) if (c >= ' ' && c <= '~') out += c;
  return out.slice(0, MAX_USER_NAME);
};
