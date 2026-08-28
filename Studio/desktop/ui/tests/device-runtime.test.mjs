import assert from 'node:assert/strict';
import test from 'node:test';

import { describeDeviceFrameTransition } from '../src/device-runtime.js';

test('draws frames while a glass plugin is running', () => {
  assert.deepEqual(describeDeviceFrameTransition(false, true), {
    drawFrame: true,
    exited: false,
  });
  assert.deepEqual(describeDeviceFrameTransition(true, true), {
    drawFrame: true,
    exited: false,
  });
});

test('reports an exit once and stops presenting stale frames', () => {
  assert.deepEqual(describeDeviceFrameTransition(true, false), {
    drawFrame: false,
    exited: true,
  });
  assert.deepEqual(describeDeviceFrameTransition(false, false), {
    drawFrame: false,
    exited: false,
  });
});
