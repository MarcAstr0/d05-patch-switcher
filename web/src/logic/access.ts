// The MIDI access panel texts (docs/design/DESIGN-WEB.md 6.3, 6.4). Pure; detection is passed in.
import type { AccessState } from '../midi/port';

export interface Env {
  userAgent: string;
  /** navigator.userAgentData.brands names, when present. */
  brands: string[];
  mobile: boolean;
  /** navigator.maxTouchPoints */
  maxTouchPoints: number;
  brave: boolean;
  /** The Permissions API can query `midi`, so "the page connects by itself" is true. */
  permissionChange: boolean;
}

export type BrowserKind = 'chrome' | 'edge' | 'brave' | 'opera' | 'firefox' | 'safari' | 'other';

export function detectEnv(nav: Navigator & { userAgentData?: { brands?: { brand: string }[]; mobile?: boolean }; brave?: unknown }, permissionChange: boolean): Env {
  return {
    userAgent: nav.userAgent,
    brands: (nav.userAgentData?.brands ?? []).map((b) => b.brand),
    mobile: nav.userAgentData?.mobile === true,
    maxTouchPoints: nav.maxTouchPoints ?? 0,
    brave: nav.brave !== undefined,
    permissionChange,
  };
}

export function browserKind(e: Env): BrowserKind {
  const ua = e.userAgent;
  if (/Firefox\//.test(ua)) return 'firefox';
  if (e.brave) return 'brave';
  if (e.brands.includes('Microsoft Edge') || /Edg\//.test(ua)) return 'edge';
  if (e.brands.includes('Opera') || /OPR\//.test(ua)) return 'opera';
  if (/Chrome\/|Chromium\/|CriOS\//.test(ua)) return 'chrome';
  if (/Safari\//.test(ua)) return 'safari';
  return 'other';
}

const isAndroid = (e: Env): boolean => e.mobile || /Android/.test(e.userAgent);
const isIos = (e: Env): boolean => /iPhone|iPad/.test(e.userAgent) || (/Macintosh/.test(e.userAgent) && e.maxTouchPoints > 1);

export interface AccessContent {
  title: string;
  /** Red title (denied). */
  red: boolean;
  /** Paragraphs of body text. */
  body: string[];
  steps: string[];
  matrix: boolean;
  /** Button text: CONNECT MIDI, TRY AGAIN or WAITING (disabled). Null for states with no button. */
  button: 'CONNECT MIDI' | 'TRY AGAIN' | 'WAITING' | null;
  note: string;
}

export const SUPPORT_MATRIX: readonly (readonly [string, string])[] = [
  ['Chrome, Edge, Brave, Opera', 'Yes'],
  ['Chrome on Android', 'Yes, with a USB OTG cable'],
  ['Firefox 108 or later', 'Yes, after a site permission'],
  ['Safari, iPhone and iPad', 'No'],
];

export const PAGE_URL = 'https://marcastr0.github.io/d05-patch-switcher/';

const BROWSER_NAME: Record<BrowserKind, string> = { chrome: 'Chrome', edge: 'Edge', brave: 'Brave', opera: 'Opera', firefox: 'Firefox', safari: 'The browser', other: 'The browser' };

export function deniedSteps(e: Env): string[] {
  const kind = browserKind(e);
  const last = e.permissionChange ? 'Come back here: the page connects by itself.' : 'Come back here and press TRY AGAIN.';
  if (kind === 'firefox') {
    return ['Press TRY AGAIN.', 'Accept the site permission add-on when Firefox offers it.', 'If Firefox does not ask, open about:addons, choose Site Permissions, remove this site and press TRY AGAIN.'];
  }
  if ((kind === 'chrome' || kind === 'edge' || kind === 'brave' || kind === 'opera') && isAndroid(e)) {
    return ['Tap the icon left of the address.', 'Tap Permissions.', 'Allow MIDI devices.', last];
  }
  if (kind === 'chrome') return ['Click View site information, left of the address.', 'Choose Site settings.', 'Set MIDI devices to Allow.', last];
  if (kind === 'edge' || kind === 'brave' || kind === 'opera') return ['Click the site information icon, left of the address.', 'Choose Site settings.', 'Set MIDI devices to Allow.', last];
  return ["Allow MIDI for this site in the browser's site settings.", 'Come back here and press TRY AGAIN.'];
}

export function accessContent(state: AccessState, e: Env): AccessContent | null {
  const none = { red: false, body: [] as string[], steps: [] as string[], matrix: false, button: null, note: '' };
  switch (state) {
    case 'granted':
      return null;
    case 'insecure':
      return { ...none, title: 'MIDI NEEDS A SECURE PAGE', body: [`Browsers offer MIDI only to pages served over https:// or from localhost. Open ${PAGE_URL} instead.`], matrix: true };
    case 'unsupported': {
      if (isIos(e)) return { ...none, title: 'NO WEB MIDI ON IPHONE OR IPAD', body: ["Every browser on iPhone and iPad uses Safari's engine, which has no Web MIDI. Use a computer or an Android phone."], matrix: true };
      if (browserKind(e) === 'safari') return { ...none, title: 'NO WEB MIDI IN SAFARI', body: ['Safari does not support Web MIDI, so this page cannot reach the D-05. Open it in Chrome, Edge, Brave or Firefox.'], matrix: true };
      return { ...none, title: 'NO WEB MIDI IN THIS BROWSER', body: ['Open this page in Chrome, Edge, Brave, Opera, or Firefox 108 or later.'], matrix: true };
    }
    case 'denied':
      return { ...none, title: 'MIDI ACCESS IS BLOCKED', red: true, body: [`${BROWSER_NAME[browserKind(e)]} is blocking MIDI for this page. To allow it:`], steps: deniedSteps(e), button: 'TRY AGAIN' };
    case 'prompt':
    case 'waiting': {
      const body = [
        isAndroid(e)
          ? 'Plug the D-05, or the interface it is on, into this phone with a USB OTG cable. Then connect and allow MIDI when Chrome asks.'
          : 'The browser asks once for permission to use MIDI devices. Allow it so this page can change patches on the D-05.',
      ];
      if (browserKind(e) === 'firefox') body.push('Firefox installs a small site permission add-on the first time. That is expected.');
      return { ...none, title: 'MIDI IS NOT CONNECTED', body, button: state === 'waiting' ? 'WAITING' : 'CONNECT MIDI', note: 'Only Bank Select and Program Change are sent, never SysEx.' };
    }
  }
}
