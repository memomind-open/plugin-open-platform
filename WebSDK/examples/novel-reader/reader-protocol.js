export const READER_CHANNEL = 0x4e52;
export const READER_EVENT_CHANNEL = 0x4e53;
export const PROTOCOL_VERSION = 1;
const MAX_CHAPTER_TITLE_BYTES = 320;

export const readerControl = Object.freeze({
  play: 1,
  pause: 2,
  lineUp: 3,
  lineDown: 4,
  pageUp: 5,
  pageDown: 6,
  setFont: 7,
  setSpeed: 8,
});

export const readerAction = Object.freeze({
  previousChapter: 1,
  nextChapter: 2,
  bookmark: 3,
});

export function encodeOpen({ session, totalBytes, offset, fontMode, speed }) {
  const data = new Uint8Array(16);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 1;
  view.setUint32(2, session);
  view.setUint32(6, totalBytes);
  view.setUint32(10, offset);
  data[14] = fontMode === 1 ? 1 : 0;
  data[15] = Math.max(2, Math.min(30, Math.round(speed)));
  return data;
}

export function encodeWindow({ session, offset, final, bytes }) {
  const data = new Uint8Array(11 + bytes.length);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 2;
  view.setUint32(2, session);
  view.setUint32(6, offset);
  data[10] = final ? 1 : 0;
  data.set(bytes, 11);
  return data;
}

export function encodeControl(session, control, value = 0) {
  const data = new Uint8Array(9);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 3;
  view.setUint32(2, session);
  data[6] = control;
  view.setUint16(7, value);
  return data;
}

export function encodeChapter(session, title) {
  const encoded = new TextEncoder().encode(String(title ?? ''));
  let length = Math.min(encoded.length, MAX_CHAPTER_TITLE_BYTES);
  while (length > 0 && length < encoded.length && (encoded[length] & 0xc0) === 0x80) {
    length -= 1;
  }
  const data = new Uint8Array(6 + length);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 5;
  view.setUint32(2, session);
  data.set(encoded.subarray(0, length), 6);
  return data;
}

export function encodeClose(session) {
  const data = new Uint8Array(6);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 4;
  view.setUint32(2, session);
  return data;
}

export function decodeReaderEvent(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length < 6 || bytes[0] !== PROTOCOL_VERSION) {
    throw new Error('Invalid novel reader event');
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const event = bytes[1];
  const session = view.getUint32(2);
  if (event === 1 && bytes.length === 12) {
    return { type: 'needWindow', session, offset: view.getUint32(6), maxBytes: view.getUint16(10) };
  }
  if (event === 2 && bytes.length === 12) {
    return {
      type: 'progress', session, offset: view.getUint32(6),
      playing: bytes[10] !== 0, fontMode: bytes[11],
    };
  }
  if (event === 3 && bytes.length === 11) {
    return { type: 'action', session, action: bytes[6], offset: view.getUint32(7) };
  }
  throw new Error('Unsupported novel reader event');
}
