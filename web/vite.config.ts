import { createHash } from 'node:crypto';
import { readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineConfig, type Plugin } from 'vitest/config';

const pkg = JSON.parse(readFileSync(new URL('./package.json', import.meta.url), 'utf8')) as { version: string };

function walk(dir: string): string[] {
  return readdirSync(dir).flatMap((n) => {
    const p = join(dir, n);
    return statSync(p).isDirectory() ? walk(p) : [p];
  });
}

/** Writes dist/sw.js from sw-template.js with the list of every built file and a content hash as the cache version. */
function serviceWorker(): Plugin {
  let out = '';
  return {
    name: 'd05-service-worker',
    apply: 'build',
    configResolved(c) {
      out = resolve(c.root, c.build.outDir);
    },
    closeBundle() {
      const files = walk(out)
        .map((f) => relative(out, f).split('\\').join('/'))
        .filter((f) => f !== 'sw.js' && !f.endsWith('.map') && !f.endsWith('OFL.txt') && !f.endsWith('README.md'))
        .sort();
      const template = readFileSync(fileURLToPath(new URL('./sw-template.js', import.meta.url)), 'utf8');
      const hash = createHash('sha256').update(template);
      for (const f of files) hash.update(f).update(readFileSync(join(out, f)));
      const list = ['./', ...files.map((f) => `./${f}`)];
      writeFileSync(join(out, 'sw.js'), template.replace('__VERSION__', hash.digest('hex').slice(0, 12)).replace('__FILES__', JSON.stringify(list)));
    },
  };
}

export default defineConfig({
  base: '/d05-patch-switcher/',
  define: { __APP_VERSION__: JSON.stringify(pkg.version) },
  plugins: [serviceWorker()],
  build: { outDir: 'dist', emptyOutDir: true },
  server: { port: 5173 },
  test: {
    include: ['tests/unit/**/*.test.ts'],
    environment: 'node',
  },
});
