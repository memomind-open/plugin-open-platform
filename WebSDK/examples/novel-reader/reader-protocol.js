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
  setMode: 9,
  setPageInterval: 10,
});

export const readerMode = Object.freeze({
  scroll: 0,
  page: 1,
});

export const readerAction = Object.freeze({
  previousChapter: 1,
  nextChapter: 2,
  bookmark: 3,
});

export function encodeOpen({
  session,
  totalBytes,
  offset,
  fontMode,
  speed,
  mode = readerMode.scroll,
  pageIntervalSeconds = 10,
}) {
  const data = new Uint8Array(18);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 1;
  view.setUint32(2, session);
  view.setUint32(6, totalBytes);
  view.setUint32(10, offset);
  data[14] = fontMode === 1 ? 1 : 0;
  data[15] = Math.max(2, Math.min(30, Math.round(speed)));
  data[16] = mode === readerMode.page ? readerMode.page : readerMode.scroll;
  data[17] = Math.max(4, Math.min(20, Math.round(pageIntervalSeconds)));
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

export function encodeImageBegin({ session, imageId, width, height, tileCount }) {
  const data = new Uint8Array(16);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 6;
  view.setUint32(2, session);
  view.setUint32(6, imageId);
  view.setUint16(10, width);
  view.setUint16(12, height);
  view.setUint16(14, tileCount);
  return data;
}

export function encodeImageTile({ session, imageId, tileIndex, y, height, stride, final, bytes }) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== height * stride) {
    throw new TypeError('Image tile bytes do not match its geometry');
  }
  const data = new Uint8Array(19 + bytes.length);
  const view = new DataView(data.buffer);
  data[0] = PROTOCOL_VERSION;
  data[1] = 7;
  view.setUint32(2, session);
  view.setUint32(6, imageId);
  view.setUint16(10, tileIndex);
  view.setUint16(12, y);
  view.setUint16(14, height);
  view.setUint16(16, stride);
  data[18] = final ? 1 : 0;
  data.set(bytes, 19);
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
  if (event === 4 && bytes.length === 14) {
    return {
      type: 'needImage', session, imageId: view.getUint32(6),
      width: view.getUint16(10), height: view.getUint16(12),
    };
  }
  if (event === 5 && bytes.length === 14) {
    return {
      type: 'imageStatus', session, imageId: view.getUint32(6),
      tileIndex: view.getUint16(10), status: bytes[12], complete: bytes[13] !== 0,
    };
  }
  throw new Error('Unsupported novel reader event');
}
