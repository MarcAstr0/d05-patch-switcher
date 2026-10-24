// The SYSTEM menu (docs/design/DESIGN-WEB.md 11.6): role=menu under the button on desktop and
// landscape, opening upward at 240 px on phones. Arrows move, Enter activates, Esc or a click outside closes.
import { el, esc } from './dom';

export interface MenuItem {
  act: string;
  label: string;
  shortcut?: string;
}

export const SEPARATOR = '-';

export function openMenu(host: HTMLElement, anchor: HTMLElement, items: readonly (MenuItem | typeof SEPARATOR)[], phone: boolean, onPick: (act: string) => void, onClosed?: () => void): { close(): void } {
  const menu = el('div', `menu${phone ? ' phone' : ''}`);
  menu.setAttribute('role', 'menu');
  const buttons: HTMLButtonElement[] = [];
  for (const it of items) {
    if (it === SEPARATOR) {
      menu.appendChild(el('div', 'msep'));
      continue;
    }
    const b = el('button', 'mi type-body');
    b.type = 'button';
    b.tabIndex = -1;
    b.setAttribute('role', 'menuitem');
    b.dataset['act'] = it.act;
    b.innerHTML = `<span>${esc(it.label)}</span>${it.shortcut ? `<span class="msc">${esc(it.shortcut)}</span>` : ''}`;
    buttons.push(b);
    menu.appendChild(b);
  }
  host.appendChild(menu);
  const r = anchor.getBoundingClientRect();
  const w = phone ? Math.min(240, window.innerWidth - 8) : menu.offsetWidth;
  if (phone) menu.style.width = `${w}px`;
  menu.style.left = `${Math.max(4, Math.round(r.right - w))}px`;
  // Phones open upward (DESIGN-WEB 11.6); where the button is near the top, downward.
  if (phone && r.top - menu.offsetHeight - 4 >= 0) menu.style.bottom = `${Math.round(window.innerHeight - r.top + 4)}px`;
  else menu.style.top = `${Math.round(r.bottom + (phone ? 4 : 0))}px`;
  anchor.setAttribute('aria-expanded', 'true');

  let closed = false;
  const close = (): void => {
    if (closed) return;
    closed = true;
    document.removeEventListener('pointerdown', onDown, true);
    document.removeEventListener('keydown', onKey, true);
    menu.remove();
    anchor.removeAttribute('aria-expanded');
    anchor.focus({ preventScroll: true });
    onClosed?.();
  };
  const onDown = (e: PointerEvent): void => {
    const t = e.target as Node;
    if (!menu.contains(t) && !anchor.contains(t)) close();
  };
  const cur = (): number => buttons.indexOf(document.activeElement as HTMLButtonElement);
  const onKey = (e: KeyboardEvent): void => {
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      close();
    } else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      e.stopPropagation();
      const n = buttons.length;
      const i = cur();
      buttons[i < 0 ? (e.key === 'ArrowDown' ? 0 : n - 1) : (i + (e.key === 'ArrowDown' ? 1 : n - 1)) % n]!.focus();
    } else if (e.key === 'Tab') {
      e.preventDefault();
      close();
    } else if (e.key === 'Enter' || e.key === ' ') {
      const b = buttons[cur()];
      if (b) {
        e.preventDefault();
        e.stopPropagation();
        b.click();
      }
    } else e.stopPropagation();
  };
  menu.addEventListener('click', (e) => {
    const b = (e.target as Element).closest<HTMLElement>('.mi');
    if (!b) return;
    const act = b.dataset['act']!;
    close();
    onPick(act);
  });
  document.addEventListener('pointerdown', onDown, true);
  document.addEventListener('keydown', onKey, true);
  buttons[0]?.focus();
  return { close };
}
