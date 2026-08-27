import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const directory = new URL('.', import.meta.url);

test('Phone and glass columns share one vertical divider', async () => {
  const css = await readFile(new URL('studio.css', directory), 'utf8');
  assert.match(css, /--paired-columns: minmax\(0, 1fr\) minmax\(0, 1fr\);/u);
  assert.match(css, /\.pair-toolbar \{[^}]*grid-template-columns: var\(--paired-columns\);[^}]*column-gap: var\(--paired-column-gap\);/u);
  assert.match(css, /\.workspace \{[^}]*grid-template-columns: var\(--paired-columns\);[^}]*column-gap: var\(--paired-column-gap\);/u);
  assert.doesNotMatch(css, /grid-template-columns: minmax\(420px, \.8fr\) minmax\(900px, 1\.2fr\)/u);
});
