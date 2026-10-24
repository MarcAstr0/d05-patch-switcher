// The status bar (docs/design/DESIGN.md 8.1, DESIGN-WEB 7.7) and the MIDI access panel (DESIGN-WEB 6).
import { PAGE_URL, SUPPORT_MATRIX, type AccessContent } from '../logic/access';
import { el, q } from './dom';

let measureCtx: CanvasRenderingContext2D | null | undefined;

/** Middle ellipsis to fit `maxPx` at the status value font. */
export function elideMiddle(text: string, maxPx: number, font = '400 11px Inter, "Helvetica Neue", Arial, sans-serif'): string {
  if (measureCtx === undefined) measureCtx = document.createElement('canvas').getContext('2d');
  const c = measureCtx;
  if (!c) return text;
  c.font = font;
  if (c.measureText(text).width <= maxPx) return text;
  let k = Math.floor(text.length / 2);
  while (k > 1) {
    const t = `${text.slice(0, k)}…${text.slice(text.length - k)}`;
    if (c.measureText(t).width <= maxPx) return t;
    k--;
  }
  return '…';
}

export interface StatusModel {
  activity: boolean;
  /** The saved output port name; null when none was ever chosen. */
  outName: string | null;
  outOpen: boolean;
  channel: number;
  inName: string | null;
  /** Follow is on, an input is chosen: show FOLLOW or its NOT FOUND. */
  wantsInput: boolean;
  inputOpen: boolean;
  message: { text: string; error: boolean } | null;
  /** Storage is blocked or a save failed (short marker for the phone layout). */
  notSaved: string | null;
  /** The panel is waiting for MIDI access: `OUT None`, no NOT FOUND. */
  accessGranted: boolean;
}

export class StatusBar {
  private readonly act: HTMLElement;
  private readonly out: HTMLElement;
  private readonly nf: HTMLElement;
  private readonly ch: HTMLElement;
  private readonly inn: HTMLElement;
  private readonly inf: HTMLElement;
  private readonly fol: HTMLElement;
  private readonly msg: HTMLElement;
  private readonly ns: HTMLElement;
  private last = '';

  constructor(readonly root: HTMLElement) {
    root.innerHTML =
      '<i class="led act"></i><span class="sl">OUT</span><span class="sv out"></span><span class="sl se nf">NOT FOUND</span>' +
      '<span class="sl gap">CH</span><span class="sv ch"></span>' +
      '<span class="sl gap inl">IN</span><span class="sv inn"></span><span class="sl se inf">NOT FOUND</span><span class="sl gap fol">FOLLOW</span>' +
      '<span class="sl se ns">NOT SAVED</span><span class="msg"></span>';
    this.act = q(root, '.act');
    this.out = q(root, '.out');
    this.nf = q(root, '.nf');
    this.ch = q(root, '.ch');
    this.inn = q(root, '.inn');
    this.inf = q(root, '.inf');
    this.fol = q(root, '.fol');
    this.msg = q(root, '.msg');
    this.ns = q(root, '.ns');
  }

  update(m: StatusModel, wide: boolean): void {
    const key = JSON.stringify([m, wide]);
    if (key === this.last) return;
    this.last = key;
    this.act.classList.toggle('lit', m.activity);
    const outName = m.accessGranted ? m.outName : null;
    this.out.textContent = outName === null ? 'None' : elideMiddle(outName, 280);
    this.out.title = outName ?? '';
    this.out.classList.toggle('dimv', outName === null);
    this.nf.hidden = !(m.accessGranted && outName !== null && !m.outOpen);
    this.ch.textContent = String(m.channel);
    this.inn.textContent = m.inName === null ? 'None' : elideMiddle(m.inName, 280);
    this.inn.title = m.inName ?? '';
    this.inn.classList.toggle('dimv', m.inName === null);
    this.inf.hidden = !(m.wantsInput && !m.inputOpen);
    this.fol.hidden = !(m.wantsInput && m.inputOpen);
    this.root.dataset['wide'] = wide ? '1' : '0';
    this.ns.hidden = m.notSaved === null;
    this.ns.title = m.notSaved ?? '';
    this.msg.textContent = m.message?.text ?? '';
    this.msg.classList.toggle('err', !!m.message?.error);
    this.msg.title = m.message?.text ?? '';
  }
}

export interface AccessHandlers {
  connect(): void;
}

/** The access panel: one block of title, body, steps, support matrix, button and note. */
export class AccessPanel {
  private button: HTMLButtonElement | null = null;
  private last = '';

  constructor(readonly root: HTMLElement, private readonly h: AccessHandlers) {}

  update(c: AccessContent | null): void {
    const key = JSON.stringify(c);
    if (key === this.last) return;
    const hadFocus = this.button !== null && document.activeElement === this.button;
    this.last = key;
    this.root.hidden = c === null;
    this.root.textContent = '';
    this.button = null;
    if (!c) return;
    const block = el('div', 'ab');
    block.appendChild(el('h2', `type-emptyTitle title${c.red ? ' red' : ''}`, c.title));
    for (const text of c.body) {
      const p = el('p', 'type-body body');
      const i = text.indexOf(PAGE_URL);
      if (i < 0) p.textContent = text;
      else {
        // The page address never breaks across lines (DESIGN-WEB 6.4).
        p.append(text.slice(0, i), Object.assign(el('span', 'nw'), { textContent: PAGE_URL }), text.slice(i + PAGE_URL.length));
      }
      block.appendChild(p);
    }
    if (c.steps.length > 0) {
      const ol = el('ol', 'steps');
      c.steps.forEach((s) => ol.appendChild(el('li', 'type-body', s)));
      block.appendChild(ol);
    }
    if (c.matrix) {
      const t = el('table', 'matrix type-body');
      for (const [b, a] of SUPPORT_MATRIX) {
        const tr = el('tr');
        tr.append(el('td', 'mb', b), el('td', 'ma', a));
        t.appendChild(tr);
      }
      block.appendChild(t);
    }
    if (c.button) {
      const b = el('button', `dbtn def type-padLabel${c.button === 'TRY AGAIN' ? ' try' : ''}`, c.button);
      b.type = 'button';
      b.disabled = c.button === 'WAITING';
      b.addEventListener('click', () => this.h.connect());
      block.appendChild(b);
      this.button = b;
    }
    if (c.note) block.appendChild(el('p', 'type-note dim note', c.note));
    this.root.appendChild(block);
    if (hadFocus && this.button && !this.button.disabled) this.button.focus();
  }

  focusButton(): void {
    this.button?.focus();
  }
}
