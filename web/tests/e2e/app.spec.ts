import { readFileSync } from 'node:fs';
import { expect, test } from '@playwright/test';
import { fake, line1, line2, open, sent } from './helpers';

const fixture = JSON.parse(readFileSync(new URL('../fixtures/desktop-library.json', import.meta.url), 'utf8')) as unknown;

test('page loads with the right title and the panel', async ({ page }) => {
  await open(page);
  await expect(page).toHaveTitle('D-05 Patch Switcher');
  await expect(page.locator('.pad.g')).toHaveCount(14);
  expect(await line1(page)).toBe('P1-11:Fantasia' + ' '.repeat(22) + 'DUAL');
  expect(await line2(page)).toBe('PRESET 1: ORIGINAL D-50');
});

test('highlighting does not send; Enter sends exactly one triplet', async ({ page }) => {
  await open(page);
  await page.locator('.list').focus();
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('End');
  expect(await sent(page)).toEqual([]);
  await page.keyboard.press('Home');
  await page.keyboard.press('Enter');
  expect(await sent(page)).toEqual(['B0 00 57', 'B0 20 00', 'C0 00']); // U1-11
  expect(await line2(page)).toBe('SENT');
});

test('auto-send: a keyboard highlight move sends, a held key sends first and last only', async ({ page }) => {
  await open(page, '', { settings: { output: { name: 'Fake D-05 Out' }, channel: 2, autoSend: true } });
  await page.locator('.list').focus();
  await page.keyboard.press('ArrowDown'); // P1-12
  expect(await sent(page)).toEqual(['B1 00 57', 'B1 20 08', 'C1 01']);
  await page.keyboard.down('ArrowDown'); // a fresh press sends
  expect((await sent(page)).length).toBe(6);
  await page.keyboard.down('ArrowDown'); // auto-repeats do not
  await page.keyboard.down('ArrowDown');
  expect((await sent(page)).length).toBe(6);
  await page.keyboard.up('ArrowDown'); // the release sends the final row
  expect((await sent(page)).length).toBe(9);
});

test('click on a row, grid entry, PREV and NEXT each send exactly one correct triplet', async ({ page }) => {
  await open(page);
  await page.locator('.row', { hasText: 'Metal Harp' }).click();
  expect(await sent(page)).toEqual(['B0 00 57', 'B0 20 08', 'C0 01']);
  await page.locator('.pad.g[data-g="10"]').click(); // P3
  expect(await line1(page)).toBe('P3-__');
  await page.locator('.pad.b[data-n="4"]').click();
  expect(await line1(page)).toBe('P3-4_');
  expect((await sent(page)).length).toBe(3);
  await page.locator('.pad.n[data-n="5"]').click();
  expect((await sent(page)).slice(3)).toEqual(['B0 00 57', 'B0 20 0A', 'C0 1C']);
  expect(await line1(page)).toContain('P3-45:');
  await page.locator('.pad.next').click();
  expect((await sent(page)).slice(6)).toEqual(['B0 00 57', 'B0 20 0A', 'C0 1D']);
  await page.locator('.pad.prev').click();
  await page.locator('.pad.prev').click();
  expect((await sent(page)).slice(12)).toEqual(['B0 00 57', 'B0 20 0A', 'C0 1B']);
  await page.locator('.pad.n[data-n="8"]').click(); // idle number: current group and bank
  expect((await sent(page)).slice(15)).toEqual(['B0 00 57', 'B0 20 0A', 'C0 1F']);
  expect((await sent(page)).length).toBe(18);
});

test('keyboard grid entry: Ctrl+G, then two digits; Esc cancels', async ({ page }) => {
  await open(page);
  await page.keyboard.press('Control+g');
  await page.keyboard.press('2');
  expect(await line1(page)).toBe('P1-2_');
  await page.keyboard.press('7');
  expect(await sent(page)).toEqual(['B0 00 57', 'B0 20 08', 'C0 0E']);
  await page.keyboard.press('Control+g');
  await page.keyboard.press('Escape');
  expect(await line1(page)).toContain('P1-27:');
  await page.keyboard.press('PageDown');
  expect((await sent(page)).length).toBe(6);
});

test('search filters, shows the match count and highlights ranges', async ({ page }) => {
  await open(page);
  await page.locator('.list').focus();
  await page.keyboard.press('/');
  await expect(page.locator('.search')).toBeFocused();
  await page.keyboard.type('brass');
  await expect(page.locator('.row mark').first()).toBeVisible();
  expect(await line2(page)).toBe('17 MATCHES');
  expect(await page.locator('.list').getAttribute('data-rows')).toBe('17');
  await page.keyboard.press('Enter');
  expect((await sent(page)).length).toBe(3);
  await page.keyboard.press('Escape');
  expect(await page.locator('.search').inputValue()).toBe('');
  expect(await page.locator('.list').getAttribute('data-rows')).toBe('896');
  await page.keyboard.type('zzzz');
  await expect.poll(() => line2(page)).toBe('NO MATCHES');
  await expect(page.locator('.empty')).toContainText('NO MATCHES');
});

test('the list renders on demand, not 896 rows', async ({ page }) => {
  await open(page);
  expect(await page.locator('.list .row').count()).toBeLessThan(40);
  await page.locator('.list').evaluate((e) => (e.scrollTop = 1e6));
  await expect(page.locator('.row[data-address="P6-88"]')).toHaveCount(1);
  expect(await page.locator('.list .row').count()).toBeLessThan(40);
});

test('favourites and user names persist across a reload', async ({ page }) => {
  await open(page);
  await page.locator('.row', { hasText: 'Metal Harp' }).locator('svg.st').click();
  await page.locator('.row', { hasText: 'Metal Harp' }).click();
  await page.keyboard.press('Control+d'); // un-star the highlighted (Metal Harp), then star again
  await page.keyboard.press('Control+d');
  await page.locator('.search').fill('u2-3');
  await page.locator('.list').focus();
  await page.keyboard.press('F2');
  await page.keyboard.type('My Lead');
  await page.keyboard.press('Enter');
  await expect(page.locator('.row', { hasText: 'My Lead' })).toHaveCount(1);
  await page.reload();
  await page.locator('.pad.v[data-v=favorites]').click();
  await expect(page.locator('.row', { hasText: 'Metal Harp' })).toHaveCount(1);
  await page.locator('.pad.v[data-v=all]').click();
  await page.locator('.search').fill('my lead');
  await expect(page.locator('.row', { hasText: 'My Lead' })).toHaveCount(1);
  await expect(page.locator('.row.fav')).toHaveCount(0);
});

test('names longer than 18 characters and non-ASCII are cut in the editor', async ({ page }) => {
  await open(page);
  await page.locator('.search').fill('u1');
  await page.locator('.list').focus();
  await page.keyboard.press('F2');
  await page.keyboard.type('Café 123456789012345678901234');
  await page.keyboard.press('Enter');
  await expect(page.locator('.row .nm').first()).toHaveText('Caf 12345678901234');
});

test('F2 on a preset says so', async ({ page }) => {
  await open(page);
  await page.locator('.list').focus();
  await page.keyboard.press('PageDown'); // sends P1-12, a preset
  await page.keyboard.press('F2');
  await expect(page.locator('.status .msg')).toHaveText('Only user slots can be renamed');
});

test('Recents: counts show in the view and the order holds while it is shown', async ({ page }) => {
  await open(page);
  await page.locator('.row', { hasText: 'Metal Harp' }).click();
  await page.locator('.row', { hasText: 'Fantasia' }).click();
  await page.locator('.row', { hasText: 'Fantasia' }).click();
  await page.keyboard.press('Control+3');
  await expect(page.locator('.row').first()).toContainText('Fantasia');
  await expect(page.locator('.row').first().locator('.ct')).toHaveText('×2');
  await page.locator('.row', { hasText: 'Metal Harp' }).click();
  await expect(page.locator('.row').first()).toContainText('Fantasia'); // order refreshes on re-entry
  await page.keyboard.press('Control+1');
  await page.keyboard.press('Control+3');
  await expect(page.locator('.row').first()).toContainText('Metal Harp');
});

test('Clear Recents asks first', async ({ page }) => {
  await open(page);
  await page.locator('.row', { hasText: 'Metal Harp' }).click();
  await page.locator('.sys').click();
  await page.getByRole('menuitem', { name: 'Clear Recents...' }).click();
  await expect(page.locator('dialog.card')).toBeVisible();
  await page.getByRole('button', { name: 'CANCEL' }).click();
  await page.keyboard.press('Control+3');
  await expect(page.locator('.row')).toHaveCount(1);
  await page.locator('.sys').click();
  await page.getByRole('menuitem', { name: 'Clear Recents...' }).click();
  await page.getByRole('button', { name: 'CLEAR' }).click();
  await expect(page.locator('.row')).toHaveCount(0);
  await expect(page.locator('.empty')).toContainText('NOTHING SENT YET');
});

test('Export then Import round-trips the desktop fixture', async ({ page }) => {
  await open(page, '', { library: fixture });
  const [dl] = await Promise.all([page.waitForEvent('download'), (async () => { await page.locator('.sys').click(); await page.getByRole('menuitem', { name: 'Export Library' }).click(); })()]);
  expect(dl.suggestedFilename()).toBe('library.json');
  const path = await dl.path();
  const exported = JSON.parse(readFileSync(path, 'utf8')) as unknown;
  expect(exported).toEqual(fixture);
  // Import replaces: start from a different library in a new profile, import the fixture
  await page.evaluate(() => localStorage.removeItem('d05.library'));
  await page.reload();
  await page.locator('.pad.v[data-v=favorites]').click();
  await expect(page.locator('.row')).toHaveCount(0);
  await page.locator('.pad.v[data-v=all]').click();
  await page.locator('.sys').click();
  await page.getByRole('menuitem', { name: 'Import Library...' }).click().catch(() => undefined);
  await page.locator('#importFile').setInputFiles(new URL('../fixtures/desktop-library.json', import.meta.url).pathname);
  await expect(page.locator('dialog.card')).toContainText('desktop-library.json');
  await page.getByRole('button', { name: 'IMPORT' }).click();
  await expect(page.locator('.status .msg')).toHaveText('Imported 4 favourites, 3 names');
  await page.keyboard.press('Control+2');
  await expect(page.locator('.row')).toHaveCount(4);
  await page.keyboard.press('Control+3');
  await expect(page.locator('.row')).toHaveCount(2);
});

test('Import refuses a file that is not a library and Cancel leaves the library alone', async ({ page }) => {
  await open(page, '', { library: fixture });
  await page.locator('#importFile').setInputFiles({ name: 'x.json', mimeType: 'application/json', buffer: Buffer.from('{"nope":1}') });
  await expect(page.locator('.status .msg')).toContainText('Not a library file');
  await page.locator('#importFile').setInputFiles({ name: 'empty.json', mimeType: 'application/json', buffer: Buffer.from('{"version":1}') });
  await expect(page.locator('dialog.card')).toBeVisible();
  await page.getByRole('button', { name: 'CANCEL' }).click();
  await page.keyboard.press('Control+2');
  await expect(page.locator('.row')).toHaveCount(4);
});

test('blocked storage is visible', async ({ page }) => {
  await page.addInitScript(() => {
    Object.defineProperty(window, 'localStorage', { get() { throw new DOMException('blocked', 'SecurityError'); } });
  });
  await page.goto('/d05-patch-switcher/?fake=1');
  await expect(page.locator('.status .msg')).toContainText('Storage is blocked');
  await expect(page.locator('.status .ns')).toBeVisible();
});

test('settings: port, channel, auto-send, follow, resend on connect, Test', async ({ page }) => {
  await open(page, '', { settings: null });
  await expect(page.locator('.status .out')).toHaveText('None');
  expect(await line2(page)).toBe('CHOOSE A PORT IN SETTINGS');
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await expect(dlg).toBeVisible();
  await expect(dlg.locator('#s-out')).toHaveValue('0'); // first port preselected
  await expect(dlg.locator('.conn span')).toHaveText('Connected to Fake D-05 Out');
  await dlg.locator('#s-ch').selectOption('3');
  await dlg.locator('#s-in').selectOption('Fake D-05 In');
  await dlg.getByLabel('Follow the panel').check();
  await dlg.getByLabel('Resend the current patch on connect').check();
  await dlg.getByRole('button', { name: 'TEST' }).click();
  expect(await sent(page)).toEqual(['B2 00 57', 'B2 20 08', 'C2 00']);
  await dlg.getByRole('button', { name: 'OK' }).click();
  await expect(page.locator('.status .out')).toHaveText('Fake D-05 Out');
  await expect(page.locator('.status .ch')).toHaveText('3');
  await expect(page.locator('.status .fol')).toBeVisible();
  // follow: a panel change on channel 3 moves the display without sending
  const before = (await sent(page)).length;
  await fake(page, '(m) => { m.inject([0xb2,0,87]); m.inject([0xb2,32,10]); m.inject([0xc2,28]); }');
  await expect.poll(() => line1(page)).toContain('P3-45:');
  expect((await sent(page)).length).toBe(before);
  // hot-unplug and replug: NO MIDI OUTPUT, then resend on connect
  await fake(page, "(m) => m.unplug('out', 'Fake D-05 Out')");
  await expect.poll(() => line2(page)).toBe('NO MIDI OUTPUT');
  await expect(page.locator('.status .nf')).toBeVisible();
  await page.locator('.pad.next').click();
  await expect(page.locator('.status .msg')).toHaveText('Not sent: Port not found');
  await fake(page, "(m) => m.plug('out', { name: 'Fake D-05 Out' })");
  await expect.poll(async () => (await sent(page)).length).toBe(before + 3);
  expect((await sent(page)).slice(-3)).toEqual(['B2 00 57', 'B2 20 0A', 'C2 1C']);
  await page.reload();
  await expect(page.locator('.status .ch')).toHaveText('3');
});

test('settings Cancel restores what Test applied', async ({ page }) => {
  await open(page);
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await dlg.locator('#s-ch').selectOption('5');
  await dlg.getByRole('button', { name: 'TEST' }).click();
  await dlg.getByRole('button', { name: 'CANCEL' }).click();
  await expect(page.locator('.status .ch')).toHaveText('1');
  await page.locator('.pad.next').click();
  expect((await sent(page)).slice(-3)[0]).toBe('B0 00 57');
});

test('MIDI access panel: prompt, then granted opens Settings by itself', async ({ page }) => {
  await open(page, 'access=prompt', { settings: null });
  expect(await line2(page)).toBe('PRESS CONNECT MIDI');
  await expect(page.locator('.pad.g').first()).toBeDisabled();
  await page.getByRole('button', { name: 'CONNECT MIDI' }).click();
  await expect(page.locator('dialog.settings')).toBeVisible();
  await expect(page.locator('.pad.g').first()).toBeEnabled();
});

test('MIDI access panel: denied and unsupported', async ({ page }) => {
  await open(page, 'access=denied');
  expect(await line2(page)).toBe('MIDI ACCESS BLOCKED');
  await expect(page.locator('.access .title')).toHaveText('MIDI ACCESS IS BLOCKED');
  await expect(page.getByRole('button', { name: 'TRY AGAIN' })).toBeVisible();
  const p2 = await page.context().newPage();
  await open(p2, 'access=unsupported');
  expect(await line2(p2)).toBe('NO WEB MIDI IN THIS BROWSER');
  await expect(p2.locator('.matrix tr')).toHaveCount(4);
  await expect(p2.getByRole('button', { name: /CONNECT|TRY/ })).toHaveCount(0);
});

test('shortcuts sheet, about and menu', async ({ page }) => {
  await open(page);
  await page.keyboard.press('F1');
  await expect(page.locator('dialog.shortcuts')).toBeVisible();
  await expect(page.locator('dialog.shortcuts .srow')).toHaveCount(17);
  await page.keyboard.press('F1');
  await expect(page.locator('dialog')).toHaveCount(0);
  await page.locator('.sys').click();
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('Escape');
  await expect(page.locator('.menu')).toHaveCount(0);
  await page.locator('.sys').click();
  await page.getByRole('menuitem', { name: 'About D-05 Patch Switcher' }).click();
  await expect(page.locator('dialog.about')).toContainText('Runs offline once loaded');
  await page.keyboard.press('Escape');
  await expect(page.locator('dialog')).toHaveCount(0);
});

test('the display redraws only when its text changes', async ({ page }) => {
  await open(page);
  const cells = async () => Number(await page.locator('.bezel').getAttribute('data-cells'));
  const a = await cells();
  await page.locator('.list').focus();
  await page.keyboard.press('ArrowDown'); // highlight only: no text change
  await page.locator('.pad.v[data-v=favorites]').click();
  expect(await cells()).toBe(a);
  expect(await page.locator('.bezel').getAttribute('data-atlases')).toBe('1');
  await page.locator('.pad.next').click();
  expect(await cells()).toBeGreaterThan(a);
});

test('layout switches keep state and the page never scrolls', async ({ page }) => {
  await open(page);
  await page.locator('.search').fill('brass');
  await page.setViewportSize({ width: 390, height: 844 });
  await expect(page.locator('html')).toHaveAttribute('data-layout', 'portrait');
  await page.setViewportSize({ width: 844, height: 390 });
  await expect(page.locator('html')).toHaveAttribute('data-layout', 'landscape');
  await expect(page.locator('.search')).toHaveValue('brass');
  await page.setViewportSize({ width: 960, height: 560 });
  await expect(page.locator('html')).toHaveAttribute('data-layout', 'desktop');
  expect(await line2(page)).toBe('17 MATCHES');
  expect(await page.evaluate(() => document.documentElement.scrollHeight <= innerHeight && document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

for (const [name, size] of [['portrait', { width: 390, height: 844 }], ['landscape', { width: 844, height: 390 }], ['landscape compact', { width: 800, height: 300 }], ['small portrait', { width: 360, height: 640 }]] as const) {
  test(`touch targets are at least 44 px (${name})`, async ({ page }) => {
    await page.setViewportSize(size);
    await open(page);
    const small = await page.evaluate(() => {
      const out: string[] = [];
      for (const e of document.querySelectorAll<HTMLElement>('.stage button, .stage .row')) {
        const r = e.getBoundingClientRect();
        if (r.width === 0 || r.height === 0) continue;
        const cs = getComputedStyle(e);
        if (cs.visibility === 'hidden') continue;
        if (r.height < 43.5 || r.width < 43.5) out.push(`${e.className} ${r.width}x${r.height}`);
      }
      return out;
    });
    expect(small).toEqual([]);
  });
}

test('portrait: search overlay stays open while tapping rows; DONE closes it', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await open(page);
  await page.locator('.searchBtn').click();
  await expect(page.locator('.search')).toBeFocused();
  await page.keyboard.type('brass');
  expect(await line2(page)).toBe('P1-11 17 MATCHES');
  await page.locator('.row').nth(0).click();
  await page.locator('.row').nth(1).click();
  expect((await sent(page)).length).toBe(6);
  await expect(page.locator('.search')).toBeVisible();
  await page.locator('.row').nth(1).locator('svg.st').click(); // star: no send
  expect((await sent(page)).length).toBe(6);
  await page.locator('.done').click();
  await expect(page.locator('.search')).toBeHidden();
  await expect(page.locator('.searchBtn')).toContainText('brass');
  expect(await line2(page)).not.toContain('MATCHES');
});

test('offline: the service worker serves the app shell', async ({ page, context }) => {
  await open(page);
  await page.evaluate(async () => { await navigator.serviceWorker.ready; });
  await expect.poll(() => page.evaluate(() => !!navigator.serviceWorker.controller)).toBe(true);
  await context.setOffline(true);
  await page.reload();
  await expect(page.locator('.pad.g')).toHaveCount(14);
  await expect(page.locator('.row').first()).toBeVisible();
  expect(await line1(page)).toContain('P1-11');
  await context.setOffline(false);
});

test('no request leaves the origin after load, and none is made after load at all', async ({ page }) => {
  const external: string[] = [];
  let loaded = false;
  const late: string[] = [];
  page.on('request', (r) => {
    const u = r.url();
    if (!u.startsWith('http://localhost:4173/') && !u.startsWith('data:') && !u.startsWith('blob:')) external.push(u);
    else if (loaded && u.startsWith('http')) late.push(u);
  });
  await open(page);
  await page.waitForLoadState('networkidle');
  loaded = true;
  await page.locator('.search').fill('brass');
  await page.locator('.sys').click();
  await page.getByRole('menuitem', { name: 'About D-05 Patch Switcher' }).click();
  await page.keyboard.press('Escape');
  await page.locator('.pad.next').click();
  await page.waitForTimeout(300);
  expect(external).toEqual([]);
  expect(late.filter((u) => !u.endsWith('/sw.js'))).toEqual([]);
});

test('library is flushed on pagehide and reloaded on a storage event', async ({ page, context }) => {
  await open(page);
  await page.locator('.row', { hasText: 'Metal Harp' }).locator('svg.st').click();
  const other = await context.newPage();
  await other.goto('/d05-patch-switcher/?fake=1');
  await page.evaluate(() => window.dispatchEvent(new Event('pagehide')));
  await expect.poll(() => page.evaluate(() => localStorage.getItem('d05.library') ?? '')).toContain('P1-12');
  await other.close();
});
