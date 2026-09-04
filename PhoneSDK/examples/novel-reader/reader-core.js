export const DEFAULT_SCROLL_SPEED = 16;
export const SCROLL_SPEED_PROFILE_VERSION = 2;

let chapterLinePattern;

const chapterPattern = /^\s*(?:第[0-9零〇一二三四五六七八九十百千万两]+[章回卷节部篇]|序章|楔子|前言|后记|尾声|番外(?:篇)?|chapter\s+[0-9ivxlcdm]+).*$/gimu;

export function normalizeNovelText(value) {
  return value
    .replace(/^\uFEFF/u, '')
    .replace(/\r\n?/gu, '\n')
    .replace(/\u0000/gu, '')
    .replace(/[\t\u00a0]+/gu, ' ')
    .replace(/\n{4,}/gu, '\n\n\n')
    .trim();
}

export function normalizeNovelSegment(value, stripBom = false) {
  const normalized = value
    .replace(/\r\n?/gu, '\n')
    .replace(/\u0000/gu, '')
    .replace(/[\t\u00a0]/gu, ' ');
  return stripBom ? normalized.replace(/^\uFEFF/u, '') : normalized;
}

export function detectNovelEncoding(buffer, requestedEncoding = 'auto') {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  if (bytes.length === 0) throw new Error('TXT file is empty');
  if (requestedEncoding !== 'auto') return requestedEncoding;
  const structural = detectBom(bytes) ?? detectUtf16Pattern(bytes);
  if (structural) return structural;
  try {
    new TextDecoder('utf-8', { fatal: true }).decode(bytes, { stream: true });
    return 'utf-8';
  } catch {
    return 'gb18030';
  }
}

export function isChapterTitle(value) {
  chapterLinePattern ??= new RegExp(chapterPattern.source, 'iu');
  return chapterLinePattern.test(String(value ?? ''));
}

export function decodeNovel(buffer, requestedEncoding = 'auto') {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  if (bytes.length === 0) throw new Error('TXT file is empty');
  const bom = detectBom(bytes);
  const candidates = requestedEncoding === 'auto'
    ? [bom, detectUtf16Pattern(bytes), 'utf-8', 'gb18030'].filter(Boolean)
    : [requestedEncoding];
  let lastError;
  for (const encoding of [...new Set(candidates)]) {
    try {
      const text = new TextDecoder(encoding, { fatal: true }).decode(bytes);
      return { text: normalizeNovelText(text), encoding };
    } catch (error) {
      lastError = error;
    }
  }
  throw new Error(`Unable to decode TXT file${lastError?.message ? `: ${lastError.message}` : ''}`);
}

export function findChapters(text) {
  const chapters = [];
  chapterPattern.lastIndex = 0;
  for (const match of text.matchAll(chapterPattern)) {
    chapters.push({ title: match[0].trim().slice(0, 80), charOffset: match.index });
  }
  if (chapters.length === 0 || chapters[0].charOffset > 0) {
    chapters.unshift({ title: 'Full Text', charOffset: 0 });
  }
  return chapters;
}

export function chapterByteOffsets(text, chapters) {
  const targets = chapters.map((chapter) => chapter.charOffset);
  const offsets = new Array(targets.length).fill(0);
  let targetIndex = 0;
  let byteOffset = 0;
  for (let index = 0; index <= text.length && targetIndex < targets.length;) {
    while (targetIndex < targets.length && targets[targetIndex] <= index) {
      offsets[targetIndex] = byteOffset;
      targetIndex += 1;
    }
    if (index === text.length) break;
    const codePoint = text.codePointAt(index);
    byteOffset += utf8Length(codePoint);
    index += codePoint > 0xffff ? 2 : 1;
  }
  return chapters.map((chapter, index) => ({
    title: chapter.title,
    byteOffset: offsets[index],
  }));
}

export function chapterIndexAt(chapters, byteOffset) {
  let low = 0;
  let high = chapters.length - 1;
  while (low <= high) {
    const middle = (low + high) >> 1;
    if (chapters[middle].byteOffset <= byteOffset) low = middle + 1;
    else high = middle - 1;
  }
  return Math.max(0, high);
}

export function safeBookTitle(filename) {
  const title = String(filename ?? '').replace(/\.txt$/iu, '').trim();
  return title || 'Untitled Novel';
}

export function trimWindowEnd(bytes, start, requestedLength) {
  let end = Math.min(bytes.length, start + requestedLength);
  if (end === bytes.length) return end;
  while (end > start && (bytes[end] & 0xc0) === 0x80) end -= 1;
  const minimum = start + Math.floor(requestedLength / 2);
  for (let cursor = end - 1; cursor > minimum; cursor -= 1) {
    if (bytes[cursor] === 0x0a) return cursor + 1;
  }
  return end;
}

export function migrateScrollSpeed(speed, profileVersion = 1) {
  const numericSpeed = Number.isFinite(speed) ? Math.round(speed) : 8;
  const migratedSpeed = profileVersion >= SCROLL_SPEED_PROFILE_VERSION
    ? numericSpeed
    : numericSpeed * 2;
  return Math.max(2, Math.min(30, migratedSpeed));
}

function detectBom(bytes) {
  if (bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) return 'utf-8';
  if (bytes[0] === 0xff && bytes[1] === 0xfe) return 'utf-16le';
  if (bytes[0] === 0xfe && bytes[1] === 0xff) return 'utf-16be';
  return null;
}

function detectUtf16Pattern(bytes) {
  const sample = Math.min(bytes.length, 512);
  let evenZero = 0;
  let oddZero = 0;
  for (let index = 0; index < sample; index += 1) {
    if (bytes[index] !== 0) continue;
    if (index % 2 === 0) evenZero += 1;
    else oddZero += 1;
  }
  const threshold = Math.max(2, Math.floor(sample / 10));
  if (oddZero >= threshold && oddZero >= evenZero * 2) return 'utf-16le';
  if (evenZero >= threshold && evenZero >= oddZero * 2) return 'utf-16be';
  return null;
}

function utf8Length(codePoint) {
  if (codePoint <= 0x7f) return 1;
  if (codePoint <= 0x7ff) return 2;
  if (codePoint <= 0xffff) return 3;
  return 4;
}
