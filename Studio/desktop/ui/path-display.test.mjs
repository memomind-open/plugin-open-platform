import assert from 'node:assert/strict';
import test from 'node:test';

import { displayPath } from './path-display.js';

test('converts an extended UNC path into a readable Samba path', () => {
  assert.equal(
    displayPath('\\\\?\\UNC\\10.152.249.21\\desheng.hong\\open_mind\\plugin.gmp'),
    '\\\\10.152.249.21\\desheng.hong\\open_mind\\plugin.gmp',
  );
});

test('removes the extended prefix from a local Windows path', () => {
  assert.equal(displayPath('\\\\?\\C:\\plugins\\plugin.gmp'), 'C:\\plugins\\plugin.gmp');
});

test('leaves regular paths unchanged', () => {
  assert.equal(displayPath('/opt/plugins/plugin.gmp'), '/opt/plugins/plugin.gmp');
});
