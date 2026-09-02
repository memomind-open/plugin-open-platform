import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import {
  DEFAULT_SCROLL_SPEED,
  MAX_FILE_BYTES,
  SCROLL_SPEED_PROFILE_VERSION,
  chapterByteOffsets,
  chapterIndexAt,
  createBookId,
  decodeNovel,
  findChapters,
  migrateScrollSpeed,
  safeBookTitle,
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
} from './reader-protocol.js';
import { ReaderStorage } from './reader-storage.js';

const element = (selector) => document.querySelector(selector);
const fileInput = element('#novel-file');
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
const scrollSpeed = element('#scroll-speed');
const pushButton = element('#push-to-glasses');
const status = element('#status');
const dialog = element('#list-dialog');
const dialogTitle = element('#dialog-title');
const dialogList = element('#dialog-list');

const database = new ReaderStorage();
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const PROGRESS_SAVE_INTERVAL_MS = 5000;
const bundledBook = Object.freeze({
  id: 'bundled-wanming-keshanmeng-v1',
  title: '晚明_柯山梦',
  source: './assets/wanming-keshanmeng.txt',
});
let gm;
let bridgeReady = false;
let connected = false;
let subscriptionId;
let books = [];
let current;
let currentBytes;
let currentChapters = [];
let currentOffset = 0;
let currentSession = 0;
let glassesChapterTitle = '';
let playing = true;
let saveTimer;
let savePromise = Promise.resolve();
let lastSaveAt = 0;
let pickerFeedbackTimer;

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
    ? `${Math.max(1, Math.round(bytes / 1024))} KB`
    : `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

function renderLibrary() {
  bookCount.textContent = `${books.length} ${books.length === 1 ? 'book' : 'books'}`;
  bookList.replaceChildren(...books.map((book) => {
    const button = document.createElement('button');
    const title = document.createElement('strong');
    const detail = document.createElement('small');
    button.type = 'button';
    button.className = `book-card${book.id === current?.id ? ' active' : ''}`;
    title.textContent = book.title;
    detail.textContent = `${Math.round((book.progressOffset ?? 0) / Math.max(1, book.textBytes) * 100)}% · ${formatSize(book.sourceBytes)}`;
    button.append(title, detail);
    button.addEventListener('click', () => void openBook(book.id));
    return button;
  }));
}

function renderReader() {
  const available = Boolean(current && currentBytes);
  emptyState.hidden = available;
  readerView.hidden = !available;
  if (!available) return;
  const percent = Math.min(100, currentOffset / Math.max(1, currentBytes.length) * 100);
  const chapter = currentChapters[chapterIndexAt(currentChapters, currentOffset)];
  const previewEnd = trimWindowEnd(currentBytes, currentOffset, 720);
  const previewText = decoder.decode(currentBytes.subarray(currentOffset, previewEnd)).trimStart();
  bookTitle.textContent = current.title;
  chapterName.textContent = chapter?.title ?? 'Full Text';
  phonePreview.textContent = previewEnd < currentBytes.length ? `${previewText}\n…` : previewText;
  progress.value = percent;
  progressText.textContent = `${percent.toFixed(1)}%`;
  playToggle.textContent = playing ? 'Pause Auto-scroll' : 'Resume Auto-scroll';
  playToggle.dataset.control = playing ? 'pause' : 'play';
  fontMode.value = String(current.fontMode ?? 0);
  scrollSpeed.value = String(current.speed ?? DEFAULT_SCROLL_SPEED);
}

async function reloadBooks() {
  books = (await database.listBooks()).sort((left, right) => right.updatedAt - left.updatedAt);
  renderLibrary();
}

async function installBundledBook() {
  const existing = await database.getMetadata(bundledBook.id);
  if (existing) return bundledBook.id;
  setStatus('Preparing the bundled novel…');
  const response = await fetch(bundledBook.source);
  if (!response.ok) throw new Error(`Bundled novel request failed: HTTP ${response.status}`);
  const bytes = new Uint8Array(await response.arrayBuffer());
  const decoded = decodeNovel(bytes, 'utf-8');
  const now = Date.now();
  await database.putBook({
    id: bundledBook.id,
    title: bundledBook.title,
    encoding: decoded.encoding,
    sourceBytes: bytes.length,
    textBytes: encoder.encode(decoded.text).length,
    addedAt: now,
    updatedAt: now,
    progressOffset: 0,
    bookmarks: [],
    fontMode: 0,
    speed: DEFAULT_SCROLL_SPEED,
    speedProfileVersion: SCROLL_SPEED_PROFILE_VERSION,
    playing: true,
    bundled: true,
  }, decoded.text);
  return bundledBook.id;
}

async function openBook(id, push = true) {
  const loaded = await database.getBook(id);
  if (!loaded) return;
  const migratedSpeed = migrateScrollSpeed(
    loaded.meta.speed ?? 8,
    loaded.meta.speedProfileVersion ?? 1,
  );
  current = {
    ...loaded.meta,
    speed: migratedSpeed,
    speedProfileVersion: SCROLL_SPEED_PROFILE_VERSION,
  };
  if (loaded.meta.speed !== migratedSpeed ||
      loaded.meta.speedProfileVersion !== SCROLL_SPEED_PROFILE_VERSION) {
    await database.putMetadata({ ...current, updatedAt: Date.now() });
  }
  currentBytes = encoder.encode(loaded.text);
  const chapters = findChapters(loaded.text);
  currentChapters = chapterByteOffsets(loaded.text, chapters);
  currentOffset = Math.min(current.progressOffset ?? 0, Math.max(0, currentBytes.length - 1));
  playing = current.playing !== false;
  renderReader();
  renderLibrary();
  setStatus(`Opened "${current.title}" with ${currentChapters.length} contents entries.`);
  if (push) await openOnGlasses(currentOffset);
}

async function importNovel(file) {
  if (!file) return;
  if (file.size > MAX_FILE_BYTES) {
    setStatus('Import failed: TXT files cannot exceed 20 MB.', true);
    return;
  }
  setStatus('Reading and parsing TXT…');
  try {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const decoded = decodeNovel(bytes, encodingSelect.value);
    if (!decoded.text) throw new Error('TXT file has no readable text');
    const id = await createBookId(bytes);
    const existing = await database.getMetadata(id);
    const now = Date.now();
    const metadata = {
      id,
      title: safeBookTitle(file.name),
      encoding: decoded.encoding,
      sourceBytes: file.size,
      textBytes: encoder.encode(decoded.text).length,
      addedAt: existing?.addedAt ?? now,
      updatedAt: now,
      progressOffset: existing?.progressOffset ?? 0,
      bookmarks: existing?.bookmarks ?? [],
      fontMode: existing?.fontMode ?? 0,
      speed: existing
        ? migrateScrollSpeed(existing.speed ?? 8, existing.speedProfileVersion ?? 1)
        : DEFAULT_SCROLL_SPEED,
      speedProfileVersion: SCROLL_SPEED_PROFILE_VERSION,
      playing: existing?.playing ?? true,
    };
    await database.putBook(metadata, decoded.text);
    await reloadBooks();
    await openBook(id);
    setStatus(`TXT imported successfully. Detected encoding: ${decoded.encoding}.`);
  } catch (error) {
    setStatus(`Import failed: ${error.message}`, true);
  } finally {
    fileInput.value = '';
  }
}

function newSession() {
  const values = new Uint32Array(1);
  globalThis.crypto?.getRandomValues?.(values);
  return values[0] || (Date.now() >>> 0) || 1;
}

async function openOnGlasses(offset) {
  if (!currentBytes || !bridgeReady) {
    setStatus('The novel is open on the phone, but the glasses plugin is unavailable.', true);
    return;
  }
  currentSession = newSession();
  glassesChapterTitle = '';
  currentOffset = Math.min(offset, Math.max(0, currentBytes.length - 1));
  try {
    await gm.plugin.sendMessage(READER_CHANNEL, encodeOpen({
      session: currentSession,
      totalBytes: currentBytes.length,
      offset: currentOffset,
      fontMode: current.fontMode ?? 0,
      speed: current.speed ?? DEFAULT_SCROLL_SPEED,
    }));
    await syncChapter(true);
    setStatus(connected ? 'Novel sent. Waiting for the glasses to request text…' : 'Reading session sent. Waiting for the device to connect.');
  } catch (error) {
    setStatus(`Send failed: ${error.message}`, true);
  }
}

async function sendTextWindow(request) {
  if (!currentBytes || request.session !== currentSession) return;
  const start = Math.min(request.offset, currentBytes.length);
  const requested = Math.min(request.maxBytes, 12_288);
  const end = trimWindowEnd(currentBytes, start, requested);
  if (end <= start) return;
  await gm.plugin.sendMessage(READER_CHANNEL, encodeWindow({
    session: currentSession,
    offset: start,
    final: end === currentBytes.length,
    bytes: currentBytes.subarray(start, end),
  }));
  setStatus('The glasses received a temporary text window for local layout and scrolling.');
}

function persistReadingState() {
  if (!current) return savePromise;
  const metadata = {
    ...current,
    progressOffset: currentOffset,
    playing,
    updatedAt: Date.now(),
  };
  current = metadata;
  lastSaveAt = metadata.updatedAt;
  const index = books.findIndex((book) => book.id === metadata.id);
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
    currentOffset = Math.min(event.offset, Math.max(0, currentBytes.length - 1));
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
    const bookmarks = [...new Set([...(current.bookmarks ?? []), currentOffset])].sort((a, b) => a - b);
    current = { ...current, bookmarks, updatedAt: Date.now() };
    await database.putMetadata(current);
    setStatus('Bookmark saved on the phone.');
    return;
  }
  const index = chapterIndexAt(currentChapters, currentOffset);
  const targetIndex = event.action === readerAction.previousChapter
    ? Math.max(0, index - 1)
    : Math.min(currentChapters.length - 1, index + 1);
  await openOnGlasses(currentChapters[targetIndex].byteOffset);
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
    for (const offset of bookmarks) {
      const button = document.createElement('button');
      button.type = 'button';
      button.textContent = `${(offset / Math.max(1, currentBytes.length) * 100).toFixed(1)}% · ${currentChapters[chapterIndexAt(currentChapters, offset)]?.title ?? 'Full Text'}`;
      button.addEventListener('click', () => {
        dialog.close();
        void openOnGlasses(offset);
      });
      dialogList.append(button);
    }
  }
  dialog.showModal();
}

fileInput.addEventListener('click', () => {
  clearTimeout(pickerFeedbackTimer);
  setStatus('Opening the system file picker…');
  pickerFeedbackTimer = setTimeout(() => {
    if (!document.hidden && !fileInput.files?.length) {
      setStatus('The host app did not open the file picker. Its WebView must support file upload callbacks.', true);
    }
  }, 1200);
});
fileInput.addEventListener('change', () => {
  clearTimeout(pickerFeedbackTimer);
  void importNovel(fileInput.files?.[0]);
});

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
      await database.deleteBook(current.id);
      current = undefined;
      currentBytes = undefined;
      currentChapters = [];
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

scrollSpeed.addEventListener('change', async () => {
  if (!current) return;
  current.speed = Number(scrollSpeed.value);
  current.speedProfileVersion = SCROLL_SPEED_PROFILE_VERSION;
  await database.putMetadata({ ...current, updatedAt: Date.now() });
  await sendControl(readerControl.setSpeed, current.speed);
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
  } catch (error) {
    bridgeState.textContent = 'Connection Timed Out · Retry';
    setStatus(`Glasses Bridge unavailable: ${error.message}. Tap the top-right button to retry.`, true);
  }
}

async function initialize() {
  const bridgeInitialization = initializeBridge();
  let bundledBookId;
  try {
    await database.open();
    bundledBookId = await installBundledBook();
    await reloadBooks();
    setStatus('The bundled novel was added to the phone library.');
  } catch (error) {
    setStatus(`Local library unavailable: ${error.message}`, true);
  }
  await bridgeInitialization;
  if (bundledBookId) await openBook(bundledBookId);
}

window.addEventListener('pagehide', () => eyeballCleanup());

function eyeballCleanup() {
  clearTimeout(saveTimer);
  clearTimeout(pickerFeedbackTimer);
  void persistReadingState();
  if (gm && currentSession) void gm.plugin.sendMessage(READER_CHANNEL, encodeClose(currentSession)).catch(() => undefined);
  if (gm && subscriptionId) void gm.device.unsubscribeEvents(subscriptionId).catch(() => undefined);
  gm?.close();
  currentBytes = undefined;
  currentSession = 0;
  glassesChapterTitle = '';
}

void initialize();
