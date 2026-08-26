import {
  decodeBase64,
  decodeGlassesMessage,
  DISPLAY_CAPABILITIES,
  encodePluginBridgeEventData,
  encodeSceneMessage,
  EVENT_CAPABILITIES,
  PLUGIN_TRANSPORT,
  SCENE_CHANNELS,
} from './scene-protocol.js';
import {
  chooseDevicePlugin,
  describeWorkspace,
  evaluateCompatibility,
  WEB_BRIDGE_PLUGIN_ID,
} from './workspace-mode.js';
import {
  hasBridgePermission,
  isTrustedPluginMessage,
  storageNamespace,
} from './bridge-security.js';
import { drawTextOverlays } from './text-overlay.js';

const { invoke } = window.__TAURI__.core;

const frame = document.querySelector('#plugin-frame');
const webEmptyState = document.querySelector('#web-empty-state');
const canvas = document.querySelector('#device-canvas');
const context = canvas.getContext('2d', { alpha: false });
const logs = document.querySelector('#logs');
const webPlugin = document.querySelector('#web-plugin');
const devicePlugin = document.querySelector('#device-plugin');
const webState = document.querySelector('#web-state');
const deviceState = document.querySelector('#device-state');
const pairStatus = document.querySelector('#pair-status');
const shareButton = document.querySelector('#share-web');
const shareDialog = document.querySelector('#share-dialog');
const shareQr = document.querySelector('#share-qr');
const subscriptions = new Map();
const outboundWaiters = new Set();
const storageNamespaces = new Map();
const importedWebPlugins = new Map();
const importedDevicePlugins = new Map();
const deviceEventButtons = [...document.querySelectorAll('[data-button-action], [data-gesture]')];
const runtimeGeneration = 1;
let sessionToken = crypto.randomUUID();
let frameOrigin = null;
let activeWebPlugin = null;
let subscriptionSequence = 0;
let deviceRunning = false;
let runningDevicePath = '';
let webActive = false;
let webDiscoveryComplete = false;
let deviceSelectionExplicit = false;
let availableWebPlugins = [];
let availableDevicePlugins = [];
let currentCompatibility = null;
let framePending = false;
let renderedFrames = 0;
let fpsWindow = performance.now();

document.querySelector('#refresh-web').addEventListener('click', discoverWebPlugins);
document.querySelector('#refresh-device').addEventListener('click', discoverDevicePlugins);
webPlugin.addEventListener('change', handleWebSelectionChange);
devicePlugin.addEventListener('change', handleDeviceSelectionChange);
document.querySelector('#import-web-workspace').addEventListener('click', () => importWebPlugin('pick_web_directory', '外部工作区'));
document.querySelector('#import-web-file').addEventListener('click', () => importWebPlugin('pick_web_package', '外部包'));
document.querySelector('#import-device-workspace').addEventListener('click', importDeviceWorkspace);
document.querySelector('#import-device-file').addEventListener('click', importDeviceFile);
document.querySelector('#run-web').addEventListener('click', runWebPlugin);
shareButton.addEventListener('click', buildAndShareWebPlugin);
document.querySelector('#run-device').addEventListener('click', runDevicePlugin);
document.querySelector('#share-device').addEventListener('click', shareDevicePlugin);
document.querySelector('#stop-device').addEventListener('click', stopDevicePlugin);
document.querySelector('#close-share').addEventListener('click', () => shareDialog.close());
document.querySelector('#stop-share').addEventListener('click', stopWebPackageShare);
document.querySelector('#clear-log').addEventListener('click', () => logs.replaceChildren());
for (const button of document.querySelectorAll('[data-button-action]')) {
  button.addEventListener('click', () => simulateDeviceEvent('simulate_button', {
    action: Number(button.dataset.buttonAction),
  }));
}
for (const button of document.querySelectorAll('[data-gesture]')) {
  button.addEventListener('click', () => simulateDeviceEvent('simulate_gesture', {
    gesture: Number(button.dataset.gesture), active: true,
  }));
}
frame.addEventListener('load', () => {
  if (!webActive) return;
  webState.textContent = '运行中';
  postBootstrap();
});

async function discoverWebPlugins() {
  const previous = webPlugin.value;
  webPlugin.disabled = true;
  webPlugin.replaceChildren(new Option('正在识别…', ''));
  try {
    const plugins = await invoke('discover_web_plugins');
    availableWebPlugins = plugins;
    webPlugin.replaceChildren(new Option('不加载 Web 插件（仅设备调试）', ''));
    for (const plugin of plugins) {
      const option = new Option(`${plugin.name}  ${plugin.version}`, plugin.path);
      option.dataset.id = plugin.id;
      webPlugin.add(option);
    }
    for (const [path, label] of importedWebPlugins) addOption(webPlugin, path, label);
    const hasPrevious = webDiscoveryComplete && [...webPlugin.options]
      .some((option) => option.value === previous);
    webPlugin.value = hasPrevious ? previous : '';
    webDiscoveryComplete = true;
    await handleWebSelectionChange();
  } catch (error) {
    webPlugin.replaceChildren(new Option('不加载 Web 插件（识别失败）', ''));
    shareButton.disabled = true;
    webState.textContent = '识别失败';
    log('WEB DISCOVERY ERROR', String(error));
  } finally {
    webPlugin.disabled = false;
  }
}

async function importWebPlugin(command, kind) {
  try {
    const path = await invoke(command);
    if (!path) return;
    const plugin = await invoke('inspect_web_plugin', { path });
    const label = `${plugin.name}  ${plugin.version} · ${kind}`;
    importedWebPlugins.set(plugin.path, label);
    availableWebPlugins = [...availableWebPlugins.filter((item) => item.path !== plugin.path), plugin];
    addOption(webPlugin, plugin.path, label);
    webPlugin.value = plugin.path;
    await handleWebSelectionChange();
  } catch (error) {
    log('WEB IMPORT ERROR', String(error));
  }
}

async function discoverDevicePlugins() {
  const previous = devicePlugin.value;
  devicePlugin.disabled = true;
  devicePlugin.replaceChildren(new Option('正在识别…', ''));
  try {
    const plugins = await invoke('discover_device_plugins');
    devicePlugin.replaceChildren();
    addDeviceOptions(plugins, false);
    for (const plugin of importedDevicePlugins.values()) addDeviceOption(plugin, true);
    availableDevicePlugins = mergeDevicePlugins(plugins, [...importedDevicePlugins.values()]);
    if (availableDevicePlugins.length === 0) {
      devicePlugin.add(new Option('尚未构建设备插件，请导入 .gmp', ''));
      document.querySelector('#share-device').disabled = true;
      deviceState.textContent = '缺少设备插件';
      updateDeviceControls();
      updatePairStatus();
      return;
    }
    devicePlugin.value = chooseDevicePlugin(availableDevicePlugins, {
      previousPath: previous,
      explicitSelection: deviceSelectionExplicit,
      webEnabled: Boolean(webPlugin.value),
      webPlugin: selectedWebPlugin(),
    });
    document.querySelector('#share-device').disabled = false;
    updateDeviceCompatibility();
    await runDevicePlugin();
  } catch (error) {
    devicePlugin.replaceChildren(new Option('识别失败', ''));
    log('DEVICE DISCOVERY ERROR', String(error));
  } finally {
    devicePlugin.disabled = false;
  }
}

async function importDeviceWorkspace() {
  try {
    const path = await invoke('pick_device_directory');
    if (!path) return;
    const plugins = await invoke('import_device_workspace', { path });
    addDeviceOptions(plugins, true);
    if (plugins[0]) {
      availableDevicePlugins = mergeDevicePlugins(availableDevicePlugins, plugins);
      deviceSelectionExplicit = true;
      devicePlugin.value = plugins[0].path;
      updateDeviceCompatibility();
      await runDevicePlugin();
    }
  } catch (error) {
    log('DEVICE WORKSPACE IMPORT ERROR', String(error));
  }
}

async function importDeviceFile() {
  try {
    const path = await invoke('pick_device_plugin');
    if (!path) return;
    const plugins = await invoke('import_device_workspace', { path });
    addDeviceOptions(plugins, true);
    availableDevicePlugins = mergeDevicePlugins(availableDevicePlugins, plugins);
    deviceSelectionExplicit = true;
    devicePlugin.value = plugins[0].path;
    updateDeviceCompatibility();
    await runDevicePlugin();
  } catch (error) {
    log('DEVICE FILE IMPORT ERROR', String(error));
  }
}

function addDeviceOptions(plugins, imported) {
  for (const plugin of plugins) addDeviceOption(plugin, imported);
}

function addDeviceOption(plugin, imported) {
  if (imported) importedDevicePlugins.set(plugin.path, plugin);
  const version = plugin.version ? `  v${plugin.version}` : '';
  const defaultRole = plugin.id === WEB_BRIDGE_PLUGIN_ID ? '  · Web 默认桥接' : '';
  const option = addOption(devicePlugin, plugin.path,
    `${plugin.name}${version}${defaultRole}${imported ? '  · 外部' : ''}`);
  option.dataset.id = plugin.id;
  option.dataset.baseLabel = option.textContent;
}

function addOption(select, value, label) {
  const existing = [...select.options].find((option) => option.value === value);
  if (existing) {
    existing.textContent = label;
    return existing;
  }
  const option = new Option(label, value);
  select.add(option);
  return option;
}

function mergeDevicePlugins(...groups) {
  return [...new Map(groups.flat().map((plugin) => [plugin.path, plugin])).values()];
}

function fileName(path) {
  return path.split(/[\\/]/u).filter(Boolean).at(-1) ?? path;
}

async function handleWebSelectionChange() {
  if (!webPlugin.value) {
    disableWebPlugin();
    updateDeviceCompatibility();
    return;
  }
  const web = selectedWebPlugin();
  if (!deviceSelectionExplicit && availableDevicePlugins.length > 0) {
    const fallback = chooseDevicePlugin(availableDevicePlugins, {
      previousPath: devicePlugin.value,
      webEnabled: true,
      webPlugin: web,
    });
    if (fallback) devicePlugin.value = fallback;
  }
  updateDeviceCompatibility();
  if (devicePlugin.value && (!deviceRunning || runningDevicePath !== devicePlugin.value)) {
    await runDevicePlugin();
  }
  await runWebPlugin();
}

async function handleDeviceSelectionChange() {
  deviceSelectionExplicit = true;
  updateDeviceCompatibility();
  await runDevicePlugin();
}

function selectedWebPlugin() {
  return availableWebPlugins.find((plugin) => plugin.path === webPlugin.value) ?? null;
}

function updateDeviceCompatibility() {
  const status = document.querySelector('#device-compatibility');
  const web = selectedWebPlugin();
  currentCompatibility = null;
  for (const option of devicePlugin.options) {
    const base = option.dataset.baseLabel ?? option.textContent;
    option.dataset.baseLabel = base;
    const device = availableDevicePlugins.find((plugin) => plugin.path === option.value);
    if (!web || !device) {
      option.textContent = base;
      continue;
    }
    const result = evaluateCompatibility(web, device);
    const marker = result.status === 'recommended' ? '推荐' :
      result.status === 'compatible' ? '兼容' :
      result.status === 'incompatible' ? '不兼容' : '未声明依赖';
    option.textContent = `${base}  · ${marker}`;
    if (option.value === devicePlugin.value) currentCompatibility = result;
  }
  status.classList.remove('compatible', 'incompatible');
  if (!webPlugin.value) {
    status.textContent = '仅设备调试：不需要 Web 配对校验';
  } else if (!currentCompatibility || currentCompatibility.status === 'unknown') {
    status.textContent = '该 Web 插件未声明设备依赖，无法预检配对';
  } else if (currentCompatibility.compatible) {
    status.textContent = currentCompatibility.preferred ? '推荐配对：设备协议完全匹配' : '设备协议兼容';
    status.classList.add('compatible');
  } else {
    status.textContent = `配对不兼容：${currentCompatibility.reasons.join('；')}`;
    status.classList.add('incompatible');
  }
  updatePairStatus();
}

function disableWebPlugin() {
  webActive = false;
  activeWebPlugin = null;
  frameOrigin = null;
  subscriptions.clear();
  frame.hidden = true;
  webEmptyState.hidden = false;
  frame.src = 'about:blank';
  webState.textContent = '未启用 · 仅设备调试';
  shareButton.disabled = true;
  document.querySelector('#run-web').disabled = true;
  updatePairStatus();
}

async function buildAndShareWebPlugin() {
  if (!webPlugin.value) return;
  shareButton.disabled = true;
  shareButton.textContent = '正在打包…';
  try {
    const result = await invoke('build_and_share_web_plugin', { path: webPlugin.value });
    showPackageShare(result, '扫码安装 Web 插件', '请在支持 Web 插件安装的 App 调试入口扫码。');
    log('WEB PACKAGE READY', { name: result.name, address: `${result.host}:${result.port}` });
  } catch (error) {
    log('WEB PACKAGE ERROR', String(error));
  } finally {
    shareButton.disabled = false;
    shareButton.textContent = '打包 / 扫码安装';
  }
}

async function shareDevicePlugin() {
  if (!devicePlugin.value) return;
  const button = document.querySelector('#share-device');
  button.disabled = true;
  button.textContent = '正在生成…';
  try {
    const result = await invoke('share_device_plugin', { path: devicePlugin.value });
    showPackageShare(result, '扫码安装设备插件', '请在支持设备插件安装的 App 调试入口扫码，App 将通过局域网下载该 .gmp。');
    log('DEVICE PACKAGE READY', { name: result.name, address: `${result.host}:${result.port}` });
  } catch (error) {
    log('DEVICE PACKAGE ERROR', String(error));
  } finally {
    button.disabled = false;
    button.textContent = '扫码安装';
  }
}

function showPackageShare(result, title, help) {
  drawQrCode(result.qrSize, result.qrModules);
  document.querySelector('#share-title').textContent = title;
  document.querySelector('#share-help').textContent = `手机与电脑需连接同一局域网。${help}`;
  document.querySelector('#share-name').textContent = result.name;
  document.querySelector('#share-address').textContent = `${result.host}:${result.port}`;
  document.querySelector('#share-path').textContent = result.packagePath;
  document.querySelector('#share-payload').textContent = result.qrPayload;
  if (!shareDialog.open) shareDialog.showModal();
}

function drawQrCode(size, modules) {
  const border = 4;
  const canvasSize = shareQr.width;
  const scale = Math.floor(canvasSize / (size + border * 2));
  const offset = Math.floor((canvasSize - (size + border * 2) * scale) / 2);
  const qrContext = shareQr.getContext('2d', { alpha: false });
  qrContext.fillStyle = '#fff';
  qrContext.fillRect(0, 0, canvasSize, canvasSize);
  qrContext.fillStyle = '#000';
  for (let y = 0; y < size; y += 1) {
    for (let x = 0; x < size; x += 1) {
      if (!modules[y * size + x]) continue;
      qrContext.fillRect(offset + (x + border) * scale, offset + (y + border) * scale, scale, scale);
    }
  }
}

async function stopWebPackageShare() {
  try {
    await invoke('stop_web_package_share');
    shareDialog.close();
    log('PACKAGE SHARE', '局域网共享已停止');
  } catch (error) {
    log('WEB PACKAGE ERROR', String(error));
  }
}

window.addEventListener('message', async (message) => {
  if (!isTrustedPluginMessage(message, frame.contentWindow, frameOrigin)) return;
  if (message.data?.type === 'gm-plugin:bootstrap-request') {
    postBootstrap();
    return;
  }
  if (message.data?.type !== 'gm-plugin:request') return;
  const request = message.data.request;
  log('REQUEST', { method: request?.method, params: summarize(request?.params) });
  const response = await handleBridgeRequest(request);
  log(response.ok ? 'RESPONSE' : 'ERROR', response.ok ? response.result : response.error);
  frame.contentWindow?.postMessage({ type: 'gm-plugin:response', response }, frameOrigin);
});

async function runWebPlugin() {
  if (!webPlugin.value) {
    disableWebPlugin();
    return;
  }
  try {
    const web = selectedWebPlugin();
    if (!web) throw new Error('Selected Web plugin is unavailable');
    const entry = await invoke('resolve_web_entry', { path: webPlugin.value });
    activeWebPlugin = web;
    frameOrigin = new URL(entry).origin;
    sessionToken = crypto.randomUUID();
    subscriptions.clear();
    webActive = true;
    frame.hidden = false;
    webEmptyState.hidden = true;
    shareButton.disabled = false;
    document.querySelector('#run-web').disabled = false;
    webState.textContent = '加载中';
    const entryUrl = new URL(entry);
    entryUrl.searchParams.set('studioGeneration', Date.now());
    entryUrl.searchParams.set('studioOrigin', window.location.origin);
    frame.src = entryUrl.href;
    updatePairStatus();
  } catch (error) {
    webActive = false;
    activeWebPlugin = null;
    frameOrigin = null;
    frame.hidden = true;
    webEmptyState.hidden = false;
    webState.textContent = '加载失败';
    updatePairStatus();
    log('WEB ERROR', String(error));
  }
}

async function runDevicePlugin() {
  if (!devicePlugin.value) return;
  try {
    const status = await invoke('load_device_plugin', { path: devicePlugin.value });
    deviceRunning = status.running;
    runningDevicePath = status.source ?? devicePlugin.value;
    deviceState.textContent = status.running ? '运行中' : '已加载';
    updateDeviceControls();
    emitConnection();
    updatePairStatus();
  } catch (error) {
    deviceRunning = false;
    runningDevicePath = '';
    deviceState.textContent = '加载失败';
    updateDeviceControls();
    updatePairStatus();
    log('DEVICE ERROR', String(error));
  }
}

async function stopDevicePlugin() {
  try {
    const status = await invoke('stop_device_plugin');
    deviceRunning = status.running;
    deviceState.textContent = status.loaded ? '已停止' : '未加载';
    emitConnection();
    updateDeviceControls();
    updatePairStatus();
  } catch (error) {
    log('DEVICE ERROR', String(error));
  }
}

async function simulateDeviceEvent(command, params) {
  try {
    const result = await invoke(command, params);
    if (!result.handled) throw bridgeError('CAPABILITY_UNAVAILABLE', 'Device plugin did not handle the simulated event');
  } catch (error) {
    log('SIMULATION ERROR', normalizeBridgeError(error));
  }
}

async function handleBridgeRequest(request) {
  const response = { requestId: request?.requestId ?? 'invalid', runtimeGeneration };
  try {
    validateEnvelope(request);
    response.result = await dispatch(request.method, request.params);
    response.ok = true;
  } catch (error) {
    response.ok = false;
    response.error = normalizeBridgeError(error);
  }
  return response;
}

async function dispatch(method, params) {
  if (!hasBridgePermission(method, activeWebPlugin?.permissions)) {
    throw bridgeError('UNAUTHORIZED', `Plugin manifest does not grant ${method}`);
  }
  const storage = () => storageNamespace(storageNamespaces, activeWebPlugin?.id);
  switch (method) {
    case 'runtime.ready':
    case 'runtime.ping': return { ready: true, generation: runtimeGeneration };
    case 'runtime.getBridgeVersion': return { version: '1.0' };
    case 'runtime.getCapabilities': return {
      display: [...DISPLAY_CAPABILITIES], events: [...EVENT_CAPABILITIES],
      rawImuDefaultEnabled: false, pluginMessaging: { maxPayloadBytes: 81901 },
    };
    case 'runtime.getLifecycleState': return { state: 'running' };
    case 'device.getInfo': return { connected: deviceRunning, transport: 'desktop-simulator' };
    case 'device.subscribeEvents': return subscribe(params.types);
    case 'device.unsubscribeEvents': return { removed: subscriptions.delete(requireString(params, 'subscriptionId')) };
    case 'plugin.sendMessage': return sendPluginMessage(params);
    case 'display.createPage': return createPage();
    case 'display.closePage':
    case 'display.updateText':
    case 'display.updateImage':
    case 'display.updateImageLz4':
    case 'display.beginFrame':
    case 'display.updateFrameImageLz4': return sendDisplayMessage(method, params);
    case 'display.rebuildPage': return rebuildPage(params);
    case 'storage.get': return { value: storage().get(requireString(params, 'key')) ?? null };
    case 'storage.set': storage().set(requireString(params, 'key'), structuredClone(params.value)); return { stored: true };
    case 'storage.remove': return { removed: storage().delete(requireString(params, 'key')) };
    case 'storage.clear': storage().clear(); return { cleared: true };
    default: throw bridgeError('METHOD_NOT_FOUND', `Desktop simulator does not support ${method}`);
  }
}

async function sendPluginMessage(params) {
  if (!deviceRunning) throw bridgeError('DEVICE_DISCONNECTED', 'Device plugin is not running');
  const channel = params?.channel;
  if (!Number.isInteger(channel) || channel < 0 || channel > 0xffff) {
    throw bridgeError('INVALID_REQUEST', 'channel must be uint16');
  }
  const payload = decodeBase64(params?.dataBase64);
  if (payload.length === 0) throw bridgeError('INVALID_REQUEST', 'payload must not be empty');
  if (payload.length > 81901) throw bridgeError('PAYLOAD_TOO_LARGE', 'payload exceeds 81901 bytes');
  return sendNativeMessage(channel, payload);
}

async function createPage() {
  const nonce = crypto.getRandomValues(new Uint8Array(12));
  const pending = prepareOutbound(
    (message) => message.kind === 'ping' && equalBytes(message.payload, nonce),
    3000,
    'Scene Bridge ping timed out',
  );
  try { await sendNativeMessage(SCENE_CHANNELS.ping, nonce); }
  catch (error) { pending.cancel(); throw error; }
  await pending.promise;
  return { created: true };
}

async function sendDisplayMessage(method, params) {
  const message = encodeSceneMessage(method, params);
  let pending;
  if (message.ack) {
    pending = prepareOutbound(
      (candidate) => candidate.kind === 'frameStatus' &&
        candidate.frameId === message.ack.frameId && candidate.tileIndex === message.ack.tileIndex,
      4000,
      `Frame ACK timed out for ${message.ack.frameId}:${message.ack.tileIndex}`,
    );
  }
  try { await sendNativeMessage(message.channel, message.payload); }
  catch (error) { pending?.cancel(); throw error; }
  if (pending) {
    const status = await pending.promise;
    if (status.status !== 0) {
      throw bridgeError('CAPABILITY_UNAVAILABLE', `Device frame status is ${status.status}`);
    }
    if (status.nextIndex !== message.ack.nextIndex) {
      throw bridgeError('INTERNAL_ERROR', `Device expected tile ${status.nextIndex}`);
    }
    return {
      frameId: status.frameId, tileIndex: status.tileIndex,
      nextIndex: status.nextIndex, complete: status.complete,
    };
  }
  if (method === 'display.closePage') return { closed: true };
  return { presented: true };
}

async function rebuildPage(params) {
  if (!Array.isArray(params?.operations) || params.operations.length > 256) {
    throw bridgeError('INVALID_REQUEST', 'operations must be a bounded list');
  }
  await sendDisplayMessage('display.closePage', {});
  for (const operation of params.operations) {
    if (operation?.type === 'text') await sendDisplayMessage('display.updateText', operation);
    else if (operation?.type === 'image') await sendDisplayMessage('display.updateImage', operation);
    else throw bridgeError('INVALID_REQUEST', 'unknown rebuild operation type');
  }
  return { presented: true, operationCount: params.operations.length };
}

async function sendNativeMessage(channel, payload) {
  if (!deviceRunning) throw bridgeError('DEVICE_DISCONNECTED', 'Device plugin is not running');
  const result = await invoke('send_plugin_message', { channel, payload: [...payload] });
  if (result.service !== PLUGIN_TRANSPORT.service ||
      result.command !== PLUGIN_TRANSPORT.phoneToGlassesCommand) {
    throw bridgeError('INTERNAL_ERROR', 'Studio returned an invalid phone-to-glasses route');
  }
  if (!result.handled) throw bridgeError('CAPABILITY_UNAVAILABLE', `Device plugin did not handle channel ${channel}`);
  return result;
}

function subscribe(types) {
  const supported = new Set(EVENT_CAPABILITIES);
  if (!Array.isArray(types) || types.length === 0 || types.length > supported.size ||
      types.some((type) => typeof type !== 'string' || !supported.has(type))) {
    throw bridgeError('INVALID_REQUEST', 'types must be a non-empty supported event list');
  }
  const id = `sub-${++subscriptionSequence}`;
  subscriptions.set(id, new Set(types));
  queueMicrotask(emitConnection);
  return { subscriptionId: id };
}

function emitConnection() {
  emitDeviceEvent('device.connection', {
    connected: deviceRunning, source: 'desktop-simulator', timestampMs: Date.now(),
  });
}

function emitDeviceEvent(name, data) {
  if (!webActive) return;
  const type = name.replace('device.', '');
  const ids = [...subscriptions].filter(([, types]) => types.has(type)).map(([id]) => id);
  if (ids.length === 0) return;
  frame.contentWindow?.postMessage({
    type: 'gm-plugin:event',
    event: { name, subscriptionIds: ids, data, runtimeGeneration },
  }, frameOrigin);
}

function emitPluginMessage(message) {
  if (!webActive) return;
  frame.contentWindow?.postMessage({
    type: 'gm-plugin:event',
    event: {
      name: 'plugin.message',
      data: encodePluginBridgeEventData(message),
      runtimeGeneration,
    },
  }, frameOrigin);
}

function validateEnvelope(request) {
  if (!request || request.version !== '1.0' || typeof request.requestId !== 'string' ||
      request.sessionToken !== sessionToken || request.runtimeGeneration !== runtimeGeneration ||
      typeof request.method !== 'string' || !request.params || typeof request.params !== 'object') {
    throw bridgeError('INVALID_REQUEST', 'Bridge request envelope is invalid');
  }
}

function postBootstrap() {
  frame.contentWindow?.postMessage({
    type: 'gm-plugin:bootstrap',
    bootstrap: { sessionToken, runtimeGeneration },
  }, frameOrigin);
}

function drawFrame(result) {
  const binary = atob(result.gray4Base64);
  const image = context.createImageData(result.width, result.height);
  for (let index = 0; index < binary.length; index += 1) {
    const packed = binary.charCodeAt(index);
    writeGreenPixel(image.data, index * 2, packed >> 4);
    writeGreenPixel(image.data, index * 2 + 1, packed & 0x0f);
  }
  context.putImageData(image, 0, 0);
  drawTextOverlays(context, result.textOverlays, result.width, result.height);
  renderedFrames += 1;
  const now = performance.now();
  if (now - fpsWindow >= 1000) {
    document.querySelector('#frame-rate').textContent = `${renderedFrames} FPS`;
    renderedFrames = 0;
    fpsWindow = now;
  }
}

function writeGreenPixel(output, pixel, level) {
  const offset = pixel * 4;
  output[offset] = Math.round(level * 2.2);
  output[offset + 1] = Math.round(level * 16.5);
  output[offset + 2] = Math.round(level * 8.5);
  output[offset + 3] = 255;
}

setInterval(async () => {
  if (framePending) return;
  framePending = true;
  try {
    const result = await invoke('tick_frame', { elapsedMs: 33 });
    const wasRunning = deviceRunning;
    deviceRunning = result.running;
    if (deviceRunning !== wasRunning) {
      updateDeviceControls();
      updatePairStatus();
    }
    for (const message of result.messages) consumeOutboundMessage(message);
    drawFrame(result);
  } catch (error) {
    log('FRAME ERROR', String(error));
  } finally {
    framePending = false;
  }
}, 33);

function consumeOutboundMessage(encoded) {
  try {
    const message = decodeGlassesMessage(encoded);
    for (const waiter of outboundWaiters) {
      if (!waiter.predicate(message)) continue;
      outboundWaiters.delete(waiter);
      clearTimeout(waiter.timer);
      waiter.resolve(message);
      return;
    }
    if (message.kind === 'event') {
      emitDeviceEvent(message.name, {
        ...message.data, sequence: message.sequence,
        timestampMs: message.timestampMs, source: 'devicePlugin',
      });
    } else if (message.kind === 'plugin') {
      log('DEVICE MESSAGE', { channel: message.channel, payloadBytes: message.payload.length });
      emitPluginMessage(message);
    }
  } catch (error) {
    log('DEVICE MESSAGE ERROR', normalizeBridgeError(error));
  }
}

function prepareOutbound(predicate, timeoutMs, timeoutMessage) {
  let waiter;
  const promise = new Promise((resolve, reject) => {
    waiter = { predicate, resolve, reject, timer: null };
    waiter.timer = setTimeout(() => {
      outboundWaiters.delete(waiter);
      reject(bridgeError('TIMEOUT', timeoutMessage));
    }, timeoutMs);
    outboundWaiters.add(waiter);
  });
  return {
    promise,
    cancel() {
      if (!outboundWaiters.delete(waiter)) return;
      clearTimeout(waiter.timer);
      waiter.reject(bridgeError('RUNTIME_CLOSED', 'Outbound wait was cancelled'));
      promise.catch(() => {});
    },
  };
}

function equalBytes(left, right) {
  return left.length === right.length && left.every((value, index) => value === right[index]);
}

function requireString(values, key) {
  const value = values?.[key];
  if (typeof value !== 'string') throw bridgeError('INVALID_REQUEST', `${key} must be a string`);
  return value;
}

function bridgeError(code, message) { return Object.assign(new Error(message), { code }); }
function normalizeBridgeError(error) { return { code: error?.code ?? 'INTERNAL_ERROR', message: error?.message ?? String(error) }; }

function summarize(params) {
  if (!params || typeof params !== 'object') return params;
  const result = { ...params };
  if (typeof result.dataBase64 === 'string') result.dataBase64 = `<base64 ${result.dataBase64.length} chars>`;
  return result;
}

function log(kind, value) {
  const item = document.createElement('li');
  item.textContent = `${new Date().toLocaleTimeString()}  ${kind}  ${typeof value === 'string' ? value : JSON.stringify(value)}`;
  logs.prepend(item);
  while (logs.children.length > 100) logs.lastElementChild.remove();
}

function updatePairStatus() {
  const workspace = describeWorkspace({ webEnabled: webActive, deviceRunning });
  const incompatible = webActive && currentCompatibility?.compatible === false;
  pairStatus.textContent = incompatible ? `${workspace.text} · 配对不兼容` : workspace.text;
  pairStatus.classList.toggle('ready', workspace.ready && !incompatible);
  document.querySelector('#bridge-mode-label').textContent = workspace.bridgeLabel;
}

function updateDeviceControls() {
  for (const button of deviceEventButtons) button.disabled = !deviceRunning;
  document.querySelector('#stop-device').disabled = !deviceRunning;
}

context.fillStyle = '#00150d';
context.fillRect(0, 0, canvas.width, canvas.height);
updateDeviceControls();
updatePairStatus();
discoverWebPlugins();
discoverDevicePlugins();
