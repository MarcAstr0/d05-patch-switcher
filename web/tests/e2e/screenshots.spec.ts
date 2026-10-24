// Screenshots of every state at desktop, phone portrait and phone landscape, for the designer's review.
// Written to web/test-results/screenshots/<size>-<state>.png (gitignored).
import { readFileSync } from 'node:fs';
import { mkdirSync } from 'node:fs';
import { expect, test, type Page } from '@playwright/test';
import { SIZES, open } from './helpers';

const fixture = JSON.parse(readFileSync(new URL('../fixtures/desktop-library.json', import.meta.url), 'utf8')) as unknown;
const lib = {
  version: 1,
  favorites: ['P1-11', 'P1-35', 'P2-57', 'P3-45', 'U1-11', 'P6-88'],
  recents: [
    { address: 'P1-11', count: 12, last: '2026-10-24T18:02:11Z' },
    { address: 'P3-45', count: 3, last: '2026-10-24T18:01:00Z' },
    { address: 'U1-11', count: 1, last: '2026-10-24T18:00:00Z' },
  ],
  userNames: { 'U1-11': 'Warm Pad Mk2' },
};
const dir = new URL('../../test-results/screenshots/', import.meta.url).pathname;
mkdirSync(dir, { recursive: true });

for (const [size, vp] of Object.entries(SIZES)) {
  test.describe(size, () => {
    test.use({ viewport: vp, hasTouch: size !== 'desktop' });
    const shot = async (page: Page, name: string) => {
      await page.waitForTimeout(60);
      await page.screenshot({ path: `${dir}${size}-${name}.png` });
    };
    const phone = size !== 'desktop';
    const search = async (page: Page, text: string) => {
      if (size === 'portrait' && !(await page.locator('.search').isVisible())) await page.locator('.searchBtn').click();
      await page.locator('.search').fill(text);
    };

    test('main states', async ({ page }) => {
      await open(page, '', { library: lib });
      await shot(page, '01-default');
      await page.locator('.pad.next').click();
      await shot(page, '02-sent');
      await page.locator('.pad.g[data-g="10"]').click();
      await shot(page, '03-entry-group');
      await page.locator('.pad.b[data-n="4"]').click();
      await shot(page, '04-entry-bank');
      await page.keyboard.press('Escape');
      await search(page, 'brass');
      await page.waitForTimeout(450);
      await shot(page, '05-search');
      await search(page, 'zzzz');
      await shot(page, '06-search-empty');
      await search(page, '');
      await page.locator('.pad.v[data-v=favorites]').click();
      await shot(page, '07-favourites');
      await page.locator('.pad.v[data-v=recents]').click();
      await shot(page, '08-recents');
      await page.locator('.pad.v[data-v=all]').click();
      await page.locator('.search').fill('u1');
      await page.locator('.row').first().dblclick();
      await shot(page, '09-rename');
      await page.keyboard.press('Escape');
    });

    test('states needing other starting points', async ({ page }) => {
      await open(page, '', { settings: { output: { name: 'Gone Port' }, channel: 3 }, library: lib });
      await shot(page, '10-disconnected');
      const p2 = await page.context().newPage();
      await p2.setViewportSize(vp);
      await open(p2, '', { settings: null });
      await p2.screenshot({ path: `${dir}${size}-11-first-run.png` });
      const p3 = await page.context().newPage();
      await p3.setViewportSize(vp);
      await open(p3);
      await p3.waitForTimeout(60);
      await p3.screenshot({ path: `${dir}${size}-12-no-favourites.png` });
      if (size === 'portrait') await p3.locator('.searchBtn').click();
      await p3.locator('.pad.v[data-v=favorites]').click();
      await p3.screenshot({ path: `${dir}${size}-13-favourites-empty.png` });
    });

    test('menu and dialogs', async ({ page }) => {
      await open(page, '', { library: lib });
      await page.locator('.sys').click();
      await shot(page, '20-menu');
      await page.getByRole('menuitem', { name: 'Settings...' }).click();
      await shot(page, '21-settings');
      await page.getByRole('button', { name: 'CANCEL' }).click();
      await page.locator('.sys').click();
      await page.getByRole('menuitem', { name: 'About D-05 Patch Switcher' }).click();
      await shot(page, '22-about');
      await page.keyboard.press('Escape');
      if (!phone) {
        await page.keyboard.press('F1');
        await shot(page, '23-shortcuts');
        await page.keyboard.press('F1');
      }
      await page.locator('.sys').click();
      await page.getByRole('menuitem', { name: 'Clear Recents...' }).click();
      await shot(page, '24-clear-recents');
      await page.getByRole('button', { name: 'CANCEL' }).click();
      await page.locator('#importFile').setInputFiles(new URL('../fixtures/desktop-library.json', import.meta.url).pathname);
      await shot(page, '25-import-confirm');
      await page.getByRole('button', { name: 'IMPORT' }).click();
      await shot(page, '26-import-done');
      expect(fixture).toBeTruthy();
    });

    for (const access of ['prompt', 'denied', 'unsupported', 'insecure']) {
      test(`access ${access}`, async ({ page }) => {
        await open(page, `access=${access}`);
        await shot(page, `30-access-${access}`);
      });
    }

    test('first grant opens Settings', async ({ page }) => {
      await open(page, 'access=prompt', { settings: null });
      await page.getByRole('button', { name: 'CONNECT MIDI' }).click();
      await expect(page.locator('dialog.settings')).toBeVisible();
      await shot(page, '31-first-grant-settings');
    });

    test('blocked storage', async ({ page }) => {
      await page.addInitScript(() => {
        Object.defineProperty(window, 'localStorage', { get() { throw new DOMException('blocked', 'SecurityError'); } });
      });
      await page.goto('/d05-patch-switcher/?fake=1');
      await expect(page.locator('.stage')).toBeVisible();
      await shot(page, '32-storage-blocked');
    });
  });
}
