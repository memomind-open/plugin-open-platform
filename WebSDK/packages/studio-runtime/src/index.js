import {
  BRIDGE_VERSION,
  CAPABILITIES,
  DEVICE_PROFILE,
  PLUGIN_MESSAGE_PROFILE,
  SCENE_TRANSPORT_PROFILE,
  isMethodName,
} from '@memomind/gm-plugin-bridge-contract';
import { decodeBase64, decodeRawLz4 } from '@memomind/gm-plugin-device-renderer';

export class StudioBridgeError extends Error {
  constructor(code, message) {
    super(message);
    this.code = code;
  }

  toJSON() {
    return { code: this.code, message: this.message };
  }
}

export class StudioRuntime {
  constructor({ renderer, sessionToken = randomToken(), runtimeGeneration = 1, pluginMessageHandler } = {}) {
    if (!renderer) throw new TypeError('renderer is required');
    this.renderer = renderer;
    this.sessionToken = sessionToken;
    this.runtimeGeneration = runtimeGeneration;
    this.lifecycleState = 'running';
    this.connected = true;
    this.storage = new Map();
    this.subscriptions = new Map();
    this.subscriptionSequence = 0;
    this.eventSequence = 0;
    this.eventListeners = new Set();
    this.frame = null;
    this.pluginMessages = [];
    this.pluginMessageHandler = pluginMessageHandler;
  }

  get bootstrap() {
    return { sessionToken: this.sessionToken, runtimeGeneration: this.runtimeGeneration };
  }

  onEvent(listener) {
    this.eventListeners.add(listener);
    return () => this.eventListeners.delete(listener);
  }

  async handle(request) {
    try {
      this.validateEnvelope(request);
      const result = await this.dispatch(request.method, request.params);
      return { requestId: request.requestId, ok: true, result, runtimeGeneration: this.runtimeGeneration };
    } catch (error) {
      const bridgeError = normalizeError(error);
      return { requestId: request?.requestId ?? 'invalid', ok: false, error: bridgeError.toJSON(), runtimeGeneration: this.runtimeGeneration };
    }
  }

  validateEnvelope(request) {
    if (!request || typeof request !== 'object' || request.version !== BRIDGE_VERSION || typeof request.requestId !== 'string' || request.requestId.length === 0 || request.requestId.length > 128 || typeof request.method !== 'string' || typeof request.params !== 'object' || request.params === null) {
      throw new StudioBridgeError('INVALID_REQUEST', 'Bridge request fields are invalid');
    }
    if (request.sessionToken !== this.sessionToken) throw new StudioBridgeError('UNAUTHORIZED', 'Session token does not match');
    if (request.runtimeGeneration !== this.runtimeGeneration) throw new StudioBridgeError('STALE_RUNTIME', 'Runtime generation is stale');
    if (!isMethodName(request.method)) throw new StudioBridgeError('METHOD_NOT_FOUND', 'Bridge method is not supported');
  }

  async dispatch(method, params) {
    switch (method) {
      case 'runtime.ready':
      case 'runtime.ping':
        return { ready: true, generation: this.runtimeGeneration };
      case 'runtime.getBridgeVersion':
        return { version: BRIDGE_VERSION };
      case 'runtime.getCapabilities':
        return CAPABILITIES;
      case 'runtime.getLifecycleState':
        return { state: this.lifecycleState };
      case 'storage.get':
        return { value: this.storage.get(requireString(params, 'key')) ?? null };
      case 'storage.set':
        this.storage.set(requireString(params, 'key'), structuredClone(params.value));
        return { stored: true };
      case 'storage.remove':
        this.storage.delete(requireString(params, 'key'));
        return { removed: true };
      case 'storage.clear':
        this.storage.clear();
        return { cleared: true };
      case 'display.createPage':
        this.requireConnection();
        return { created: true };
      case 'display.closePage':
        this.requireConnection();
        this.renderer.clear();
        return { closed: true };
      case 'display.updateText':
        this.requireConnection();
        return this.presentText(params);
      case 'display.updateImage': {
        this.requireConnection();
        return this.presentImage(params);
      }
      case 'display.updateImageLz4': {
        this.requireConnection();
        return this.presentImageLz4(params);
      }
      case 'display.beginFrame': {
        this.requireConnection();
        return this.beginFrame(params);
      }
      case 'display.updateFrameImageLz4': {
        this.requireConnection();
        return this.presentFrameImageLz4(params);
      }
      case 'display.rebuildPage': {
        this.requireConnection();
        if (!Array.isArray(params.operations) || params.operations.length > 128) throw new StudioBridgeError('INVALID_REQUEST', 'operations must be a bounded list');
        this.renderer.clear();
        const transports = [];
        for (const operation of params.operations) {
          if (operation?.type === 'text') transports.push(this.presentText(operation).transport);
          else if (operation?.type === 'image') transports.push(this.presentImage(operation).transport);
          else throw new StudioBridgeError('INVALID_REQUEST', 'Unknown rebuild operation');
        }
        return { presented: true, operationCount: params.operations.length, transports };
      }
      case 'device.getInfo':
        return { connected: this.connected, transport: 'studio', profile: DEVICE_PROFILE.id };
      case 'device.subscribeEvents':
        return this.subscribeEvents(params.types);
      case 'device.unsubscribeEvents':
        return { removed: this.subscriptions.delete(requireString(params, 'subscriptionId')) };
      case 'plugin.sendMessage':
        return this.sendPluginMessage(params);
      default:
        throw new StudioBridgeError('METHOD_NOT_FOUND', 'Bridge method is not supported');
    }
  }

  async sendPluginMessage(params) {
    this.requireConnection();
    const channel = requireInteger(params, 'channel');
    if (channel < 0 || channel > 0xffff) {
      throw new StudioBridgeError('INVALID_REQUEST', 'channel must be uint16');
    }
    const data = decodePluginMessageBase64(params.dataBase64);
    if (data.length > PLUGIN_MESSAGE_PROFILE.maxPayloadBytes) {
      throw new StudioBridgeError(
        'PAYLOAD_TOO_LARGE',
        `plugin message payload is ${data.length} bytes; max ${PLUGIN_MESSAGE_PROFILE.maxPayloadBytes}`,
      );
    }
    const message = { channel, data: Uint8Array.from(data), timestampMs: Date.now() };
    if (this.pluginMessageHandler) await this.pluginMessageHandler(message);
    this.pluginMessages.push(message);
    if (this.pluginMessages.length > 100) this.pluginMessages.shift();
    return { sent: true, channel, payloadBytes: data.length };
  }

  subscribeEvents(types) {
    const supported = new Set(CAPABILITIES.events);
    if (!Array.isArray(types) || types.length === 0 || types.length > supported.size || types.some((type) => typeof type !== 'string' || !supported.has(type))) {
      throw new StudioBridgeError('INVALID_REQUEST', 'types must be a non-empty supported list');
    }
    if (this.subscriptions.size >= 16) throw new StudioBridgeError('QUOTA_EXCEEDED', 'Event subscription limit reached');
    const subscriptionId = `sub-${++this.subscriptionSequence}`;
    this.subscriptions.set(subscriptionId, new Set(types));
    queueMicrotask(() => this.emit('device.connection', { connected: this.connected, source: 'studio' }));
    return { subscriptionId };
  }

  emit(name, data) {
    const capability = eventCapability(name);
    const subscriptionIds = [...this.subscriptions]
      .filter(([, types]) => types.has(capability))
      .map(([id]) => id);
    if (name !== 'runtime.lifecycleChanged' && subscriptionIds.length === 0) return;
    const event = { name, subscriptionIds, data: { sequence: ++this.eventSequence, timestampMs: Date.now(), ...data }, runtimeGeneration: this.runtimeGeneration };
    for (const listener of this.eventListeners) listener(event);
  }

  setConnected(connected) {
    this.connected = Boolean(connected);
    if (!this.connected) {
      this.frame = null;
      this.renderer.abortFrame?.();
    }
    this.emit('device.connection', { connected: this.connected, source: 'studio' });
  }

  setLifecycle(state) {
    const allowed = new Set(['starting', 'running', 'suspended', 'stopped', 'failed']);
    if (!allowed.has(state)) throw new RangeError(`Unknown lifecycle state: ${state}`);
    this.lifecycleState = state;
    this.emit('runtime.lifecycleChanged', { state });
  }

  emitButton(action) {
    const allowed = new Set(['single', 'double', 'long']);
    if (!allowed.has(action)) throw new RangeError(`Unknown button action: ${action}`);
    this.emit('device.button', { button: 'primary', action, source: 'studio' });
  }

  emitGesture(gesture, active = true) {
    const allowed = new Set(['nod', 'headRaise', 'headLower', 'shake', 'headRaiseTimeout', 'headLowerTimeout', 'left', 'right']);
    if (!allowed.has(gesture)) throw new RangeError(`Unknown gesture: ${gesture}`);
    this.emit('device.imuGesture', { gesture, active: Boolean(active), source: 'studio' });
  }

  emitPluginMessage(channel, data) {
    this.requireConnection();
    if (!Number.isInteger(channel) || channel < 0 || channel > 0xffff) {
      throw new StudioBridgeError('INVALID_REQUEST', 'channel must be uint16');
    }
    if (!(data instanceof Uint8Array) || data.length === 0) {
      throw new StudioBridgeError('INVALID_REQUEST', 'data must be a non-empty Uint8Array');
    }
    if (data.length > PLUGIN_MESSAGE_PROFILE.maxPayloadBytes) {
      throw new StudioBridgeError(
        'PAYLOAD_TOO_LARGE',
        `plugin message payload is ${data.length} bytes; max ${PLUGIN_MESSAGE_PROFILE.maxPayloadBytes}`,
      );
    }
    const event = {
      name: 'plugin.message',
      data: { channel, dataBase64: encodeBytes(data) },
      runtimeGeneration: this.runtimeGeneration,
    };
    for (const listener of this.eventListeners) listener(event);
  }

  requireConnection() {
    if (!this.connected) throw new StudioBridgeError('DEVICE_DISCONNECTED', 'Device is disconnected');
  }

  presentText(params) {
    const text = requireString(params, 'text');
    const textBytes = new TextEncoder().encode(text).length;
    const channel = SCENE_TRANSPORT_PROFILE.channels.text;
    const payloadBytes = channel.headerBytes + textBytes;
    requireTransportPayload(payloadBytes, `Channel ${channel.id} text`);
    this.renderer.updateText(params);
    return { presented: true, transport: transportSummary(channel.id, 'text', payloadBytes, textBytes) };
  }

  presentImage(params) {
    const encoded = requireImageBase64(params);
    const pixels = decodeBase64(encoded);
    const geometry = imageGeometry(params);
    const minimumStride = Math.ceil(geometry.width / 2);
    if (geometry.stride < minimumStride) throw new StudioBridgeError('INVALID_REQUEST', `stride must be at least ${minimumStride} for width ${geometry.width}`);
    if (pixels.length !== geometry.stride * geometry.height) throw new StudioBridgeError('INVALID_REQUEST', `pixel length must equal stride * height (${geometry.stride * geometry.height})`);
    const channel = SCENE_TRANSPORT_PROFILE.channels.gray4;
    const payloadBytes = channel.headerBytes + pixels.length;
    if (payloadBytes > SCENE_TRANSPORT_PROFILE.maxPayloadBytes) {
      const rows = Math.floor((SCENE_TRANSPORT_PROFILE.maxPayloadBytes - channel.headerBytes) / geometry.stride);
      throw payloadTooLarge(
        `Channel ${channel.id} GRAY_4 payload is ${payloadBytes} bytes; max ${SCENE_TRANSPORT_PROFILE.maxPayloadBytes}. Split into vertical tiles of at most ${rows} rows for stride ${geometry.stride}, or use Channel 7 raw LZ4 when every tile also fits its decoded-size limit.`,
      );
    }
    this.renderer.updateImage({ ...geometry, pixels });
    return { presented: true, transport: transportSummary(channel.id, 'gray4', payloadBytes, pixels.length) };
  }

  presentImageLz4(params) {
    const encoded = requireImageBase64(params);
    const compressed = decodeBase64(encoded);
    const geometry = imageGeometry(params);
    const expectedStride = Math.ceil(geometry.width / 2);
    if (geometry.stride !== expectedStride) throw new StudioBridgeError('INVALID_REQUEST', `stride must equal ${expectedStride} for LZ4 width ${geometry.width}`);
    const decodedSize = requireInteger(params, 'decodedSize');
    const expectedSize = geometry.stride * geometry.height;
    if (decodedSize !== expectedSize) throw new StudioBridgeError('INVALID_REQUEST', `decodedSize must equal stride * height (${expectedSize})`);
    const channel = SCENE_TRANSPORT_PROFILE.channels.gray4Lz4;
    if (decodedSize <= 0 || decodedSize > channel.maxDecodedBytes) {
      const rows = Math.floor(channel.maxDecodedBytes / geometry.stride);
      throw payloadTooLarge(
        `Channel ${channel.id} decoded LZ4 bitmap is ${decodedSize} bytes; max ${channel.maxDecodedBytes}. Split into vertical tiles of at most ${rows} rows for stride ${geometry.stride} and compress every tile independently.`,
      );
    }
    if (compressed.length === 0) throw new StudioBridgeError('INVALID_REQUEST', 'compressed LZ4 block must not be empty');
    const payloadBytes = channel.headerBytes + compressed.length;
    if (payloadBytes > SCENE_TRANSPORT_PROFILE.maxPayloadBytes) {
      throw payloadTooLarge(
        `Channel ${channel.id} compressed LZ4 payload is ${payloadBytes} bytes; max ${SCENE_TRANSPORT_PROFILE.maxPayloadBytes}. Split into smaller tiles and compress every tile independently.`,
      );
    }
    this.renderer.updateImage({ ...geometry, pixels: decodeRawLz4(compressed, decodedSize) });
    return { presented: true, transport: transportSummary(channel.id, 'gray4-lz4', payloadBytes, decodedSize) };
  }

  beginFrame(params) {
    const frameId = requireUint32(params, 'frameId');
    const tileCount = requireInteger(params, 'tileCount');
    const channel = SCENE_TRANSPORT_PROFILE.channels.frameBegin;
    if (tileCount <= 0 || tileCount > channel.maxTiles) {
      throw new StudioBridgeError('INVALID_REQUEST', `tileCount must be between 1 and ${channel.maxTiles}`);
    }
    this.frame = { frameId, tileCount, nextIndex: 0 };
    this.renderer.beginFrame();
    return { frameId, tileIndex: 0xffff, nextIndex: 0, complete: false };
  }

  presentFrameImageLz4(params) {
    const frameId = requireUint32(params, 'frameId');
    const tileIndex = requireInteger(params, 'tileIndex');
    const frame = this.frame;
    if (!frame || frame.frameId !== frameId) {
      throw new StudioBridgeError('INVALID_REQUEST', 'frame is not active');
    }
    if (tileIndex !== frame.nextIndex || tileIndex < 0 || tileIndex >= frame.tileCount) {
      throw new StudioBridgeError('INVALID_REQUEST', `tileIndex must equal ${frame.nextIndex}`);
    }
    const encoded = requireImageBase64(params);
    const compressed = decodeBase64(encoded);
    const geometry = imageGeometry(params);
    const expectedStride = Math.ceil(geometry.width / 2);
    if (geometry.stride !== expectedStride) throw new StudioBridgeError('INVALID_REQUEST', `stride must equal ${expectedStride} for LZ4 width ${geometry.width}`);
    const decodedSize = requireInteger(params, 'decodedSize');
    const expectedSize = geometry.stride * geometry.height;
    if (decodedSize !== expectedSize) throw new StudioBridgeError('INVALID_REQUEST', `decodedSize must equal stride * height (${expectedSize})`);
    const channel = SCENE_TRANSPORT_PROFILE.channels.frameTileLz4;
    if (decodedSize <= 0 || decodedSize > channel.maxDecodedBytes) {
      throw payloadTooLarge(`Channel ${channel.id} decoded LZ4 bitmap is ${decodedSize} bytes; max ${channel.maxDecodedBytes}.`);
    }
    if (compressed.length === 0) throw new StudioBridgeError('INVALID_REQUEST', 'compressed LZ4 block must not be empty');
    const payloadBytes = channel.headerBytes + compressed.length;
    requireTransportPayload(payloadBytes, `Channel ${channel.id} framed LZ4`);
    const complete = tileIndex + 1 === frame.tileCount;
    this.renderer.updateFrameImage(
      { ...geometry, pixels: decodeRawLz4(compressed, decodedSize) },
      complete,
    );
    frame.nextIndex += 1;
    if (complete) this.frame = null;
    return {
      frameId,
      tileIndex,
      nextIndex: frame.nextIndex,
      complete,
      transport: transportSummary(channel.id, 'atomic-framed-lz4', payloadBytes, decodedSize),
    };
  }
}

function requireString(values, key) {
  const value = values[key];
  if (typeof value !== 'string' || value.length === 0) throw new StudioBridgeError('INVALID_REQUEST', `${key} must be a non-empty string`);
  return value;
}

function requireInteger(values, key) {
  const value = values[key];
  if (!Number.isInteger(value)) throw new StudioBridgeError('INVALID_REQUEST', `${key} must be an integer`);
  return value;
}

function requireUint32(values, key) {
  const value = requireInteger(values, key);
  if (value < 0 || value > 0xffffffff) throw new StudioBridgeError('INVALID_REQUEST', `${key} must be uint32`);
  return value;
}

function requireImageBase64(values) {
  const value = requireString(values, 'dataBase64');
  if (value.length > SCENE_TRANSPORT_PROFILE.maxBase64Characters) {
    throw payloadTooLarge(`image base64 is ${value.length} characters; max ${SCENE_TRANSPORT_PROFILE.maxBase64Characters}`);
  }
  return value;
}

function decodePluginMessageBase64(value) {
  if (typeof value !== 'string' || value.length === 0 || value.length % 4 !== 0 || !/^[A-Za-z0-9+/]*={0,2}$/.test(value)) {
    throw new StudioBridgeError('INVALID_REQUEST', 'dataBase64 must be non-empty valid base64');
  }
  try {
    const data = decodeBase64(value);
    if (data.length === 0) throw new Error('empty');
    return data;
  } catch {
    throw new StudioBridgeError('INVALID_REQUEST', 'dataBase64 must be non-empty valid base64');
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

function imageGeometry(values) {
  return { x: requireInteger(values, 'x'), y: requireInteger(values, 'y'), width: requireInteger(values, 'width'), height: requireInteger(values, 'height'), stride: requireInteger(values, 'stride') };
}

function eventCapability(name) {
  if (name === 'device.button') return 'button';
  if (name === 'device.imuGesture') return 'imuGesture';
  if (name === 'device.rawImu') return 'rawImu';
  return 'connection';
}

function requireTransportPayload(payloadBytes, label) {
  if (payloadBytes <= 0 || payloadBytes > SCENE_TRANSPORT_PROFILE.maxPayloadBytes) {
    throw payloadTooLarge(`${label} payload is ${payloadBytes} bytes; max ${SCENE_TRANSPORT_PROFILE.maxPayloadBytes}`);
  }
}

function payloadTooLarge(message) {
  return new StudioBridgeError('PAYLOAD_TOO_LARGE', message);
}

function transportSummary(channel, codec, payloadBytes, decodedBytes) {
  return {
    channel,
    codec,
    payloadBytes,
    decodedBytes,
    maxPayloadBytes: SCENE_TRANSPORT_PROFILE.maxPayloadBytes,
  };
}

function normalizeError(error) {
  if (error instanceof StudioBridgeError) return error;
  if (error instanceof RangeError || error instanceof TypeError) return new StudioBridgeError('INVALID_REQUEST', error.message);
  return new StudioBridgeError('INTERNAL_ERROR', 'Studio operation failed');
}

function randomToken() {
  return globalThis.crypto?.randomUUID?.() ?? `studio-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}
