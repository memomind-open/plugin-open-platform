import assert from 'node:assert/strict';
import test from 'node:test';

import {
  encodePetState,
  PET_STATE_CHANNEL,
} from '../../examples/talking-pet/device-protocol.js';

test('talking-pet sends one compact native-animation state packet', () => {
  assert.equal(PET_STATE_CHANNEL, 0x4d50);
  assert.deepEqual(
    encodePetState('playing', { happy: 78, food: 62, energy: 84, xp: 1200 }),
    new Uint8Array([1, 3, 78, 62, 84, 0, 13]),
  );
});

test('talking-pet state packets clamp values and unknown moods', () => {
  assert.deepEqual(
    encodePetState('unknown', { happy: 110, food: -2, energy: 50.6, xp: -100 }),
    new Uint8Array([1, 0, 100, 0, 51, 0, 1]),
  );
});
