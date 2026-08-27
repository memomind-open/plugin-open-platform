import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const directory = new URL('.', import.meta.url);

test('Package information keeps the install status aligned beside the QR', async () => {
  const css = await readFile(new URL('studio.css', directory), 'utf8');
  assert.match(css, /grid-template-columns: minmax\(0, 1fr\) auto auto;/u);
  assert.match(css, /\.web-share-copy strong, \.device-share-copy strong \{ grid-column: 3;/u);
  assert.match(css, /#web-share-name, #device-share-name \{ grid-column: 1;/u);
  assert.match(css, /#web-share-name, #device-share-name \{[^}]*height: 2\.7em;[^}]*-webkit-line-clamp: 2;/u);
});

test('Web plugin selection owns the inline package QR and automatic run flow', async () => {
  const [html, script] = await Promise.all([
    readFile(new URL('index.html', directory), 'utf8'),
    readFile(new URL('studio.js', directory), 'utf8'),
  ]);

  assert.match(html, /id="web-share-qr"/u);
  assert.match(html, /class="compatibility-status picker-hint"/u);
  assert.match(html, /class="web-share-copy"[\s\S]*class="picker-actions"/u);
  assert.doesNotMatch(html, /id="share-web"/u);
  assert.doesNotMatch(html, /id="run-web"/u);
  assert.doesNotMatch(html, /id="import-web-workspace"/u);
  assert.match(script, /webPlugin\.addEventListener\('change', handleWebSelectionChange\)/u);
  assert.doesNotMatch(script, /if \(!deviceSelectionExplicit && availableDevicePlugins\.length > 0\)/u);
  assert.match(script, /devicePlugin\.value = fallback;\s+deviceSelectionExplicit = false;/u);
  assert.match(script, /invoke\('build_and_share_web_plugin', \{ path \}\)/u);
  assert.match(script, /await runWebPlugin\(\);/u);
  assert.match(script, /frame\.addEventListener\('load', \(\) => \{/u);
  assert.match(script, /queueWebPackageShareWhenIdle\(path\);/u);
  assert.match(script, /requestIdleCallback\(startPackaging, \{ timeout: 1500 \}\)/u);
  assert.match(script, /webShareName\.textContent = `路径：\$\{result\.packagePath\}`;/u);
  assert.match(html, /id="web-share-qr"[^>]*width="160" height="160"/u);
});

test('Device plugin selection owns the inline package QR and automatic run flow', async () => {
  const [html, script] = await Promise.all([
    readFile(new URL('index.html', directory), 'utf8'),
    readFile(new URL('studio.js', directory), 'utf8'),
  ]);

  assert.match(html, /id="device-share-qr"[^>]*width="160" height="160"/u);
  assert.match(html, /class="device-share-copy"[\s\S]*class="picker-actions"/u);
  assert.doesNotMatch(html, /id="run-device"/u);
  assert.doesNotMatch(html, /id="share-device"/u);
  assert.doesNotMatch(html, /id="import-device-workspace"/u);
  assert.doesNotMatch(html, /id="stop-device"/u);
  assert.doesNotMatch(html, /id="share-dialog"/u);
  assert.match(script, /devicePlugin\.addEventListener\('change', handleDeviceSelectionChange\)/u);
  assert.match(script, /webPlugin\.value = '';\s+disableWebPlugin\(\);\s+void stopWebPackageShare\(\);/u);
  assert.match(script, /invoke\('share_device_plugin', \{ path \}\)/u);
  assert.match(script, /queueDevicePackageShareAfterPaint\(devicePlugin\.value\);/u);
  assert.match(script, /deviceShareName\.textContent = `路径：\$\{result\.packagePath\}`;/u);
});

test('Package QR codes open a centered zoom view without multi-click dismissal', async () => {
  const [html, css, script] = await Promise.all([
    readFile(new URL('index.html', directory), 'utf8'),
    readFile(new URL('studio.css', directory), 'utf8'),
    readFile(new URL('studio.js', directory), 'utf8'),
  ]);

  assert.match(html, /id="web-share-qr" class="clickable-qr"/u);
  assert.match(html, /id="device-share-qr" class="clickable-qr"/u);
  assert.match(html, /id="qr-zoom" class="qr-zoom" hidden role="dialog"/u);
  assert.match(css, /\.qr-zoom \{ position: fixed;[\s\S]*place-items: center;/u);
  assert.match(script, /qr\.addEventListener\('click', \(\) => openQrZoom\(qr\)\)/u);
  assert.match(script, /performance\.now\(\) - Number\(qrZoom\.dataset\.openedAt \|\| 0\) < 400/u);
  assert.match(script, /event\.key === 'Escape'/u);
});
