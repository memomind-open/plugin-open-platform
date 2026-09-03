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
  encodeOpen,
  encodeWindow,
  readerAction,
  readerControl,
  readerMode,
} from './reader-protocol.js';
import { ReaderStorage } from './reader-storage.js';
import {
  NOVEL_INDEX_VERSION,
  indexPersistentNovel,
  readNovelWindow,
  readNovelWindowSpan,
} from './reader-file.js';

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
  if (!available) return;
  const percent = Math.min(100, currentOffset / Math.max(1, current.textBytes) * 100);
  const chapter = currentChapters[chapterIndexAt(currentChapters, currentOffset)];
  const localOffset = Math.max(0, currentOffset - activeWindow.byteOffset);
  const previewEnd = trimWindowEnd(activeWindow.bytes, localOffset, 720);
  const previewText = decoder.decode(activeWindow.bytes.subarray(localOffset, previewEnd)).trimStart();
  bookTitle.textContent = current.title;
  chapterName.textContent = chapter?.title ?? 'Full Text';
  phonePreview.textContent = previewEnd < activeWindow.bytes.length || !activeWindow.final
    ? `${previewText}\n…`
    : previewText;
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
  const selectedEncoding = requestedEncoding ?? loaded.meta.encoding ?? 'auto';
  const needsIndex = requestedEncoding !== undefined ||
    novelIndex.indexVersion !== NOVEL_INDEX_VERSION || novelIndex.encoding === 'auto' ||
    !Number.isSafeInteger(novelIndex.textBytes) || novelIndex.textBytes <= 0 ||
    !Array.isArray(novelIndex.chapters) || novelIndex.chapters.length === 0;
  if (needsIndex) {
    setStatus(`Indexing "${loaded.meta.title}" from the binary stream…`);
    let shownProgress = -1;
    const result = await indexPersistentNovel(database, loaded.file, selectedEncoding,
      (loadedBytes, totalBytes) => {
        const percent = Math.floor(loadedBytes / Math.max(1, totalBytes) * 100);
        if (percent >= shownProgress + 10) {
          shownProgress = percent;
          setStatus(`Indexing "${loaded.meta.title}": ${percent}%`);
        }
      });
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
  setStatus(`Opened "${current.title}" with ${currentChapters.length} contents entries.`);
  if (push) await openOnGlasses(cursor);
}

async function importNovel() {
  if (!database) return;
  let file;
  try {
    setStatus('Opening the app file picker…');
    file = await database.pickBook();
    if (!file) {
      setStatus('Import cancelled.');
      return;
    }
    await reloadBooks();
    await openBook(file.fileId, true, encodingSelect.value);
    setStatus(`TXT imported successfully. Detected encoding: ${current.encoding}.`);
  } catch (error) {
    if (file?.fileId) await database.deleteBook(file.fileId).catch(() => undefined);
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
  const loaded = await readNovelWindow(
    database,
    currentFile,
    current.encoding,
    sourceOffset,
    maxBytes,
  );
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
    setStatus('Bridge ready. You can now import a TXT file.');
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
      setStatus('Library ready. Import a TXT file to start reading.');
    }
  } catch (error) {
    setStatus(`Local library unavailable: ${error.message}`, true);
  }
}

window.addEventListener('pagehide', () => eyeballCleanup());

function eyeballCleanup() {
  clearTimeout(saveTimer);
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
