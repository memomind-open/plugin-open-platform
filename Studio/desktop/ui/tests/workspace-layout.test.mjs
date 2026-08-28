import assert from 'node:assert/strict';
import { access, readFile } from 'node:fs/promises';
import test from 'node:test';

const directory = new URL('.', import.meta.url);

test('Phone and glass columns share one vertical divider', async () => {
  const css = await readFile(new URL('../src/studio.css', directory), 'utf8');
  assert.match(css, /--paired-columns: minmax\(0, 1fr\) minmax\(0, 1fr\);/u);
  assert.match(css, /\.pair-toolbar \{[^}]*grid-template-columns: var\(--paired-columns\);[^}]*column-gap: var\(--paired-column-gap\);/u);
  assert.match(css, /\.workspace \{[^}]*grid-template-columns: var\(--paired-columns\);[^}]*column-gap: var\(--paired-column-gap\);/u);
  assert.doesNotMatch(css, /grid-template-columns: minmax\(420px, \.8fr\) minmax\(900px, 1\.2fr\)/u);
});

test('Plugin selectors start at the top without a dedicated header', async () => {
  const [html, css, script] = await Promise.all([
    readFile(new URL('../index.html', directory), 'utf8'),
    readFile(new URL('../src/studio.css', directory), 'utf8'),
    readFile(new URL('../src/studio.js', directory), 'utf8'),
  ]);
  assert.doesNotMatch(html, /class="app-header"|id="pair-status"/u);
  assert.doesNotMatch(css, /\.app-header|\.pair-status/u);
  assert.doesNotMatch(script, /querySelector\('#pair-status'\)/u);
  assert.match(css, /\.workspace \{[^}]*height: calc\(100vh - 298px\);/u);
});

test('Glass controls place head motion directly below the button controls', async () => {
  const [html, script] = await Promise.all([
    readFile(new URL('../index.html', directory), 'utf8'),
    readFile(new URL('../src/studio.js', directory), 'utf8'),
  ]);
  const buttonsPosition = html.indexOf('<span>BUTTON</span>');
  const headMotionPosition = html.indexOf('<span>HEAD MOTION</span>');

  assert.ok(buttonsPosition >= 0);
  assert.ok(headMotionPosition > buttonsPosition);
  assert.equal((html.match(/data-device-button/gu) ?? []).length, 1);
  assert.doesNotMatch(html, /data-button-action/u);
  assert.doesNotMatch(html.slice(buttonsPosition, headMotionPosition), /GESTURES|data-gesture/u);
  assert.doesNotMatch(script, /querySelectorAll\('\[data-gesture\]'\)/u);
  assert.doesNotMatch(script, /deviceActionButton\.addEventListener\('lostpointercapture'/u);
  assert.match(script, /window\.addEventListener\('pointerup', finishDeviceButtonPress\)/u);
});

test('Studio entry module imports resolve inside the source directory', async () => {
  const script = await readFile(new URL('../src/studio.js', directory), 'utf8');
  const imports = [...script.matchAll(/from '(\.\/[^']+)'/gu)]
    .map((match) => match[1]);

  assert.ok(imports.length > 0);
  await Promise.all(imports.map((specifier) => (
    access(new URL(`../src/${specifier.slice(2)}`, directory))
  )));
});
