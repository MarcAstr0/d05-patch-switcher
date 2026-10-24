export const APP_NAME = 'D-05 Patch Switcher';

export function start(root: HTMLElement): void {
  root.textContent = APP_NAME;
}

if (typeof document !== 'undefined') {
  const root = document.getElementById('app');
  if (root) start(root);
}
