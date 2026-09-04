import {
  AUDIO_STREAM_ENVELOPE,
  AUDIO_STREAM_PORT_KIND,
  AUDIO_PROFILE,
  BRIDGE_VERSION,
  EVENT_NAMES,
  PLUGIN_MESSAGE_PROFILE,
  STREAM_PORT_MESSAGE_TYPE,
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
    this.streamPorts = new Map();
    this.pendingStreamResponses = new Map();
    this.onWindowMessage = this.onWindowMessage.bind(this);
    globalObject.addEventListener?.('message', this.onWindowMessage);
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
      this.pendingStreamResponses.clear();
      this.#closeStreamPorts();
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
        this.#deletePending(request.requestId);
        reject(new GMPluginError('TIMEOUT', `Bridge request timed out: ${request.method}`));
      }, requestTimeoutMs(request.method, this.timeoutMs));
      this.pending.set(request.requestId, { resolve, reject, timer, request });
      this.channel.postMessage(JSON.stringify(request));
    });
  }

  resolve(response) {
    const pending = this.pending.get(response?.requestId);
    if (!pending) return;
    if (!response.ok) {
      this.#complete(response.requestId, pending, undefined, toPluginError(response.error));
      return;
    }
    const result = response.result;
    if (pending.request.method !== 'audio.openCapture' || result?.mode !== 'stream') {
      this.#complete(response.requestId, pending, result);
      return;
    }
    const descriptor = result.streamDescriptor;
    if (!isAudioStreamDescriptor(descriptor, result.sessionId, pending.request.runtimeGeneration)) {
      this.#complete(response.requestId, pending, undefined, new GMPluginError(
        'INTERNAL_ERROR', 'Host returned an invalid audio stream descriptor',
      ));
      return;
    }
    pending.streamDescriptor = descriptor;
    pending.responseResult = result;
    this.pendingStreamResponses.set(descriptor.id, response.requestId);
    this.#completeStreamResponse(descriptor.id);
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  onWindowMessage(event) {
    const envelope = parseWindowMessage(event?.data);
    if (envelope?.type !== STREAM_PORT_MESSAGE_TYPE) return;
    const descriptor = envelope.descriptor;
    const port = event?.ports?.[0];
    if (!isAudioStreamDescriptor(descriptor) || typeof port?.postMessage !== 'function' ||
        descriptor.runtimeGeneration !== this.bootstrapData?.runtimeGeneration) {
      port?.close?.();
      return;
    }
    const existing = this.streamPorts.get(descriptor.id);
    if (existing) {
      port.close?.();
      return;
    }
    this.streamPorts.set(descriptor.id, { descriptor, port });
    while (this.streamPorts.size > 8) {
      const [oldestId, oldest] = this.streamPorts.entries().next().value;
      oldest.port.close?.();
      this.streamPorts.delete(oldestId);
    }
    this.#completeStreamResponse(descriptor.id);
  }

  close() {
    this.globalObject.removeEventListener?.('message', this.onWindowMessage);
    for (const { reject, timer } of this.pending.values()) {
      clearTimeout(timer);
      reject(new GMPluginError('RUNTIME_CLOSED', 'Bridge transport closed'));
    }
    this.pending.clear();
    this.pendingStreamResponses.clear();
    this.#closeStreamPorts();
    this.listeners.clear();
    this.bootstrapListeners.clear();
    this.bootstrapWaiters = [];
  }

  #completeStreamResponse(streamId) {
    const requestId = this.pendingStreamResponses.get(streamId);
    const pending = requestId ? this.pending.get(requestId) : undefined;
    const entry = this.streamPorts.get(streamId);
    if (!pending || !entry || !sameAudioStreamDescriptor(pending.streamDescriptor, entry.descriptor)) return;
    this.streamPorts.delete(streamId);
    const result = { ...pending.responseResult, streamPort: entry.port };
    // responseResult is assigned below for hosts whose JSON response and port
    // arrive in either order.
    this.#complete(requestId, pending, result);
  }

  #complete(requestId, pending, result, error) {
    clearTimeout(pending.timer);
    this.#deletePending(requestId);
    if (error) pending.reject(error);
    else pending.resolve(result);
  }

  #deletePending(requestId) {
    const pending = this.pending.get(requestId);
    this.pending.delete(requestId);
    const streamId = pending?.streamDescriptor?.id;
    if (streamId) {
      this.pendingStreamResponses.delete(streamId);
      const entry = this.streamPorts.get(streamId);
      entry?.port?.close?.();
      this.streamPorts.delete(streamId);
    }
  }

  #closeStreamPorts() {
    for (const { port } of this.streamPorts.values()) port.close?.();
    this.streamPorts.clear();
  }
}

function parseWindowMessage(value) {
  if (typeof value !== 'string') return value;
  try {
    return JSON.parse(value);
  } catch {
    return null;
  }
}

function isAudioStreamDescriptor(value, sessionId = value?.sessionId, runtimeGeneration = value?.runtimeGeneration) {
  return value && typeof value === 'object' && value.kind === AUDIO_STREAM_PORT_KIND &&
    typeof value.id === 'string' && value.id.length >= 16 && value.id.length <= 256 &&
    typeof value.sessionId === 'string' && value.sessionId.length > 0 && value.sessionId === sessionId &&
    Number.isSafeInteger(value.runtimeGeneration) && value.runtimeGeneration >= 0 &&
    value.runtimeGeneration === runtimeGeneration;
}

function sameAudioStreamDescriptor(left, right) {
  return isAudioStreamDescriptor(left) && isAudioStreamDescriptor(right) &&
    left.id === right.id && left.kind === right.kind && left.sessionId === right.sessionId &&
    left.runtimeGeneration === right.runtimeGeneration;
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
      openCapture: async ({ signal, ...options } = {}) => {
        if (signal !== undefined && !isAbortSignal(signal)) {
          throw new GMPluginError('INVALID_REQUEST', 'signal must be an AbortSignal');
        }
        if (signal?.aborted) throw signal.reason ?? new DOMException('Aborted', 'AbortError');
        const ticket = await call('audio.openCapture', options);
        if (signal?.aborted) {
          try {
            ticket?.streamPort?.postMessage?.(JSON.stringify({
              type: 'cancel', reason: 'capture aborted while opening',
            }));
            ticket?.streamPort?.close?.();
          } finally {
            void call('audio.stopCapture', { sessionId: ticket?.sessionId }).catch(() => {});
          }
          throw signal.reason ?? new DOMException('Aborted', 'AbortError');
        }
        return openAudioCapture(ticket, call, signal);
      },
      stopCapture: async (sessionId) => validateAudioCaptureResult(
        await call('audio.stopCapture', { sessionId }), sessionId,
      ),
      playRecording: ({ recordingId, voice = 'original' }) =>
        call('audio.playRecording', { recordingId, voice }),
      stopPlayback: () => call('audio.stopPlayback'),
      onCaptureState: (listener) => typedListener(listener, 'audio.captureState'),
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

function openAudioCapture(ticket, call, signal) {
  const sessionId = ticket?.sessionId;
  const mode = ticket?.mode;
  const resolvedOptions = ticket?.resolvedOptions;
  const port = ticket?.streamPort;
  const validTicket = typeof sessionId === 'string' && sessionId.length > 0 &&
    isResolvedAudioCaptureOptions(resolvedOptions, mode) &&
    (mode === 'recording' ? port === undefined : typeof port?.postMessage === 'function');
  if (!validTicket) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio capture ticket');
  }
  const localCapture = mode === 'stream'
    ? createAudioCaptureStream(port, sessionId, signal)
    : null;
  let stopPromise;
  const session = {
    mode,
    sessionId,
    resolvedOptions,
    stream: localCapture?.stream ?? null,
    stop() {
      if (!stopPromise) {
        stopPromise = call('audio.stopCapture', { sessionId }).then(
          (result) => validateAudioCaptureResult(result, sessionId, mode),
        ).catch((error) => {
          localCapture?.close(false);
          throw error;
        }).finally(() => {
          if (mode === 'recording') signal?.removeEventListener('abort', abortRecording);
        });
      }
      return stopPromise;
    },
  };
  if (mode === 'recording') signal?.addEventListener('abort', abortRecording, { once: true });
  return session;

  function abortRecording() {
    void session.stop().catch(() => {});
  }
}

function createAudioCaptureStream(port, sessionId, signal) {
  let controller;
  let pullPending = false;
  let finished = false;
  const closePort = () => {
    port.onmessage = null;
    port.onmessageerror = null;
    port.close?.();
  };
  const fail = (reason, notifyHost = true) => {
    if (finished) return;
    finished = true;
    signal?.removeEventListener('abort', abort);
    try {
      if (notifyHost) port.postMessage(JSON.stringify({ type: 'cancel' }));
    } finally {
      closePort();
      controller.error(reason);
    }
  };
  const stream = new ReadableStream({
    start(value) {
      controller = value;
      port.onmessage = (event) => {
        if (finished) return;
        pullPending = false;
        const payload = event.data;
        if (payload instanceof ArrayBuffer) {
          try {
            controller.enqueue(decodeAudioChunk(payload, sessionId));
          } catch (error) {
            fail(error);
          }
          return;
        }
        const message = parseAudioPortControl(payload);
        if (message?.type === 'end') {
          finished = true;
          signal?.removeEventListener('abort', abort);
          closePort();
          controller.close();
        } else if (message?.type === 'error') {
          finished = true;
          signal?.removeEventListener('abort', abort);
          closePort();
          controller.error(new GMPluginError(
            typeof message.code === 'string' ? message.code : 'INTERNAL_ERROR',
            typeof message.message === 'string' ? message.message : 'Host audio capture failed',
          ));
        } else {
          fail(new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio stream message'));
        }
      };
      port.onmessageerror = () => fail(new GMPluginError('INTERNAL_ERROR', 'Host audio stream failed'));
      port.start?.();
      if (signal?.aborted) fail(signal.reason ?? new DOMException('Aborted', 'AbortError'));
      else signal?.addEventListener('abort', abort, { once: true });
    },
    pull() {
      if (finished || pullPending) return;
      pullPending = true;
      port.postMessage(JSON.stringify({ type: 'pull' }));
    },
    cancel(reason) {
      if (finished) return;
      finished = true;
      signal?.removeEventListener('abort', abort);
      try {
        port.postMessage(JSON.stringify({ type: 'cancel', reason: String(reason ?? '') }));
      } finally {
        closePort();
      }
    },
  }, { highWaterMark: 0 });

  function abort() {
    fail(signal.reason ?? new DOMException('Aborted', 'AbortError'));
  }

  return {
    stream,
    close(notifyHost = true) {
      if (finished) return;
      finished = true;
      signal?.removeEventListener('abort', abort);
      try {
        if (notifyHost) port.postMessage(JSON.stringify({ type: 'cancel' }));
      } finally {
        closePort();
        controller.close();
      }
    },
  };
}

function decodeAudioChunk(buffer, sessionId) {
  const profile = AUDIO_STREAM_ENVELOPE;
  if (!(buffer instanceof ArrayBuffer) || buffer.byteLength < profile.baseHeaderBytes) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio chunk');
  }
  const view = new DataView(buffer);
  const magic = view.getUint32(0, false);
  const version = view.getUint8(4);
  const messageType = view.getUint8(5);
  const flags = view.getUint16(6, false);
  const headerBytes = view.getUint16(8, false);
  const frameCount = view.getUint16(10, false);
  const sequence = view.getUint32(12, false);
  const timestampUs = view.getUint32(16, false) * 0x100000000 + view.getUint32(20, false);
  const durationMs = view.getUint32(24, false);
  const droppedFrameCount = view.getUint32(28, false);
  const queueLatencyMs = view.getUint32(32, false);
  const payloadBytes = view.getUint32(36, false);
  const expectedHeaderBytes = profile.baseHeaderBytes + frameCount * profile.frameLengthBytes;
  if (magic !== profile.magic || version !== profile.version || messageType !== profile.chunkType ||
      (flags & ~profile.discontinuityFlag) !== 0 || frameCount < 1 ||
      frameCount > profile.maxFramesPerChunk || headerBytes !== expectedHeaderBytes ||
      !Number.isSafeInteger(timestampUs) || durationMs !== frameCount * 20 || payloadBytes < 1 ||
      buffer.byteLength !== headerBytes + payloadBytes) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio chunk');
  }
  const frameLengths = [];
  let totalFrameBytes = 0;
  for (let index = 0; index < frameCount; index += 1) {
    const length = view.getUint16(profile.baseHeaderBytes + index * profile.frameLengthBytes, false);
    if (length < 1) throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio chunk');
    frameLengths.push(length);
    totalFrameBytes += length;
  }
  if (totalFrameBytes !== payloadBytes) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio chunk');
  }
  return {
    sessionId,
    sequence,
    timestampUs,
    durationMs,
    frameCount,
    frameLengths,
    droppedFrameCount,
    discontinuity: (flags & profile.discontinuityFlag) !== 0,
    queueLatencyMs,
    data: new Uint8Array(buffer, headerBytes, payloadBytes),
  };
}

function parseAudioPortControl(value) {
  if (typeof value !== 'string') return null;
  try {
    const parsed = JSON.parse(value);
    return parsed && typeof parsed === 'object' ? parsed : null;
  } catch {
    return null;
  }
}

function validateAudioCaptureResult(value, sessionId, expectedMode) {
  const validBase = value && typeof value === 'object' && value.sessionId === sessionId &&
    ['recording', 'stream'].includes(value.mode) && (!expectedMode || value.mode === expectedMode) &&
    Number.isSafeInteger(value.durationMs) && value.durationMs >= 0;
  const validRecording = value?.mode === 'recording' && typeof value.recordingId === 'string' &&
    value.recordingId.length > 0 && Number.isSafeInteger(value.frameCount) && value.frameCount > 0 &&
    Number.isSafeInteger(value.opusBytes) && value.opusBytes > 0;
  const validStream = value?.mode === 'stream' && Number.isSafeInteger(value.deliveredFrameCount) &&
    value.deliveredFrameCount >= 0 && Number.isSafeInteger(value.droppedFrameCount) && value.droppedFrameCount >= 0;
  if (!validBase || (!validRecording && !validStream)) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid audio capture result');
  }
  return value;
}

function isResolvedAudioCaptureOptions(value, mode) {
  if (!value || typeof value !== 'object' || value.mode !== mode ||
      value.codec !== 'opus' || value.sampleRate !== 16000 || value.channels !== 1 ||
      !AUDIO_PROFILE.pickupModes.includes(value.pickupMode) || typeof value.noiseReduction !== 'boolean') return false;
  if (mode === 'recording') {
    return Number.isInteger(value.maxDurationMs) && value.maxDurationMs >= 1000 &&
      value.maxDurationMs <= AUDIO_PROFILE.modes.recording.maxDurationMs;
  }
  return mode === 'stream' && ['interactive', 'balanced', 'reliable', 'custom'].includes(value.profile) &&
    Number.isInteger(value.chunkDurationMs) && value.chunkDurationMs >= 20 && value.chunkDurationMs <= 200 &&
    value.chunkDurationMs % 20 === 0 && Number.isInteger(value.maxQueueMs) && value.maxQueueMs >= 100 &&
    value.maxQueueMs <= 5000 && value.maxQueueMs >= value.chunkDurationMs &&
    ['drop-oldest', 'drop-newest', 'error'].includes(value.overflowStrategy) &&
    (value.maxDurationMs === null || (Number.isInteger(value.maxDurationMs) &&
      value.maxDurationMs >= 1000 && value.maxDurationMs <= 60 * 60 * 1000));
}

function isAbortSignal(value) {
  return value && typeof value === 'object' && typeof value.aborted === 'boolean' &&
    typeof value.addEventListener === 'function' && typeof value.removeEventListener === 'function';
}

async function openUserFileStream(ticket, requestedFileId, fetchImpl, signal) {
  const resourceUrl = ticket?.resourceUrl;
  const streamPort = ticket?.streamPort;
  const values = [ticket?.size, ticket?.offset, ticket?.length];
  const hasStreamPort = typeof streamPort?.postMessage === 'function';
  const hasResourceUrl = typeof resourceUrl === 'string' && resourceUrl.length > 0;
  if (ticket?.fileId !== requestedFileId || (!hasStreamPort && !hasResourceUrl) ||
      values.some((value) => !Number.isSafeInteger(value) || value < 0) ||
      ticket.offset > ticket.size || ticket.length > ticket.size - ticket.offset) {
    throw new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid file stream ticket');
  }
  if (hasStreamPort) {
    return {
      fileId: ticket.fileId,
      size: ticket.size,
      offset: ticket.offset,
      length: ticket.length,
      stream: createPortReadableStream(streamPort, signal),
    };
  }
  if (typeof fetchImpl !== 'function') {
    throw new GMPluginError('CAPABILITY_UNAVAILABLE', 'Binary file streaming is unavailable');
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

function createPortReadableStream(port, signal) {
  let controller;
  let pullPending = false;
  let finished = false;
  const closePort = () => {
    port.onmessage = null;
    port.onmessageerror = null;
    port.close?.();
  };
  const fail = (reason) => {
    if (finished) return;
    finished = true;
    try {
      port.postMessage({ type: 'cancel' });
    } finally {
      closePort();
      controller.error(reason);
    }
  };
  return new ReadableStream({
    start(value) {
      controller = value;
      port.onmessage = (event) => {
        if (finished) return;
        pullPending = false;
        const message = event.data;
        if (message?.type === 'chunk' && message.buffer instanceof ArrayBuffer && message.buffer.byteLength > 0) {
          controller.enqueue(new Uint8Array(message.buffer));
        } else if (message?.type === 'end') {
          finished = true;
          signal?.removeEventListener('abort', abort);
          closePort();
          controller.close();
        } else if (message?.type === 'error') {
          finished = true;
          signal?.removeEventListener('abort', abort);
          closePort();
          controller.error(new GMPluginError(
            typeof message.code === 'string' ? message.code : 'INTERNAL_ERROR',
            typeof message.message === 'string' ? message.message : 'Host file stream failed',
          ));
        } else {
          fail(new GMPluginError('INTERNAL_ERROR', 'Host returned an invalid file stream message'));
        }
      };
      port.onmessageerror = () => fail(new GMPluginError('INTERNAL_ERROR', 'Host file stream failed'));
      port.start?.();
      if (signal?.aborted) fail(signal.reason ?? new DOMException('Aborted', 'AbortError'));
      else signal?.addEventListener('abort', abort, { once: true });
    },
    pull() {
      if (finished || pullPending) return;
      pullPending = true;
      port.postMessage({ type: 'pull' });
    },
    cancel(reason) {
      if (finished) return;
      finished = true;
      signal?.removeEventListener('abort', abort);
      try {
        port.postMessage({ type: 'cancel', reason: String(reason ?? '') });
      } finally {
        closePort();
      }
    },
  }, { highWaterMark: 0 });

  function abort() {
    fail(signal.reason ?? new DOMException('Aborted', 'AbortError'));
  }
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
