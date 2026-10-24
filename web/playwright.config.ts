import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: 'tests/e2e',
  fullyParallel: true,
  forbidOnly: !!process.env['CI'],
  reporter: 'list',
  outputDir: 'test-results/output',
  use: { baseURL: 'http://localhost:4173', serviceWorkers: 'allow' },
  projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
  webServer: {
    command: 'npm run build && npx vite preview --port 4173 --strictPort',
    url: 'http://localhost:4173/d05-patch-switcher/',
    reuseExistingServer: !process.env['CI'],
  },
});
