import {
  detectNovelEncoding,
  isChapterTitle,
  normalizeNovelSegment,
} from './reader-core.js';

export const NOVEL_INDEX_VERSION = 1;
const ENCODING_SAMPLE_BYTES = 64 * 1024;
const SOURCE_WINDOW_BYTES = 48 * 1024;
const MAX_PENDING_SOURCE_BYTES = 1024 * 1024;
const MAX_CHAPTER_ENTRIES = 4000;
const encoder = new TextEncoder();

export async function indexPersistentNovel(storage, file, requestedEncoding = 'auto', onProgress = () => {}) {
  requirePersistentFile(storage, file);
  const sampleLength = Math.min(file.size, ENCODING_SAMPLE_BYTES);
  const sample = await readRange(storage, file, 0, sampleLength);
  let encoding = detectNovelEncoding(sample, requestedEncoding);
  try {
    return await scanNovel(storage, file, encoding, onProgress);
  } catch (error) {
    if (requestedEncoding !== 'auto' || encoding !== 'utf-8') throw error;
    encoding = 'gb18030';
    return scanNovel(storage, file, encoding, onProgress);
  }
}

export async function readNovelWindow(storage, file, encoding, sourceOffset, maxUtf8Bytes) {
  requirePersistentFile(storage, file);
  if (!Number.isSafeInteger(sourceOffset) || sourceOffset < 0 || sourceOffset >= file.size) {
    throw new TypeError('sourceOffset is outside the novel');
  }
  if (!Number.isSafeInteger(maxUtf8Bytes) || maxUtf8Bytes < 1) {
    throw new TypeError('maxUtf8Bytes must be a positive integer');
  }
  const source = await readRange(
    storage,
    file,
    sourceOffset,
    Math.min(SOURCE_WINDOW_BYTES, file.size - sourceOffset),
  );
  const prefix = selectDecodedPrefix(
    source,
    encoding,
    maxUtf8Bytes,
    sourceOffset === 0,
    sourceOffset + source.length >= file.size,
  );
  return {
    sourceOffset,
    sourceLength: prefix.sourceLength,
    text: prefix.text,
    bytes: prefix.bytes,
    final: sourceOffset + prefix.sourceLength >= file.size,
  };
}

async function scanNovel(storage, file, encoding, onProgress) {
  const opened = await storage.openRead(file.fileId);
  validateOpenedStream(opened, file, 0, file.size);
  const reader = opened.stream.getReader();
  let pending = new Uint8Array();
  let pendingOffset = 0;
  let loadedBytes = 0;
  let logicalOffset = 0;
  let sawText = false;
  let continuingLine = false;
  const chapters = [];

  const consume = (bytes, final) => {
    const completeLength = final ? bytes.length : lastCompleteLineEnd(bytes, encoding);
    if (completeLength === 0) return bytes;
    const complete = bytes.subarray(0, completeLength);
    const decoded = new TextDecoder(encoding, { fatal: true }).decode(complete);
    const lines = splitSourceLines(complete, encoding);
    const normalized = normalizeNovelSegment(decoded, pendingOffset === 0);
    const decodedLines = normalized.split('\n');
    if (decodedLines.length < lines.length) {
      throw new Error('TXT line decoding did not preserve source boundaries');
    }
    for (let index = 0; index < lines.length; index += 1) {
      const line = decodedLines[index] ?? '';
      if (line.length > 0) sawText = true;
      if (!continuingLine && chapters.length < MAX_CHAPTER_ENTRIES && isChapterTitle(line)) {
        chapters.push({
          title: line.trim().slice(0, 80),
          sourceOffset: pendingOffset + lines[index].start,
          byteOffset: logicalOffset,
        });
      }
      logicalOffset += encoder.encode(line).length;
      if (lines[index].terminated) {
        logicalOffset += 1;
        continuingLine = false;
      }
    }
    pendingOffset += completeLength;
    return bytes.slice(completeLength);
  };

  const consumeLongLinePrefix = () => {
    const decoded = decodeValidPrefix(pending, MAX_PENDING_SOURCE_BYTES, encoding, pendingOffset === 0);
    if (!decoded) throw new Error('Could not decode the TXT stream');
    const text = normalizeNovelSegment(decoded.text, pendingOffset === 0);
    if (text.length > 0) sawText = true;
    logicalOffset += encoder.encode(text).length;
    pendingOffset += decoded.sourceLength;
    pending = pending.slice(decoded.sourceLength);
    continuingLine = true;
  };

  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (!(value instanceof Uint8Array) || value.length === 0) continue;
      loadedBytes += value.length;
      pending = concatenate(pending, value);
      pending = consume(pending, false);
      while (pending.length > MAX_PENDING_SOURCE_BYTES) consumeLongLinePrefix();
      onProgress(Math.min(loadedBytes, file.size), file.size);
    }
    pending = consume(pending, true);
  } finally {
    reader.releaseLock();
  }
  if (loadedBytes !== file.size || pending.length !== 0) {
    throw new Error('Host file stream ended before its declared size');
  }
  if (!sawText || logicalOffset === 0) throw new Error('TXT file has no readable text');
  if (chapters.length === 0 || chapters[0].byteOffset > 0) {
    chapters.unshift({ title: 'Full Text', sourceOffset: 0, byteOffset: 0 });
  }
  return {
    indexVersion: NOVEL_INDEX_VERSION,
    encoding,
    textBytes: logicalOffset,
    chapters,
  };
}

async function readRange(storage, file, offset, length) {
  if (length === 0) return new Uint8Array();
  const opened = await storage.openRead(file.fileId, { offset, length });
  validateOpenedStream(opened, file, offset, length);
  const output = new Uint8Array(length);
  const reader = opened.stream.getReader();
  let written = 0;
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (!(value instanceof Uint8Array) || value.length === 0 || written + value.length > length) {
        throw new Error('Host returned an invalid binary file stream');
      }
      output.set(value, written);
      written += value.length;
    }
  } finally {
    reader.releaseLock();
  }
  if (written !== length) throw new Error('Host file stream ended before its declared range');
  return output;
}

function validateOpenedStream(opened, file, offset, length) {
  if (opened?.fileId !== file.fileId || opened?.size !== file.size ||
      opened?.offset !== offset || opened?.length !== length || !opened?.stream?.getReader) {
    throw new Error('Host returned an invalid file stream ticket');
  }
}

function selectDecodedPrefix(source, encoding, maxUtf8Bytes, stripBom, final) {
  let low = 1;
  let high = source.length;
  let selected;
  while (low <= high) {
    const candidate = Math.floor((low + high) / 2);
    const decoded = decodeValidPrefix(source, candidate, encoding, stripBom);
    if (!decoded) {
      high = candidate - 1;
      continue;
    }
    const text = normalizeNovelSegment(decoded.text, stripBom);
    const bytes = encoder.encode(text);
    if (bytes.length <= maxUtf8Bytes) {
      selected = { sourceLength: decoded.sourceLength, text, bytes };
      low = candidate + 1;
    } else {
      high = candidate - 1;
    }
  }
  if (!selected) throw new Error('Could not decode a readable TXT window');
  if (!final && selected.sourceLength < source.length) {
    const lineEnd = lastCompleteLineEnd(
      source.subarray(0, selected.sourceLength),
      encoding,
      Math.floor(selected.sourceLength / 2),
    );
    if (lineEnd > 0) {
      const decoded = decodeValidPrefix(source, lineEnd, encoding, stripBom);
      if (decoded) {
        const text = normalizeNovelSegment(decoded.text, stripBom);
        selected = { sourceLength: decoded.sourceLength, text, bytes: encoder.encode(text) };
      }
    }
  }
  const safeLength = avoidSplitCrlf(source, selected.sourceLength, encoding);
  if (safeLength !== selected.sourceLength) {
    const decoded = decodeValidPrefix(source, safeLength, encoding, stripBom);
    if (!decoded) throw new Error('Could not align the TXT window boundary');
    const text = normalizeNovelSegment(decoded.text, stripBom);
    selected = { sourceLength: decoded.sourceLength, text, bytes: encoder.encode(text) };
  }
  return selected;
}

function avoidSplitCrlf(source, length, encoding) {
  if (encoding === 'utf-16le') {
    return length >= 2 && length + 1 < source.length &&
      source[length - 2] === 13 && source[length - 1] === 0 &&
      source[length] === 10 && source[length + 1] === 0
      ? length - 2 : length;
  }
  if (encoding === 'utf-16be') {
    return length >= 2 && length + 1 < source.length &&
      source[length - 2] === 0 && source[length - 1] === 13 &&
      source[length] === 0 && source[length + 1] === 10
      ? length - 2 : length;
  }
  return length > 0 && length < source.length &&
    source[length - 1] === 13 && source[length] === 10
    ? length - 1 : length;
}

function decodeValidPrefix(source, requestedLength, encoding, stripBom) {
  const alignment = encoding.startsWith('utf-16') ? 2 : 1;
  let length = requestedLength - (requestedLength % alignment);
  for (let attempt = 0; attempt < 4 && length > 0; attempt += 1, length -= alignment) {
    try {
      return {
        sourceLength: length,
        text: new TextDecoder(encoding, { fatal: true }).decode(source.subarray(0, length)),
        stripBom,
      };
    } catch {
      // Retry at an earlier character boundary.
    }
  }
  return null;
}

function lastCompleteLineEnd(bytes, encoding, minimum = 0) {
  const utf16 = encoding === 'utf-16le' || encoding === 'utf-16be';
  const step = utf16 ? 2 : 1;
  const cr = encoding === 'utf-16be' ? [0, 13] : utf16 ? [13, 0] : [13];
  const lf = encoding === 'utf-16be' ? [0, 10] : utf16 ? [10, 0] : [10];
  let last = 0;
  for (let index = 0; index + step <= bytes.length; index += step) {
    if (!matches(bytes, index, cr) && !matches(bytes, index, lf)) continue;
    let end = index + step;
    if (matches(bytes, index, cr) && matches(bytes, end, lf)) end += step;
    if (end >= minimum) last = end;
    if (end > index + step) index += step;
  }
  return last;
}

function splitSourceLines(bytes, encoding) {
  const utf16 = encoding === 'utf-16le' || encoding === 'utf-16be';
  const step = utf16 ? 2 : 1;
  const cr = encoding === 'utf-16be' ? [0, 13] : utf16 ? [13, 0] : [13];
  const lf = encoding === 'utf-16be' ? [0, 10] : utf16 ? [10, 0] : [10];
  const lines = [];
  let start = 0;
  for (let index = 0; index + step <= bytes.length; index += step) {
    if (!matches(bytes, index, cr) && !matches(bytes, index, lf)) continue;
    let end = index + step;
    if (matches(bytes, index, cr) && matches(bytes, end, lf)) end += step;
    lines.push({ start, end, terminated: true });
    start = end;
    if (end > index + step) index += step;
  }
  if (start < bytes.length) lines.push({ start, end: bytes.length, terminated: false });
  return lines;
}

function matches(bytes, offset, pattern) {
  return offset + pattern.length <= bytes.length &&
    pattern.every((value, index) => bytes[offset + index] === value);
}

function concatenate(left, right) {
  if (left.length === 0) return Uint8Array.from(right);
  const output = new Uint8Array(left.length + right.length);
  output.set(left);
  output.set(right, left.length);
  return output;
}

function requirePersistentFile(storage, file) {
  if (!storage?.openRead) throw new TypeError('storage.openRead is required');
  if (!file?.fileId || !Number.isSafeInteger(file.size) || file.size <= 0) {
    throw new TypeError('A non-empty persistent file is required');
  }
}
