import { BRIDGE_VERSION, EVENT_NAMES, PLUGIN_MESSAGE_PROFILE } from '@memomind/gm-plugin-bridge-contract';

export class GMPluginError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'GMPluginError';
    this.code = code;
  }
}

export class ParentFrameTransport {
  constructor({ windowObject = globalThis.window, timeoutMs = 5000 } = {}) {
    if (!windowObject?.parent || windowObject.parent === windowObject) {
      throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'Studio parent frame is unavailable');
    }
    this.window = windowObject;
    this.timeoutMs = timeoutMs;
    this.pending = new Map();
    this.listeners = new Set();
    this.onMessage = this.onMessage.bind(this);
    this.window.addEventListener('message', this.onMessage);
  }

  async send(request) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.requestId);
        reject(new GMPluginError('TIMEOUT', `Bridge request timed out: ${request.method}`));
      }, this.timeoutMs);
      this.pending.set(request.requestId, { resolve, reject, timer });
      this.window.parent.postMessage({ type: 'gm-plugin:request', request }, '*');
    });
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  onMessage(message) {
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
      this.bootstrap?.(envelope.bootstrap);
    }
  }

  waitForBootstrap() {
    if (this.bootstrapData) return Promise.resolve(this.bootstrapData);
    return new Promise((resolve) => {
      this.bootstrap = (value) => {
        this.bootstrapData = value;
        resolve(value);
      };
      this.window.parent.postMessage({ type: 'gm-plugin:bootstrap-request' }, '*');
    });
  }

  close() {
    this.window.removeEventListener('message', this.onMessage);
    for (const { reject, timer } of this.pending.values()) {
      clearTimeout(timer);
      reject(new GMPluginError('RUNTIME_CLOSED', 'Bridge transport closed'));
    }
    this.pending.clear();
    this.listeners.clear();
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
    this.bootstrapWaiters = [];
    globalObject.__memoPluginBootstrap = (sessionToken, runtimeGeneration) => {
      this.bootstrapData = { sessionToken, runtimeGeneration };
      for (const resolve of this.bootstrapWaiters.splice(0)) resolve(this.bootstrapData);
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

  send(request) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.requestId);
        reject(new GMPluginError('TIMEOUT', `Bridge request timed out: ${request.method}`));
      }, this.timeoutMs);
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

export function createGMPlugin({ transport = detectTransport(), timeoutMs = 5000 } = {}) {
  let sequence = 0;
  let bootstrapData;
  const eventListeners = new Map(EVENT_NAMES.map((name) => [name, new Set()]));

  const ensureBootstrap = async () => {
    bootstrapData ??= await transport.waitForBootstrap();
    return bootstrapData;
  };

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
        timeoutMs,
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
    if (bootstrapData && event.runtimeGeneration !== bootstrapData.runtimeGeneration) return;
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
    },
    close: () => transport.close?.(),
  };
}

function encodeBytes(value) {
  if (typeof Buffer !== 'undefined') return Buffer.from(value).toString('base64');
  let binary = '';
  for (let offset = 0; offset < value.length; offset += 0x8000) {
    binary += String.fromCharCode(...value.subarray(offset, offset + 0x8000));
  }
  return globalThis.btoa(binary);
}

function detectTransport() {
  if (globalThis.MemoPluginBridge?.postMessage) return new AppWebViewTransport();
  if (globalThis.window?.parent && globalThis.window.parent !== globalThis.window) {
    return new ParentFrameTransport();
  }
  throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'No GM Plugin host detected');
}

function toPluginError(error) {
  if (error instanceof GMPluginError) return error;
  return new GMPluginError(error?.code ?? 'INTERNAL_ERROR', error?.message ?? 'Bridge operation failed');
}
