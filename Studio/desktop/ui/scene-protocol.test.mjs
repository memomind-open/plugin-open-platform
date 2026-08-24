import test from 'node:test';
import assert from 'node:assert/strict';

import {
  decodeGlassesMessage,
  decodeDeviceMessage,
  encodePluginBridgeEventData,
  encodeSceneMessage,
  PLUGIN_TRANSPORT,
  SCENE_CHANNELS,
} from './scene-protocol.js';

test('desktop routes glasses plugin messages only on uplink command 0x29', () => {
  assert.deepEqual(PLUGIN_TRANSPORT, {
    service: 0x0f,
    phoneToGlassesCommand: 0x28,
    glassesToPhoneCommand: 0x29,
  });
  const encoded = {
    service: 0x0f,
    command: 0x29,
    channel: 0x4648,
    dataBase64: 'AQIDBA==',
  };
  assert.deepEqual(decodeGlassesMessage(encoded), {
    kind: 'plugin', channel: 0x4648, payload: Uint8Array.of(1, 2, 3, 4),
  });
  assert.throws(
    () => decodeGlassesMessage({ ...encoded, command: 0x28 }),
    /unexpected glasses-to-phone command/,
  );
  assert.deepEqual(encodePluginBridgeEventData(decodeGlassesMessage(encoded)), {
    channel: 0x4648,
    dataBase64: 'AQIDBA==',
  });
});

test('desktop Scene codec matches Aphrodite LZ4 Channel 7 framing', () => {
  const encoded = encodeSceneMessage('display.updateImageLz4', {
    x: 304, y: 16, width: 4, height: 2, stride: 2, decodedSize: 4,
    dataBase64: Buffer.from([0x40, 1, 2, 3, 4]).toString('base64'),
  });
  assert.equal(encoded.channel, SCENE_CHANNELS.gray4Lz4);
  assert.deepEqual([...encoded.payload.slice(0, 14)], [
    1, 48, 0, 16, 0, 4, 0, 2, 0, 2, 0, 0, 0, 4,
  ]);
});

test('desktop Scene codec frames atomic tile metadata in big endian', () => {
  const encoded = encodeSceneMessage('display.updateFrameImageLz4', {
    frameId: 0x01020304, tileIndex: 2,
    x: 0, y: 175, width: 200, height: 175, stride: 100, decodedSize: 17500,
    dataBase64: Buffer.from([0xf0, 1]).toString('base64'),
  });
  assert.equal(encoded.channel, SCENE_CHANNELS.frameTileLz4);
  assert.deepEqual([...encoded.payload.slice(0, 6)], [1, 2, 3, 4, 0, 2]);
  assert.deepEqual(encoded.ack, { frameId: 0x01020304, tileIndex: 2, nextIndex: 3 });
});

test('desktop decodes web_bridge button and frame status messages', () => {
  const button = Uint8Array.of(1, 1, 0, 0, 0, 9, 0, 0, 0, 33, 0, 1, 0, 2);
  assert.deepEqual(decodeDeviceMessage(SCENE_CHANNELS.button, button), {
    kind: 'event', name: 'device.button', sequence: 9, timestampMs: 33,
    data: { button: 'primary', action: 'double' },
  });

  const status = Uint8Array.of(
    1, 5, 0, 0, 0, 10, 0, 0, 0, 20,
    1, 2, 3, 4, 0, 2, 0, 3, 0, 1,
  );
  assert.deepEqual(decodeDeviceMessage(SCENE_CHANNELS.frameStatus, status), {
    kind: 'frameStatus', sequence: 10, timestampMs: 20,
    frameId: 0x01020304, tileIndex: 2, nextIndex: 3, status: 0, complete: true,
  });
});
