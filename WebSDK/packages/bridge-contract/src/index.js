export const BRIDGE_VERSION = '1.0';

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

export const AUDIO_PROFILE = Object.freeze({
  codec: 'opus',
  sampleRate: 16000,
  channels: 1,
  maxDurationMs: 15000,
  maxFrames: 750,
  maxOpusBytes: 64 * 1024,
  streamEvent: 'audio.frames',
  streamIsLossyObservation: true,
  pickupModes: Object.freeze([
    'unchanged', 'frontFixed', 'meetingAuto', 'nonWearerFocus',
    'frontBalanced', 'frontFocus',
  ]),
  voices: Object.freeze(['original', 'cute', 'deep', 'overlord']),
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
  'audio.configure',
  'audio.startRecording',
  'audio.stopRecording',
  'audio.playRecording',
  'audio.stopPlayback',
]);

export const EVENT_NAMES = Object.freeze([
  'device.button',
  'device.imuGesture',
  'device.rawImu',
  'device.connection',
  'plugin.message',
  'audio.frames',
  'audio.state',
  'audio.playbackState',
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
