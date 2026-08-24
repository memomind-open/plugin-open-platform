import assert from 'node:assert/strict';
import test from 'node:test';

import {
  EVENT_CHANNEL,
  EVENT_VERSION,
  FIGHT_EVENT_TYPES,
  INPUT_CHANNEL,
  PAUSE_BIT,
  bytesToBase64,
  decodeFightEvent,
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

test('Fighter controller decodes the 0x4648 game event protocol', () => {
  assert.equal(EVENT_CHANNEL, 0x4648);
  assert.equal(EVENT_VERSION, 1);
  assert.deepEqual(FIGHT_EVENT_TYPES, {
    1: 'hit', 2: 'block', 3: 'guardBreak', 4: 'specialLaunch',
    5: 'roundEnd', 6: 'attack', 7: 'jump', 8: 'roundStart',
    9: 'menu', 10: 'ko', 11: 'music',
  });
  assert.deepEqual(decodeFightEvent({
    channel: EVENT_CHANNEL,
    data: Uint8Array.of(EVENT_VERSION, 19, 10, 1),
  }), { sequence: 19, type: 'ko', value: 1 });
  assert.equal(decodeFightEvent({
    channel: INPUT_CHANNEL,
    data: Uint8Array.of(EVENT_VERSION, 19, 10, 1),
  }), null);
  assert.equal(decodeFightEvent({
    channel: EVENT_CHANNEL,
    data: Uint8Array.of(2, 19, 10, 1),
  }), null);
  assert.equal(decodeFightEvent({
    channel: EVENT_CHANNEL,
    data: Uint8Array.of(EVENT_VERSION, 19, 99, 1),
  }), null);
});
