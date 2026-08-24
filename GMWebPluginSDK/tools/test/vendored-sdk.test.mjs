import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import test from 'node:test';

import { bundleWebSdk } from '../bundle-web-sdk.mjs';

const repositoryRoot = resolve(import.meta.dirname, '../..');
const packageJson = JSON.parse(await readFile(resolve(repositoryRoot, 'package.json'), 'utf8'));

test('plugin SDK vendor copies match the formal standalone SDK', async () => {
  const expected = await bundleWebSdk(repositoryRoot, packageJson.version);
  for (const plugin of ['fighter-controller', 'gm-life-desk']) {
    const actual = await readFile(
      resolve(repositoryRoot, `plugins/${plugin}/vendor/gm-plugin-web-sdk.esm.js`),
      'utf8',
    );
    assert.equal(actual, expected, `${plugin} vendor SDK is stale`);
    assert.match(actual, /sendMessage:\s*async/u);
  }
});
