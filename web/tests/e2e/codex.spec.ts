// Codex review findings: same-named ports, resend rules, existing grant, follow state.
import { expect, test } from '@playwright/test';
import { fake, line1, line2, open, sent } from './helpers';

const A = { name: 'USB MIDI', manufacturer: 'Acme' };
const B = { name: 'USB MIDI', manufacturer: 'Other' };

test('same-named ports: another maker is not a fallback, and switching makers resends on connect', async ({ page }) => {
  await open(page, '', { settings: { output: A, channel: 1, resendOnConnect: true } });
  await fake(page, `(m) => m.plug('out', ${JSON.stringify(B)})`);
  expect(await line2(page)).toBe('NO MIDI OUTPUT'); // B is present, A is not
  await fake(page, `(m) => m.plug('out', ${JSON.stringify(A)})`);
  await expect.poll(async () => (await sent(page)).length).toBe(3);
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await dlg.locator('#s-out').selectOption({ label: 'USB MIDI (Other)' });
  await dlg.getByRole('button', { name: 'OK' }).click();
  await expect.poll(async () => (await sent(page)).length).toBe(6);
});

test('TEST then OK sends the patch once', async ({ page }) => {
  await open(page, '', { settings: { output: { name: 'Fake D-05 Out' }, channel: 1, resendOnConnect: true } });
  await expect.poll(async () => (await sent(page)).length).toBe(3);
  await fake(page, "(m) => m.plug('out', { name: 'Other Port' })");
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await dlg.locator('#s-out').selectOption({ label: 'Other Port' });
  await dlg.getByRole('button', { name: 'TEST' }).click();
  expect((await sent(page)).length).toBe(6);
  await dlg.getByRole('button', { name: 'OK' }).click();
  await page.waitForTimeout(200);
  expect((await sent(page)).length).toBe(6);
});

test('an existing grant does not open Settings by itself', async ({ page }) => {
  await open(page, 'access=prompt', { settings: null });
  await fake(page, "(m) => m.setAccess('granted')");
  await page.waitForTimeout(200);
  await expect(page.locator('dialog.settings')).toHaveCount(0);
  expect(await line2(page)).toBe('CHOOSE A PORT IN SETTINGS');
});

test('changing the input forgets a half-received bank select', async ({ page }) => {
  await open(page, '', { settings: { output: { name: 'Fake D-05 Out' }, input: { name: 'Fake D-05 In' }, follow: true, channel: 1 } });
  await fake(page, "(m) => m.plug('in', { name: 'In Two' })");
  await fake(page, "(m) => { m.inject([0xb0, 0, 87]); m.inject([0xb0, 32, 9]); }");
  await page.keyboard.press('Control+,');
  const dlg = page.locator('dialog.settings');
  await dlg.locator('#s-in').selectOption({ label: 'In Two' });
  await dlg.getByRole('button', { name: 'OK' }).click();
  await fake(page, '(m) => m.inject([0xc0, 0])');
  await page.waitForTimeout(150);
  expect(await line1(page)).toMatch(/^P1-11/);
});
