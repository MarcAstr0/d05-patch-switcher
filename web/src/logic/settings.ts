// App settings in localStorage (ports by name, never by id). Falls back to memory when storage is blocked.
import type { PortInfo } from '../midi/port';

/** The port in `list` that is `want`: name and manufacturer first, then the name alone. */
export function findPort<T extends PortInfo>(list: readonly T[], want: PortInfo | null): T | undefined {
  if (!want) return undefined;
  const exact = list.find((p) => p.name === want.name && p.manufacturer === want.manufacturer);
  if (exact) return exact;
  // Name alone matches only when one side does not know its manufacturer; a same-named port from
  // another maker is a different device.
  return list.find((p) => p.name === want.name && (!want.manufacturer || !p.manufacturer));
}

export interface Settings {
  output: PortInfo | null;
  input: PortInfo | null;
  channel: number;
  autoSend: boolean;
  follow: boolean;
  resendOnConnect: boolean;
  /** Catalogue index of the last patch, or -1. */
  lastPatch: number;
}

export const defaultSettings = (): Settings => ({ output: null, input: null, channel: 1, autoSend: false, follow: false, resendOnConnect: false, lastPatch: -1 });

export const SETTINGS_KEY = 'd05.settings';

const port = (v: unknown): PortInfo | null => {
  if (typeof v !== 'object' || v === null) return null;
  const o = v as { name?: unknown; manufacturer?: unknown };
  if (typeof o.name !== 'string' || o.name === '') return null;
  return typeof o.manufacturer === 'string' && o.manufacturer !== '' ? { name: o.name, manufacturer: o.manufacturer } : { name: o.name };
};

export function parseSettings(text: string | null): Settings {
  const s = defaultSettings();
  if (!text) return s;
  let o: Record<string, unknown>;
  try {
    const v: unknown = JSON.parse(text);
    if (typeof v !== 'object' || v === null || Array.isArray(v)) return s;
    o = v as Record<string, unknown>;
  } catch {
    return s;
  }
  s.output = port(o['output']);
  s.input = port(o['input']);
  const ch = o['channel'];
  if (typeof ch === 'number' && Number.isInteger(ch) && ch >= 1 && ch <= 16) s.channel = ch;
  s.autoSend = o['autoSend'] === true;
  s.follow = o['follow'] === true;
  s.resendOnConnect = o['resendOnConnect'] === true;
  const lp = o['lastPatch'];
  if (typeof lp === 'number' && Number.isInteger(lp) && lp >= 0 && lp < 896) s.lastPatch = lp;
  return s;
}

export class SettingsStore {
  value: Settings;
  /** False when storage is blocked: settings last until the page closes. */
  persistent = true;

  constructor(private readonly store: Pick<Storage, 'getItem' | 'setItem'> | null) {
    let text: string | null = null;
    try {
      text = store ? store.getItem(SETTINGS_KEY) : null;
      if (!store) this.persistent = false;
    } catch {
      this.persistent = false;
    }
    this.value = parseSettings(text);
  }

  /** Re-reads storage (another tab changed it). */
  reload(): void {
    try {
      this.value = parseSettings(this.store ? this.store.getItem(SETTINGS_KEY) : null);
    } catch {
      /* keep what we have */
    }
  }

  /** Writes only `lastPatch`, merged into what is stored now, so a stale tab cannot overwrite newer settings. */
  saveLastPatch(index: number): void {
    this.value.lastPatch = index;
    try {
      if (!this.store) return;
      const stored = parseSettings(this.store.getItem(SETTINGS_KEY));
      stored.lastPatch = index;
      this.store.setItem(SETTINGS_KEY, JSON.stringify(stored));
    } catch {
      this.persistent = false;
    }
  }

  save(): void {
    try {
      this.store?.setItem(SETTINGS_KEY, JSON.stringify(this.value));
    } catch {
      this.persistent = false;
    }
  }
}

/** localStorage, or null when merely touching it throws. */
export function localStore(win: Window): Storage | null {
  try {
    return win.localStorage;
  } catch {
    return null;
  }
}
