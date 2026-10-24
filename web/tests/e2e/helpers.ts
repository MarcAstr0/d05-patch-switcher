import { expect, type Page } from '@playwright/test';

export const BASE = '/d05-patch-switcher/';

export interface Seed {
  /** Saved settings (output defaults to the fake port, channel 1). Null: a fresh profile. */
  settings?: Record<string, unknown> | null;
  library?: unknown;
}

/** Opens the app against the fake MIDI port. Seeds storage once per browser context, so reloads keep what the app saved. */
export async function open(page: Page, query = '', seed: Seed = {}): Promise<void> {
  const settings = seed.settings === undefined ? { output: { name: 'Fake D-05 Out' }, channel: 1 } : seed.settings;
  await page.addInitScript(
    ([s, lib]) => {
      if (localStorage.getItem('__seeded')) return;
      localStorage.setItem('__seeded', '1');
      if (s) localStorage.setItem('d05.settings', JSON.stringify(s));
      if (lib) localStorage.setItem('d05.library', JSON.stringify(lib));
    },
    [settings, seed.library ?? null] as const,
  );
  await page.goto(`${BASE}?fake=1${query ? `&${query}` : ''}`);
  await expect(page.locator('.stage')).toBeVisible();
}

export const sent = (page: Page): Promise<string[]> => page.evaluate(() => (window as unknown as { __fakeMidi: { hex(): string[] } }).__fakeMidi.hex());
export const fake = <T>(page: Page, fn: string): Promise<T> => page.evaluate(`(() => { const m = window.__fakeMidi; return (${fn})(m); })()`) as Promise<T>;
export const line1 = (page: Page): Promise<string> => page.evaluate(() => document.querySelector<HTMLElement>('.bezel')!.dataset['line1'] ?? '');
export const line2 = (page: Page): Promise<string> => page.evaluate(() => document.querySelector<HTMLElement>('.bezel')!.dataset['line2'] ?? '');

export const SIZES = {
  desktop: { width: 960, height: 560 },
  portrait: { width: 390, height: 844 },
  landscape: { width: 844, height: 390 },
} as const;
