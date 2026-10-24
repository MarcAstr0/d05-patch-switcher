import { expect, test } from 'vitest';
import { watchUpdates } from '../../src/update';

function container(controller: unknown) {
  const t = new EventTarget();
  return Object.assign(t, { controller }) as unknown as ServiceWorkerContainer;
}

test('an update (previous controller) is announced once', () => {
  const sw = container({});
  let n = 0;
  watchUpdates(sw, () => n++);
  sw.dispatchEvent(new Event('controllerchange'));
  sw.dispatchEvent(new Event('controllerchange'));
  expect(n).toBe(1);
});

test('the first install claiming the page is not an update', () => {
  const sw = container(null);
  let n = 0;
  watchUpdates(sw, () => n++);
  sw.dispatchEvent(new Event('controllerchange'));
  expect(n).toBe(0);
});
