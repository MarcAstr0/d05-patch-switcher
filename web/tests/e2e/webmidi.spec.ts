// The real Web MIDI layer against a page-injected mock of navigator.requestMIDIAccess (no ?fake).
import { expect, test, type Page } from '@playwright/test';
import { BASE } from './helpers';

type Mode = 'grant' | 'deny' | 'unsupported';

async function inject(page: Page, mode: Mode, permission: 'prompt' | 'granted' | 'denied' | 'throw' = 'prompt'): Promise<void> {
  await page.addInitScript(
    ([m, perm]) => {
      localStorage.setItem('d05.settings', JSON.stringify({ output: { name: 'Mock Out', manufacturer: 'Mock' }, channel: 3 }));
      const sent: number[][] = [];
      const w = window as unknown as Record<string, unknown>;
      w['__sent'] = sent;
      const nav = navigator as unknown as Record<string, unknown>;
      if (m === 'unsupported') {
        Object.defineProperty(navigator, 'requestMIDIAccess', { value: undefined, configurable: true });
        return;
      }
      const out = { id: 'x1', name: 'Mock Out', manufacturer: 'Mock', state: 'connected', open: () => Promise.resolve(out), send: (d: number[]) => void sent.push([...d]) };
      const access = { outputs: new Map([['x1', out]]), inputs: new Map(), onstatechange: null as null | (() => void) };
      w['__access'] = access;
      nav['requestMIDIAccess'] = (o: unknown) => {
        w['__req'] = o;
        return m === 'grant' ? Promise.resolve(access) : Promise.reject(new DOMException('no', 'SecurityError'));
      };
      Object.defineProperty(navigator, 'permissions', {
        configurable: true,
        value: { query: () => (perm === 'throw' ? Promise.reject(new TypeError('x')) : Promise.resolve({ state: perm, onchange: null })) },
      });
    },
    [mode, permission] as const,
  );
  await page.goto(BASE);
  await expect(page.locator('.stage')).toBeVisible();
}

test('prompt -> CONNECT MIDI -> granted, then the saved port sends on channel 3', async ({ page }) => {
  await inject(page, 'grant');
  await expect(page.locator('.access .title')).toHaveText('MIDI IS NOT CONNECTED');
  await page.getByRole('button', { name: 'CONNECT MIDI' }).click();
  await expect(page.locator('.access')).toBeHidden();
  expect(await page.evaluate(() => (window as unknown as { __req: unknown }).__req)).toEqual({ sysex: false });
  await page.locator('.list').focus();
  await page.keyboard.press('Home');
  await page.keyboard.press('Enter');
  await expect.poll(() => page.evaluate(() => (window as unknown as { __sent: number[][] }).__sent.slice(-3))).toEqual([[0xb2, 0, 87], [0xb2, 32, 0], [0xc2, 0]]);
});

test('already granted needs no click', async ({ page }) => {
  await inject(page, 'grant', 'granted');
  await expect(page.locator('.access')).toBeHidden();
});

test('denied is a normal state with TRY AGAIN', async ({ page }) => {
  await inject(page, 'deny');
  await page.getByRole('button', { name: 'CONNECT MIDI' }).click();
  await expect(page.locator('.access .title')).toHaveText('MIDI ACCESS IS BLOCKED');
  await expect(page.getByRole('button', { name: 'TRY AGAIN' })).toBeVisible();
});

test('stored denied shows blocked at once', async ({ page }) => {
  await inject(page, 'deny', 'denied');
  await expect(page.locator('.access .title')).toHaveText('MIDI ACCESS IS BLOCKED');
});

test('unsupported browsers see the matrix and no button', async ({ page }) => {
  await inject(page, 'unsupported');
  await expect(page.locator('.matrix tr')).toHaveCount(4);
  await expect(page.getByRole('button', { name: /CONNECT|TRY/ })).toHaveCount(0);
});

test('unplugging the saved output shows NO MIDI OUTPUT', async ({ page }) => {
  await inject(page, 'grant', 'granted');
  await expect(page.locator('.access')).toBeHidden();
  await page.evaluate(() => {
    const a = (window as unknown as { __access: { outputs: Map<string, unknown>; onstatechange: () => void } }).__access;
    a.outputs.clear();
    a.onstatechange();
  });
  await expect.poll(() => page.evaluate(() => document.querySelector<HTMLElement>('.bezel')!.dataset['line2'] ?? '')).toContain('NO MIDI OUTPUT');
});
