import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { bundlePhoneSdk } from '../bundle-phone-sdk.mjs';

const repositoryRoot = resolve(fileURLToPath(new URL('../..', import.meta.url)));
const packageJson = JSON.parse(await readFile(resolve(repositoryRoot, 'package.json'), 'utf8'));

test('example SDK copies match the formal standalone SDK', async () => {
  const expected = await bundlePhoneSdk(repositoryRoot, packageJson.version);
  for (const plugin of [
    'examples/permission-debug',
    'examples/fighter-controller',
    'examples/life-desk',
    'examples/novel-reader',
    'examples/talking-pet',
  ]) {
    const actual = await readFile(
      resolve(repositoryRoot, plugin, 'vendor/gm-plugin-web-sdk.esm.js'),
      'utf8',
    );
    assert.equal(actual, expected, `${plugin} vendor SDK is stale`);
    assert.match(actual, /sendMessage:\s*async/u);
  }
});
