import { i18n, t, localizedError } from './i18n.js';
import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import { WIDTH, HEIGHT, ALIGNMENTS, composeImage, decodeImage, readImageBlob } from './image.js';
import { ImageTransfer } from './protocol.js';
import { compressLz4 } from './lz4.js';
import { pickImageFile, waitForFilePickerReturn, throwIfAborted } from './files.js';

i18n.apply();
let gm;
const card = document.querySelector('.card');
const slotList = document.querySelector('#slot-list');
const addSlot = document.querySelector('#add-slot');
const status = document.querySelector('#status');
const progress = document.querySelector('#progress');
const cancel = document.querySelector('#cancel');
const retry = document.querySelector('#retry');
const slots = Array(10).fill(null);
// Keep saved references even if a temporary read/decode failure prevents restore.
let fileIds = Array(10).fill(null), selectedSlot = 0;
const key = 'pixel-image-viewer.slots.v1';
const alignmentKey = 'pixel-image-viewer.alignments.v1';
const alignmentSelect = document.querySelector('#alignment');
let alignments = [];
const slotAlignment = i => alignments[i] || 'center';
let ready = false, connected = false, active = true, operation, subscriptionId, transfer;
const listeners = [];
let operationKind, pickerOpen = false, initializing = false, closed = false;

// Canvas backing pixels stay 600 × 350. CSS pixels can span several physical
// pixels on Android; compensate only for that density, never resize the image.
function syncPreviewSize() {
  const density = (window.devicePixelRatio || 1) * (window.visualViewport?.scale || 1);
  document.documentElement.style.setProperty('--preview-width', `${WIDTH / density}px`);
  document.documentElement.style.setProperty('--preview-height', `${HEIGHT / density}px`);
}
syncPreviewSize();
window.addEventListener('resize', syncPreviewSize);
window.visualViewport?.addEventListener('resize', syncPreviewSize);

let statusMessage = () => t('notice.initializing');
status.textContent = statusMessage();
function report(message, error = false) {
  statusMessage = typeof message === 'function' ? message : () => message;
  status.textContent = statusMessage(); status.hidden = !status.textContent;
  status.title = status.textContent;
  status.classList.toggle('error', error);
  renderConnectionActions();
}
function errorText(error) {
  if (['PERMISSION_DENIED', 'UNAUTHORIZED'].includes(error?.code) || error?.message === 'NOT_GRANTED') {
    return t('error.permission');
  }
  return error?.message || String(error);
}
function renderConnectionActions() {
  card.querySelector('.name').hidden = !status.hidden;
  const sending = !!operation && operationKind === 'send';
  card.querySelector('.feedback').classList.toggle('sending', sending);
  cancel.hidden = !sending;
  progress.hidden = !sending;
  retry.hidden = initializing || !!operation ||
    (ready && connected && !!subscriptionId && !status.classList.contains('error'));
  document.querySelector('.connection-actions').hidden = cancel.hidden && retry.hidden;
}
function render() {
  document.querySelector('#connection').textContent = t(connected ? 'connection.on' : 'connection.off');
  const busy = !ready || !active || initializing || !!operation;
  card.querySelector('.pick').disabled = busy;
  card.querySelector('.send').disabled = busy || !connected || !slots[selectedSlot];
  addSlot.disabled = busy;
  alignmentSelect.disabled = busy;
  alignmentSelect.value = slotAlignment(selectedSlot);
  const slotButtons = [...slotList.querySelectorAll('.slot')];
  let added = false;
  while (slotButtons.length < slots.length) {
    const i = slotButtons.length;
    const button = document.createElement('button');
    button.type = 'button'; button.className = 'slot'; button.textContent = String(i + 1);
    button.addEventListener('click', () => {
      if (!ready || !active || initializing || operation) return;
      selectedSlot = i;
      showPreview(); render();
      progress.value = 0;
      report('');
    });
    slotList.insertBefore(button, addSlot);
    slotButtons.push(button); added = true;
  }
  slotButtons.forEach((button, i) => {
    button.disabled = busy;
    button.setAttribute('aria-pressed', String(i === selectedSlot));
    button.setAttribute('aria-label', t(slots[i] ? 'slot.loadedLabel' : 'slot.emptyLabel', { slot: i + 1 }));
    button.classList.toggle('filled', !!slots[i]);
  });
  // Keep the newly selected slot and trailing + reachable inside this panel.
  if (added && selectedSlot === slots.length - 1) slotList.scrollTop = slotList.scrollHeight;
  renderConnectionActions();
  cancel.disabled = !operation;
  retry.disabled = closed || initializing || !active || !!operation;
}
async function run(work, kind = 'file') {
  if (operation) return;
  const current = new AbortController();
  operation = current; operationKind = kind; render();
  try { await work(current.signal); }
  catch (error) { report(() => errorText(error), true); }
  finally { if (operation === current) operation = undefined; render(); }
}
function cancelWork(key) {
  operation?.abort(localizedError(key));
  transfer?.cancel();
}
async function prepare(file, signal, alignment = 'center') {
  const blob = await readImageBlob(gm, file, signal);
  throwIfAborted(signal);
  report(() => t('notice.decoding'));
  const decoded = await decodeImage(blob);
  try {
    throwIfAborted(signal);
    const canvas = document.createElement('canvas');
    const bytes = composeImage(decoded.image, decoded.width, decoded.height, canvas, alignment);
    return { file, canvas, bytes, compressed: compressLz4(bytes), width: decoded.width, height: decoded.height };
  } finally { decoded.close(); }
}
function showPreview() {
  const item = slots[selectedSlot];
  card.setAttribute('aria-label', t('slot.preview', { slot: selectedSlot + 1 }));
  const ctx = card.querySelector('canvas').getContext('2d');
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, WIDTH, HEIGHT);
  if (item) ctx.drawImage(item.canvas, 0, 0);
  card.querySelector('.pick').textContent = t(item ? 'ui.replace' : 'ui.pick');
  card.querySelector('.name').textContent = item?.file.name ?? t('ui.empty');
  card.querySelector('.name').title = item?.file.name ?? '';
  card.querySelector('.geometry').textContent = item
    ? t('preview.details', { width: item.width, height: item.height, raw: i18n.number(item.bytes.length), compressed: i18n.number(item.compressed.length) })
    : t('preview.empty');
}
function showSlot(i, item) {
  slots[i] = item;
  if (i === selectedSlot) showPreview();
}
async function appendSlot(signal) {
  const ids = [...fileIds, null];
  throwIfAborted(signal);
  await gm.storage.set(key, ids);
  fileIds = ids;
  slots.push(null);
  selectedSlot = slots.length - 1;
  showPreview(); progress.value = 0;
  report('');
}
async function pick(i, signal) {
  report(() => t('notice.pick'));
  let file, saved = false;
  try {
    pickerOpen = true;
    try {
      file = await pickImageFile(gm);
      await waitForFilePickerReturn(gm, signal);
      active = true;
    } finally { pickerOpen = false; }
    throwIfAborted(signal);
    if (!file) { report(() => t('notice.pickCancelled')); return; }
    report(() => t('notice.reading'));
    const item = await prepare(file, signal, slotAlignment(i));
    const previousId = fileIds[i];
    const ids = [...fileIds];
    ids[i] = file.fileId;
    throwIfAborted(signal);
    await gm.storage.set(key, ids);
    saved = true; fileIds = ids;
    showSlot(i, item);
    report('');
    if (previousId && !fileIds.includes(previousId)) {
      const deleted = await gm.files.delete(previousId);
      if (!deleted.deleted) throw localizedError('error.cleanup');
    }
  } catch (error) {
    if (file && !saved && !fileIds.includes(file.fileId)) {
      await gm.files.delete(file.fileId).catch(() => {});
    }
    throw error;
  }
}
async function send(i, signal) {
  // Let the Host authorize actual API calls, as Novel Reader does. Capability
  // inventories differ between App versions and cannot gate local file picking.
  connected = (await gm.device.getInfo()).connected === true;
  if (!connected) throw localizedError('error.disconnected');
  progress.value = 0;
  report(() => t('notice.sending'));
  const result = await transfer.send(slots[i].bytes, (sent, total, stats) => {
    progress.max = total; progress.value = sent;
    report(() => t('notice.progress', { encoding: stats.encoding, bytes: i18n.number(total), percent: Math.floor(sent * 100 / total), waiting: sent === total ? t('notice.waiting') : '' }));
  }, signal, slots[i].compressed);
  report(() => t(result.status === 1 ? 'notice.call' : 'notice.displayed') +
    t('notice.stats', { encoding: result.encoding, raw: i18n.number(result.originalBytes), wire: i18n.number(result.wireBytes), seconds: (result.elapsedMs / 1000).toFixed(2), reason: result.reasonKey ? t(result.reasonKey) : '' }));
}

card.querySelector('.pick').addEventListener('click', () => {
  const i = selectedSlot;
  void run(signal => pick(i, signal));
});
card.querySelector('.send').addEventListener('click', () => {
  const i = selectedSlot;
  void run(signal => send(i, signal), 'send');
});
alignmentSelect.addEventListener('change', () => {
  const i = selectedSlot, alignment = alignmentSelect.value;
  if (!ready || !active || initializing || operation || !ALIGNMENTS.includes(alignment)) { render(); return; }
  void run(async signal => {
    // Rebuild both preview and wire payload before publishing the new setting.
    const item = slots[i] ? await prepare(slots[i].file, signal, alignment) : null;
    throwIfAborted(signal);
    const next = [...alignments];
    next[i] = alignment;
    await gm.storage.set(alignmentKey, next);
    alignments = next;
    if (item) showSlot(i, item);
    progress.value = 0;
    report('');
  }, 'alignment');
});
addSlot.addEventListener('click', () => void run(appendSlot, 'slots'));
showPreview();
cancel.addEventListener('click', () => cancelWork('error.cancelled'));
function initializationFailed(error) {
  report(() => t('error.init', { error: errorText(error) }), true);
  render();
}
async function checkConnection() {
  if (closed || initializing || operation || !active) return;
  if (!ready) { await initialize().catch(initializationFailed); return; }
  // Keep the App's WebView, SDK transport and bootstrap session alive. Reloading
  // tears down Gecko extension ports and can crash the native App.
  await run(async signal => {
    connected = false; render();
    report(() => t('notice.checking'));
    const info = await gm.device.getInfo();
    throwIfAborted(signal);
    connected = info.connected === true;
    if (!subscriptionId) subscriptionId = (await gm.device.subscribeEvents(['connection'])).subscriptionId;
    throwIfAborted(signal);
    report(() => connected ? '' : t('notice.offline'));
  }, 'connection');
}
retry.addEventListener('click', () => void checkConnection());

async function initialize() {
  if (closed || initializing || operation) return;
  initializing = true; ready = false; render();
  try {
    gm ??= createGMPlugin();
    let bootstrapTimer;
    try {
      await Promise.race([gm.ready(), new Promise((_, reject) => {
        bootstrapTimer = setTimeout(() => reject(localizedError('error.hostTimeout')), 8000);
      })]);
    } finally { clearTimeout(bootstrapTimer); }
    if (closed) return;
    if (!transfer) {
      transfer = new ImageTransfer(gm);
      listeners.push(gm.device.onConnection(event => {
        connected = event.connected === true;
        if (!connected && operationKind === 'send') cancelWork('error.bluetooth');
        render();
      }));
      listeners.push(gm.on('runtime.lifecycleChanged', ({ state }) => {
        active = state === 'running';
        if (!active && !(state === 'suspended' && pickerOpen)) cancelWork('error.suspended');
        if (active) void gm.device.getInfo().then(info => { connected = info.connected === true; render(); }).catch(error => report(() => errorText(error), true));
        render();
      }));
    }
    let deviceError;
    try {
      if (!subscriptionId) subscriptionId = (await gm.device.subscribeEvents(['connection'])).subscriptionId;
      connected = (await gm.device.getInfo()).connected === true;
    } catch (error) { deviceError = error; }
    active = (await gm.runtime.getLifecycleState()).state === 'running';
    await run(async signal => {
      const savedAlignments = (await gm.storage.get(alignmentKey)).value;
      alignments = Array.isArray(savedAlignments)
        ? savedAlignments.map(value => ALIGNMENTS.includes(value) ? value : 'center') : [];
      const saved = (await gm.storage.get(key)).value;
      if (Array.isArray(saved)) {
        fileIds = Array.from({ length: Math.max(10, saved.length) }, (_, i) => typeof saved[i] === 'string' ? saved[i] : null);
        while (slots.length < fileIds.length) slots.push(null);
        render();
      }
      const { files } = await gm.files.list();
      const failures = [];
      for (let i = 0; i < slots.length; i++) {
        if (slots[i]) continue;
        const id = fileIds[i];
        if (!id) continue;
        const file = files.find(entry => entry.fileId === id);
        try {
          if (!file) throw localizedError('error.missingFile');
          showSlot(i, await prepare(file, signal, slotAlignment(i)));
        } catch (error) { failures.push({ slot: i + 1, error }); }
      }
      report(() => failures.length ? t('notice.restoreErrors', { errors: failures.map(f => t('notice.slotError', { slot: f.slot, error: errorText(f.error) })).join('; ') }) : '', failures.length > 0);
    });
    ready = true; render();
    if (deviceError) report(() => t('notice.deviceError', { error: errorText(deviceError) }), true);
  } finally { initializing = false; render(); }
}
window.addEventListener('pagehide', event => {
  active = false;
  cancelWork('error.closed');
  if (event.persisted) { render(); return; }
  closed = true; ready = false;
  transfer?.close();
  listeners.splice(0).forEach(off => off());
  // The Host releases subscriptions when its session ends. Do not issue new
  // asynchronous Bridge requests while the document/extension is unloading.
  gm?.close();
});
window.addEventListener('pageshow', event => {
  if (event.persisted && !closed) { active = true; render(); void checkConnection(); }
});
void initialize().catch(initializationFailed);
