import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import {
  DEFAULT_SCROLL_SPEED,
  SCROLL_SPEED_PROFILE_VERSION,
  chapterIndexAt,
  migrateScrollSpeed,
  trimWindowEnd,
} from './reader-core.js';
import {
  READER_CHANNEL,
  READER_EVENT_CHANNEL,
  decodeReaderEvent,
  encodeChapter,
  encodeClose,
  encodeControl,
  encodeImageBegin,
  encodeImageTile,
  encodeOpen,
  encodeWindow,
  readerAction,
  readerControl,
  readerMode,
} from './reader-protocol.js';
import { ReaderStorage } from './reader-storage.js';
import { rgbaToGray4 } from './reader-image.js';
import {
  NOVEL_INDEX_VERSION,
  indexPersistentNovel,
  readNovelWindow,
  readNovelWindowSpan,
} from './reader-file.js';
import {
  EPUB_INDEX_VERSION,
  imageMarkerAt,
  indexPersistentEpub,
  readEpubImage,
  readEpubWindow,
} from './reader-epub.js';

const element = (selector) => document.querySelector(selector);
const importButton = element('#import-button');
const fileImportHint = element('#file-import-hint');
const encodingSelect = element('#encoding');
const bridgeState = element('#bridge-state');
const bookList = element('#book-list');
const bookCount = element('#book-count');
const emptyState = element('#empty-state');
const readerView = element('#reader');
const bookTitle = element('#book-title');
const chapterName = element('#chapter-name');
const phonePreview = element('#phone-preview');
const phoneIllustration = element('#phone-illustration');
const progress = element('#progress');
const progressText = element('#progress-text');
const playToggle = element('#play-toggle');
const fontMode = element('#font-mode');
const readingMode = element('#reading-mode');
const scrollSpeed = element('#scroll-speed');
const scrollSpeedSetting = element('#scroll-speed-setting');
const pageIntervalSetting = element('#page-interval-setting');
const pageInterval = element('#page-interval');
const pageIntervalValue = element('#page-interval-value');
const pushButton = element('#push-to-glasses');
const status = element('#status');
const dialog = element('#list-dialog');
const dialogTitle = element('#dialog-title');
const dialogList = element('#dialog-list');

const decoder = new TextDecoder();
const PROGRESS_SAVE_INTERVAL_MS = 1000;
const GLASSES_WINDOW_BYTES = 12_288;
const WINDOW_CACHE_LIMIT = 6;
const MAX_BOOKMARKS = 500;
const IMAGE_TILE_ROWS = 24;
const IMAGE_ACK_TIMEOUT_MS = 6000;
let gm;
let database;
let bridgeReady = false;
let connected = false;
let subscriptionId;
let books = [];
let current;
let currentFile;
let currentChapters = [];
let windowCache = [];
let currentOffset = 0;
let currentSession = 0;
let glassesChapterTitle = '';
let playing = true;
let saveTimer;
let savePromise = Promise.resolve();
let lastSaveAt = 0;
let phoneImageUrl;
let phoneImageId;
let phoneImageRequest = 0;
let imageStatusWaiter;
let imageTransfer = Promise.resolve();

async function waitWithTimeout(promise, timeoutMs, message) {
  let timer;
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => {
        timer = setTimeout(() => reject(new Error(message)), timeoutMs);
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}

function setStatus(message, isError = false) {
  status.textContent = message;
  status.classList.toggle('error', isError);
}

function formatSize(bytes) {
  return bytes < 1024 * 1024
    ? `${Math.round(bytes / 1024)} KB`
    : `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

function renderLibrary() {
  bookCount.textContent = `${books.length} ${books.length === 1 ? 'book' : 'books'}`;
  bookList.replaceChildren(...books.map((book) => {
    const button = document.createElement('button');
    const title = document.createElement('strong');
    const detail = document.createElement('small');
    button.type = 'button';
    button.className = `book-card${book.fileId === current?.fileId ? ' active' : ''}`;
    title.textContent = book.title;
    detail.textContent = `${Math.round((book.progressOffset ?? 0) / Math.max(1, book.textBytes) * 100)}% · ${formatSize(book.sourceBytes)}`;
    button.append(title, detail);
    button.addEventListener('click', () => void openBook(book.fileId));
    return button;
  }));
}

function renderReader() {
  const activeWindow = findCachedWindow(currentOffset);
  const available = Boolean(current && currentFile && activeWindow);
  emptyState.hidden = available;
  readerView.hidden = !available;
  if (!available) {
    clearPhoneIllustration();
    return;
  }
  const percent = Math.min(100, currentOffset / Math.max(1, current.textBytes) * 100);
  const chapter = currentChapters[chapterIndexAt(currentChapters, currentOffset)];
  const localOffset = Math.max(0, currentOffset - activeWindow.byteOffset);
  const previewEnd = trimWindowEnd(activeWindow.bytes, localOffset, 720);
  const marker = current.format === 'epub' ? imageMarkerAt(activeWindow.bytes, localOffset) : null;
  const previewText = decoder.decode(activeWindow.bytes.subarray(localOffset, previewEnd)).trimStart()
    .replace(/\x1eGMIMG:[0-9a-f]{8}\x1e/giu, '[Illustration]');
  bookTitle.textContent = current.title;
  chapterName.textContent = chapter?.title ?? 'Full Text';
  phonePreview.textContent = previewEnd < activeWindow.bytes.length || !activeWindow.final
    ? `${previewText}\n…`
    : previewText;
  if (marker) void renderPhoneIllustration(marker.imageId);
  else clearPhoneIllustration();
  progress.value = percent;
  progressText.textContent = `${percent.toFixed(1)}%`;
  const pageTurnMode = current.readingMode === 'page';
  playToggle.textContent = playing
    ? `Pause Auto-${pageTurnMode ? 'turn' : 'scroll'}`
    : `Resume Auto-${pageTurnMode ? 'turn' : 'scroll'}`;
  playToggle.dataset.control = playing ? 'pause' : 'play';
  fontMode.value = String(current.fontMode ?? 0);
  readingMode.value = current.readingMode === 'page' ? 'page' : 'scroll';
  scrollSpeed.value = String(current.speed ?? DEFAULT_SCROLL_SPEED);
  const intervalSeconds = Math.max(4, Math.min(20, current.pageIntervalSeconds ?? 10));
  pageInterval.value = String(intervalSeconds);
  pageIntervalValue.textContent = `${intervalSeconds}s`;
  scrollSpeedSetting.hidden = pageTurnMode;
  pageIntervalSetting.hidden = !pageTurnMode;
}

async function renderPhoneIllustration(imageId) {
  if (phoneImageId === imageId && !phoneIllustration.hidden) return;
  const request = ++phoneImageRequest;
  try {
    const image = await readEpubImage(database, currentFile, current, imageId);
    if (request !== phoneImageRequest) return;
    clearPhoneIllustration(false);
    phoneImageUrl = URL.createObjectURL(new Blob([image.bytes], { type: image.mediaType }));
    phoneIllustration.src = phoneImageUrl;
    phoneIllustration.alt = image.alt || 'EPUB illustration';
    phoneIllustration.hidden = false;
    phoneImageId = imageId;
  } catch (error) {
    if (request === phoneImageRequest) setStatus(`Illustration preview failed: ${error.message}`, true);
  }
}

function clearPhoneIllustration(invalidate = true) {
  if (invalidate) phoneImageRequest += 1;
  phoneIllustration.hidden = true;
  phoneIllustration.removeAttribute('src');
  if (phoneImageUrl) URL.revokeObjectURL(phoneImageUrl);
  phoneImageUrl = undefined;
  phoneImageId = undefined;
}

async function reloadBooks() {
  books = (await database.listBooks()).sort((left, right) => right.updatedAt - left.updatedAt);
  renderLibrary();
  const usage = await database.getUsage();
  fileImportHint.textContent = `${formatSize(usage.totalBytes)} of ${formatSize(usage.maxTotalBytes)} used. Book count is not limited.`;
}

async function openBook(fileId, push = true, requestedEncoding) {
  const loaded = await database.getBook(fileId);
  if (!loaded) return;
  let novelIndex = loaded.meta;
  const format = loaded.meta.format === 'epub' ? 'epub' : 'txt';
  const selectedEncoding = requestedEncoding ?? loaded.meta.encoding ?? 'auto';
  const needsIndex = requestedEncoding !== undefined ||
    novelIndex.indexVersion !== (format === 'epub' ? EPUB_INDEX_VERSION : NOVEL_INDEX_VERSION) ||
    novelIndex.encoding === 'auto' ||
    !Number.isSafeInteger(novelIndex.textBytes) || novelIndex.textBytes <= 0 ||
    !Array.isArray(novelIndex.chapters) || novelIndex.chapters.length === 0 ||
    (format === 'epub' && (!Array.isArray(novelIndex.epubSections) ||
      !Array.isArray(novelIndex.epubImages)));
  if (needsIndex) {
    setStatus(`Indexing ${format.toUpperCase()} "${loaded.meta.title}" from the binary stream…`);
    let shownProgress = -1;
    const reportProgress = (loadedBytes, totalBytes) => {
      const percent = Math.floor(loadedBytes / Math.max(1, totalBytes) * 100);
      if (percent >= shownProgress + 10) {
        shownProgress = percent;
        setStatus(`Indexing "${loaded.meta.title}": ${percent}%`);
      }
    };
    const result = format === 'epub'
      ? await indexPersistentEpub(database, loaded.file, reportProgress)
      : await indexPersistentNovel(database, loaded.file, selectedEncoding, reportProgress);
    novelIndex = { ...novelIndex, ...result };
  }
  const migratedSpeed = migrateScrollSpeed(
    loaded.meta.speed ?? 8,
    loaded.meta.speedProfileVersion ?? 1,
  );
  current = {
    ...loaded.meta,
    ...novelIndex,
    fileId,
    format,
    title: novelIndex.epubTitle || loaded.meta.title,
    sourceBytes: loaded.file.size,
    speed: migratedSpeed,
    speedProfileVersion: SCROLL_SPEED_PROFILE_VERSION,
    updatedAt: Date.now(),
  };
  await database.putMetadata(current, { writeIndex: needsIndex });
  currentFile = loaded.file;
  currentChapters = current.chapters;
  windowCache = [];
  const cursor = savedCursor(current);
  currentOffset = cursor.offset;
  await loadWindow(cursor.sourceOffset, cursor.windowOffset);
  playing = current.playing !== false;
  const index = books.findIndex((book) => book.fileId === fileId);
  if (index >= 0) books[index] = current;
  renderReader();
  renderLibrary();
  const illustrationCount = current.format === 'epub' ? ` and ${current.epubImages.length} illustrations` : '';
  setStatus(`Opened "${current.title}" with ${currentChapters.length} contents entries${illustrationCount}.`);
  if (push) await openOnGlasses(cursor);
}

async function importNovel() {
  if (!database) return;
  let importedFile;
  try {
    setStatus('Opening the app file picker…');
    const picked = await database.pickBook();
    if (!picked) {
      setStatus('Import cancelled.');
      return;
    }
    const { file, duplicate } = picked;
    if (!duplicate) importedFile = file;
    await reloadBooks();
    await openBook(file.fileId, true,
      file.extension === 'epub' || /\.epub$/iu.test(file.name) ? undefined : encodingSelect.value);
    if (duplicate) {
      setStatus(`"${current.title}" is already in the library. Opened the existing copy.`);
    } else {
      setStatus(current.format === 'epub'
        ? `EPUB imported successfully with ${current.epubImages.length} supported illustrations.`
        : `TXT imported successfully. Detected encoding: ${current.encoding}.`);
    }
  } catch (error) {
    if (importedFile?.fileId) await database.deleteBook(importedFile.fileId).catch(() => undefined);
    await reloadBooks().catch(() => undefined);
    setStatus(`Import failed: ${error.message}`, true);
  }
}

function newSession() {
  const values = new Uint32Array(1);
  globalThis.crypto?.getRandomValues?.(values);
  return values[0] || (Date.now() >>> 0) || 1;
}

function savedCursor(book) {
  const offset = Number.isSafeInteger(book.progressOffset) ? book.progressOffset : 0;
  if (Number.isSafeInteger(book.progressSourceOffset) &&
      Number.isSafeInteger(book.progressWindowOffset) &&
      book.progressWindowOffset <= offset) {
    return {
      offset: Math.min(offset, Math.max(0, book.textBytes - 1)),
      sourceOffset: book.progressSourceOffset,
      windowOffset: book.progressWindowOffset,
    };
  }
  const chapter = book.chapters?.[chapterIndexAt(book.chapters, offset)] ??
    { byteOffset: 0, sourceOffset: 0 };
  return { offset: chapter.byteOffset, sourceOffset: chapter.sourceOffset, windowOffset: chapter.byteOffset };
}

function findCachedWindow(offset) {
  return windowCache.find((entry) => offset >= entry.byteOffset &&
    (offset < entry.byteOffset + entry.bytes.length ||
      (entry.final && offset === entry.byteOffset + entry.bytes.length)));
}

function cursorAt(offset) {
  const activeWindow = findCachedWindow(offset);
  if (activeWindow) {
    return { offset, sourceOffset: activeWindow.sourceOffset, windowOffset: activeWindow.byteOffset };
  }
  const bookmark = current?.bookmarks?.find((entry) => entry?.offset === offset);
  if (bookmark) return bookmark;
  const chapter = currentChapters[chapterIndexAt(currentChapters, offset)] ??
    { byteOffset: 0, sourceOffset: 0 };
  return { offset: chapter.byteOffset, sourceOffset: chapter.sourceOffset, windowOffset: chapter.byteOffset };
}

async function loadWindow(sourceOffset, byteOffset, maxBytes = GLASSES_WINDOW_BYTES) {
  const loaded = current.format === 'epub'
    ? await readEpubWindow(database, currentFile, current, byteOffset, maxBytes)
    : await readNovelWindow(database, currentFile, current.encoding, sourceOffset, maxBytes);
  const entry = { ...loaded, byteOffset };
  cacheWindow(entry);
  return entry;
}

function cacheWindow(entry) {
  windowCache = [
    ...windowCache.filter((window) => window.byteOffset !== entry.byteOffset),
    entry,
  ].slice(-WINDOW_CACHE_LIMIT);
}

async function openOnGlasses(cursorOrOffset) {
  if (!currentFile || !bridgeReady) {
    setStatus('The novel is open on the phone, but the glasses plugin is unavailable.', true);
    return;
  }
  const cursor = typeof cursorOrOffset === 'number' ? cursorAt(cursorOrOffset) : cursorOrOffset;
  if (!findCachedWindow(cursor.offset)) {
    await loadWindow(cursor.sourceOffset, cursor.windowOffset);
  }
  cancelPendingImageStatus('Reading session changed');
  currentSession = newSession();
  glassesChapterTitle = '';
  currentOffset = Math.min(cursor.offset, Math.max(0, current.textBytes - 1));
  try {
    await gm.plugin.sendMessage(READER_CHANNEL, encodeOpen({
      session: currentSession,
      totalBytes: current.textBytes,
      offset: currentOffset,
      fontMode: current.fontMode ?? 0,
      speed: current.speed ?? DEFAULT_SCROLL_SPEED,
      mode: current.readingMode === 'page' ? readerMode.page : readerMode.scroll,
      pageIntervalSeconds: current.pageIntervalSeconds ?? 10,
    }));
    await syncChapter(true);
    setStatus(connected ? 'Novel sent. Waiting for the glasses to request text…' : 'Reading session sent. Waiting for the device to connect.');
  } catch (error) {
    setStatus(`Send failed: ${error.message}`, true);
  }
}

async function sendTextWindow(request) {
  if (!currentFile || request.session !== currentSession) return;
  const start = Math.min(request.offset, current.textBytes);
  const requested = Math.min(request.maxBytes, GLASSES_WINDOW_BYTES);
  let activeWindow = findCachedWindow(start);
  if (current.format === 'epub') {
    if (!activeWindow || activeWindow.byteOffset !== start) {
      activeWindow = {
        ...(await readEpubWindow(database, currentFile, current, start, requested)),
        byteOffset: start,
      };
      cacheWindow(activeWindow);
    }
    await gm.plugin.sendMessage(READER_CHANNEL, encodeWindow({
      session: currentSession,
      offset: start,
      final: activeWindow.final,
      bytes: activeWindow.bytes.subarray(0, requested),
    }));
    setStatus('The glasses received EPUB content for local layout.');
    return;
  }
  if (!activeWindow) {
    const previous = windowCache.find((entry) => !entry.final &&
      entry.byteOffset + entry.bytes.length === start);
    if (!previous) throw new Error('The requested reading cursor is no longer available');
    activeWindow = await loadWindow(
      previous.sourceOffset + previous.sourceLength,
      previous.byteOffset + previous.bytes.length,
      requested,
    );
  }
  const span = await readNovelWindowSpan(
    database,
    currentFile,
    current.encoding,
    activeWindow,
    start,
    requested,
  );
  for (const loadedWindow of span.loadedWindows) cacheWindow(loadedWindow);
  if (span.bytes.length === 0) return;
  await gm.plugin.sendMessage(READER_CHANNEL, encodeWindow({
    session: currentSession,
    offset: start,
    final: span.final,
    bytes: span.bytes,
  }));
  setStatus('The glasses received a temporary text window for local layout and scrolling.');
}

function queueImageTransfer(request) {
  imageTransfer = imageTransfer.catch(() => undefined).then(async () => {
    try {
      await sendEpubImage(request);
    } catch (error) {
      setStatus(`Failed to send illustration: ${error.message}`, true);
    }
  });
}

async function sendEpubImage(request) {
  if (current?.format !== 'epub' || !currentFile || request.session !== currentSession) return;
  const session = currentSession;
  const width = Math.max(1, Math.min(2048, request.width));
  const height = Math.max(1, Math.min(2048, request.height));
  setStatus('Preparing EPUB illustration for the glasses…');
  const source = await readEpubImage(database, currentFile, current, request.imageId);
  const frame = await renderGray4Frame(source, width, height);
  if (session !== currentSession) return;
  const stride = Math.ceil(width / 2);
  const tileCount = Math.ceil(height / IMAGE_TILE_ROWS);
  await gm.plugin.sendMessage(READER_CHANNEL, encodeImageBegin({
    session, imageId: request.imageId, width, height, tileCount,
  }));
  for (let tileIndex = 0; tileIndex < tileCount; tileIndex += 1) {
    const y = tileIndex * IMAGE_TILE_ROWS;
    const rows = Math.min(IMAGE_TILE_ROWS, height - y);
    const acknowledged = waitForImageStatus(session, request.imageId, tileIndex);
    await gm.plugin.sendMessage(READER_CHANNEL, encodeImageTile({
      session,
      imageId: request.imageId,
      tileIndex,
      y,
      height: rows,
      stride,
      final: tileIndex + 1 === tileCount,
      bytes: frame.subarray(y * stride, (y + rows) * stride),
    }));
    await acknowledged;
    if (session !== currentSession) return;
  }
  setStatus(`Illustration sent (${source.alt || `image ${request.imageId}`}).`);
}

function waitForImageStatus(session, imageId, tileIndex) {
  if (imageStatusWaiter) imageStatusWaiter.reject(new Error('Image transfer was replaced'));
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      if (imageStatusWaiter?.timer === timer) imageStatusWaiter = undefined;
      reject(new Error(`Glasses did not acknowledge image tile ${tileIndex + 1}`));
    }, IMAGE_ACK_TIMEOUT_MS);
    imageStatusWaiter = { session, imageId, tileIndex, resolve, reject, timer };
  });
}

function acceptImageStatus(event) {
  const waiter = imageStatusWaiter;
  if (!waiter || waiter.session !== event.session || waiter.imageId !== event.imageId ||
      waiter.tileIndex !== event.tileIndex) return;
  clearTimeout(waiter.timer);
  imageStatusWaiter = undefined;
  if (event.status === 0) waiter.resolve(event);
  else waiter.reject(new Error(`Glasses rejected image tile ${event.tileIndex + 1} (status ${event.status})`));
}

async function renderGray4Frame(source, width, height) {
  const blob = new Blob([source.bytes], { type: source.mediaType });
  const drawable = await decodeBrowserImage(blob);
  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext('2d', { alpha: false, willReadFrequently: true });
  if (!context) throw new Error('Canvas rendering is unavailable');
  context.fillStyle = '#000';
  context.fillRect(0, 0, width, height);
  const scale = Math.min(width / drawable.width, height / drawable.height);
  const drawWidth = Math.max(1, Math.round(drawable.width * scale));
  const drawHeight = Math.max(1, Math.round(drawable.height * scale));
  context.drawImage(drawable, Math.floor((width - drawWidth) / 2),
    Math.floor((height - drawHeight) / 2), drawWidth, drawHeight);
  drawable.close?.();
  const rgba = context.getImageData(0, 0, width, height).data;
  return rgbaToGray4(rgba, width, height);
}

async function decodeBrowserImage(blob) {
  if (typeof createImageBitmap === 'function') return createImageBitmap(blob);
  const url = URL.createObjectURL(blob);
  try {
    const image = new Image();
    image.decoding = 'async';
    image.src = url;
    await new Promise((resolve, reject) => {
      image.addEventListener('load', resolve, { once: true });
      image.addEventListener('error', () => reject(new Error('EPUB image decoding failed')), { once: true });
    });
    return image;
  } finally {
    URL.revokeObjectURL(url);
  }
}

function persistReadingState() {
  if (!current) return savePromise;
  const cursor = cursorAt(currentOffset);
  const metadata = {
    ...current,
    progressOffset: currentOffset,
    progressSourceOffset: cursor.sourceOffset,
    progressWindowOffset: cursor.windowOffset,
    playing,
    updatedAt: Date.now(),
  };
  current = metadata;
  lastSaveAt = metadata.updatedAt;
  const index = books.findIndex((book) => book.fileId === metadata.fileId);
  if (index >= 0) books[index] = metadata;
  renderLibrary();
  savePromise = savePromise
    .then(() => database.putMetadata(metadata))
    .catch((error) => setStatus(`Failed to save reading progress: ${error.message}`, true));
  return savePromise;
}

function scheduleSave() {
  if (saveTimer) return;
  const delay = Math.max(0, PROGRESS_SAVE_INTERVAL_MS - (Date.now() - lastSaveAt));
  saveTimer = setTimeout(() => {
    saveTimer = undefined;
    void persistReadingState();
  }, delay);
}

async function handleReaderEvent(message) {
  if (message.channel !== READER_EVENT_CHANNEL) return;
  let event;
  try {
    event = decodeReaderEvent(message.data);
  } catch {
    return;
  }
  if (event.session !== currentSession) return;
  if (event.type === 'imageStatus') {
    acceptImageStatus(event);
    return;
  }
  if (event.type === 'needImage') {
    queueImageTransfer(event);
    return;
  }
  if (event.type === 'needWindow') {
    try {
      await sendTextWindow(event);
    } catch (error) {
      setStatus(`Failed to send text: ${error.message}`, true);
    }
    return;
  }
  if (event.type === 'progress') {
    currentOffset = Math.min(event.offset, Math.max(0, current.textBytes - 1));
    playing = event.playing;
    current.fontMode = event.fontMode;
    renderReader();
    scheduleSave();
    await syncChapter();
    return;
  }
  if (event.type !== 'action') return;
  currentOffset = event.offset;
  if (event.action === readerAction.bookmark) {
    const added = { ...cursorAt(currentOffset), savedAt: Date.now() };
    const bookmarks = [...(current.bookmarks ?? []).filter((entry) => entry.offset !== added.offset), added]
      .sort((left, right) => (right.savedAt ?? 0) - (left.savedAt ?? 0))
      .slice(0, MAX_BOOKMARKS)
      .sort((left, right) => left.offset - right.offset);
    current = { ...current, bookmarks, updatedAt: Date.now() };
    await database.putMetadata(current);
    setStatus('Bookmark saved on the phone.');
    return;
  }
  const index = chapterIndexAt(currentChapters, currentOffset);
  const targetIndex = event.action === readerAction.previousChapter
    ? Math.max(0, index - 1)
    : Math.min(currentChapters.length - 1, index + 1);
  const chapter = currentChapters[targetIndex];
  await openOnGlasses({
    offset: chapter.byteOffset,
    sourceOffset: chapter.sourceOffset,
    windowOffset: chapter.byteOffset,
  });
}

function cancelPendingImageStatus(message) {
  if (!imageStatusWaiter) return;
  clearTimeout(imageStatusWaiter.timer);
  imageStatusWaiter.reject(new Error(message));
  imageStatusWaiter = undefined;
}

async function sendControl(control, value = 0) {
  if (!gm || !currentSession) return false;
  try {
    await gm.plugin.sendMessage(READER_CHANNEL, encodeControl(currentSession, control, value));
    return true;
  } catch (error) {
    setStatus(`Control failed: ${error.message}`, true);
    return false;
  }
}

function chapterTitleAt(offset) {
  if (currentChapters.length === 0) return 'Full Text';
  const index = chapterIndexAt(currentChapters, offset);
  return currentChapters[index]?.title ?? 'Full Text';
}

async function syncChapter(force = false) {
  if (!gm || !currentSession) return;
  const chapterTitle = chapterTitleAt(currentOffset);
  if (!force && chapterTitle === glassesChapterTitle) return;
  try {
    await gm.plugin.sendMessage(READER_CHANNEL, encodeChapter(currentSession, chapterTitle));
    glassesChapterTitle = chapterTitle;
  } catch (error) {
    setStatus(`Chapter sync failed: ${error.message}`, true);
  }
}

function showList(kind) {
  if (!current) return;
  dialogList.replaceChildren();
  if (kind === 'directory') {
    dialogTitle.textContent = 'Contents';
    for (const chapter of currentChapters) {
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = chapter.title;
      button.addEventListener('click', () => {
        dialog.close();
        void openOnGlasses(chapter.byteOffset);
      });
      dialogList.append(button);
    }
  } else {
    dialogTitle.textContent = 'Bookmarks';
    const bookmarks = current.bookmarks ?? [];
    if (bookmarks.length === 0) {
      const note = document.createElement('p');
      note.textContent = 'No bookmarks yet. Double-click the primary glasses button to add one.';
      dialogList.append(note);
    }
    for (const bookmark of bookmarks) {
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = `${(bookmark.offset / Math.max(1, current.textBytes) * 100).toFixed(1)}% · ${currentChapters[chapterIndexAt(currentChapters, bookmark.offset)]?.title ?? 'Full Text'}`;
      button.addEventListener('click', () => {
        dialog.close();
        void openOnGlasses(bookmark);
      });
      dialogList.append(button);
    }
  }
  dialog.showModal();
}

importButton.addEventListener('click', () => void importNovel());

document.querySelectorAll('[data-control]').forEach((button) => {
  button.addEventListener('click', () => {
    const control = readerControl[button.dataset.control];
    if (control) void sendControl(control);
  });
});

document.querySelectorAll('[data-action]').forEach((button) => {
  button.addEventListener('click', async () => {
    const action = button.dataset.action;
    if (action === 'directory' || action === 'bookmark') showList(action);
    if (action === 'delete' && current && confirm(`Remove "${current.title}" from the phone library?`)) {
      if (gm && currentSession) await gm.plugin.sendMessage(READER_CHANNEL, encodeClose(currentSession)).catch(() => undefined);
      await database.deleteBook(current.fileId);
      current = undefined;
      currentFile = undefined;
      currentChapters = [];
      windowCache = [];
      currentSession = 0;
      glassesChapterTitle = '';
      await reloadBooks();
      renderReader();
      setStatus('Novel removed from the phone library.');
    }
  });
});

fontMode.addEventListener('change', async () => {
  if (!current) return;
  current.fontMode = Number(fontMode.value);
  await database.putMetadata({ ...current, updatedAt: Date.now() });
  await sendControl(readerControl.setFont, current.fontMode);
});

readingMode.addEventListener('change', async () => {
  if (!current) return;
  current.readingMode = readingMode.value === 'page' ? 'page' : 'scroll';
  await database.putMetadata({ ...current, updatedAt: Date.now() });
  await sendControl(readerControl.setMode,
    current.readingMode === 'page' ? readerMode.page : readerMode.scroll);
  renderReader();
});

scrollSpeed.addEventListener('change', async () => {
  if (!current) return;
  current.speed = Number(scrollSpeed.value);
  current.speedProfileVersion = SCROLL_SPEED_PROFILE_VERSION;
  await database.putMetadata({ ...current, updatedAt: Date.now() });
  await sendControl(readerControl.setSpeed, current.speed);
});

pageInterval.addEventListener('input', () => {
  pageIntervalValue.textContent = `${pageInterval.value}s`;
});

pageInterval.addEventListener('change', async () => {
  if (!current) return;
  current.pageIntervalSeconds = Math.max(4, Math.min(20, Number(pageInterval.value)));
  await database.putMetadata({ ...current, updatedAt: Date.now() });
  await sendControl(readerControl.setPageInterval, current.pageIntervalSeconds);
});

pushButton.addEventListener('click', () => void openOnGlasses(currentOffset));
element('#dialog-close').addEventListener('click', () => dialog.close());
bridgeState.addEventListener('click', () => {
  if (!bridgeReady) globalThis.location.reload();
});

async function initializeBridge() {
  try {
    gm = createGMPlugin();
    await waitWithTimeout(gm.ready(), 8000, 'Bridge bootstrap timed out');
    gm.plugin.onMessage((message) => void handleReaderEvent(message));
    gm.device.onConnection((event) => {
      connected = event.connected === true;
      bridgeState.textContent = connected ? 'Glasses Connected' : 'Glasses Disconnected';
      bridgeState.classList.toggle('connected', connected);
      if (connected && current) void openOnGlasses(currentOffset);
    });
    subscriptionId = (await gm.device.subscribeEvents(['connection'])).subscriptionId;
    const info = await gm.device.getInfo();
    connected = info.connected === true;
    bridgeReady = true;
    bridgeState.textContent = connected ? 'Glasses Connected' : 'Bridge Ready';
    bridgeState.classList.toggle('connected', connected);
    setStatus('Bridge ready. You can now import a TXT or EPUB file.');
    return true;
  } catch (error) {
    bridgeState.textContent = 'Connection Timed Out · Retry';
    setStatus(`Glasses Bridge unavailable: ${error.message}. Tap the top-right button to retry.`, true);
    return false;
  }
}

async function initialize() {
  importButton.disabled = true;
  const initialized = await initializeBridge();
  if (!initialized) return;
  try {
    database = new ReaderStorage(gm);
    await database.open();
    await reloadBooks();
    importButton.disabled = false;
    if (books.length > 0) {
      await openBook(books[0].fileId);
    } else {
      setStatus('Library ready. Import a TXT or EPUB file to start reading.');
    }
  } catch (error) {
    setStatus(`Local library unavailable: ${error.message}`, true);
  }
}

window.addEventListener('pagehide', () => eyeballCleanup());

function eyeballCleanup() {
  clearTimeout(saveTimer);
  cancelPendingImageStatus('Reader closed');
  clearPhoneIllustration();
  const pendingSave = persistReadingState();
  if (gm && currentSession) void gm.plugin.sendMessage(READER_CHANNEL, encodeClose(currentSession)).catch(() => undefined);
  if (gm && subscriptionId) void gm.device.unsubscribeEvents(subscriptionId).catch(() => undefined);
  void pendingSave.finally(() => gm?.close());
  currentFile = undefined;
  windowCache = [];
  currentSession = 0;
  glassesChapterTitle = '';
}

void initialize();
