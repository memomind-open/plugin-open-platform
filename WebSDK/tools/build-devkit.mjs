#!/usr/bin/env node
import { createHash } from 'node:crypto';
import { cp, mkdir, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import { dirname, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { createZip } from './build-mmpkg.mjs';
import { bundleWebSdk } from './bundle-web-sdk.mjs';

const repositoryRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const packageJson = JSON.parse(await readFile(resolve(repositoryRoot, 'package.json'), 'utf8'));
const version = packageJson.version;
const name = `gm-web-plugin-devkit-${version}`;
const outputDirectory = resolve(repositoryRoot, 'dist');
const staging = resolve(outputDirectory, name);
const outputZip = resolve(outputDirectory, `${name}.zip`);

await rm(staging, { recursive: true, force: true });
await rm(outputZip, { force: true });
await mkdir(staging, { recursive: true });

await copy('docs/web-plugin', 'docs/web-plugin');
await copy('docs/GM-Web-Plugin-Developer-Guide-Draft.md', 'GM-Web-Plugin-Developer-Guide-Draft.md');
await copy('browser-studio', 'internal/browser-studio');
await copy('packages/bridge-contract/src', 'internal/packages/bridge-contract/src');
await copy('packages/device-renderer/src', 'internal/packages/device-renderer/src');
await copy('packages/studio-runtime/src', 'internal/packages/studio-runtime/src');
await copy('packages/web-sdk/src', 'internal/packages/web-sdk/src');
await copy('examples/counter', 'examples/counter');
await copy('tools/build-mmpkg.mjs', 'tools/build-mmpkg.mjs');
await copy('tools/studio-cli.mjs', 'studio/gm-plugin-studio.mjs');
await copy('tools/studio-paths.mjs', 'studio/studio-paths.mjs');
await copy('tools/devkit/README.md', 'README.md');
await prepareStudioCli();

const bundledSdk = await bundleWebSdk(repositoryRoot, version);
await mkdir(resolve(staging, 'sdk'), { recursive: true });
await writeFile(resolve(staging, 'sdk/gm-plugin-web-sdk.esm.js'), bundledSdk);
await writeFile(resolve(staging, 'sdk/index.js'), bundledSdk);
await cp(resolve(repositoryRoot, 'packages/web-sdk/src/index.d.ts'), resolve(staging, 'sdk/gm-plugin-web-sdk.d.ts'));

await prepareStandaloneExample(bundledSdk);
await writeFile(resolve(staging, 'package.json'), `${JSON.stringify({
  name: 'gm-web-plugin-devkit-preview',
  version,
  private: true,
  type: 'module',
  engines: { node: '>=18.0.0' },
}, null, 2)}\n`);
await writeFile(resolve(staging, 'DEVKIT-MANIFEST.json'), `${JSON.stringify({
  name: 'GM Web Plugin DevKit',
  version,
  bridgeVersion: '1.0',
  distribution: 'internal-zip-preview',
  node: '>=18.0.0',
  npmMigration: {
    sdk: '@memomind/gm-plugin-web-sdk',
    studio: '@memomind/gm-plugin-studio',
  },
}, null, 2)}\n`);

const zipEntries = await collectEntries(staging);
const archive = createZip(zipEntries);
await writeFile(outputZip, archive);
const checksum = createHash('sha256').update(archive).digest('hex');
await writeFile(`${outputZip}.sha256`, `${checksum}  ${name}.zip\n`);
await rm(staging, { recursive: true, force: true });
process.stdout.write(`${outputZip}\n${zipEntries.length} files, ${archive.length} bytes\nSHA-256 ${checksum}\n`);

async function copy(source, target) {
  const destination = resolve(staging, target);
  await mkdir(dirname(destination), { recursive: true });
  await cp(resolve(repositoryRoot, source), destination, { recursive: true });
}

async function prepareStandaloneExample(sdk) {
  const exampleRoot = resolve(staging, 'examples/counter');
  const indexPath = resolve(exampleRoot, 'index.html');
  const pluginPath = resolve(exampleRoot, 'plugin.js');
  const index = await readFile(indexPath, 'utf8');
  const plugin = await readFile(pluginPath, 'utf8');
  await writeFile(indexPath, index.replace(/\s*<script type="importmap">[\s\S]*?<\/script>\s*/u, '\n'));
  await writeFile(pluginPath, plugin.replace("'@memomind/gm-plugin-web-sdk'", "'./vendor/gm-plugin-web-sdk.esm.js'"));
  await mkdir(resolve(exampleRoot, 'vendor'), { recursive: true });
  await writeFile(resolve(exampleRoot, 'vendor/gm-plugin-web-sdk.esm.js'), sdk);
}

async function prepareStudioCli() {
  const cliPath = resolve(staging, 'studio/gm-plugin-studio.mjs');
  const cli = await readFile(cliPath, 'utf8');
  await writeFile(cliPath, cli
    .replace("resolve(repositoryRoot, 'packages/web-sdk/src')", "resolve(repositoryRoot, 'sdk')")
    .replace("resolve(repositoryRoot, 'packages/bridge-contract/src')", "resolve(repositoryRoot, 'internal/packages/bridge-contract/src')")
    .replace("resolve(repositoryRoot, 'packages/device-renderer/src')", "resolve(repositoryRoot, 'internal/packages/device-renderer/src')")
    .replace("resolve(repositoryRoot, 'packages/studio-runtime/src')", "resolve(repositoryRoot, 'internal/packages/studio-runtime/src')")
    .replace("resolve(repositoryRoot, 'browser-studio')", "resolve(repositoryRoot, 'internal/browser-studio')"));
}

async function collectEntries(root) {
  const result = [];
  await walk(root);
  result.sort((left, right) => left.path.localeCompare(right.path));
  return result;

  async function walk(directory) {
    const entries = await readdir(directory, { withFileTypes: true });
    for (const entry of entries) {
      const absolute = resolve(directory, entry.name);
      if (entry.isDirectory()) await walk(absolute);
      else if (entry.isFile()) result.push({
        path: `${name}/${relative(root, absolute).split('\\').join('/')}`,
        bytes: await readFile(absolute),
      });
    }
  }
}
