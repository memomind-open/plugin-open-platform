import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import {
  DISPLAY_CONTROL_COMMAND_CHANNEL,
  decodeDisplayControlState,
  displayControlOperation,
  encodeDisplayControlCommand,
} from './display-control-protocol.js';
import { createI18n } from './i18n.js';

const gm = createGMPlugin();
const i18n = createI18n();
const COMMAND_TIMEOUT_MS = 3500;
const RESTORE_TIMEOUT_MS = 12000;
const element = (id) => document.querySelector(`#${id}`);
const ui = {
  languageToggle: element('language-toggle'),
  hostStatus: element('host-status'),
  deviceStatus: element('device-status'),
  runtimeMode: element('runtime-mode'),
  notice: element('notice'),
  screenValue: element('screen-value'),
  brightness: element('brightness'),
  brightnessValue: element('brightness-value'),
  height: element('height'),
  heightValue: element('height-value'),
  distance: element('distance'),
  distanceValue: element('distance-value'),
  restore: element('restore'),
};

const state = {
  screenOn: true,
  requestedScreenOn: true,
  previewOnly: false,
  autoBrightnessBlocked: false,
  brightness: 6,
  height: 4,
  distance: 4,
};
const pending = new Map();
let bridgeReady = false;
let deviceConnected = false;
let transport = 'unknown';
let browserPreview = false;
let desktopPreview = false;
let commandRunning = false;
let activeOperation = null;
let requestId = 0;
let subscriptionId;
let offPluginMessage;
let offConnection;
let noticeKey = 'notice.initializing';
let noticeTone = '';
let noticeVariables;

function text(key, variables) {
  return i18n.t(key, variables);
}

function setNotice(key, tone = '', variables) {
  noticeKey = key;
  noticeTone = tone;
  noticeVariables = variables;
  ui.notice.textContent = text(key, variables);
  ui.notice.className = `notice${tone ? ` ${tone}` : ''}`;
}

function runtimeModeKey() {
  if (browserPreview) return 'runtime.browser';
  if (desktopPreview) return 'runtime.desktop';
  return 'runtime.physical';
}

function shownScreenOn() {
  return state.previewOnly ? state.requestedScreenOn : state.screenOn;
}

function renderState() {
  const shownOn = shownScreenOn();
  const value = text(shownOn ? 'screen.onValue' : 'screen.offValue');
  ui.screenValue.textContent = state.previewOnly
    ? text('screen.previewValue', { value })
    : text('screen.current', { value });
  document.querySelectorAll('[data-screen]').forEach((button) => {
    button.classList.toggle('active', Number(button.dataset.screen) === Number(shownOn));
  });
  ui.brightness.value = String(state.brightness);
  ui.height.value = String(state.height);
  ui.distance.value = String(state.distance);
  ui.brightnessValue.textContent = String(state.brightness);
  ui.heightValue.textContent = String(state.height);
  ui.distanceValue.textContent = String(state.distance);
}

function updateAvailability() {
  const available = bridgeReady && (browserPreview || deviceConnected) && !commandRunning;
  document.querySelectorAll('[data-screen], input[type="range"]').forEach((control) => {
    control.disabled = !available;
  });
  ui.restore.disabled = !available;
  const restoring = commandRunning && activeOperation === displayControlOperation.restore;
  ui.restore.textContent = text(restoring ? 'action.restoring' : 'action.restore');
  ui.restore.setAttribute('aria-busy', String(restoring));
  ui.hostStatus.textContent = bridgeReady ? text('runtime.connected') : text('runtime.connecting');
  ui.deviceStatus.textContent = deviceConnected || browserPreview
    ? text('runtime.connected')
    : text('runtime.disconnected');
  ui.runtimeMode.textContent = bridgeReady ? text(runtimeModeKey()) : '—';
}

function renderAll() {
  i18n.apply();
  ui.languageToggle.textContent = text('language.switch');
  renderState();
  updateAvailability();
  ui.notice.textContent = text(noticeKey, noticeVariables);
  ui.notice.className = `notice${noticeTone ? ` ${noticeTone}` : ''}`;
}

function nextRequestId() {
  requestId = (requestId + 1) >>> 0;
  if (requestId === 0) requestId = 1;
  return requestId;
}

function acceptState(next) {
  Object.assign(state, {
    screenOn: next.screenOn,
    requestedScreenOn: next.requestedScreenOn,
    previewOnly: next.previewOnly,
    autoBrightnessBlocked: next.autoBrightnessBlocked,
    brightness: next.brightness,
    height: next.height,
    distance: next.distance,
  });
  renderState();
  const wokeFromGlasses = next.requestId === 0 &&
    next.lastOperation === displayControlOperation.setScreen &&
    (next.previewOnly ? next.requestedScreenOn : next.screenOn);
  if (wokeFromGlasses) setNotice('notice.woke', 'success');
  const waiter = pending.get(next.requestId);
  if (waiter) {
    pending.delete(next.requestId);
    clearTimeout(waiter.timer);
    if (next.statusCode === 0) waiter.resolve(next);
    else waiter.reject(new Error(next.status));
  }
}

function waitForState(id, timeoutMs = COMMAND_TIMEOUT_MS) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error('GLASSES_RESPONSE_TIMEOUT'));
    }, timeoutMs);
    pending.set(id, { resolve, reject, timer });
  });
}

function simulateCommand(operation, value) {
  state.previewOnly = true;
  if (operation === displayControlOperation.setScreen) {
    state.requestedScreenOn = value === 1;
    state.screenOn = true;
  } else if (operation === displayControlOperation.setBrightness) {
    state.brightness = value;
    state.autoBrightnessBlocked = true;
  } else if (operation === displayControlOperation.setHeight) {
    state.height = value;
  } else if (operation === displayControlOperation.setDistance) {
    state.distance = value;
  } else if (operation === displayControlOperation.restore) {
    Object.assign(state, {
      screenOn: true,
      requestedScreenOn: true,
      brightness: 6,
      height: 4,
      distance: 4,
      autoBrightnessBlocked: false,
    });
  }
  renderState();
  return { ...state, statusCode: 0, status: 'OK', operation };
}

async function sendCommand(operation, value = 0) {
  if (browserPreview) return simulateCommand(operation, value);
  const id = nextRequestId();
  const timeoutMs = operation === displayControlOperation.restore
    ? RESTORE_TIMEOUT_MS : COMMAND_TIMEOUT_MS;
  const response = waitForState(id, timeoutMs);
  try {
    await gm.plugin.sendMessage(DISPLAY_CONTROL_COMMAND_CHANNEL,
      encodeDisplayControlCommand({
        requestId: id,
        operation,
        value,
        previewOnly: desktopPreview && operation === displayControlOperation.setScreen,
      }));
    return await response;
  } catch (error) {
    const waiter = pending.get(id);
    if (waiter) {
      clearTimeout(waiter.timer);
      pending.delete(id);
    }
    throw error;
  }
}

async function runCommand(operation, value = 0) {
  if (commandRunning) return;
  commandRunning = true;
  activeOperation = operation;
  if (operation === displayControlOperation.restore) {
    setNotice('notice.restoring');
  }
  updateAvailability();
  try {
    const result = await sendCommand(operation, value);
    setNotice(result.previewOnly || browserPreview || desktopPreview
      ? 'notice.preview' : 'notice.sent', 'success');
  } catch (error) {
    setNotice('notice.failed', 'error', { error: error.message });
    if (operation !== displayControlOperation.query && !browserPreview) {
      await sendCommand(displayControlOperation.query).catch(() => undefined);
    }
  } finally {
    commandRunning = false;
    activeOperation = null;
    updateAvailability();
  }
}

function bindRange(input, output, operation) {
  input.addEventListener('input', () => { output.textContent = input.value; });
  input.addEventListener('change', () => void runCommand(operation, Number(input.value)));
}

document.querySelectorAll('[data-screen]').forEach((button) => {
  button.addEventListener('click', () => {
    void runCommand(displayControlOperation.setScreen, Number(button.dataset.screen));
  });
});
bindRange(ui.brightness, ui.brightnessValue, displayControlOperation.setBrightness);
bindRange(ui.height, ui.heightValue, displayControlOperation.setHeight);
bindRange(ui.distance, ui.distanceValue, displayControlOperation.setDistance);
ui.restore.addEventListener('click', () => void runCommand(displayControlOperation.restore));
ui.languageToggle.addEventListener('click', () => {
  i18n.toggle();
  renderAll();
});

async function refreshFromGlasses() {
  if (browserPreview) {
    simulateCommand(displayControlOperation.query, 0);
    setNotice('notice.preview', 'success');
    return;
  }
  if (!deviceConnected) {
    setNotice('notice.unavailable', 'error');
    return;
  }
  await runCommand(displayControlOperation.query);
}

async function initialize() {
  renderAll();
  try {
    await gm.ready();
    offPluginMessage = gm.plugin.onMessage((message) => {
      const next = decodeDisplayControlState(message);
      if (next) acceptState(next);
    });
    offConnection = gm.device.onConnection((event) => {
      deviceConnected = event.connected === true;
      updateAvailability();
      if (!bridgeReady) return;
      if (deviceConnected) void refreshFromGlasses();
      else setNotice('notice.unavailable', 'error');
    });
    subscriptionId = (await gm.device.subscribeEvents(['connection'])).subscriptionId;
    const info = await gm.device.getInfo();
    transport = String(info.transport ?? 'unknown').toLowerCase();
    browserPreview = transport === 'studio';
    desktopPreview = transport === 'desktop-simulator' || transport.includes('simulator');
    deviceConnected = info.connected === true;
    bridgeReady = true;
    updateAvailability();
    setNotice('notice.ready', 'success');
    await refreshFromGlasses();
  } catch (error) {
    bridgeReady = false;
    updateAvailability();
    setNotice('notice.failed', 'error', { error: error.message });
  }
}

window.addEventListener('pagehide', () => {
  for (const waiter of pending.values()) {
    clearTimeout(waiter.timer);
    waiter.reject(new Error('RUNTIME_CLOSED'));
  }
  pending.clear();
  offPluginMessage?.();
  offConnection?.();
  if (subscriptionId) void gm.device.unsubscribeEvents(subscriptionId).catch(() => undefined);
  gm.close();
});

void initialize();
