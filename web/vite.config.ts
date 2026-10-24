import { defineConfig } from 'vitest/config';

export default defineConfig({
  base: '/d05-patch-switcher/',
  build: { outDir: 'dist', emptyOutDir: true },
  server: { port: 5173 },
  test: {
    include: ['tests/unit/**/*.test.ts'],
    environment: 'node',
  },
});
