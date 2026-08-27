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

test('Plugin selectors start at the top without a dedicated header', async () => {
  const [html, css, script] = await Promise.all([
    readFile(new URL('index.html', directory), 'utf8'),
    readFile(new URL('studio.css', directory), 'utf8'),
    readFile(new URL('studio.js', directory), 'utf8'),
  ]);
  assert.doesNotMatch(html, /class="app-header"|id="pair-status"/u);
  assert.doesNotMatch(css, /\.app-header|\.pair-status/u);
  assert.doesNotMatch(script, /querySelector\('#pair-status'\)/u);
  assert.match(css, /\.workspace \{[^}]*height: calc\(100vh - 298px\);/u);
});
