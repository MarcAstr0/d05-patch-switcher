import { expect, test } from '@playwright/test';
import { fake, line1, open, sent } from './helpers';

const hold = async (page: import('@playwright/test').Page, key: string) => {
  await page.keyboard.down(key);
  await page.keyboard.down(key); // Playwright marks these as auto-repeats
  await page.keyboard.down(key);
  await page.keyboard.up(key);
};

test('held keys never stream sends', async ({ page }) => {
  await open(page);
  await hold(page, 'PageDown');
  expect((await sent(page)).length).toBe(3);
  await page.keyboard.press('Control+g');
  await hold(page, '2');
  expect((await sent(page)).length).toBe(3);
  await page.keyboard.press('Escape');
  // Enter on a native button (NEXT) and on the group strip and grid containers
  await page.locator('.pad.next').focus();
  await hold(page, 'Enter');
  expect((await sent(page)).length).toBe(6);
  await page.locator('.grid').focus();
  await page.keyboard.press('ArrowDown'); // number row, cursor on 1
  await hold(page, 'Enter');
  expect((await sent(page)).length).toBe(9);
  await page.locator('.groups').focus();
  await hold(page, 'Enter'); // begins an entry (no send), repeats ignored
  expect((await sent(page)).length).toBe(9);
  await page.locator('.list').focus();
  await hold(page, 'Enter');
  expect((await sent(page)).length).toBe(12);
  await page.locator('.row').first().click();
});

test('a double-click on a row sends once, then renames a user slot', async ({ page }) => {
  await open(page);
  await page.locator('.search').fill('u1');
  const before = (await sent(page)).length;
  await page.locator('.row').first().dblclick();
  expect((await sent(page)).length).toBe(before + 3);
  await expect(page.locator('.nameEditor')).toBeVisible();
});

test('two tabs: a stale tab does not overwrite settings, and settings follow the other tab', async ({ page, context }) => {
  await open(page);
  const b = await context.newPage();
  await b.goto('/d05-patch-switcher/?fake=1');
  await b.keyboard.press('Control+Shift+A'); // auto-send on in tab B, saved at once
  await expect(b.locator('.pad.auto')).toHaveClass(/lit/);
  await expect(page.locator('.pad.auto')).toHaveClass(/lit/); // tab A took it over from the storage event
  await page.evaluate(() => window.dispatchEvent(new Event('pagehide')));
  const s = await page.evaluate(() => JSON.parse(localStorage.getItem('d05.settings')!) as { autoSend: boolean; lastPatch: number });
  expect(s.autoSend).toBe(true);
  expect(s.lastPatch).toBe(512);
});

test('Cancel after TEST does not resend', async ({ page }) => {
  await open(page, '', { settings: { output: { name: 'Fake D-05 Out' }, channel: 1, resendOnConnect: true } });
  await expect.poll(async () => (await sent(page)).length).toBe(3); // the start-up resend
  await fake(page, "(m) => m.plug('out', { name: 'Other Port' })");
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await dlg.locator('#s-out').selectOption({ label: 'Other Port' });
  await dlg.getByRole('button', { name: 'TEST' }).click();
  expect((await sent(page)).length).toBe(6);
  await dlg.getByRole('button', { name: 'CANCEL' }).click();
  await page.waitForTimeout(200);
  expect((await sent(page)).length).toBe(6);
});

test('leaving portrait pops the overlay history entry', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await open(page);
  await page.locator('.searchBtn').click();
  expect(await page.evaluate(() => (history.state as { d05?: string } | null)?.d05)).toBe('overlay');
  await page.setViewportSize({ width: 960, height: 560 });
  await expect.poll(() => page.evaluate(() => (history.state as { d05?: string } | null)?.d05 ?? null)).toBeNull();
});

test('ports with the same name keep their manufacturer', async ({ page }) => {
  await open(page, '', { settings: null });
  await fake(page, "(m) => m.plug('out', { name: 'Fake D-05 Out', manufacturer: 'Other Maker' })");
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await expect(dlg.locator('#s-out option')).toHaveCount(2);
  await dlg.locator('#s-out').selectOption({ index: 1 });
  await dlg.getByRole('button', { name: 'OK' }).click();
  const saved = await page.evaluate(() => JSON.parse(localStorage.getItem('d05.settings')!) as { output: { name: string; manufacturer?: string } });
  expect(saved.output).toEqual({ name: 'Fake D-05 Out', manufacturer: 'Other Maker' });
  await page.reload();
  await fake(page, "(m) => m.plug('out', { name: 'Fake D-05 Out', manufacturer: 'Other Maker' })");
  await page.keyboard.press('Control+,');
  await expect(page.locator('#s-out')).toHaveValue('1'); // name and manufacturer win over name alone
});

test('auto-send also on Home and End, with the held-key rule', async ({ page }) => {
  await open(page, '', { settings: { output: { name: 'Fake D-05 Out' }, channel: 1, autoSend: true } });
  await page.locator('.list').focus();
  await page.keyboard.press('End');
  expect((await sent(page)).slice(-3)).toEqual(['B0 00 57', 'B0 20 0D', 'C0 3F']); // P6-88
  await hold(page, 'Home');
  expect((await sent(page)).length).toBe(6); // first press sent U1-11, repeats did not
  expect((await sent(page)).slice(-3)).toEqual(['B0 00 57', 'B0 20 00', 'C0 00']);
});

test('portrait shows messages as a card', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await open(page, '', { settings: { output: { name: 'Gone Port' }, channel: 1 } });
  await page.locator('.pad.next').click();
  await expect(page.locator('dialog.note')).toContainText('Not sent: Port not found');
  await page.getByRole('button', { name: 'OK' }).click();
  await expect(page.locator('dialog')).toHaveCount(0);
});

test('Import while Recents is shown lists most recent first', async ({ page }) => {
  const lib = { version: 1, favorites: [], recents: [{ address: 'P1-11', count: 1, last: '2026-10-24T18:02:00Z' }, { address: 'P3-45', count: 1, last: '2026-10-24T18:01:00Z' }], userNames: {} };
  await open(page, '', { library: lib });
  await page.keyboard.press('Control+3');
  expect(await page.locator('.row').evaluateAll((r) => r.map((x) => (x as HTMLElement).dataset['address']))).toEqual(['P1-11', 'P3-45']);
  const file = { version: 1, favorites: [], recents: [{ address: 'P2-57', count: 1, last: '2026-10-24T17:00:00Z' }, { address: 'P3-45', count: 4, last: '2026-10-24T19:00:00Z' }], userNames: {} };
  await page.locator('#importFile').setInputFiles({ name: 'n.json', mimeType: 'application/json', buffer: Buffer.from(JSON.stringify(file)) });
  await page.getByRole('button', { name: 'IMPORT' }).click();
  await expect.poll(() => page.locator('.row').evaluateAll((r) => r.map((x) => (x as HTMLElement).dataset['address']))).toEqual(['P3-45', 'P2-57']);
  expect(await line1(page)).toContain('P1-11');
});
