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
  encodeImageBegin,
  encodeImageTile,
  encodeOpen,
  encodeWindow,
  readerControl,
  readerMode,
} from '../../examples/novel-reader/reader-protocol.js';
import {
  EPUB_INDEX_VERSION,
  imageMarkerAt,
  indexPersistentEpub,
  readEpubImage,
  readEpubWindow,
} from '../../examples/novel-reader/reader-epub.js';
import {
  NOVEL_INDEX_VERSION,
  indexPersistentNovel,
  readNovelWindow,
  readNovelWindowSpan,
} from '../../examples/novel-reader/reader-file.js';
import { ReaderStorage } from '../../examples/novel-reader/reader-storage.js';
import { rgbaToGray4 } from '../../examples/novel-reader/reader-image.js';

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
  const open = encodeOpen({
    session: 0x01020304, totalBytes: 9000, offset: 120, fontMode: 1, speed: 16,
    mode: readerMode.page, pageIntervalSeconds: 12,
  });
  assert.equal(open.length, 18);
  assert.deepEqual([...open.slice(2, 6)], [1, 2, 3, 4]);
  assert.equal(open[15], 16);
  assert.equal(open[16], readerMode.page);
  assert.equal(open[17], 12);
  const control = encodeControl(0x01020304, readerControl.pageDown, 0);
  assert.equal(control[6], 6);
  const chapter = encodeChapter(0x01020304, '第三章 山雨欲来');
  assert.equal(chapter[1], 5);
  assert.deepEqual([...chapter.slice(2, 6)], [1, 2, 3, 4]);
  assert.equal(new TextDecoder().decode(chapter.slice(6)), '第三章 山雨欲来');
  const window = encodeWindow({ session: 7, offset: 10, final: true, bytes: Uint8Array.of(65, 66) });
  assert.deepEqual([...window.slice(-2)], [65, 66]);
  const begin = encodeImageBegin({ session: 7, imageId: 3, width: 600, height: 350, tileCount: 15 });
  assert.equal(begin.length, 16);
  assert.deepEqual([...begin.slice(6, 10)], [0, 0, 0, 3]);
  const tile = encodeImageTile({
    session: 7, imageId: 3, tileIndex: 2, y: 48, height: 2, stride: 3,
    final: false, bytes: Uint8Array.of(1, 2, 3, 4, 5, 6),
  });
  assert.equal(tile.length, 25);
  assert.deepEqual([...tile.slice(-6)], [1, 2, 3, 4, 5, 6]);

  const need = Uint8Array.of(1, 1, 0, 0, 0, 7, 0, 0, 0, 10, 0x30, 0x00);
  assert.deepEqual(decodeReaderEvent(need), {
    type: 'needWindow', session: 7, offset: 10, maxBytes: 12_288,
  });
  const progress = Uint8Array.of(1, 2, 0, 0, 0, 7, 0, 0, 0, 20, 1, 0);
  assert.deepEqual(decodeReaderEvent(progress), {
    type: 'progress', session: 7, offset: 20, playing: true, fontMode: 0,
  });
  const needImage = Uint8Array.of(1, 4, 0, 0, 0, 7, 0, 0, 0, 3, 2, 88, 1, 94);
  assert.deepEqual(decodeReaderEvent(needImage), {
    type: 'needImage', session: 7, imageId: 3, width: 600, height: 350,
  });
  const imageStatus = Uint8Array.of(1, 5, 0, 0, 0, 7, 0, 0, 0, 3, 0, 2, 0, 1);
  assert.deepEqual(decodeReaderEvent(imageStatus), {
    type: 'imageStatus', session: 7, imageId: 3, tileIndex: 2, status: 0, complete: true,
  });
});

test('indexes a reflowable EPUB and exposes its illustration as a logical page', async () => {
  const imageBytes = Uint8Array.of(137, 80, 78, 71, 13, 10, 26, 10);
  const epub = createStoredZip(new Map([
    ['mimetype', new TextEncoder().encode('application/epub+zip')],
    ['META-INF/container.xml', new TextEncoder().encode(
      '<?xml version="1.0"?><container><rootfiles><rootfile full-path="OEBPS/content.opf"/></rootfiles></container>')],
    ['OEBPS/content.opf', new TextEncoder().encode(`<?xml version="1.0"?>
      <package><metadata><dc:title xmlns:dc="urn:dc">Illustrated Demo</dc:title></metadata>
      <manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
      <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
      <item id="picture" href="images/picture.png" media-type="image/png"/></manifest>
      <spine><itemref idref="chapter"/></spine></package>`)],
    ['OEBPS/nav.xhtml', new TextEncoder().encode(
      '<html xmlns="http://www.w3.org/1999/xhtml"><body><nav epub:type="toc" xmlns:epub="urn:epub"><a href="chapter.xhtml">Demo Chapter</a></nav></body></html>')],
    ['OEBPS/chapter.xhtml', new TextEncoder().encode(
      '<html xmlns="http://www.w3.org/1999/xhtml"><body><h1>Chapter One</h1><p>Before image.</p><img src="images/picture.png" alt="Rabbit"/><p>After image.</p></body></html>')],
    ['OEBPS/images/picture.png', imageBytes],
  ]));
  const file = { fileId: 'e'.repeat(32), size: epub.length };
  const storage = memoryFileStorage(epub);
  const index = await indexPersistentEpub(storage, file);
  assert.equal(index.indexVersion, EPUB_INDEX_VERSION);
  assert.equal(index.format, 'epub');
  assert.equal(index.epubTitle, 'Illustrated Demo');
  assert.deepEqual(index.chapters.map(({ title }) => title), ['Demo Chapter']);
  assert.deepEqual(index.epubImages, [{
    id: 1, path: 'OEBPS/images/picture.png', mediaType: 'image/png', alt: 'Rabbit',
  }]);
  const window = await readEpubWindow(storage, file, index, 0, 12_288);
  const markerOffset = window.bytes.indexOf(0x1e);
  assert.match(window.text, /Before image\./u);
  assert.match(window.text, /After image\./u);
  assert.deepEqual(imageMarkerAt(window.bytes, markerOffset), { imageId: 1, length: 17 });
  assert.deepEqual((await readEpubImage(storage, file, index, 1)).bytes, imageBytes);
});

test('packs EPUB illustration pixels into the glasses GRAY_4 format', () => {
  const packed = rgbaToGray4(new Uint8ClampedArray([
    0, 0, 0, 255,
    255, 255, 255, 255,
    255, 0, 0, 255,
  ]), 3, 1);
  assert.deepEqual([...packed], [0x0f, 0x30]);
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

test('fills a forward-reading span across persistent window boundaries', async () => {
  const lines = Array.from(
    { length: 2000 },
    (_, index) => `line-${String(index).padStart(4, '0')} novel text`,
  );
  const source = new TextEncoder().encode(lines.join('\r\n'));
  const normalized = new TextEncoder().encode(lines.join('\n'));
  const storage = {
    async openRead(fileId, { offset = 0, length = source.length - offset } = {}) {
      const selected = source.subarray(offset, offset + length);
      return {
        fileId,
        size: source.length,
        offset,
        length,
        stream: new ReadableStream({
          start(controller) {
            controller.enqueue(selected.slice());
            controller.close();
          },
        }),
      };
    },
  };
  const file = { fileId: '3'.repeat(32), size: source.length };
  const first = {
    ...(await readNovelWindow(storage, file, 'utf-8', 0, 1024)),
    byteOffset: 0,
  };
  const start = first.bytes.length - 160;
  const span = await readNovelWindowSpan(storage, file, 'utf-8', first, start, 1024);

  assert.ok(span.bytes.length > first.bytes.length - start);
  assert.ok(span.loadedWindows.length >= 1);
  assert.deepEqual(span.bytes, normalized.subarray(start, start + span.bytes.length));
  assert.equal(span.final, false);
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
  assert.equal(initial.readingMode, 'scroll');
  assert.equal(initial.pageIntervalSeconds, 10);
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
    readingMode: 'page',
    pageIntervalSeconds: 14,
    bookmarks: [{ offset: 12_345, sourceOffset: 10_000, windowOffset: 12_000 }],
  }, { writeIndex: true });

  const restarted = await new ReaderStorage(gm).open();
  const [restored] = await restarted.listBooks();
  assert.equal(restored.progressOffset, 12_345);
  assert.equal(restored.readingMode, 'page');
  assert.equal(restored.pageIntervalSeconds, 14);
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

function memoryFileStorage(source) {
  return {
    async openRead(fileId, { offset = 0, length = source.length - offset } = {}) {
      const selected = source.subarray(offset, offset + length);
      return {
        fileId,
        size: source.length,
        offset,
        length,
        stream: new ReadableStream({
          start(controller) {
            controller.enqueue(selected.slice());
            controller.close();
          },
        }),
      };
    },
  };
}

function createStoredZip(files) {
  const encoder = new TextEncoder();
  const locals = [];
  const centrals = [];
  let offset = 0;
  for (const [name, bytes] of files) {
    const encodedName = encoder.encode(name);
    const local = new Uint8Array(30 + encodedName.length + bytes.length);
    const localView = new DataView(local.buffer);
    localView.setUint32(0, 0x04034b50, true);
    localView.setUint16(4, 20, true);
    localView.setUint16(6, 0x0800, true);
    localView.setUint32(18, bytes.length, true);
    localView.setUint32(22, bytes.length, true);
    localView.setUint16(26, encodedName.length, true);
    local.set(encodedName, 30);
    local.set(bytes, 30 + encodedName.length);
    locals.push(local);

    const central = new Uint8Array(46 + encodedName.length);
    const centralView = new DataView(central.buffer);
    centralView.setUint32(0, 0x02014b50, true);
    centralView.setUint16(4, 20, true);
    centralView.setUint16(6, 20, true);
    centralView.setUint16(8, 0x0800, true);
    centralView.setUint32(20, bytes.length, true);
    centralView.setUint32(24, bytes.length, true);
    centralView.setUint16(28, encodedName.length, true);
    centralView.setUint32(42, offset, true);
    central.set(encodedName, 46);
    centrals.push(central);
    offset += local.length;
  }
  const directorySize = centrals.reduce((total, item) => total + item.length, 0);
  const eocd = new Uint8Array(22);
  const eocdView = new DataView(eocd.buffer);
  eocdView.setUint32(0, 0x06054b50, true);
  eocdView.setUint16(8, files.size, true);
  eocdView.setUint16(10, files.size, true);
  eocdView.setUint32(12, directorySize, true);
  eocdView.setUint32(16, offset, true);
  const output = new Uint8Array(offset + directorySize + eocd.length);
  let cursor = 0;
  for (const part of [...locals, ...centrals, eocd]) {
    output.set(part, cursor);
    cursor += part.length;
  }
  return output;
}
