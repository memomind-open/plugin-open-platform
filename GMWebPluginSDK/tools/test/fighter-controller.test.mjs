import assert from 'node:assert/strict';
import test from 'node:test';

import {
  INPUT_CHANNEL,
  PAUSE_BIT,
  bytesToBase64,
  encodeInput,
} from '../../plugins/fighter-controller/protocol.js';

test('Fighter controller matches the GMPluginWindows v2 wire format', () => {
  assert.equal(INPUT_CHANNEL, 0x4647);
  assert.deepEqual([...encodeInput(7, (1 << 0) | (1 << 5) | (1 << 7))],
    [2, 7, 0, 0xa1]);
  assert.deepEqual([...encodeInput(260, 1 << 8)], [2, 4, 1, 0]);
  assert.deepEqual([...encodeInput(9, (1 << 0) | (1 << 7), true)],
    [2, 9, PAUSE_BIT >> 8, 0x81]);
  assert.equal(bytesToBase64(encodeInput(7, 0xa1)), 'AgcAoQ==');
});
