import './ui/theme.css';
import './ui/app.css';
import { Controller } from './controller';
import { createPort } from './midi';
import { watchUpdates } from './update';

export const APP_NAME = 'D-05 Patch Switcher';

export function start(root: HTMLElement): Controller {
  const c = new Controller(root, window, createPort(window));
  if (import.meta.env.PROD && 'serviceWorker' in navigator) {
    watchUpdates(navigator.serviceWorker, () => c.notifyUpdated());
    // The service worker caches the app shell so the page loads offline (docs/WEB.md 3).
    window.addEventListener('load', () => {
      navigator.serviceWorker.register(`${import.meta.env.BASE_URL}sw.js`, { scope: import.meta.env.BASE_URL }).catch(() => undefined);
    });
  }
  return c;
}

if (typeof document !== 'undefined') {
  const root = document.getElementById('app');
  if (root) start(root);
}
