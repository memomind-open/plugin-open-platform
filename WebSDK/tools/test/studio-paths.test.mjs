import assert from 'node:assert/strict';
import { mkdtemp, mkdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';

import { resolveServedFile } from '../studio-paths.mjs';

test('studio file resolver rejects lexical and symbolic-link traversal', async () => {
  const temporary = await mkdtemp(join(tmpdir(), 'gm-studio-paths-'));
  const root = join(temporary, 'root');
  const outside = join(temporary, 'outside');
  await mkdir(root);
  await mkdir(outside);
  await writeFile(join(root, 'index.html'), '<!doctype html>');
  await writeFile(join(outside, 'secret.txt'), 'secret');
  await symlink(outside, join(root, 'escape'));
  try {
    assert.equal(resolveServedFile(root, 'index.html'), join(root, 'index.html'));
    assert.equal(resolveServedFile(root, '../outside/secret.txt'), null);
    assert.equal(resolveServedFile(root, 'escape/secret.txt'), null);
  } finally {
    await rm(temporary, { recursive: true, force: true });
  }
});
