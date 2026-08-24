export const SCENE_CHANNELS = Object.freeze({
  clear: 1,
  text: 2,
  gray4: 6,
  gray4Lz4: 7,
  frameBegin: 8,
  frameTileLz4: 9,
  button: 0x0100,
  imuGesture: 0x0101,
  rawImu: 0x0102,
  connection: 0x0103,
  frameStatus: 0x0104,
  ping: 0x7ffe,
});

export const PLUGIN_TRANSPORT = Object.freeze({
  service: 0x0f,
  phoneToGlassesCommand: 0x28,
  glassesToPhoneCommand: 0x29,
});

export const DISPLAY_CAPABILITIES = Object.freeze([
  'text', 'gray4', 'clear', 'gray4-lz4', 'atomic-framed-lz4',
]);

export const EVENT_CAPABILITIES = Object.freeze([
  'button', 'imuGesture', 'rawImu', 'connection',
]);

const MAX_PAYLOAD_BYTES = 81901;
const MAX_FRAME_TILES = 256;

export function encodeSceneMessage(method, params = {}) {
  switch (method) {
    case 'display.closePage':
      return { channel: SCENE_CHANNELS.clear, payload: Uint8Array.of(1) };
    case 'display.updateText':
      return encodeText(params);
    case 'display.updateImage':
      return encodeImage(params);
    case 'display.updateImageLz4':
      return encodeImageLz4(params);
    case 'display.beginFrame':
      return encodeFrameBegin(params);
    case 'display.updateFrameImageLz4':
      return encodeFrameTile(params);
    default:
      throw protocolError('METHOD_NOT_FOUND', `No Scene codec for ${method}`);
  }
}

export function decodeDeviceMessage(channel, payload) {
  if (!(payload instanceof Uint8Array)) throw protocolError('INVALID_REQUEST', 'device payload must be bytes');
  if (channel === SCENE_CHANNELS.ping) return { kind: 'ping', payload };
  if (channel === SCENE_CHANNELS.frameStatus) return decodeFrameStatus(payload);
  const definitions = {
    [SCENE_CHANNELS.button]: { type: 1, length: 14, name: 'device.button' },
    [SCENE_CHANNELS.imuGesture]: { type: 2, length: 13, name: 'device.imuGesture' },
    [SCENE_CHANNELS.rawImu]: { type: 3, length: 26, name: 'device.rawImu' },
    [SCENE_CHANNELS.connection]: { type: 4, length: 11, name: 'device.connection' },
  };
  const definition = definitions[channel];
  if (!definition) return { kind: 'plugin', channel, payload };
  if (payload.length !== definition.length || payload[0] !== 1 || payload[1] !== definition.type) {
    throw protocolError('INVALID_REQUEST', `invalid ${definition.name} payload`);
  }
  const view = dataView(payload);
  const sequence = view.getUint32(2, false);
  const timestampMs = view.getUint32(6, false);
  let data;
  if (channel === SCENE_CHANNELS.button) {
    const buttons = ['unknown', 'primary'];
    const actions = ['unknown', 'single', 'double', 'long', 'veryLong', 'release'];
    const button = buttons[view.getUint16(10, false)];
    const action = actions[view.getUint16(12, false)];
    if (!button || !action) throw protocolError('INVALID_REQUEST', 'unknown button event enum');
    data = { button, action };
  } else if (channel === SCENE_CHANNELS.imuGesture) {
    const gestures = ['none', 'nod', 'headRaise', 'headLower', 'shake', 'headRaiseTimeout', 'headLowerTimeout', 'left', 'right'];
    const gesture = gestures[view.getUint16(10, false)];
    if (!gesture) throw protocolError('INVALID_REQUEST', 'unknown gesture event enum');
    data = { gesture, active: decodeBool(payload[12]) };
  } else if (channel === SCENE_CHANNELS.rawImu) {
    data = {
      accelRaw: { x: view.getInt16(10, false), y: view.getInt16(12, false), z: view.getInt16(14, false) },
      gyroRaw: { x: view.getInt16(16, false), y: view.getInt16(18, false), z: view.getInt16(20, false) },
      temperatureRaw: view.getInt16(22, false),
      pitchDegrees: view.getInt16(24, false),
    };
  } else {
    data = { connected: decodeBool(payload[10]) };
  }
  return { kind: 'event', name: definition.name, sequence, timestampMs, data };
}

export function decodeGlassesMessage(encoded) {
  if (!encoded || typeof encoded !== 'object') {
    throw protocolError('INVALID_REQUEST', 'glasses message must be an object');
  }
  if (encoded.service !== PLUGIN_TRANSPORT.service) {
    throw protocolError('INVALID_REQUEST', `unexpected plugin service ${encoded.service}`);
  }
  if (encoded.command !== PLUGIN_TRANSPORT.glassesToPhoneCommand) {
    throw protocolError('INVALID_REQUEST', `unexpected glasses-to-phone command ${encoded.command}`);
  }
  return decodeDeviceMessage(encoded.channel, decodeBase64(encoded.dataBase64));
}

export function encodePluginBridgeEventData(message) {
  if (message?.kind !== 'plugin') {
    throw protocolError('INVALID_REQUEST', 'plugin message is required');
  }
  uint(message.channel, 16, 'channel');
  if (!(message.payload instanceof Uint8Array) || message.payload.length === 0) {
    throw protocolError('INVALID_REQUEST', 'plugin payload must be non-empty bytes');
  }
  if (message.payload.length > MAX_PAYLOAD_BYTES) {
    throw protocolError('PAYLOAD_TOO_LARGE', `plugin payload exceeds ${MAX_PAYLOAD_BYTES} bytes`);
  }
  return { channel: message.channel, dataBase64: encodeBase64(message.payload) };
}

export function decodeBase64(value) {
  if (typeof value !== 'string' || value.length === 0 || value.length % 4 !== 0 ||
      !/^[A-Za-z0-9+/]*={0,2}$/.test(value)) {
    throw protocolError('INVALID_REQUEST', 'dataBase64 must be valid standard Base64');
  }
  try { return Uint8Array.from(atob(value), (character) => character.charCodeAt(0)); }
  catch { throw protocolError('INVALID_REQUEST', 'dataBase64 must be valid standard Base64'); }
}

function encodeBase64(value) {
  let binary = '';
  for (let offset = 0; offset < value.length; offset += 0x8000) {
    binary += String.fromCharCode(...value.subarray(offset, offset + 0x8000));
  }
  return btoa(binary);
}

function encodeText(params) {
  const id = uint(params.id, 8, 'id');
  const geometry = readGeometry(params, false);
  const border = uint(params.border, 8, 'border');
  const radius = uint(params.radius, 8, 'radius');
  if (typeof params.text !== 'string' || params.text.length === 0) {
    throw protocolError('INVALID_REQUEST', 'text must be a non-empty string');
  }
  const text = new TextEncoder().encode(params.text);
  return scene(SCENE_CHANNELS.text, concat(
    Uint8Array.of(id), u16(geometry.x), u16(geometry.y), u16(geometry.width),
    u16(geometry.height), Uint8Array.of(border, radius), text,
  ));
}

function encodeImage(params) {
  const geometry = readGeometry(params, true);
  const pixels = decodeBase64(params.dataBase64);
  if (pixels.length !== geometry.stride * geometry.height) {
    throw protocolError('INVALID_REQUEST', 'pixel length must equal stride * height');
  }
  return scene(SCENE_CHANNELS.gray4, concat(imageHeader(geometry), pixels));
}

function encodeImageLz4(params) {
  const geometry = readGeometry(params, true, true);
  const decodedSize = uint(params.decodedSize, 32, 'decodedSize');
  if (decodedSize !== geometry.stride * geometry.height || decodedSize > MAX_PAYLOAD_BYTES) {
    throw protocolError('PAYLOAD_TOO_LARGE', 'decodedSize must equal stride * height and fit the Scene limit');
  }
  const compressed = decodeBase64(params.dataBase64);
  return scene(SCENE_CHANNELS.gray4Lz4, concat(imageHeader(geometry), u32(decodedSize), compressed));
}

function encodeFrameBegin(params) {
  const frameId = uint(params.frameId, 32, 'frameId');
  const tileCount = uint(params.tileCount, 16, 'tileCount');
  if (tileCount < 1 || tileCount > MAX_FRAME_TILES) {
    throw protocolError('INVALID_REQUEST', `tileCount must be between 1 and ${MAX_FRAME_TILES}`);
  }
  return scene(SCENE_CHANNELS.frameBegin, concat(u32(frameId), u16(tileCount)), {
    frameId, tileIndex: 0xffff, nextIndex: 0,
  });
}

function encodeFrameTile(params) {
  const frameId = uint(params.frameId, 32, 'frameId');
  const tileIndex = uint(params.tileIndex, 16, 'tileIndex');
  if (tileIndex >= MAX_FRAME_TILES) throw protocolError('INVALID_REQUEST', 'tileIndex is out of range');
  const image = encodeImageLz4(params);
  return scene(SCENE_CHANNELS.frameTileLz4, concat(u32(frameId), u16(tileIndex), image.payload), {
    frameId, tileIndex, nextIndex: tileIndex + 1,
  });
}

function decodeFrameStatus(payload) {
  if (payload.length !== 20 || payload[0] !== 1 || payload[1] !== 5) {
    throw protocolError('INVALID_REQUEST', 'invalid frame status payload');
  }
  const view = dataView(payload);
  const status = payload[18];
  if (status > 8) throw protocolError('INVALID_REQUEST', `unknown device frame status ${status}`);
  return {
    kind: 'frameStatus',
    sequence: view.getUint32(2, false),
    timestampMs: view.getUint32(6, false),
    frameId: view.getUint32(10, false),
    tileIndex: view.getUint16(14, false),
    nextIndex: view.getUint16(16, false),
    status,
    complete: decodeBool(payload[19]),
  };
}

function readGeometry(params, withStride, strictStride = false) {
  const value = {
    x: uint(params.x, 16, 'x'), y: uint(params.y, 16, 'y'),
    width: positiveUint16(params.width, 'width'), height: positiveUint16(params.height, 'height'),
  };
  if (value.x + value.width > 0xffff || value.y + value.height > 0xffff) {
    throw protocolError('INVALID_REQUEST', 'image geometry exceeds uint16 bounds');
  }
  if (withStride) {
    value.stride = positiveUint16(params.stride, 'stride');
    const minimum = Math.ceil(value.width / 2);
    if (value.stride < minimum || (strictStride && value.stride !== minimum)) {
      throw protocolError('INVALID_REQUEST', `stride must ${strictStride ? 'equal' : 'be at least'} ${minimum}`);
    }
  }
  return value;
}

function imageHeader(value) {
  return concat(u16(value.x), u16(value.y), u16(value.width), u16(value.height), u16(value.stride));
}

function scene(channel, payload, ack = null) {
  if (payload.length === 0 || payload.length > MAX_PAYLOAD_BYTES) {
    throw protocolError('PAYLOAD_TOO_LARGE', `Scene payload exceeds ${MAX_PAYLOAD_BYTES} bytes`);
  }
  return { channel, payload, ack };
}

function uint(value, bits, name) {
  const maximum = bits === 32 ? 0xffffffff : (2 ** bits) - 1;
  if (!Number.isInteger(value) || value < 0 || value > maximum) {
    throw protocolError('INVALID_REQUEST', `${name} must be uint${bits}`);
  }
  return value;
}

function positiveUint16(value, name) {
  uint(value, 16, name);
  if (value === 0) throw protocolError('INVALID_REQUEST', `${name} must be positive`);
  return value;
}

function u16(value) { return Uint8Array.of((value >>> 8) & 0xff, value & 0xff); }
function u32(value) { return Uint8Array.of((value >>> 24) & 0xff, (value >>> 16) & 0xff, (value >>> 8) & 0xff, value & 0xff); }

function concat(...parts) {
  const output = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
  let offset = 0;
  for (const part of parts) { output.set(part, offset); offset += part.length; }
  return output;
}

function dataView(payload) { return new DataView(payload.buffer, payload.byteOffset, payload.byteLength); }

function decodeBool(value) {
  if (value === 0) return false;
  if (value === 1) return true;
  throw protocolError('INVALID_REQUEST', 'boolean byte must be 0 or 1');
}

function protocolError(code, message) { return Object.assign(new Error(message), { code }); }
