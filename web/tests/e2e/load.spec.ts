import { expect, test } from '@playwright/test';

test('page loads with the right title', async ({ page }) => {
  await page.goto('/d05-patch-switcher/');
  await expect(page).toHaveTitle('D-05 Patch Switcher');
  await expect(page.locator('#app')).toHaveText('D-05 Patch Switcher');
});
