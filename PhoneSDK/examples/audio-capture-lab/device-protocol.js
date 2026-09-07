export const AUDIO_LAB_STATE_CHANNEL = 0x414c;

const stateIds = Object.freeze({
  ready: 0,
  starting: 1,
  recording: 2,
  streaming: 3,
  stopping: 4,
  playback: 5,
  error: 6,
  unavailable: 7,
});

const modeIds = Object.freeze({ none: 0, recording: 1, stream: 2 });
const pickupIds = Object.freeze({
  unchanged: 0,
  frontFixed: 1,
  meetingAuto: 2,
  nonWearerFocus: 3,
  frontBalanced: 4,
  frontFocus: 5,
});
const profileIds = Object.freeze({ none: 0, interactive: 1, balanced: 2, reliable: 3, custom: 4 });
const voiceIds = Object.freeze({ original: 0, cute: 1, deep: 2, overlord: 3 });
const errorIds = Object.freeze({
  none: 0,
  INVALID_REQUEST: 1,
  PERMISSION_DENIED: 2,
  AUDIO_BUSY: 3,
  NO_AUDIO: 4,
  BUFFER_OVERFLOW: 5,
  DEVICE_DISCONNECTED: 6,
  CAPABILITY_UNAVAILABLE: 7,
  TIMEOUT: 8,
  RUNTIME_CLOSED: 9,
  RUNTIME_REPLACED: 10,
  INTERNAL_ERROR: 11,
});

function boundedInteger(value, max) {
  const number = Number.isFinite(Number(value)) ? Math.round(Number(value)) : 0;
  return Math.max(0, Math.min(max, number));
}

function writeU16(bytes, offset, value) {
  const safe = boundedInteger(value, 0xffff);
  bytes[offset] = safe >>> 8;
  bytes[offset + 1] = safe;
}

function writeU32(bytes, offset, value) {
  const safe = boundedInteger(value, 0xffffffff);
  bytes[offset] = safe >>> 24;
  bytes[offset + 1] = safe >>> 16;
  bytes[offset + 2] = safe >>> 8;
  bytes[offset + 3] = safe;
}

export function encodeAudioLabState(model = {}) {
  const bytes = new Uint8Array(23);
  bytes[0] = 1;
  bytes[1] = stateIds[model.state] ?? stateIds.error;
  bytes[2] = modeIds[model.mode] ?? modeIds.none;
  bytes[3] = pickupIds[model.pickupMode] ?? pickupIds.unchanged;
  bytes[4] = profileIds[model.profile] ?? profileIds.none;
  bytes[5] = model.noiseReduction ? 1 : 0;
  bytes[6] = voiceIds[model.voice] ?? voiceIds.original;
  bytes[7] = errorIds[model.errorCode ?? 'none'] ?? 0xff;
  writeU32(bytes, 8, model.elapsedMs);
  writeU32(bytes, 12, model.chunkCount);
  writeU32(bytes, 16, model.droppedFrameCount);
  writeU16(bytes, 20, model.queueLatencyMs);
  bytes[22] = model.language === 'zh' ? 1 : 0;
  return bytes;
}
