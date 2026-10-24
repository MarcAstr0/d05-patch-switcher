import { expect, test } from '@playwright/test';

test('page loads with the right title and, without ?fake, the stub access state', async ({ page }) => {
  await page.goto('/d05-patch-switcher/');
  await expect(page).toHaveTitle('D-05 Patch Switcher');
  await expect(page.locator('.stage')).toBeVisible();
  await expect(page.locator('.access .title')).toHaveText('MIDI IS NOT CONNECTED');
});
