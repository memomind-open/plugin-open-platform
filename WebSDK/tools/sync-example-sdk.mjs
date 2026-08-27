#!/usr/bin/env node
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { bundleWebSdk } from './bundle-web-sdk.mjs';

const repositoryRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const packageJson = JSON.parse(await readFile(resolve(repositoryRoot, 'package.json'), 'utf8'));
const bundled = await bundleWebSdk(repositoryRoot, packageJson.version);

for (const plugin of [
  'examples/fighter-controller',
  'examples/life-desk',
  'examples/talking-pet',
]) {
  const vendorDirectory = resolve(repositoryRoot, plugin, 'vendor');
  await mkdir(vendorDirectory, { recursive: true });
  await writeFile(resolve(vendorDirectory, 'gm-plugin-web-sdk.esm.js'), bundled);
}
