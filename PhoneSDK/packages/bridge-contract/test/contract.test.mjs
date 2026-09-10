import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AUDIO_STREAM_ENVELOPE,
  AUDIO_STREAM_PORT_KIND,
  AUDIO_PROFILE,
  BRIDGE_VERSION,
  CAPABILITIES,
  DEVICE_PROFILE,
  ERROR_CODES,
  EVENT_NAMES,
  FILE_PROFILE,
  METHOD_NAMES,
  PLUGIN_MESSAGE_PROFILE,
  SCENE_TRANSPORT_PROFILE,
  STREAM_PORT_MESSAGE_TYPE,
  isMethodName,
} from '../src/index.js';

test('Bridge v2 contract contains unique methods and events', () => {
  assert.equal(BRIDGE_VERSION, '2.0');
  assert.equal(new Set(METHOD_NAMES).size, METHOD_NAMES.length);
  assert.equal(new Set(EVENT_NAMES).size, EVENT_NAMES.length);
  assert.equal(new Set(ERROR_CODES).size, ERROR_CODES.length);
  assert.equal(ERROR_CODES.includes('PERMISSION_DENIED'), true);
  assert.equal(ERROR_CODES.includes('FILE_NOT_FOUND'), true);
  assert.equal(ERROR_CODES.includes('RUNTIME_REPLACED'), true);
  assert.equal(isMethodName('display.updateText'), true);
  assert.equal(isMethodName('plugin.sendMessage'), true);
  assert.equal(isMethodName('display.beginFrame'), true);
  assert.equal(isMethodName('display.updateFrameImageLz4'), true);
  assert.equal(isMethodName('private.method'), false);
  assert.equal(EVENT_NAMES.includes('plugin.message'), true);
});

test('audio uses one capture session API and never exposes Base64 frame events', () => {
  assert.equal(METHOD_NAMES.includes('audio.openCapture'), true);
  assert.equal(METHOD_NAMES.includes('audio.stopCapture'), true);
  assert.equal(METHOD_NAMES.includes('audio.configure'), false);
  assert.equal(METHOD_NAMES.includes('audio.startRecording'), false);
  assert.equal(METHOD_NAMES.includes('audio.stopRecording'), false);
  assert.equal(EVENT_NAMES.includes('audio.frames'), false);
  assert.equal(EVENT_NAMES.includes('audio.captureState'), true);
  assert.equal(ERROR_CODES.includes('BUFFER_OVERFLOW'), true);
  assert.equal('modes' in AUDIO_PROFILE, false);
  assert.equal(AUDIO_PROFILE.transport, 'message-port');
  assert.equal(AUDIO_PROFILE.payload, 'binary-envelope-v1');
  assert.equal(AUDIO_PROFILE.envelope, AUDIO_STREAM_ENVELOPE);
  assert.equal(AUDIO_PROFILE.defaultProfile, 'interactive');
  assert.equal(STREAM_PORT_MESSAGE_TYPE, 'gm-plugin:stream-port');
  assert.equal(AUDIO_STREAM_PORT_KIND, 'audio.capture');
  assert.deepEqual(AUDIO_STREAM_ENVELOPE, {
    magic: 0x474d4155,
    version: 1,
    chunkType: 1,
    discontinuityFlag: 0x0001,
    baseHeaderBytes: 40,
    frameLengthBytes: 2,
    maxFramesPerChunk: 10,
    byteOrder: 'big-endian',
  });
  assert.deepEqual(AUDIO_PROFILE.profiles.interactive, {
    chunkDurationMs: 40,
    maxQueueMs: 200,
    overflowStrategy: 'drop-oldest',
  });
});

test('Plugin message transport exposes the firmware payload limit', () => {
  assert.deepEqual(PLUGIN_MESSAGE_PROFILE, {
    maxPayloadBytes: 81901,
    uplinkEvent: 'plugin.message',
  });
  assert.equal(CAPABILITIES.pluginMessaging, PLUGIN_MESSAGE_PROFILE);
});

test('persistent user-file capabilities expose App-owned limits', () => {
  assert.deepEqual(FILE_PROFILE, {
    persistent: true,
    maxFileBytes: 400 * 1024 * 1024,
    maxTotalBytes: 400 * 1024 * 1024,
    maxPickFiles: 20,
    readTransport: 'binary-stream',
    supportsRanges: true,
  });
  assert.equal(CAPABILITIES.files, FILE_PROFILE);
  for (const method of [
    'files.pick', 'files.list', 'files.stat', 'files.openRead',
    'files.getUsage', 'files.delete',
  ]) assert.equal(isMethodName(method), true);
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
