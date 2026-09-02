import assert from 'node:assert/strict';
import test from 'node:test';

import {
  SCROLL_SPEED_PROFILE_VERSION,
  chapterByteOffsets,
  chapterIndexAt,
  decodeNovel,
  findChapters,
  migrateScrollSpeed,
  normalizeNovelText,
  trimWindowEnd,
} from '../../examples/novel-reader/reader-core.js';
import {
  decodeReaderEvent,
  encodeChapter,
  encodeControl,
  encodeOpen,
  encodeWindow,
  readerControl,
} from '../../examples/novel-reader/reader-protocol.js';

test('normalizes UTF-8 TXT content and detects chapters', () => {
  const source = new TextEncoder().encode('\uFEFF前言\r\n\r\n第一章 开始\r\n你好，世界。\r\n第二章 继续\r\n正文');
  const decoded = decodeNovel(source);
  assert.equal(decoded.encoding, 'utf-8');
  assert.equal(decoded.text, '前言\n\n第一章 开始\n你好，世界。\n第二章 继续\n正文');
  const chapters = chapterByteOffsets(decoded.text, findChapters(decoded.text));
  assert.deepEqual(chapters.map((chapter) => chapter.title), ['前言', '第一章 开始', '第二章 继续']);
  assert.equal(chapterIndexAt(chapters, chapters[1].byteOffset), 1);
});

test('detects UTF-16 LE without a BOM', () => {
  const bytes = new Uint8Array(Buffer.from('第一章\r\n内容', 'utf16le'));
  const decoded = decodeNovel(bytes);
  assert.equal(decoded.encoding, 'utf-16le');
  assert.equal(decoded.text, '第一章\n内容');
});

test('normalization removes NUL characters and bounds empty lines', () => {
  assert.equal(normalizeNovelText('\u0000A\n\n\n\n\nB'), 'A\n\n\nB');
});

test('text windows end on UTF-8 and paragraph boundaries', () => {
  const bytes = new TextEncoder().encode(`${'小说内容'.repeat(40)}\n${'下一段'.repeat(80)}`);
  const end = trimWindowEnd(bytes, 0, 700);
  assert.equal(bytes[end - 1], 0x0a);
  assert.doesNotThrow(() => new TextDecoder('utf-8', { fatal: true }).decode(bytes.subarray(0, end)));
});

test('doubles legacy scroll presets exactly once', () => {
  assert.equal(migrateScrollSpeed(4), 8);
  assert.equal(migrateScrollSpeed(8), 16);
  assert.equal(migrateScrollSpeed(12), 24);
  assert.equal(migrateScrollSpeed(24, SCROLL_SPEED_PROFILE_VERSION), 24);
});

test('encodes commands and decodes glasses events in network byte order', () => {
  const open = encodeOpen({ session: 0x01020304, totalBytes: 9000, offset: 120, fontMode: 1, speed: 16 });
  assert.equal(open.length, 16);
  assert.deepEqual([...open.slice(2, 6)], [1, 2, 3, 4]);
  assert.equal(open[15], 16);
  const control = encodeControl(0x01020304, readerControl.pageDown, 0);
  assert.equal(control[6], 6);
  const chapter = encodeChapter(0x01020304, '第三章 山雨欲来');
  assert.equal(chapter[1], 5);
  assert.deepEqual([...chapter.slice(2, 6)], [1, 2, 3, 4]);
  assert.equal(new TextDecoder().decode(chapter.slice(6)), '第三章 山雨欲来');
  const window = encodeWindow({ session: 7, offset: 10, final: true, bytes: Uint8Array.of(65, 66) });
  assert.deepEqual([...window.slice(-2)], [65, 66]);

  const need = Uint8Array.of(1, 1, 0, 0, 0, 7, 0, 0, 0, 10, 0x30, 0x00);
  assert.deepEqual(decodeReaderEvent(need), {
    type: 'needWindow', session: 7, offset: 10, maxBytes: 12_288,
  });
  const progress = Uint8Array.of(1, 2, 0, 0, 0, 7, 0, 0, 0, 20, 1, 0);
  assert.deepEqual(decodeReaderEvent(progress), {
    type: 'progress', session: 7, offset: 20, playing: true, fontMode: 0,
  });
});
