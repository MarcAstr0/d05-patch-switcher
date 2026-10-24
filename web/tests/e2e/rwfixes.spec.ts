// RW review fixes: stalled MIDI request, update notice, Settings option churn, two-tab ports, import cap.
import { expect, test } from '@playwright/test';
import { BASE, SIZES, fake, open, sent } from './helpers';

test('a MIDI request that never answers shows a hint after 20 s and allows one more click', async ({ page }) => {
  await page.clock.install();
  await page.addInitScript(() => {
    const w = window as unknown as Record<string, unknown>;
    w['__reqs'] = 0;
    (navigator as unknown as Record<string, unknown>)['requestMIDIAccess'] = () => {
      (w['__reqs'] as number)++;
      return new Promise(() => undefined);
    };
  });
  await page.goto(BASE);
  await page.getByRole('button', { name: 'CONNECT MIDI' }).click();
  await expect(page.getByRole('button', { name: 'WAITING' })).toBeDisabled();
  await expect(page.locator('.access .note')).not.toContainText('No answer');
  await page.clock.fastForward(21_000);
  await expect(page.locator('.access .note')).toContainText('No answer from the browser? Reload the page, or check the site permission.');
  await page.getByRole('button', { name: 'CONNECT MIDI' }).click();
  await expect(page.getByRole('button', { name: 'WAITING' })).toBeDisabled();
  expect(await page.evaluate(() => (window as unknown as { __reqs: number }).__reqs)).toBe(2);
});

async function withWorker(page: import('@playwright/test').Page, controller: boolean): Promise<void> {
  await page.addInitScript((c) => {
    const sw = new EventTarget() as EventTarget & Record<string, unknown>;
    sw['controller'] = c ? {} : null;
    sw['register'] = () => Promise.resolve({});
    Object.defineProperty(navigator, 'serviceWorker', { value: sw, configurable: true });
    (window as unknown as Record<string, unknown>)['__sw'] = sw;
  }, controller);
}

test('an update notice appears on controllerchange, without reloading', async ({ page }) => {
  await withWorker(page, true);
  await open(page);
  await page.evaluate(() => (window as unknown as { __sw: EventTarget }).__sw.dispatchEvent(new Event('controllerchange')));
  await expect(page.locator('.status .msg')).toHaveText('Updated. Reload to use the new version.');
});

test('no notice on the first install', async ({ page }) => {
  await withWorker(page, false);
  await open(page);
  await page.evaluate(() => (window as unknown as { __sw: EventTarget }).__sw.dispatchEvent(new Event('controllerchange')));
  await page.waitForTimeout(100);
  await expect(page.locator('.status .msg')).not.toContainText('Updated');
});

test('portrait: the update notice is a card', async ({ page }) => {
  await page.setViewportSize(SIZES.portrait);
  await withWorker(page, true);
  await open(page);
  await page.evaluate(() => (window as unknown as { __sw: EventTarget }).__sw.dispatchEvent(new Event('controllerchange')));
  await expect(page.locator('dialog.note .ctext')).toHaveText('Updated. Reload to use the new version.');
});

test('Settings keeps its option nodes when a statechange changes nothing', async ({ page }) => {
  await open(page);
  await page.keyboard.press('Control+,');
  const sel = page.locator('dialog.settings #s-out');
  await expect(sel).toBeVisible();
  await sel.evaluate((s) => ((s.querySelector('option') as unknown as Record<string, unknown>)['mark'] = 1));
  await fake(page, "(m) => m.setAccess('granted')");
  await fake(page, "(m) => m.setAccess('granted')");
  expect(await sel.evaluate((s) => (s.querySelector('option') as unknown as Record<string, unknown>)['mark'])).toBe(1);
  await fake(page, "(m) => m.plug('out', { name: 'Another' })");
  await expect(sel.locator('option')).toHaveCount(2);
});

test('two tabs: unrelated setting changes do not reopen ports; a port change does', async ({ page, context }) => {
  await open(page);
  await page.evaluate(() => {
    const m = (window as unknown as { __fakeMidi: { setOutput(p: unknown): void } }).__fakeMidi;
    const w = window as unknown as Record<string, unknown>;
    w['__opens'] = 0;
    const orig = m.setOutput.bind(m);
    m.setOutput = (p) => {
      (w['__opens'] as number)++;
      orig(p);
    };
  });
  const opens = (): Promise<number> => page.evaluate(() => (window as unknown as { __opens: number }).__opens);
  const b = await context.newPage();
  await b.goto(`${BASE}?fake=1`);
  await b.keyboard.press('Control+Shift+A');
  await expect(page.locator('.pad.auto')).toHaveClass(/lit/);
  expect(await opens()).toBe(0);
  await b.evaluate(() => {
    const s = JSON.parse(localStorage.getItem('d05.settings')!) as Record<string, unknown>;
    s['output'] = { name: 'Other Port' };
    localStorage.setItem('d05.settings', JSON.stringify(s));
  });
  await expect.poll(opens).toBe(1);
});

test('two tabs: an open Settings dialog keeps its channel; the other tab\'s channel applies after it closes', async ({ page, context }) => {
  await open(page);
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await expect(dlg).toBeVisible();
  const b = await context.newPage();
  await b.goto(`${BASE}?fake=1`);
  await b.evaluate(() => {
    const s = JSON.parse(localStorage.getItem('d05.settings')!) as Record<string, unknown>;
    s['channel'] = 5;
    localStorage.setItem('d05.settings', JSON.stringify(s));
  });
  await page.waitForTimeout(150);
  await expect(dlg.locator('#s-ch')).toHaveValue('1');
  await dlg.getByRole('button', { name: 'CANCEL' }).click();
  await page.locator('.list').focus();
  await page.keyboard.press('Home');
  await page.keyboard.press('Enter');
  expect((await sent(page)).slice(-3)[0]).toBe('B4 00 57');
});

test('Import refuses a file over 1 MB without reading it', async ({ page }) => {
  await open(page);
  await page.locator('#importFile').setInputFiles({ name: 'big.json', mimeType: 'application/json', buffer: Buffer.alloc(1024 * 1024 + 1, 32) });
  await expect(page.locator('.status .msg')).toContainText('larger than 1 MB');
});
