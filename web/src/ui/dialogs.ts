// In-page dialogs (docs/design/DESIGN-WEB.md 11): <dialog> with a title strip on desktop and
// landscape, full-viewport sheets on phones. Settings, Shortcuts, About, confirmations.
import { SUPPORT_MATRIX } from '../logic/access';
import { sheetSections } from '../logic/keymap';
import type { MidiPort, PortInfo } from '../midi/port';
import { findPort, type Settings } from '../logic/settings';
import { samePort } from '../midi/port';
import { el, esc } from './dom';

export interface DialogOptions {
  title: string;
  width: number;
  /** Phone sheet: full viewport, history entry, button bar. */
  sheet: boolean;
  cls?: string;
}

export interface OpenDialog {
  dlg: HTMLDialogElement;
  content: HTMLElement;
  /** Buttons live here: below the content in a sheet, at the end of the content otherwise. */
  bar: HTMLElement;
  /** Appends a node to the content, above the buttons. */
  add(node: Node): void;
  close(): void;
}

export const modalOpen = (): boolean => document.querySelector('dialog[open]') !== null;

/** Builds, shows and (on close) removes a modal dialog. `onClose` runs once after it closes. */
export function openDialog(host: HTMLElement, o: DialogOptions, onClose?: () => void): OpenDialog {
  const dlg = el('dialog', `dlg${o.sheet ? ' sheet' : ''}${o.cls ? ` ${o.cls}` : ''}`);
  if (!o.sheet) dlg.style.width = `${o.width}px`;
  dlg.setAttribute('aria-label', o.title);
  const strip = el('div', 'dt');
  strip.appendChild(el('span', 'dtt type-dialogTitle', o.title));
  const closeBtn = el('button', `dclose ${o.sheet ? 'type-padLabel' : 'type-panelLabel'}`, 'CLOSE');
  closeBtn.type = 'button';
  closeBtn.tabIndex = -1;
  strip.appendChild(closeBtn);
  const content = el('div', 'dc');
  const bar = el('div', 'dbar');
  dlg.append(strip, content);
  if (o.sheet) dlg.appendChild(bar);
  else content.appendChild(bar);
  host.appendChild(dlg);
  let pushed = false;
  let closing = false;
  const finish = (): void => {
    if (closing) return;
    closing = true;
    window.removeEventListener('popstate', onPop);
    if (pushed) {
      pushed = false;
      history.back();
    }
    dlg.remove();
    onClose?.();
  };
  const onPop = (): void => {
    pushed = false;
    if (dlg.open) dlg.close();
  };
  dlg.addEventListener('close', finish);
  // Closing is synchronous (the `close` event comes a task later): callers may open the next dialog at once.
  const shut = (): void => {
    if (dlg.open) dlg.close();
    finish();
  };
  closeBtn.addEventListener('click', shut);
  dlg.addEventListener('click', (e) => {
    if (e.target === dlg && !o.sheet) shut();
  });
  dlg.tabIndex = -1;
  dlg.showModal();
  if (document.activeElement === closeBtn) dlg.focus(); // the CLOSE strip is not a tab stop (DESIGN-WEB 11.1)
  if (o.sheet) {
    history.pushState({ d05: 'dialog' }, '');
    pushed = true;
    window.addEventListener('popstate', onPop);
  }
  return { dlg, content, bar, add: (n) => void content.insertBefore(n, bar.parentNode === content ? bar : null), close: shut };
}

const button = (text: string, cls = ''): HTMLButtonElement => {
  const b = el('button', `dbtn ${cls}`.trim(), text);
  b.type = 'button';
  return b;
};

const rule = (title: string): HTMLElement => {
  const r = el('div', 'rule secondary');
  r.innerHTML = `<i></i><b class="type-panelLabel">${esc(title)}</b><i></i>`;
  return r;
};

// ------------------------------------------------------------------ confirmations

export interface ConfirmOptions {
  title: string;
  text: string;
  /** Confirm button text, drawn in red (the object role). Empty: a notice with OK only. */
  confirm: string;
  sheet: boolean;
  /** Text in labelRed (a refusal). */
  red?: boolean;
}

/** A 360 px confirmation (DESIGN.md 10.5). CANCEL is the default and has the focus. */
export function confirmDialog(host: HTMLElement, o: ConfirmOptions, onConfirm: () => void): OpenDialog {
  const d = openDialog(host, { title: o.title, width: 360, sheet: false, cls: `card${o.sheet ? ' cardPhone' : ''}` });
  const p = el('p', `type-body ctext${o.red ? ' red' : ''}`, o.text);
  d.add(p);
  d.bar.classList.add('right');
  if (o.confirm) {
    const ok = button(o.confirm, 'danger');
    const cancel = button('CANCEL', 'def');
    ok.addEventListener('click', () => {
      d.close();
      onConfirm();
    });
    cancel.addEventListener('click', () => d.close());
    d.bar.append(ok, cancel);
    if (o.sheet) d.dlg.focus();
    else cancel.focus();
  } else {
    const ok = button('OK', 'def');
    ok.addEventListener('click', () => d.close());
    d.bar.appendChild(ok);
    if (o.sheet) d.dlg.focus();
    else ok.focus();
  }
  return d;
}

// ------------------------------------------------------------------ About

export function aboutDialog(host: HTMLElement, version: string, sheet: boolean): OpenDialog {
  const d = openDialog(host, { title: 'ABOUT', width: 440, sheet, cls: 'about' });
  const wm = el('div', 'wordmark');
  wm.innerHTML = '<span class="type-wordmark">PATCH SWITCHER</span><span class="wmr"></span><span class="type-wordmarkSub">FOR ROLAND D-05</span>';
  d.add(wm);
  const para = (cls: string, html: string): void => {
    const p = el('p', cls);
    p.innerHTML = html;
    d.add(p);
  };
  para('type-body', `Version ${esc(version)}`);
  para('type-body', 'Selects patches on a Roland Boutique D-05 over MIDI, in the browser.');
  para('type-note', '<a href="https://github.com/MarcAstr0/d05-patch-switcher" target="_blank" rel="noopener noreferrer">github.com/MarcAstr0/d05-patch-switcher</a>');
  para('type-note', 'MIT licence. Copyright 2026 Mario Castro Squella.');
  para('type-note', 'Roland, Boutique, D-05 and D-50 are trademarks of Roland Corporation. This app is not affiliated with or endorsed by Roland.');
  para('type-note', 'Inter typeface, SIL Open Font License 1.1.');
  const br = rule('BROWSERS');
  br.classList.add('mt16');
  d.add(br);
  const t = el('table', 'matrix type-body');
  for (const [b, a] of SUPPORT_MATRIX) {
    const tr = el('tr');
    tr.append(el('td', 'mb', b), el('td', 'ma', a));
    t.appendChild(tr);
  }
  d.add(t);
  para('type-note dim', 'Runs offline once loaded. Nothing leaves this page except MIDI.');
  const ok = button('OK', 'def');
  ok.addEventListener('click', () => d.close());
  d.bar.classList.add('right');
  d.bar.appendChild(ok);
  if (sheet) d.dlg.focus();
  else ok.focus();
  return d;
}

// ------------------------------------------------------------------ Shortcuts

export function shortcutsDialog(host: HTMLElement, mac: boolean): OpenDialog {
  const d = openDialog(host, { title: 'KEYBOARD SHORTCUTS', width: 680, sheet: false, cls: 'shortcuts' });
  const cols = [el('div', 'scol'), el('div', 'scol')];
  for (const s of sheetSections(mac)) {
    const sec = el('div', 'ssec');
    sec.appendChild(rule(s.title));
    for (const r of s.rows) {
      const row = el('div', 'srow');
      const keys = el('div', 'skeys');
      for (const p of r.parts) keys.appendChild(el('span', p.kind === 'key' ? 'keycap' : 'kw type-note', p.text));
      row.append(keys, el('div', 'sact type-body', r.text));
      sec.appendChild(row);
    }
    cols[s.column]!.appendChild(sec);
  }
  const wrap = el('div', 'scols');
  wrap.append(...cols);
  d.add(wrap);
  d.add(el('div', 'sfoot type-note dim', 'Esc or F1 closes'));
  d.dlg.addEventListener('keydown', (e) => {
    if (e.key === 'F1') {
      e.preventDefault();
      d.close();
    }
  });
  return d;
}

// ------------------------------------------------------------------ Settings

export interface SettingsOptions {
  port: MidiPort;
  settings: Settings;
  sheet: boolean;
  /** Storage problem to show at the bottom (blocked or failing). */
  storageNote: string | null;
  /** TEST: apply the pending port and channel to the live link and send the current patch once. */
  onTest(p: { output: PortInfo | null; channel: number }): void;
  onOk(next: Settings): void;
  /** Cancel, Esc, CLOSE or the back gesture: the controller restores whatever TEST applied. */
  onCancel(): void;
  /** After the dialog is gone, whichever way it closed. */
  onClosed(): void;
}

export function settingsDialog(host: HTMLElement, o: SettingsOptions): OpenDialog {
  let ok = false;
  const d = openDialog(host, { title: 'SETTINGS', width: 520, sheet: o.sheet, cls: 'settings' }, () => {
    if (!ok) o.onCancel();
    o.onClosed();
  });
  const pend: Settings = { ...o.settings };
  if (!pend.output && o.port.outputs().length > 0) pend.output = o.port.outputs()[0]!;
  const add = (n: HTMLElement): void => d.add(n);
  const row = (label: string, ctl: HTMLElement, extra?: HTMLElement): HTMLLabelElement => {
    const r = el('div', 'frow');
    const l = el('label', 'fl type-panelLabel', label);
    r.append(l, ctl);
    if (extra) r.appendChild(extra);
    add(r);
    return l;
  };
  const combo = (sel: HTMLSelectElement, cls = ''): HTMLElement => {
    const w = el('span', `combo ${cls}`.trim());
    w.appendChild(sel);
    return w;
  };
  const check = (label: string, checked: boolean): { row: HTMLElement; box: HTMLInputElement } => {
    const r = el('label', 'chk type-body');
    const box = el('input');
    box.type = 'checkbox';
    box.checked = checked;
    r.append(box, document.createTextNode(label));
    add(r);
    return { row: r, box };
  };

  add(rule('MIDI OUTPUT'));
  const outSel = el('select');
  outSel.id = 's-out';
  const outLabel = row('PORT', combo(outSel, 'wide'));
  outLabel.htmlFor = 's-out';
  const chSel = el('select');
  chSel.id = 's-ch';
  for (let i = 1; i <= 16; i++) chSel.appendChild(Object.assign(el('option'), { value: String(i), textContent: String(i) }));
  chSel.value = String(pend.channel);
  row('CHANNEL', combo(chSel, 'chan')).setAttribute('for', 's-ch');
  const conn = el('div', 'conn');
  conn.innerHTML = '<i class="led"></i><span class="type-body"></span>';
  add(conn);
  add(rule('MIDI INPUT'));
  const inSel = el('select');
  inSel.id = 's-in';
  const inProb = el('span', 'inprob type-body');
  row('PORT', combo(inSel), inProb).setAttribute('for', 's-in');
  const follow = check('Follow the panel', pend.follow);
  add(rule('BEHAVIOUR'));
  const auto = check('Auto-send on highlight', pend.autoSend);
  const resend = check('Resend the current patch on connect', pend.resendOnConnect);
  add(rule('ON THE D-05'));
  add(el('p', 'type-note note', 'The D-05 changes patch only when SYSTEM > MIDI > Prog.C is ON. If your unit also has a Bank.S switch, turn it ON too.'));
  if (o.storageNote) add(el('p', 'type-note note red', o.storageNote));

  // Options carry the list index (or `missing`), so two ports with the same name stay distinct.
  const fill = (sel: HTMLSelectElement, list: readonly PortInfo[], want: PortInfo | null, none: boolean): void => {
    const sig = JSON.stringify([none, list.map((p) => [p.name, p.manufacturer ?? '']), want ? [want.name, want.manufacturer ?? ''] : null]);
    if (sel.dataset['sig'] === sig) return;
    sel.dataset['sig'] = sig;
    sel.textContent = '';
    if (none) sel.appendChild(Object.assign(el('option'), { value: '', textContent: 'None' }));
    list.forEach((p, i) => {
      const dup = list.some((q, j) => j !== i && q.name === p.name);
      sel.appendChild(Object.assign(el('option'), { value: String(i), textContent: dup && p.manufacturer ? `${p.name} (${p.manufacturer})` : p.name }));
    });
    const found = findPort(list, want);
    if (want && !found) sel.appendChild(Object.assign(el('option'), { value: 'missing', textContent: `${want.name} (not found)` }));
    sel.value = found ? String(list.indexOf(found)) : want ? 'missing' : '';
  };
  const pick = (sel: HTMLSelectElement, list: readonly PortInfo[], prev: PortInfo | null): PortInfo | null => {
    if (sel.value === '') return null;
    if (sel.value === 'missing') return prev;
    return list[Number(sel.value)] ?? null;
  };
  const refresh = (): void => {
    const outs = o.port.outputs();
    const ins = o.port.inputs();
    fill(outSel, outs, pend.output, false);
    fill(inSel, ins, pend.input, true);
    const connText = conn.querySelector<HTMLElement>('span')!;
    const found = findPort(outs, pend.output) !== undefined;
    const err = found && o.port.output.state === 'error' && pend.output && samePort(pend.output, o.settings.output) ? o.port.output.text : undefined;
    conn.dataset['state'] = found && !err ? 'ok' : 'error';
    connText.textContent = err ? `Error: ${err}` : found ? `Connected to ${pend.output!.name}` : 'Port not found';
    conn.querySelector('.led')!.classList.toggle('lit', found && !err);
    const inFound = findPort(ins, pend.input) !== undefined;
    inProb.textContent = pend.input && !inFound ? 'Port not found' : pend.input && o.port.input.state === 'error' && o.port.input.text ? `Error: ${o.port.input.text}` : '';
    inProb.title = inProb.textContent;
    follow.box.disabled = pend.input === null;
    follow.row.classList.toggle('disabled', pend.input === null);
    test.disabled = !found;
  };
  outSel.addEventListener('change', () => {
    pend.output = pick(outSel, o.port.outputs(), pend.output);
    refresh();
  });
  inSel.addEventListener('change', () => {
    pend.input = pick(inSel, o.port.inputs(), pend.input);
    refresh();
  });
  chSel.addEventListener('change', () => {
    pend.channel = Number(chSel.value);
  });

  const test = button('TEST');
  const cancel = button('CANCEL');
  const okb = button('OK', 'def');
  const read = (): Settings => ({ ...pend, channel: Number(chSel.value), follow: follow.box.checked && pend.input !== null, autoSend: auto.box.checked, resendOnConnect: resend.box.checked });
  test.addEventListener('click', () => o.onTest({ output: pend.output, channel: Number(chSel.value) }));
  cancel.addEventListener('click', () => d.close());
  okb.addEventListener('click', () => {
    ok = true;
    const next = read();
    o.onOk(next);
    d.close();
  });
  d.bar.classList.add('split');
  d.bar.append(test, cancel, okb);
  const off = o.port.onChange(refresh);
  d.dlg.addEventListener('close', off);
  refresh();
  if (o.sheet) d.dlg.focus();
  else outSel.focus();
  return d;
}
