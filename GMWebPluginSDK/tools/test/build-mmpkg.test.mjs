import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdtemp, mkdir, readFile, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { inflateRawSync } from 'node:zlib';

import { buildMmpkg } from '../build-mmpkg.mjs';

test('builds an App-compatible mmpkg with a complete SHA-256 file table', async (context) => {
  const root = await mkdtemp(join(tmpdir(), 'gm-mmpkg-'));
  context.after(async () => (await import('node:fs/promises')).rm(root, { recursive: true, force: true }));
  const source = join(root, 'plugin');
  await mkdir(join(source, 'assets'), { recursive: true });
  await writeFile(join(source, 'manifest.json'), JSON.stringify({
    id: 'com.example.weather',
    name: 'Weather',
    version: '1.2.3',
    entry: 'index.html',
    bridgeVersion: '1.0',
    permissions: ['display', 'storage'],
    deviceRequirements: {
      preferredPluginId: 'com.gm.example.web-bridge',
      protocols: [{ id: 'gm.scene', minVersion: '1.0' }],
    },
  }));
  await writeFile(join(source, 'index.html'), '<!doctype html><script src="./assets/app.js"></script>');
  await writeFile(join(source, 'assets/app.js'), 'globalThis.ready = true;');
  const output = join(root, 'release', 'weather.mmpkg');

  const result = await buildMmpkg(source, output);
  const entries = readZip(await readFile(output));
  const manifest = JSON.parse(entries.get('manifest.json').toString());

  assert.equal(result.id, 'com.example.weather');
  assert.deepEqual([...entries.keys()], ['manifest.json', 'assets/app.js', 'index.html']);
  assert.equal(manifest.schemaVersion, 1);
  assert.deepEqual(manifest.deviceRequirements, {
    preferredPluginId: 'com.gm.example.web-bridge',
    protocols: [{ id: 'gm.scene', minVersion: '1.0' }],
  });
  assert.deepEqual(manifest.files, {
    'assets/app.js': `sha256:${createHash('sha256').update(entries.get('assets/app.js')).digest('hex')}`,
    'index.html': `sha256:${createHash('sha256').update(entries.get('index.html')).digest('hex')}`,
  });
});

test('rejects symlinks and output paths inside the plugin directory', async (context) => {
  const root = await mkdtemp(join(tmpdir(), 'gm-mmpkg-invalid-'));
  context.after(async () => (await import('node:fs/promises')).rm(root, { recursive: true, force: true }));
  await writeFile(join(root, 'manifest.json'), JSON.stringify({
    id: 'com.example.invalid',
    name: 'Invalid',
    version: '1.0.0',
    entry: 'index.html',
    bridgeVersion: '1.0',
    permissions: [],
  }));
  await writeFile(join(root, 'index.html'), '<!doctype html>');
  await assert.rejects(buildMmpkg(root, join(root, 'plugin.mmpkg')), /不能位于/);
  await symlink(join(root, 'index.html'), join(root, 'linked.html'));
  await assert.rejects(buildMmpkg(root, join(tmpdir(), `invalid-${Date.now()}.mmpkg`)), /符号链接/);
});

function readZip(archive) {
  const entries = new Map();
  let offset = 0;
  while (archive.readUInt32LE(offset) === 0x04034b50) {
    const compression = archive.readUInt16LE(offset + 8);
    const compressedSize = archive.readUInt32LE(offset + 18);
    const nameLength = archive.readUInt16LE(offset + 26);
    const extraLength = archive.readUInt16LE(offset + 28);
    const nameStart = offset + 30;
    const dataStart = nameStart + nameLength + extraLength;
    const name = archive.subarray(nameStart, nameStart + nameLength).toString();
    const compressed = archive.subarray(dataStart, dataStart + compressedSize);
    assert.equal(compression, 8);
    entries.set(name, inflateRawSync(compressed));
    offset = dataStart + compressedSize;
  }
  return entries;
}
