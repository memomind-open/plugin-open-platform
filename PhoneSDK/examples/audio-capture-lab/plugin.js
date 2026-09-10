import { createGMPlugin, opusRecordingToOgg } from './vendor/gm-plugin-web-sdk.esm.js';
import { AUDIO_LAB_STATE_CHANNEL, encodeAudioLabState } from './device-protocol.js';
import { createI18n } from './i18n.js';
import { StreamMetrics } from './stream-metrics.js';

const i18n = createI18n();
i18n.apply();
const { t } = i18n;
const gm = createGMPlugin();
const element = (id) => document.querySelector(`#${id}`);
const ui = {
  languageToggle: element('language-toggle'), labState: element('lab-state'),
  notice: element('notice'), hostStatus: element('host-status'),
  deviceStatus: element('device-status'), formatStatus: element('format-status'),
  capabilityStatus: element('capability-status'), pickupMode: element('pickup-mode'),
  noiseReduction: element('noise-reduction'), recordingSettings: element('recording-settings'),
  streamSettings: element('stream-settings'), recordingDuration: element('recording-duration'),
  recordingDurationOutput: element('recording-duration-output'), streamProfile: element('stream-profile'),
  streamAdvanced: element('stream-advanced'), customSettings: element('custom-settings'),
  chunkDuration: element('chunk-duration'),
  maxQueue: element('max-queue'), overflowStrategy: element('overflow-strategy'),
  unlimitedDuration: element('unlimited-duration'), streamDurationField: element('stream-duration-field'),
  streamDuration: element('stream-duration'), streamDurationOutput: element('stream-duration-output'),
  consumerDelay: element('consumer-delay'), startCapture: element('start-capture'),
  stopCapture: element('stop-capture'), abortCapture: element('abort-capture'),
  playbackPanel: element('playback-panel'), playRecording: element('play-recording'),
  stopPlayback: element('stop-playback'), recordingPlayer: element('recording-player'), resolvedOptions: element('resolved-options'),
  finalResult: element('final-result'), capabilityJson: element('capability-json'),
  eventLog: element('event-log'), clearEvents: element('clear-events'),
  recordingMetrics: element('recording-metrics'), streamMetrics: element('stream-metrics'),
  recordingMetricDuration: element('recording-metric-duration'),
  recordingMetricFrames: element('recording-metric-frames'),
  recordingMetricBytes: element('recording-metric-bytes'),
  streamDiagnosticsMetrics: element('stream-diagnostics-metrics'),
  metricElapsed: element('metric-elapsed'), metricChunks: element('metric-chunks'),
  metricFrames: element('metric-frames'), metricBytes: element('metric-bytes'),
  metricBitrate: element('metric-bitrate'), metricLatency: element('metric-latency'),
  metricDropped: element('metric-dropped'), metricDiscontinuities: element('metric-discontinuities'),
  lastChunk: element('last-chunk'),
};

const PROFILE_DEFAULTS = Object.freeze({
  interactive: { chunkDurationMs: 40, maxQueueMs: 200, overflowStrategy: 'drop-oldest' },
  balanced: { chunkDurationMs: 100, maxQueueMs: 500, overflowStrategy: 'drop-oldest' },
  reliable: { chunkDurationMs: 100, maxQueueMs: 3000, overflowStrategy: 'error' },
});

const metrics = new StreamMetrics();
let selectedMode = 'recording';
let audioCapability;
let audioAvailable = false;
let deviceConnected = false;
let currentCapture;
let currentCaptureMode;
let captureOpening = false;
let captureAbortController;
let consumePromise;
let operationStartedAt = 0;
let recordingResult;
let recordingUrl;
let playbackActive = false;
let currentState = 'unavailable';
let lastErrorCode;
let lastMetricRender = 0;
let lastDeviceMetricSync = 0;
let deviceSyncTimer;
let deviceSyncInFlight = false;
let deviceSyncPending = false;
let captureGeneration = 0;

for (let duration = 20; duration <= 200; duration += 20) {
  const option = document.createElement('option');
  option.value = String(duration);
  option.textContent = `${duration} ms`;
  ui.chunkDuration.append(option);
}
ui.chunkDuration.value = '100';

function asJson(value) {
  return JSON.stringify(value, (_key, nested) => nested instanceof Uint8Array
    ? `<Uint8Array ${nested.byteLength} bytes>` : nested, 2);
}

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}

function formatElapsed(milliseconds) {
  const seconds = Math.max(0, Math.floor(milliseconds / 1000));
  return `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`;
}

function errorCode(error) {
  return error?.code ?? error?.errorCode ?? 'INTERNAL_ERROR';
}

function setNotice(message, tone = '') {
  ui.notice.textContent = message;
  ui.notice.className = `notice${tone ? ` ${tone}` : ''}`;
}

function setLabState(state, message, code) {
  currentState = state;
  lastErrorCode = state === 'error' ? code ?? 'INTERNAL_ERROR' : undefined;
  ui.labState.dataset.state = state;
  ui.labState.querySelector('strong').textContent = t(`state.${state}`);
  if (message) setNotice(message, state === 'error' ? 'error' : state === 'ready' ? 'success' : '');
  updateControls();
  scheduleDeviceSync();
}

function logEvent(kind, data = {}) {
  const item = document.createElement('li');
  const time = document.createElement('span');
  const label = document.createElement('strong');
  const detail = document.createElement('span');
  time.textContent = new Date().toLocaleTimeString([], { hour12: false });
  label.textContent = kind;
  const serialized = typeof data === 'string' ? data : JSON.stringify(data);
  detail.textContent = serialized.length > 260 ? `${serialized.slice(0, 257)}…` : serialized;
  item.append(time, label, detail);
  ui.eventLog.prepend(item);
  while (ui.eventLog.children.length > 80) ui.eventLog.lastElementChild.remove();
}

function renderRecordingMetrics(result) {
  const active = selectedMode === 'recording' && (captureOpening || currentCaptureMode === 'recording');
  const durationMs = result?.durationMs ?? (active ? currentElapsedMs() : 0);
  ui.recordingMetricDuration.textContent = formatElapsed(durationMs);
  ui.recordingMetricFrames.textContent = Number.isSafeInteger(result?.frameCount)
    ? result.frameCount.toLocaleString() : '—';
  ui.recordingMetricBytes.textContent = Number.isSafeInteger(result?.opusBytes)
    ? formatBytes(result.opusBytes) : '—';
}

function renderStreamMetrics(snapshot = metrics.snapshot()) {
  ui.metricElapsed.textContent = formatElapsed(snapshot.elapsedMs);
  ui.metricChunks.textContent = snapshot.chunkCount.toLocaleString();
  ui.metricFrames.textContent = snapshot.frameCount.toLocaleString();
  ui.metricBytes.textContent = formatBytes(snapshot.byteCount);
  ui.metricBitrate.textContent = `${snapshot.bitrateKbps.toFixed(1)} kbps`;
  ui.metricLatency.textContent = `${snapshot.queueLatencyMs} ms`;
  ui.metricLatency.title = `Maximum observed: ${snapshot.maxQueueLatencyMs} ms`;
  ui.metricDropped.textContent = snapshot.droppedFrameCount.toLocaleString();
  ui.metricDiscontinuities.textContent = snapshot.discontinuityCount.toLocaleString();
  ui.lastChunk.textContent = snapshot.latestSequence === null
    ? t('diagnostics.noChunk')
    : `sequence=${snapshot.latestSequence} / timestampUs=${snapshot.latestTimestampUs} / duration=${snapshot.latestDurationMs}ms / frameLengths=[${snapshot.latestFrameLengths.join(', ')}]`;
}

function currentElapsedMs() {
  return operationStartedAt ? Math.max(0, performance.now() - operationStartedAt) : 0;
}

function deviceStateModel() {
  const snapshot = metrics.snapshot();
  return {
    state: deviceConnected ? currentState : 'unavailable',
    mode: currentCaptureMode ?? selectedMode,
    pickupMode: ui.pickupMode.value,
    profile: selectedMode === 'stream' ? ui.streamProfile.value : 'none',
    noiseReduction: ui.noiseReduction.checked,
    voice: 'original',
    elapsedMs: currentElapsedMs(),
    chunkCount: snapshot.chunkCount,
    droppedFrameCount: snapshot.droppedFrameCount,
    queueLatencyMs: snapshot.queueLatencyMs,
    errorCode: lastErrorCode,
    language: i18n.language,
  };
}

function scheduleDeviceSync(delay = 40) {
  window.clearTimeout(deviceSyncTimer);
  deviceSyncTimer = window.setTimeout(() => void syncDeviceState(), delay);
}

async function syncDeviceState() {
  if (!deviceConnected) return;
  if (deviceSyncInFlight) {
    deviceSyncPending = true;
    return;
  }
  deviceSyncInFlight = true;
  try {
    await gm.plugin.sendMessage(AUDIO_LAB_STATE_CHANNEL, encodeAudioLabState(deviceStateModel()));
  } catch {
    // Device lifecycle events are authoritative; state sync is best effort.
  } finally {
    deviceSyncInFlight = false;
    if (deviceSyncPending) {
      deviceSyncPending = false;
      scheduleDeviceSync(80);
    }
  }
}

function applyProfileDefaults(profile) {
  const defaults = PROFILE_DEFAULTS[profile];
  if (!defaults) return;
  ui.chunkDuration.value = String(defaults.chunkDurationMs);
  ui.maxQueue.value = String(defaults.maxQueueMs);
  ui.overflowStrategy.value = defaults.overflowStrategy;
}

function setMode(mode) {
  if (captureOpening || currentCapture || playbackActive) return;
  selectedMode = mode;
  document.querySelectorAll('.mode-tab').forEach((tab) => tab.classList.toggle('active', tab.dataset.mode === mode));
  ui.recordingSettings.classList.toggle('hidden', mode !== 'recording');
  ui.streamSettings.classList.toggle('hidden', mode !== 'stream');
  ui.playbackPanel.classList.toggle('hidden', mode !== 'recording');
  ui.recordingMetrics.classList.toggle('hidden', mode !== 'recording');
  ui.streamMetrics.classList.toggle('hidden', mode !== 'stream');
  ui.streamDiagnosticsMetrics.classList.toggle('hidden', mode !== 'stream');
  ui.lastChunk.classList.toggle('hidden', mode !== 'stream');
  ui.startCapture.textContent = t(mode === 'recording' ? 'action.startRecording' : 'action.startStream');
  updateControls();
  scheduleDeviceSync();
}

function updateControls() {
  const busy = captureOpening || Boolean(currentCapture) || playbackActive;
  const canStart = audioAvailable && deviceConnected && !busy;
  ui.startCapture.disabled = !canStart;
  ui.stopCapture.disabled = !currentCapture;
  ui.abortCapture.disabled = !captureOpening && !currentCapture;
  ui.playRecording.disabled = !recordingResult || busy;
  ui.stopPlayback.disabled = !playbackActive;
  ui.languageToggle.disabled = busy;
  document.querySelectorAll('.mode-tab').forEach((button) => { button.disabled = busy; });
  [ui.pickupMode, ui.noiseReduction, ui.recordingDuration, ui.streamProfile,
    ui.chunkDuration, ui.maxQueue, ui.overflowStrategy, ui.unlimitedDuration,
    ui.streamDuration, ui.consumerDelay].forEach((control) => { control.disabled = busy || (control === ui.streamDuration && ui.unlimitedDuration.checked); });
}

function buildCaptureOptions(signal) {
  const common = {
    pickupMode: ui.pickupMode.value,
    noiseReduction: ui.noiseReduction.checked,
    codec: 'opus',
    sampleRate: 16000,
    channels: 1,
    signal,
  };
  if (selectedMode === 'recording') {
    return { ...common, maxDurationMs: Number(ui.recordingDuration.value) * 1000 };
  }
  const profile = ui.streamProfile.value;
  const options = {
    ...common,
    profile,
    maxDurationMs: ui.unlimitedDuration.checked ? null : Number(ui.streamDuration.value) * 1000,
  };
  if (profile === 'custom') {
    options.chunkDurationMs = Number(ui.chunkDuration.value);
    options.maxQueueMs = Number(ui.maxQueue.value);
    options.overflowStrategy = ui.overflowStrategy.value;
    if (options.maxQueueMs < options.chunkDurationMs) {
      const validationError = new Error(t('notice.queueInvalid'));
      validationError.code = 'INVALID_REQUEST';
      throw validationError;
    }
  }
  return options;
}

function applyFinalResult(result, mode) {
  if (!result) return;
  ui.finalResult.textContent = asJson(result);
  if (mode === 'recording' && result.data instanceof Uint8Array) {
    recordingResult = result;
    renderRecordingMetrics(result);
  }
  if (mode === 'stream') {
    const snapshot = metrics.snapshot();
    renderStreamMetrics(snapshot);
  }
  updateControls();
}

async function consumeStream(session, generation) {
  logEvent('stream.open', { sessionId: session.sessionId });
  let reader;
  try {
    if (typeof session.stream?.getReader !== 'function') {
      throw new TypeError('Host returned an invalid audio ReadableStream');
    }
    reader = session.stream.getReader();
    while (generation === captureGeneration) {
      const { value: chunk, done } = await reader.read();
      if (done) break;
      if (generation !== captureGeneration) break;
      const delay = Number(ui.consumerDelay.value);
      if (delay > 0) await new Promise((resolve) => window.setTimeout(resolve, delay));
      const snapshot = metrics.add(chunk);
      const now = performance.now();
      if (now - lastMetricRender >= 200) {
        lastMetricRender = now;
        renderStreamMetrics(snapshot);
      }
      if (now - lastDeviceMetricSync >= 1000) {
        lastDeviceMetricSync = now;
        scheduleDeviceSync(0);
      }
      if (chunk.discontinuity) logEvent('stream.drop', { sequence: chunk.sequence, droppedFrameCount: chunk.droppedFrameCount });
    }
    logEvent('stream.end', metrics.snapshot());
  } catch (error) {
    if (generation !== captureGeneration) return;
    logEvent('stream.error', { code: errorCode(error), message: error.message });
    setLabState('error', t('notice.streamFailed', { message: error.message }), errorCode(error));
  } finally {
    reader?.releaseLock?.();
    renderStreamMetrics();
  }
}

async function startCapture() {
  if (!audioAvailable || !deviceConnected || captureOpening || currentCapture || playbackActive
      || document.visibilityState === 'hidden') return;
  const generation = ++captureGeneration;
  recordingResult = undefined;
  metrics.reset();
  renderRecordingMetrics();
  renderStreamMetrics();
  ui.finalResult.textContent = t('empty.result');
  const abortController = new AbortController();
  captureAbortController = abortController;
  let options;
  try {
    options = buildCaptureOptions(abortController.signal);
  } catch (error) {
    captureAbortController = undefined;
    logEvent('capture.error', { code: errorCode(error), message: error.message });
    setLabState('error', error.message, errorCode(error));
    return;
  }
  const requestForDisplay = { ...options };
  delete requestForDisplay.signal;
  captureOpening = true;
  operationStartedAt = performance.now();
  setLabState('starting', t(selectedMode === 'recording'
    ? 'notice.openingRecording' : 'notice.openingStream'));
  logEvent('capture.request', requestForDisplay);
  try {
    const mode = selectedMode;
    const session = mode === 'recording'
      ? await gm.audio.openRecording(options)
      : await gm.audio.openCapture(options);
    if (generation !== captureGeneration || abortController.signal.aborted
        || document.visibilityState === 'hidden') {
      captureOpening = false;
      await session.stop().catch(() => {});
      if (captureAbortController === abortController) captureAbortController = undefined;
      operationStartedAt = 0;
      setLabState('ready', t('notice.aborted'));
      return;
    }
    currentCapture = session;
    currentCaptureMode = mode;
    captureOpening = false;
    ui.resolvedOptions.textContent = asJson(session.resolvedOptions);
    const activeState = mode === 'recording' ? 'recording' : 'streaming';
    setLabState(activeState, t(mode === 'recording'
      ? 'notice.recording' : 'notice.streaming'));
    if (mode === 'stream') consumePromise = consumeStream(session, generation);
  } catch (error) {
    captureOpening = false;
    currentCapture = undefined;
    currentCaptureMode = undefined;
    if (abortController.signal.aborted) {
      operationStartedAt = 0;
      setLabState('ready', t('notice.aborted'));
    } else {
      logEvent('capture.error', { code: errorCode(error), message: error.message });
      setLabState('error', t('notice.captureFailed', { message: error.message }), errorCode(error));
    }
  } finally {
    if (!captureOpening && !currentCapture && captureAbortController === abortController) {
      captureAbortController = undefined;
    }
    updateControls();
  }
}

async function stopCapture(reason = 'user') {
  const session = currentCapture;
  if (!session) return;
  const mode = currentCaptureMode;
  setLabState('stopping', t('notice.stopping'));
  logEvent('capture.stop', { sessionId: session.sessionId, reason });
  try {
    const result = await session.stop();
    applyFinalResult(result, mode);
    if (mode === 'stream') await consumePromise;
    if (currentCapture === session) currentCapture = undefined;
    currentCaptureMode = undefined;
    operationStartedAt = 0;
    setLabState('ready', t(mode === 'recording'
      ? 'notice.recordingDone' : 'notice.streamDone'));
  } catch (error) {
    if (currentCapture === session) currentCapture = undefined;
    currentCaptureMode = undefined;
    operationStartedAt = 0;
    logEvent('capture.error', { code: errorCode(error), message: error.message });
    setLabState('error', t('notice.stopFailed', { message: error.message }), errorCode(error));
  } finally {
    captureAbortController = undefined;
    updateControls();
  }
}

function abortCapture() {
  if (!captureAbortController || captureAbortController.signal.aborted) return;
  logEvent('capture.abort', { mode: selectedMode });
  setLabState('stopping', t('notice.abort'));
  captureAbortController.abort('Audio Capture Lab user abort');
}

async function playRecording() {
  if (!recordingResult || currentCapture || captureOpening || playbackActive) return;
  try {
    playbackActive = true;
    setLabState('playback', t('notice.playback'));
    if (recordingUrl) URL.revokeObjectURL(recordingUrl);
    recordingUrl = URL.createObjectURL(opusRecordingToOgg(recordingResult));
    ui.recordingPlayer.src = recordingUrl;
    logEvent('playback.h5', { frames: recordingResult.frameCount, bytes: recordingResult.opusBytes });
    await ui.recordingPlayer.play();
  } catch (error) {
    playbackActive = false;
    logEvent('playback.error', { code: errorCode(error), message: error.message });
    setLabState('error', t('notice.playbackFailed', { message: error.message }), errorCode(error));
  } finally {
    updateControls();
  }
}

async function stopPlayback() {
  if (!playbackActive) return;
  ui.recordingPlayer.pause();
  playbackActive = false;
  setLabState('ready', t('notice.playbackState', { state: t('playback.stopped') }));
  updateControls();
}

async function stopActiveOperation(reason) {
  if (captureOpening) abortCapture();
  else if (currentCapture) await stopCapture(reason);
  else if (playbackActive) await stopPlayback();
  else setNotice(t('notice.noOperation'));
}

gm.audio.onCaptureState((event) => {
  logEvent('capture.state', event);
  if (event.state === 'starting') setLabState('starting');
  if (event.state === 'capturing') {
    operationStartedAt ||= performance.now();
    setLabState((currentCaptureMode ?? selectedMode) === 'recording' ? 'recording' : 'streaming');
  }
  if (event.state === 'stopping') setLabState('stopping');
  if (event.state === 'stopped') {
    if (currentCapture?.sessionId === event.sessionId) void stopCapture('host-limit');
  }
  if (event.state === 'error') {
    currentCapture = undefined;
    currentCaptureMode = undefined;
    captureOpening = false;
    captureAbortController = undefined;
    operationStartedAt = 0;
    setLabState('error', event.message ?? event.errorCode ?? t('notice.audioFailed'), event.errorCode);
  }
});

ui.recordingPlayer.addEventListener('ended', () => {
  playbackActive = false;
  setLabState('ready', t('notice.playbackState', { state: t('playback.completed') }));
  updateControls();
});

gm.device.onButton((event) => {
  logEvent('device.button', event);
  if (event.action === 'single') void stopActiveOperation('glasses-button');
});

gm.device.onConnection((event) => {
  deviceConnected = event.connected;
  ui.deviceStatus.textContent = t(event.connected ? 'runtime.connected' : 'runtime.disconnected');
  logEvent('device.connection', event);
  if (!event.connected && (captureOpening || currentCapture || playbackActive)) {
    setLabState('error', t('notice.deviceDisconnected'), 'DEVICE_DISCONNECTED');
  } else if (event.connected && audioAvailable && currentState === 'unavailable') {
    setLabState('ready', t('notice.ready'));
  }
  updateControls();
  scheduleDeviceSync(0);
});

document.querySelectorAll('.mode-tab').forEach((button) => button.addEventListener('click', () => setMode(button.dataset.mode)));
ui.recordingDuration.addEventListener('input', () => { ui.recordingDurationOutput.textContent = `${ui.recordingDuration.value} s`; scheduleDeviceSync(); });
ui.streamDuration.addEventListener('input', () => { ui.streamDurationOutput.textContent = `${ui.streamDuration.value} s`; });
ui.unlimitedDuration.addEventListener('change', () => {
  ui.streamDuration.disabled = ui.unlimitedDuration.checked;
  ui.streamDurationField.classList.toggle('disabled', ui.unlimitedDuration.checked);
  updateControls();
});
ui.streamProfile.addEventListener('change', () => {
  const custom = ui.streamProfile.value === 'custom';
  ui.customSettings.classList.toggle('hidden', !custom);
  if (custom) ui.streamAdvanced.open = true;
  else applyProfileDefaults(ui.streamProfile.value);
  scheduleDeviceSync();
});
ui.chunkDuration.addEventListener('change', () => {
  const minimumQueue = Math.max(100, Number(ui.chunkDuration.value));
  ui.maxQueue.min = String(minimumQueue);
  if (Number(ui.maxQueue.value) < minimumQueue) ui.maxQueue.value = String(minimumQueue);
});
[ui.pickupMode, ui.noiseReduction].forEach((control) => control.addEventListener('change', () => scheduleDeviceSync()));
ui.languageToggle.addEventListener('click', () => i18n.toggle());
ui.startCapture.addEventListener('click', () => void startCapture());
ui.stopCapture.addEventListener('click', () => void stopCapture('phone-button'));
ui.abortCapture.addEventListener('click', abortCapture);
ui.playRecording.addEventListener('click', () => void playRecording());
ui.stopPlayback.addEventListener('click', () => void stopPlayback());
ui.clearEvents.addEventListener('click', () => { ui.eventLog.replaceChildren(); });

async function initialize() {
  setMode('recording');
  renderRecordingMetrics();
  renderStreamMetrics();
  try {
    await gm.ready();
    const capabilities = await gm.runtime.getCapabilities();
    audioCapability = capabilities.audio;
    ui.capabilityJson.textContent = audioCapability ? asJson(audioCapability) : t('empty.noCapability');
    audioAvailable = Boolean(audioCapability?.transport === 'message-port'
      && audioCapability?.payload === 'binary-envelope-v1');
    ui.capabilityStatus.textContent = t(audioAvailable ? 'runtime.available' : 'runtime.unavailable');
    if (audioCapability) {
      ui.formatStatus.textContent = `${audioCapability.codec} · ${audioCapability.sampleRate / 1000} kHz · ${audioCapability.channels === 1 ? 'Mono' : `${audioCapability.channels} ch`}`;
    }
    const device = await gm.device.getInfo();
    deviceConnected = Boolean(device.connected);
    ui.deviceStatus.textContent = t(deviceConnected ? 'runtime.connected' : 'runtime.disconnected');
    ui.hostStatus.textContent = device.transport ?? 'Plugin Host';
    await gm.device.subscribeEvents(['button', 'connection']);
    if (!audioAvailable) {
      setLabState('unavailable', t('notice.noAudio'));
    } else if (!deviceConnected) {
      setLabState('unavailable', t('notice.noDevice'));
    } else {
      setLabState('ready', t('notice.ready'));
    }
    logEvent('runtime.ready', { transport: device.transport, audioAvailable, connected: deviceConnected });
    scheduleDeviceSync(0);
  } catch (error) {
    audioAvailable = false;
    ui.capabilityStatus.textContent = t('runtime.unavailable');
    ui.hostStatus.textContent = t('runtime.disconnected');
    logEvent('runtime.error', { code: errorCode(error), message: error.message });
    setLabState('error', t('notice.initFailed', { message: error.message }), errorCode(error));
  }
  updateControls();
}

const telemetryTimer = window.setInterval(() => {
  if (!currentCapture) return;
  if (currentCaptureMode === 'recording') renderRecordingMetrics();
  else renderStreamMetrics();
  if (performance.now() - lastDeviceMetricSync >= 1000) {
    lastDeviceMetricSync = performance.now();
    scheduleDeviceSync(0);
  }
}, 500);

async function cleanup() {
  window.clearInterval(telemetryTimer);
  window.clearTimeout(deviceSyncTimer);
  captureGeneration += 1;
  captureAbortController?.abort('Audio Capture Lab lifecycle ended');
  if (currentCapture) await currentCapture.stop().catch(() => {});
  if (playbackActive) ui.recordingPlayer.pause();
  if (recordingUrl) URL.revokeObjectURL(recordingUrl);
}

window.addEventListener('pagehide', () => { void cleanup(); }, { once: true });
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'hidden') void stopActiveOperation('page-hidden');
});

void initialize();
