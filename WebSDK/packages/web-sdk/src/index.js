import {
  AUDIO_PROFILE,
  BRIDGE_VERSION,
  EVENT_NAMES,
  PLUGIN_MESSAGE_PROFILE,
} from '@memomind/gm-plugin-bridge-contract';

const MAX_PLUGIN_MESSAGE_BASE64_CHARACTERS =
  Math.ceil(PLUGIN_MESSAGE_PROFILE.maxPayloadBytes / 3) * 4;
const USER_FILE_PICK_TIMEOUT_MS = 10 * 60 * 1000;

function requestTimeoutMs(method, defaultTimeoutMs) {
  return method === 'files.pick'
    ? Math.max(defaultTimeoutMs, USER_FILE_PICK_TIMEOUT_MS)
    : defaultTimeoutMs;
}

export class GMPluginError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'GMPluginError';
    this.code = code;
  }
}

export class ParentFrameTransport {
  constructor({ windowObject = globalThis.window, timeoutMs = 5000, parentOrigin } = {}) {
    if (!windowObject?.parent || windowObject.parent === windowObject) {
      throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'Studio parent frame is unavailable');
    }
    this.window = windowObject;
    this.parentOrigin = parentOrigin ?? inferParentOrigin(windowObject);
    if (!this.parentOrigin || this.parentOrigin === 'null') {
      throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'Studio parent origin is unavailable');
    }
    this.timeoutMs = timeoutMs;
    this.pending = new Map();
    this.listeners = new Set();
    this.bootstrapListeners = new Set();
    this.onMessage = this.onMessage.bind(this);
    this.window.addEventListener('message', this.onMessage);
  }

  async send(request) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.requestId);
        reject(new GMPluginError('TIMEOUT', `Bridge request timed out: ${request.method}`));
      }, requestTimeoutMs(request.method, this.timeoutMs));
      this.pending.set(request.requestId, { resolve, reject, timer });
      this.window.parent.postMessage({ type: 'gm-plugin:request', request }, this.parentOrigin);
    });
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  onMessage(message) {
    if (message.source !== this.window.parent || message.origin !== this.parentOrigin) return;
    const envelope = message.data;
    if (!envelope || typeof envelope !== 'object') return;
    if (envelope.type === 'gm-plugin:response') {
      const response = envelope.response;
      const pending = this.pending.get(response?.requestId);
      if (!pending) return;
      clearTimeout(pending.timer);
      this.pending.delete(response.requestId);
      if (response.ok) pending.resolve(response.result);
      else pending.reject(toPluginError(response.error));
    } else if (envelope.type === 'gm-plugin:event') {
      for (const listener of this.listeners) listener(envelope.event);
    } else if (envelope.type === 'gm-plugin:bootstrap') {
      this.replaceBootstrap(envelope.bootstrap);
    }
  }

  waitForBootstrap() {
    if (this.bootstrapData) return Promise.resolve(this.bootstrapData);
    return new Promise((resolve) => {
      this.bootstrap = (value) => {
        resolve(value);
      };
      this.window.parent.postMessage({ type: 'gm-plugin:bootstrap-request' }, this.parentOrigin);
    });
  }

  replaceBootstrap(value) {
    if (this.bootstrapData && (this.bootstrapData.sessionToken !== value?.sessionToken ||
        this.bootstrapData.runtimeGeneration !== value?.runtimeGeneration)) {
      this.rejectPending('Bridge runtime was replaced');
    }
    this.bootstrapData = value;
    this.bootstrap?.(value);
    this.bootstrap = undefined;
    for (const listener of this.bootstrapListeners) listener(value);
  }

  currentBootstrap() { return this.bootstrapData; }

  subscribeBootstrap(listener) {
    this.bootstrapListeners.add(listener);
    return () => this.bootstrapListeners.delete(listener);
  }

  rejectPending(message, code = 'RUNTIME_REPLACED') {
    for (const { reject, timer } of this.pending.values()) {
      clearTimeout(timer);
      reject(new GMPluginError(code, message));
    }
    this.pending.clear();
  }

  close() {
    this.window.removeEventListener('message', this.onMessage);
    this.rejectPending('Bridge transport closed', 'RUNTIME_CLOSED');
    this.listeners.clear();
    this.bootstrapListeners.clear();
  }
}

export class AppWebViewTransport {
  constructor({ globalObject = globalThis, timeoutMs = 5000 } = {}) {
    const channel = globalObject.MemoPluginBridge;
    if (!channel?.postMessage) {
      throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'MemoPluginBridge is unavailable');
    }
    this.globalObject = globalObject;
    this.channel = channel;
    this.timeoutMs = timeoutMs;
    this.pending = new Map();
    this.listeners = new Set();
    this.bootstrapListeners = new Set();
    this.bootstrapWaiters = [];
    globalObject.__memoPluginBootstrap = (sessionToken, runtimeGeneration) => {
      this.replaceBootstrap({ sessionToken, runtimeGeneration });
    };
    globalObject.__memoPluginResolve = (response) => this.resolve(response);
    globalObject.__memoPluginEmit = (event) => {
      for (const listener of this.listeners) listener(event);
    };
    globalObject.__memoPluginHeartbeat = () => true;
  }

  waitForBootstrap() {
    if (this.bootstrapData) return Promise.resolve(this.bootstrapData);
    return new Promise((resolve) => this.bootstrapWaiters.push(resolve));
  }

  replaceBootstrap(value) {
    if (this.bootstrapData && (this.bootstrapData.sessionToken !== value.sessionToken ||
        this.bootstrapData.runtimeGeneration !== value.runtimeGeneration)) {
      for (const { reject, timer } of this.pending.values()) {
        clearTimeout(timer);
        reject(new GMPluginError('RUNTIME_REPLACED', 'Bridge runtime was replaced'));
      }
      this.pending.clear();
    }
    this.bootstrapData = value;
    for (const resolve of this.bootstrapWaiters.splice(0)) resolve(value);
    for (const listener of this.bootstrapListeners) listener(value);
  }

  currentBootstrap() { return this.bootstrapData; }

  subscribeBootstrap(listener) {
    this.bootstrapListeners.add(listener);
    return () => this.bootstrapListeners.delete(listener);
  }

  send(request) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.requestId);
        reject(new GMPluginError('TIMEOUT', `Bridge request timed out: ${request.method}`));
      }, requestTimeoutMs(request.method, this.timeoutMs));
      this.pending.set(request.requestId, { resolve, reject, timer });
      this.channel.postMessage(JSON.stringify(request));
    });
  }

  resolve(response) {
    const pending = this.pending.get(response?.requestId);
    if (!pending) return;
    clearTimeout(pending.timer);
    this.pending.delete(response.requestId);
    if (response.ok) pending.resolve(response.result);
    else pending.reject(toPluginError(response.error));
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }
}

export function createGMPlugin({
  transport = detectTransport(),
  timeoutMs = 5000,
  fetchImpl = globalThis.fetch?.bind(globalThis),
} = {}) {
  let sequence = 0;
  let bootstrapData;
  const eventListeners = new Map(EVENT_NAMES.map((name) => [name, new Set()]));

  const ensureBootstrap = async () => {
    bootstrapData = transport.currentBootstrap?.() ?? bootstrapData ??
      await transport.waitForBootstrap();
    return bootstrapData;
  };

  transport.subscribeBootstrap?.((value) => {
    bootstrapData = value;
  });

  const call = async (method, params = {}) => {
    const bootstrap = await ensureBootstrap();
    const request = {
      version: BRIDGE_VERSION,
      sessionToken: bootstrap.sessionToken,
      requestId: `req-${Date.now()}-${++sequence}`,
      method,
      params,
      runtimeGeneration: bootstrap.runtimeGeneration,
    };
    return new Promise((resolve, reject) => {
      const timer = setTimeout(
        () => reject(new GMPluginError('TIMEOUT', `SDK request timed out: ${method}`)),
        requestTimeoutMs(method, timeoutMs),
      );
      transport.send(request).then(
        (result) => {
          clearTimeout(timer);
          resolve(result);
        },
        (error) => {
          clearTimeout(timer);
          reject(error);
        },
      );
    });
  };

  transport.subscribe((event) => {
    if (!bootstrapData || event.runtimeGeneration !== bootstrapData.runtimeGeneration) return;
    const listeners = eventListeners.get(event.name);
    if (!listeners) return;
    for (const listener of listeners) listener(event.data, event);
  });

  const on = (eventName, listener) => {
    if (!eventListeners.has(eventName)) {
      throw new GMPluginError('INVALID_REQUEST', `Unknown event: ${eventName}`);
    }
    eventListeners.get(eventName).add(listener);
    return () => eventListeners.get(eventName).delete(listener);
  };

  return {
    ready: async () => {
      await ensureBootstrap();
      return call('runtime.ready');
    },
    call,
    on,
    runtime: {
      ping: () => call('runtime.ping'),
      getBridgeVersion: () => call('runtime.getBridgeVersion'),
      getCapabilities: () => call('runtime.getCapabilities'),
      getLifecycleState: () => call('runtime.getLifecycleState'),
    },
    storage: {
      get: (key) => call('storage.get', { key }),
      set: (key, value) => call('storage.set', { key, value }),
      remove: (key) => call('storage.remove', { key }),
      clear: () => call('storage.clear'),
    },
    files: {
      pick: ({ extensions, allowMultiple } = {}) =>
        call('files.pick', {
          ...(extensions === undefined ? {} : { extensions }),
          ...(allowMultiple === undefined ? {} : { allowMultiple }),
        }),
      list: () => call('files.list'),
      stat: (fileId) => call('files.stat', { fileId }),
      openRead: async (fileId, { offset, length, signal } = {}) => {
        if (offset !== undefined && (!Number.isSafeInteger(offset) || offset < 0)) {
          throw new GMPluginError('INVALID_REQUEST', 'offset must be a non-negative safe integer');
        }
        if (length !== undefined && (!Number.isSafeInteger(length) || length < 1)) {
          throw new GMPluginError('INVALID_REQUEST', 'length must be a positive safe integer');
        }
        const ticket = await call('files.openRead', {
          fileId,
          ...(offset === undefined ? {} : { offset }),
          ...(length === undefined ? {} : { length }),
        });
        return openUserFileStream(ticket, fileId, fetchImpl, signal);
      },
      getUsage: () => call('files.getUsage'),
      delete: (fileId) => call('files.delete', { fileId }),
    },
    display: {
      createPage: () => call('display.createPage'),
      rebuildPage: (operations) => call('display.rebuildPage', { operations }),
      updateText: (params) => call('display.updateText', params),
      updateImage: (params) => call('display.updateImage', params),
      updateImageLz4: (params) => call('display.updateImageLz4', params),
      beginFrame: (params) => call('display.beginFrame', params),
      updateFrameImageLz4: (params) => call('display.updateFrameImageLz4', params),
      closePage: () => call('display.closePage'),
    },
    device: {
      getInfo: () => call('device.getInfo'),
      subscribeEvents: (types) => call('device.subscribeEvents', { types }),
      unsubscribeEvents: (subscriptionId) => call('device.unsubscribeEvents', { subscriptionId }),
      onButton: (listener) => on('device.button', listener),
      onGesture: (listener) => on('device.imuGesture', listener),
      onRawImu: (listener) => on('device.rawImu', listener),
      onConnection: (listener) => on('device.connection', listener),
    },
    plugin: {
      sendMessage: async (channel, data) => {
        if (!Number.isInteger(channel) || channel < 0 || channel > 0xffff) {
          throw new GMPluginError('INVALID_REQUEST', 'plugin message channel must be uint16');
        }
        if (!(data instanceof Uint8Array) || data.length === 0) {
          throw new GMPluginError('INVALID_REQUEST', 'plugin message data must be a non-empty Uint8Array');
        }
        if (data.length > PLUGIN_MESSAGE_PROFILE.maxPayloadBytes) {
          throw new GMPluginError('PAYLOAD_TOO_LARGE', `plugin message data exceeds ${PLUGIN_MESSAGE_PROFILE.maxPayloadBytes} bytes`);
        }
        return call('plugin.sendMessage', { channel, dataBase64: encodeBytes(data) });
      },
      onMessage: (listener) => {
        if (typeof listener !== 'function') {
          throw new GMPluginError('INVALID_REQUEST', 'plugin message listener must be a function');
        }
        return on('plugin.message', (data, event) => {
          let message;
          try {
            message = decodePluginMessage(data);
          } catch (error) {
            if (error instanceof GMPluginError) return;
            throw error;
          }
          listener(message, event);
        });
      },
    },
    audio: {
      configure: ({ noiseReduction = true, pickupMode = 'unchanged' } = {}) =>
        call('audio.configure', { noiseReduction, pickupMode }),
      startRecording: () => call('audio.startRecording'),
      stopRecording: () => call('audio.stopRecording'),
      playRecording: ({ recordingId, voice = 'original' }) =>
        call('audio.playRecording', { recordingId, voice }),
      stopPlayback: () => call('audio.stopPlayback'),
      onFrames: (listener) => typedListener(listener, 'audio.frames', (data) => ({
        ...data,
        frames: decodeAudioFrames(data),
      })),
      onState: (listener) => typedListener(listener, 'audio.state'),
      onPlaybackState: (listener) => typedListener(listener, 'audio.playbackState'),
    },
    close: () => transport.close?.(),
  };

  function typedListener(listener, eventName, transform = (data) => data) {
    if (typeof listener !== 'function') {
      throw new GMPluginError('INVALID_REQUEST', `${eventName} listener must be a function`);
    }
    return on(eventName, (data, event) => {
      try {
        listener(transform(data), event);
      } catch (error) {
        if (!(error instanceof GMPluginError)) throw error;
      }
    });
  }
}

async function openUserFileStream(ticket, requestedFileId, fetchImpl, signal) {
  if (typeof fetchImpl !== 'function') {
    throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'Binary file streaming is unavailable');
  }
  const resourceUrl = ticket?.resourceUrl;
  const values = [ticket?.size, ticket?.offset, ticket?.length];
  if (ticket?.fileId !== requestedFileId || typeof resourceUrl !== 'string' ||
      resourceUrl.length === 0 || values.some((value) => !Number.isSafeInteger(value) || value < 0) ||
      ticket.offset > ticket.size || ticket.length > ticket.size - ticket.offset) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid file stream ticket');
  }
  let response;
  try {
    response = await fetchImpl(resourceUrl, { method: 'GET', cache: 'no-store', signal });
  } catch (error) {
    if (error?.name === 'AbortError') throw error;
    throw new GMPluginError('INTERNAL_ERROR', `Could not open file stream: ${error?.message ?? error}`);
  }
  if (!response?.ok || !response.body?.getReader) {
    throw new GMPluginError(
      response?.status === 401 || response?.status === 403 ? 'UNAUTHORIZED' : 'INTERNAL_ERROR',
      `Host file stream failed${Number.isInteger(response?.status) ? `: HTTP ${response.status}` : ''}`,
    );
  }
  return {
    fileId: ticket.fileId,
    size: ticket.size,
    offset: ticket.offset,
    length: ticket.length,
    stream: response.body,
  };
}

function decodeAudioFrames(value) {
  const encodedFrames = value?.framesBase64;
  if (!Array.isArray(encodedFrames) || encodedFrames.length > AUDIO_PROFILE.maxFrames) {
    throw new GMPluginError('INVALID_REQUEST', 'audio frame event is invalid');
  }
  let totalBytes = 0;
  return encodedFrames.map((encoded) => {
    if (typeof encoded !== 'string' || encoded.length === 0 ||
        encoded.length % 4 !== 0 || !/^[A-Za-z0-9+/]*={0,2}$/.test(encoded)) {
      throw new GMPluginError('INVALID_REQUEST', 'audio frame payload is invalid');
    }
    const frame = typeof Buffer !== 'undefined'
      ? Uint8Array.from(Buffer.from(encoded, 'base64'))
      : Uint8Array.from(globalThis.atob(encoded), (character) => character.charCodeAt(0));
    totalBytes += frame.length;
    if (frame.length === 0 || totalBytes > AUDIO_PROFILE.maxOpusBytes) {
      throw new GMPluginError('PAYLOAD_TOO_LARGE', 'audio frame event exceeds the limit');
    }
    return frame;
  });
}

function encodeBytes(value) {
  if (typeof Buffer !== 'undefined') return Buffer.from(value).toString('base64');
  let binary = '';
  for (let offset = 0; offset < value.length; offset += 0x8000) {
    binary += String.fromCharCode(...value.subarray(offset, offset + 0x8000));
  }
  return globalThis.btoa(binary);
}

function decodePluginMessage(value) {
  const channel = value?.channel;
  if (!Number.isInteger(channel) || channel < 0 || channel > 0xffff) {
    throw new GMPluginError('INVALID_REQUEST', 'plugin message event channel must be uint16');
  }
  const encoded = value?.dataBase64;
  if (typeof encoded !== 'string' || encoded.length === 0 || encoded.length % 4 !== 0) {
    throw new GMPluginError('INVALID_REQUEST', 'plugin message event dataBase64 must be non-empty valid base64');
  }
  if (encoded.length > MAX_PLUGIN_MESSAGE_BASE64_CHARACTERS) {
    throw new GMPluginError(
      'PAYLOAD_TOO_LARGE',
      `plugin message event data exceeds ${PLUGIN_MESSAGE_PROFILE.maxPayloadBytes} bytes`,
    );
  }
  if (!/^[A-Za-z0-9+/]*={0,2}$/.test(encoded)) {
    throw new GMPluginError('INVALID_REQUEST', 'plugin message event dataBase64 must be non-empty valid base64');
  }
  const paddingBytes = encoded.endsWith('==') ? 2 : encoded.endsWith('=') ? 1 : 0;
  const decodedLength = encoded.length / 4 * 3 - paddingBytes;
  if (decodedLength > PLUGIN_MESSAGE_PROFILE.maxPayloadBytes) {
    throw new GMPluginError(
      'PAYLOAD_TOO_LARGE',
      `plugin message event data exceeds ${PLUGIN_MESSAGE_PROFILE.maxPayloadBytes} bytes`,
    );
  }
  try {
    const data = typeof Buffer !== 'undefined'
      ? Uint8Array.from(Buffer.from(encoded, 'base64'))
      : Uint8Array.from(globalThis.atob(encoded), (character) => character.charCodeAt(0));
    if (data.length === 0) throw new Error('empty');
    if (data.length > PLUGIN_MESSAGE_PROFILE.maxPayloadBytes) {
      throw new GMPluginError(
        'PAYLOAD_TOO_LARGE',
        `plugin message event data exceeds ${PLUGIN_MESSAGE_PROFILE.maxPayloadBytes} bytes`,
      );
    }
    return { channel, data };
  } catch (error) {
    if (error instanceof GMPluginError) throw error;
    throw new GMPluginError('INVALID_REQUEST', 'plugin message event dataBase64 must be non-empty valid base64');
  }
}

function detectTransport() {
  if (globalThis.MemoPluginBridge?.postMessage) return new AppWebViewTransport();
  if (globalThis.window?.parent && globalThis.window.parent !== globalThis.window) {
    return new ParentFrameTransport();
  }
  throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'No GM Plugin host detected');
}

function inferParentOrigin(windowObject) {
  const configured = new URLSearchParams(windowObject.location?.search ?? '')
    .get('studioOrigin');
  if (configured) return configured;
  try {
    return new URL(windowObject.document?.referrer).origin;
  } catch {
    return null;
  }
}

function toPluginError(error) {
  if (error instanceof GMPluginError) return error;
  return new GMPluginError(error?.code ?? 'INTERNAL_ERROR', error?.message ?? 'Bridge operation failed');
}
