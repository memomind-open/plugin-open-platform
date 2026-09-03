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
import {
  NOVEL_INDEX_VERSION,
  indexPersistentNovel,
  readNovelWindow,
} from '../../examples/novel-reader/reader-file.js';
import { ReaderStorage } from '../../examples/novel-reader/reader-storage.js';

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

test('indexes a binary stream and reads only the requested novel window', async () => {
  const source = new TextEncoder().encode(
    `前言\r\n第一章 开始\r\n${'小说正文内容。'.repeat(10_000)}\r\n第二章 继续\r\n结尾`,
  );
  const calls = [];
  const storage = {
    async openRead(fileId, { offset = 0, length = source.length - offset } = {}) {
      calls.push({ fileId, offset, length });
      const selected = source.subarray(offset, offset + length);
      let cursor = 0;
      return {
        fileId,
        size: source.length,
        offset,
        length,
        stream: new ReadableStream({
          pull(controller) {
            if (cursor >= selected.length) {
              controller.close();
              return;
            }
            const end = Math.min(selected.length, cursor + 4096);
            controller.enqueue(selected.slice(cursor, end));
            cursor = end;
          },
        }),
      };
    },
  };
  const file = {
    fileId: '1'.repeat(32),
    size: source.length,
  };
  const index = await indexPersistentNovel(storage, file);
  assert.equal(index.indexVersion, NOVEL_INDEX_VERSION);
  assert.equal(index.encoding, 'utf-8');
  assert.deepEqual(index.chapters.map(({ title }) => title), ['前言', '第一章 开始', '第二章 继续']);
  assert.ok(index.textBytes > 100_000);

  const window = await readNovelWindow(storage, file, index.encoding, 0, 12_288);
  assert.ok(window.bytes.length <= 12_288);
  assert.ok(window.sourceLength < source.length);
  assert.equal(window.final, false);
  assert.match(window.text, /^前言\n第一章 开始\n/u);
  assert.ok(calls.some(({ length }) => length === source.length));
  assert.equal(calls.some((call) => 'dataBase64' in call), false);
});

test('indexes a very long source line with bounded streaming carry-over', async () => {
  const source = new TextEncoder().encode('A'.repeat(1024 * 1024 + 65_537));
  const storage = {
    async openRead(fileId, { offset = 0, length = source.length - offset } = {}) {
      let cursor = 0;
      const selected = source.subarray(offset, offset + length);
      return {
        fileId,
        size: source.length,
        offset,
        length,
        stream: new ReadableStream({
          pull(controller) {
            if (cursor === selected.length) return controller.close();
            const end = Math.min(selected.length, cursor + 8192);
            controller.enqueue(selected.slice(cursor, end));
            cursor = end;
          },
        }),
      };
    },
  };
  const index = await indexPersistentNovel(storage, {
    fileId: '2'.repeat(32),
    size: source.length,
  });
  assert.equal(index.textBytes, source.length);
  assert.deepEqual(index.chapters, [{ title: 'Full Text', sourceOffset: 0, byteOffset: 0 }]);
});

test('restores metadata by stable fileId and deletes file state together', async () => {
  const fileId = 'a'.repeat(32);
  const file = {
    fileId,
    name: 'persistent-book.txt',
    size: 32 * 1024 * 1024,
    importedAt: '2026-09-03T00:00:00.000Z',
    extension: 'txt',
  };
  const files = new Map([[fileId, file]]);
  const state = new Map();
  const gm = {
    files: {
      list: async () => ({ files: [...files.values()] }),
      stat: async (id) => ({ file: files.get(id) }),
      pick: async () => ({ files: [file] }),
      openRead: async () => { throw new Error('not used'); },
      getUsage: async () => ({
        fileCount: files.size,
        totalBytes: [...files.values()].reduce((total, item) => total + item.size, 0),
        maxTotalBytes: 400 * 1024 * 1024,
      }),
      delete: async (id) => ({ deleted: files.delete(id) }),
    },
    storage: {
      get: async (key) => ({ value: state.get(key) ?? null }),
      set: async (key, value) => { state.set(key, structuredClone(value)); return { stored: true }; },
      remove: async (key) => ({ removed: state.delete(key) }),
    },
  };

  const firstRun = await new ReaderStorage(gm).open();
  const [initial] = await firstRun.listBooks();
  assert.equal(initial.fileId, fileId);
  assert.equal(initial.sourceBytes, 32 * 1024 * 1024);
  const chapters = Array.from({ length: 1200 }, (_, index) => ({
    title: `Chapter ${index} ${'title'.repeat(8)}`,
    sourceOffset: index * 1000,
    byteOffset: index * 1200,
  }));
  await firstRun.putMetadata({
    ...initial,
    indexVersion: NOVEL_INDEX_VERSION,
    chapters,
    progressOffset: 12_345,
    bookmarks: [{ offset: 12_345, sourceOffset: 10_000, windowOffset: 12_000 }],
  }, { writeIndex: true });

  const restarted = await new ReaderStorage(gm).open();
  const [restored] = await restarted.listBooks();
  assert.equal(restored.progressOffset, 12_345);
  assert.equal('chapters' in restored, false);
  const restoredBook = await restarted.getBook(fileId);
  assert.deepEqual(restoredBook.meta.chapters, chapters);
  assert.deepEqual(restoredBook.meta.bookmarks, [
    { offset: 12_345, sourceOffset: 10_000, windowOffset: 12_000 },
  ]);
  assert.ok(state.size > 2);

  assert.equal(await restarted.deleteBook(fileId), true);
  assert.equal(files.size, 0);
  assert.equal(state.size, 0);
});
