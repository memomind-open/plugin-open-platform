import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

const studioRoot = new URL('../', import.meta.url);

test('export staging separates platforms and records checksums', () => {
  const temporary = mkdtempSync(join(tmpdir(), 'memomind-studio-export-'));
  try {
    const executable = join(temporary, 'gm-plugin-studio-desktop.exe');
    const output = join(temporary, 'export');
    writeFileSync(executable, 'portable-test-binary');
    execFileSync(process.execPath, [
      fileURLToPath(new URL('scripts/export-studio.mjs', studioRoot)),
      '--platform', 'windows-x64',
      '--output-root', output,
      '--input', executable,
    ]);

    const manifest = JSON.parse(readFileSync(join(output, 'windows-x64', 'manifest.json'), 'utf8'));
    assert.equal(manifest.productName, 'MemoMind Plugin Studio');
    assert.equal(manifest.platform, 'windows-x64');
    assert.equal(typeof manifest.sourceDirty, 'boolean');
    assert.equal(manifest.artifacts[0].name, 'MemoMind-Plugin-Studio-portable.exe');
    assert.match(readFileSync(join(output, 'windows-x64', 'SHA256SUMS.txt'), 'utf8'),
      /MemoMind-Plugin-Studio-portable\.exe/u);
  } finally {
    rmSync(temporary, { recursive: true, force: true });
  }
});

test('single-file export keeps only the directly usable platform package', () => {
  const temporary = mkdtempSync(join(tmpdir(), 'memomind-studio-single-export-'));
  try {
    const portable = join(temporary, 'gm-plugin-studio-desktop.exe');
    const installer = join(temporary, 'MemoMind Plugin Studio_0.1.0_x64-setup.exe');
    const output = join(temporary, 'export');
    writeFileSync(portable, 'portable-test-binary');
    writeFileSync(installer, 'installer-test-binary');
    execFileSync(process.execPath, [
      fileURLToPath(new URL('scripts/export-studio.mjs', studioRoot)),
      '--platform', 'windows-x64',
      '--output-root', output,
      '--single-file',
      '--input', portable,
      '--input', installer,
    ]);

    assert.deepEqual(readdirSync(join(output, 'windows-x64')), [
      'MemoMind Plugin Studio_0.1.0_x64-setup.exe',
    ]);
  } finally {
    rmSync(temporary, { recursive: true, force: true });
  }
});
