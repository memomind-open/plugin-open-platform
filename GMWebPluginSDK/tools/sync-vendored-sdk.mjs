#!/usr/bin/env node
import { readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { bundleWebSdk } from './bundle-web-sdk.mjs';

const repositoryRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const packageJson = JSON.parse(await readFile(resolve(repositoryRoot, 'package.json'), 'utf8'));
const bundled = await bundleWebSdk(repositoryRoot, packageJson.version);

for (const plugin of ['fighter-controller', 'gm-life-desk']) {
  await writeFile(resolve(repositoryRoot, `plugins/${plugin}/vendor/gm-plugin-web-sdk.esm.js`), bundled);
}
