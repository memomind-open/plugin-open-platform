import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const directory = new URL('.', import.meta.url);

test('Desktop window and document use the MemoMind product name', async () => {
  const [html, config] = await Promise.all([
    readFile(new URL('../index.html', directory), 'utf8'),
    readFile(new URL('../../src-tauri/tauri.conf.json', directory), 'utf8'),
  ]);
  assert.match(html, /<title>MemoMind Plugin Studio<\/title>/u);
  assert.equal(JSON.parse(config).productName, 'MemoMind Plugin Studio');
  assert.equal(JSON.parse(config).app.windows[0].title, 'MemoMind Plugin Studio');
});
