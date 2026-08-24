import assert from 'node:assert/strict';
import test from 'node:test';

import {
  hasBridgePermission,
  isTrustedPluginMessage,
  storageNamespace,
} from './bridge-security.js';

test('only accepts messages from the active plugin frame and origin', () => {
  const frame = {};
  assert.equal(isTrustedPluginMessage({ source: frame, origin: 'http://127.0.0.1:1' }, frame,
    'http://127.0.0.1:1'), true);
  assert.equal(isTrustedPluginMessage({ source: {}, origin: 'http://127.0.0.1:1' }, frame,
    'http://127.0.0.1:1'), false);
  assert.equal(isTrustedPluginMessage({ source: frame, origin: 'https://attacker.invalid' }, frame,
    'http://127.0.0.1:1'), false);
});

test('maps protected bridge methods to declared permissions', () => {
  assert.equal(hasBridgePermission('display.createPage', ['display']), true);
  assert.equal(hasBridgePermission('display.createPage', []), false);
  assert.equal(hasBridgePermission('device.subscribeEvents', ['device.events']), true);
  assert.equal(hasBridgePermission('device.unsubscribeEvents', []), false);
  assert.equal(hasBridgePermission('storage.set', ['storage']), true);
  assert.equal(hasBridgePermission('storage.get', []), false);
  assert.equal(hasBridgePermission('plugin.sendMessage', []), true);
});

test('isolates storage values by plugin id', () => {
  const namespaces = new Map();
  storageNamespace(namespaces, 'plugin-a').set('key', 'a');
  storageNamespace(namespaces, 'plugin-b').set('key', 'b');
  assert.equal(storageNamespace(namespaces, 'plugin-a').get('key'), 'a');
  assert.equal(storageNamespace(namespaces, 'plugin-b').get('key'), 'b');
});
