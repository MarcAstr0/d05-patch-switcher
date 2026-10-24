import { expect, test } from 'vitest';
import { APP_NAME } from '../../src/app';

test('app name is set', () => {
  expect(APP_NAME).toBe('D-05 Patch Switcher');
});
