export const DISPLAY_CONTROL_COMMAND_CHANNEL = 0x4443;
export const DISPLAY_CONTROL_STATE_CHANNEL = 0x4444;
export const DISPLAY_CONTROL_PROTOCOL_VERSION = 1;

export const displayControlOperation = Object.freeze({
  query: 1,
  setScreen: 2,
  setBrightness: 3,
  setDistance: 4,
  setHeight: 5,
  restore: 6,
});

const statusNames = Object.freeze({
  0: 'OK',
  1: 'INVALID_ARGUMENT',
  2: 'NOT_SUPPORTED',
  3: 'BUSY',
  4: 'OUT_OF_MEMORY',
  5: 'IO_ERROR',
  6: 'NOT_PERMITTED',
  7: 'INVALID_STATE',
  8: 'VERSION_MISMATCH',
  255: 'UNKNOWN_ERROR',
});

function requireByte(value, name) {
  if (!Number.isInteger(value) || value < 0 || value > 0xff) {
    throw new TypeError(`${name} must be an unsigned byte`);
  }
  return value;
}

function requireRequestId(value) {
  if (!Number.isInteger(value) || value < 0 || value > 0xffffffff) {
    throw new TypeError('requestId must be an unsigned 32-bit integer');
  }
  return value;
}

function validateOperationValue(operation, value) {
  if (!Object.values(displayControlOperation).includes(operation)) {
    throw new TypeError('Unknown display-control operation');
  }
  if (operation === displayControlOperation.setScreen && value > 1) {
    throw new RangeError('Screen value must be 0 or 1');
  }
  if ((operation === displayControlOperation.query ||
       operation === displayControlOperation.restore) && value !== 0) {
    throw new RangeError('Query and restore values must be zero');
  }
  if (operation === displayControlOperation.setBrightness && (value < 1 || value > 10)) {
    throw new RangeError('Brightness must be between 1 and 10');
  }
  if ((operation === displayControlOperation.setDistance ||
       operation === displayControlOperation.setHeight) && value > 8) {
    throw new RangeError('Optical level must be between 0 and 8');
  }
}

export function encodeDisplayControlCommand({
  requestId,
  operation,
  value = 0,
  previewOnly = false,
}) {
  requireRequestId(requestId);
  requireByte(operation, 'operation');
  requireByte(value, 'value');
  validateOperationValue(operation, value);
  const data = new Uint8Array(8);
  const view = new DataView(data.buffer);
  data[0] = DISPLAY_CONTROL_PROTOCOL_VERSION;
  data[1] = operation;
  data[2] = value;
  data[3] = previewOnly ? 1 : 0;
  view.setUint32(4, requestId, false);
  return data;
}

export function decodeDisplayControlState(message) {
  if (message?.channel !== DISPLAY_CONTROL_STATE_CHANNEL ||
      !(message.data instanceof Uint8Array) || message.data.byteLength !== 12) return null;
  const data = message.data;
  if (data[0] !== DISPLAY_CONTROL_PROTOCOL_VERSION || data[1] !== 1 ||
      data[4] < 1 || data[4] > 10 || data[5] > 8 || data[6] > 8) return null;
  const flags = data[3];
  return {
    statusCode: data[2],
    status: statusNames[data[2]] ?? statusNames[255],
    screenOn: (flags & 0x01) !== 0,
    autoBrightnessBlocked: (flags & 0x02) !== 0,
    previewOnly: (flags & 0x04) !== 0,
    requestedScreenOn: (flags & 0x08) !== 0,
    restoreInProgress: (flags & 0x10) !== 0,
    brightness: data[4],
    distance: data[5],
    height: data[6],
    lastOperation: data[7],
    requestId: new DataView(data.buffer, data.byteOffset, data.byteLength).getUint32(8, false),
  };
}
