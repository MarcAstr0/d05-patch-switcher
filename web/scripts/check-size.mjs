// Total gzipped size of all JS and CSS in dist/. Fails above the budget (docs/WEB.md section 4).
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync } from 'node:zlib';

const BUDGET = 60 * 1024;
const dist = fileURLToPath(new URL('../dist', import.meta.url));

function walk(dir) {
  return readdirSync(dir).flatMap((n) => {
    const p = join(dir, n);
    return statSync(p).isDirectory() ? walk(p) : [p];
  });
}

let total = 0;
for (const f of walk(dist)) {
  if (!['.js', '.css', '.mjs'].includes(extname(f))) continue;
  const s = gzipSync(readFileSync(f), { level: 9 }).length;
  total += s;
  console.log(`${String(s).padStart(8)} B  ${f.slice(dist.length + 1)}`);
}
console.log(`${String(total).padStart(8)} B  total gzipped JS+CSS (budget ${BUDGET})`);
if (total > BUDGET) {
  console.error('Over budget.');
  process.exit(1);
}
