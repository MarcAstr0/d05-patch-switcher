import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

export const readRepoFile = (rel: string): string =>
  readFileSync(fileURLToPath(new URL(`../../../${rel}`, import.meta.url)), 'utf8');

export const readWebFile = (rel: string): string =>
  readFileSync(fileURLToPath(new URL(`../../${rel}`, import.meta.url)), 'utf8');
