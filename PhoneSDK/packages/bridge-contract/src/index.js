export const BRIDGE_VERSION = '2.0';

export const DEVICE_PROFILE = Object.freeze({
  id: 'jhv3-reference-v1',
  width: 600,
  height: 350,
  refreshHz: 30,
  pixelFormat: 'GRAY_4',
  defaultFontPx: 17,
  largeFontPx: 20,
});

export const SCENE_TRANSPORT_PROFILE = Object.freeze({
  maxPayloadBytes: 81901,
  maxBase64Characters: 128 * 1024,
  channels: Object.freeze({
    clear: Object.freeze({ id: 1, headerBytes: 0 }),
    text: Object.freeze({ id: 2, headerBytes: 11 }),
    gray4: Object.freeze({ id: 6, headerBytes: 10 }),
    gray4Lz4: Object.freeze({ id: 7, headerBytes: 14, maxDecodedBytes: 81901 }),
    frameBegin: Object.freeze({ id: 8, headerBytes: 6, maxTiles: 256 }),
    frameTileLz4: Object.freeze({ id: 9, headerBytes: 20, maxDecodedBytes: 81901 }),
    frameStatus: Object.freeze({ id: 0x0104, payloadBytes: 20 }),
  }),
});

export const PLUGIN_MESSAGE_PROFILE = Object.freeze({
  maxPayloadBytes: 81901,
  uplinkEvent: 'plugin.message',
});

export const STREAM_PORT_MESSAGE_TYPE = 'gm-plugin:stream-port';
export const AUDIO_STREAM_PORT_KIND = 'audio.capture';
export const AUDIO_STREAM_ENVELOPE = Object.freeze({
  magic: 0x474d4155,
  version: 1,
  chunkType: 1,
  discontinuityFlag: 0x0001,
  baseHeaderBytes: 40,
  frameLengthBytes: 2,
  maxFramesPerChunk: 10,
  byteOrder: 'big-endian',
});

export const AUDIO_PROFILE = Object.freeze({
  codec: 'opus',
  sampleRate: 16000,
  channels: 1,
  noiseReduction: true,
  pickupModes: Object.freeze([
    'unchanged', 'frontFixed', 'meetingAuto', 'nonWearerFocus',
    'frontBalanced', 'frontFocus',
  ]),
  transport: 'message-port',
  payload: 'binary-envelope-v1',
  envelope: AUDIO_STREAM_ENVELOPE,
  frameDurationMs: 20,
  chunkDurationMs: Object.freeze({ min: 20, max: 200 }),
  maxQueueMs: Object.freeze({ min: 100, max: 5000 }),
  maxDurationMs: Object.freeze({ min: 1000, max: 60 * 60 * 1000, unlimited: true }),
  defaultProfile: 'interactive',
  profiles: Object.freeze({
    interactive: Object.freeze({
      chunkDurationMs: 40,
      maxQueueMs: 200,
      overflowStrategy: 'drop-oldest',
    }),
    balanced: Object.freeze({
      chunkDurationMs: 100,
      maxQueueMs: 500,
      overflowStrategy: 'drop-oldest',
    }),
    reliable: Object.freeze({
      chunkDurationMs: 100,
      maxQueueMs: 3000,
      overflowStrategy: 'error',
    }),
  }),
  overflowStrategies: Object.freeze(['drop-oldest', 'drop-newest', 'error']),
});

export const FILE_PROFILE = Object.freeze({
  persistent: true,
  maxFileBytes: 400 * 1024 * 1024,
  maxTotalBytes: 400 * 1024 * 1024,
  maxPickFiles: 20,
  readTransport: 'binary-stream',
  supportsRanges: true,
});

export const METHOD_NAMES = Object.freeze([
  'runtime.ready',
  'runtime.ping',
  'runtime.getBridgeVersion',
  'runtime.getCapabilities',
  'runtime.getLifecycleState',
  'storage.get',
  'storage.set',
  'storage.remove',
  'storage.clear',
  'files.pick',
  'files.list',
  'files.stat',
  'files.openRead',
  'files.getUsage',
  'files.delete',
  'display.createPage',
  'display.rebuildPage',
  'display.updateText',
  'display.updateImage',
  'display.updateImageLz4',
  'display.beginFrame',
  'display.updateFrameImageLz4',
  'display.closePage',
  'device.getInfo',
  'device.subscribeEvents',
  'device.unsubscribeEvents',
  'plugin.sendMessage',
  'audio.openCapture',
  'audio.stopCapture',
  'location.getCurrentPosition',
  'location.watchPosition',
  'location.clearWatch',
]);

export const EVENT_NAMES = Object.freeze([
  'device.button',
  'device.imuGesture',
  'device.rawImu',
  'device.connection',
  'plugin.message',
  'audio.captureState',
  'location.position',
  'location.error',
  'runtime.lifecycleChanged',
]);

export const ERROR_CODES = Object.freeze([
  'INVALID_REQUEST',
  'PAYLOAD_TOO_LARGE',
  'UNAUTHORIZED',
  'PERMISSION_DENIED',
  'FILE_NOT_FOUND',
  'STALE_RUNTIME',
  'METHOD_NOT_FOUND',
  'RATE_LIMITED',
  'BUSY',
  'AUDIO_BUSY',
  'NO_AUDIO',
  'BUFFER_OVERFLOW',
  'QUOTA_EXCEEDED',
  'TIMEOUT',
  'DEVICE_DISCONNECTED',
  'CAPABILITY_UNAVAILABLE',
  'RUNTIME_CLOSED',
  'RUNTIME_REPLACED',
  'INTERNAL_ERROR',
]);

export const CAPABILITIES = Object.freeze({
  display: Object.freeze(['text', 'gray4', 'clear', 'gray4-lz4', 'atomic-framed-lz4']),
  events: Object.freeze(['button', 'imuGesture', 'rawImu', 'connection']),
  rawImuDefaultEnabled: false,
  pluginMessaging: PLUGIN_MESSAGE_PROFILE,
  files: FILE_PROFILE,
});

export function isMethodName(value) {
  return METHOD_NAMES.includes(value);
}
