import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BRIDGE_VERSION,
  CAPABILITIES,
  DEVICE_PROFILE,
  ERROR_CODES,
  EVENT_NAMES,
  METHOD_NAMES,
  PLUGIN_MESSAGE_PROFILE,
  SCENE_TRANSPORT_PROFILE,
  isMethodName,
} from '../src/index.js';

test('Bridge v1 contract contains unique methods and events', () => {
  assert.equal(BRIDGE_VERSION, '1.0');
  assert.equal(new Set(METHOD_NAMES).size, METHOD_NAMES.length);
  assert.equal(new Set(EVENT_NAMES).size, EVENT_NAMES.length);
  assert.equal(new Set(ERROR_CODES).size, ERROR_CODES.length);
  assert.equal(isMethodName('display.updateText'), true);
  assert.equal(isMethodName('plugin.sendMessage'), true);
  assert.equal(isMethodName('display.beginFrame'), true);
  assert.equal(isMethodName('display.updateFrameImageLz4'), true);
  assert.equal(isMethodName('private.method'), false);
});

test('Plugin message transport exposes the firmware payload limit', () => {
  assert.deepEqual(PLUGIN_MESSAGE_PROFILE, { maxPayloadBytes: 81901 });
  assert.equal(CAPABILITIES.pluginMessaging, PLUGIN_MESSAGE_PROFILE);
});

test('reference device profile matches the public glasses geometry', () => {
  assert.deepEqual(
    {
      width: DEVICE_PROFILE.width,
      height: DEVICE_PROFILE.height,
      refreshHz: DEVICE_PROFILE.refreshHz,
      pixelFormat: DEVICE_PROFILE.pixelFormat,
    },
    { width: 600, height: 350, refreshHz: 30, pixelFormat: 'GRAY_4' },
  );
});

test('Scene transport limits match App legacy and atomic frame channels', () => {
  assert.equal(SCENE_TRANSPORT_PROFILE.maxPayloadBytes, 81901);
  assert.deepEqual(SCENE_TRANSPORT_PROFILE.channels.gray4, { id: 6, headerBytes: 10 });
  assert.deepEqual(SCENE_TRANSPORT_PROFILE.channels.gray4Lz4, { id: 7, headerBytes: 14, maxDecodedBytes: 81901 });
  assert.deepEqual(SCENE_TRANSPORT_PROFILE.channels.frameBegin, { id: 8, headerBytes: 6, maxTiles: 256 });
  assert.deepEqual(SCENE_TRANSPORT_PROFILE.channels.frameTileLz4, { id: 9, headerBytes: 20, maxDecodedBytes: 81901 });
  assert.deepEqual(SCENE_TRANSPORT_PROFILE.channels.frameStatus, { id: 0x0104, payloadBytes: 20 });
});
