import {authorize, normalizePermissions, validateApprovals, grantedMethods} from '../../bridge-contract/src/permission-policy.js';
import {SimulatedLocation} from '../../bridge-contract/src/simulated-location.js';
import {
  BRIDGE_VERSION,
  CAPABILITIES,
  DEVICE_PROFILE,
  FILE_PROFILE,
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
  constructor({
    renderer,
    permissions = [],
    approvals = {},
    sessionToken = randomToken(),
    runtimeGeneration = 1,
    pluginMessageHandler,
    storage = new Map(),
    fileStore = new Map(),
    filePicker,
    fileStreamFactory = createMessagePortResource,
  } = {}) {
    if (!renderer) throw new TypeError('renderer is required');
    this.permissions = normalizePermissions(permissions);
    this.approvals = validateApprovals(this.permissions, structuredClone(approvals));
    this.location = new SimulatedLocation((name,data)=>this.emit(name,data),()=>this.lifecycleState==='running');
    this.renderer = renderer;
    this.sessionToken = sessionToken;
    this.runtimeGeneration = runtimeGeneration;
    this.lifecycleState = 'running';
    this.connected = true;
    this.storage = storage;
    this.fileStore = fileStore;
    this.filePicker = filePicker;
    this.fileStreamFactory = fileStreamFactory;
    this.fileStreams = new Map();
    this.fileStreamSequence = 0;
    this.fileSequence = fileStore.size;
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
    if (!request || typeof request !== 'object' || request.version !== BRIDGE_VERSION || typeof request.requestId !== 'string' || request.requestId.length === 0 || request.requestId.length > 128 || typeof request.method !== 'string' || typeof request.params !== 'object' || request.params === null || Array.isArray(request.params) || request.method.length < 1 || request.method.length > 128 || new TextEncoder().encode(JSON.stringify(request)).length > 128 * 1024) {
      throw new StudioBridgeError('INVALID_REQUEST', 'Bridge request fields are invalid');
    }
    if (request.sessionToken !== this.sessionToken) throw new StudioBridgeError('UNAUTHORIZED', 'Session token does not match');
    if (request.runtimeGeneration !== this.runtimeGeneration) throw new StudioBridgeError('STALE_RUNTIME', 'Runtime generation is stale');
    if (!isMethodName(request.method)) throw new StudioBridgeError('METHOD_NOT_FOUND', 'METHOD_NOT_FOUND');
  }

  async dispatch(method, params) {
    authorize(method, params, this.permissions, this.approvals);
    if(this.lifecycleState !== 'running' && !method.startsWith('runtime.')) throw new StudioBridgeError('PERMISSION_DENIED','NOT_FOREGROUND');
    switch (method) {
      case 'runtime.ready':
      case 'runtime.ping':
        return { ready: true, generation: this.runtimeGeneration };
      case 'runtime.getBridgeVersion':
        return { version: BRIDGE_VERSION };
      case 'runtime.getCapabilities':
        return {
          methods: grantedMethods(this.permissions,this.approvals).filter(m=>!m.startsWith('audio.')),
          storage: 'storage' in this.approvals,
          display: 'display' in this.approvals ? CAPABILITIES.display : [],
          events: this.approvals['device.events']?.types ?? [],
          rawImuDefaultEnabled:false, audioPlayback:false,
          ...('device.messaging' in this.approvals ? {pluginMessaging:{...PLUGIN_MESSAGE_PROFILE,channels:this.approvals['device.messaging'].channels.filter(channel=>{try{authorize('plugin.sendMessage',{channel},this.permissions,this.approvals);return true;}catch(_){return false;}})}}:{}),
          ...('files.user-selected' in this.approvals ? {files:FILE_PROFILE}:{}),
          ...('location.foreground' in this.approvals ? {location:{simulated:true,coordinateSystem:'WGS84'}}:{}),
        };
      case 'location.getCurrentPosition': return this.location.current(params);
      case 'location.watchPosition': return this.location.watch(params);
      case 'location.clearWatch': return this.location.clear(params);
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
      case 'files.pick':
        return this.pickFiles(params);
      case 'files.list':
        return { files: this.listFiles() };
      case 'files.stat':
        return { file: { ...this.requireFile(params.fileId).metadata } };
      case 'files.openRead':
        return this.openFileRead(params);
      case 'files.getUsage':
        return {
          fileCount: this.fileStore.size,
          totalBytes: [...this.fileStore.values()]
            .reduce((total, entry) => total + entry.bytes.length, 0),
          maxTotalBytes: FILE_PROFILE.maxTotalBytes,
        };
      case 'files.delete':
        return this.deleteFile(params.fileId);
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
      case 'audio.openCapture':
      case 'audio.stopCapture':
        throw new StudioBridgeError(
          'CAPABILITY_UNAVAILABLE',
          'Native glasses audio is unavailable in Studio',
        );
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

  async pickFiles(params) {
    const extensions = normalizeExtensions(params.extensions);
    const allowMultiple = params.allowMultiple === true;
    if (params.allowMultiple !== undefined && typeof params.allowMultiple !== 'boolean') {
      throw new StudioBridgeError('INVALID_REQUEST', 'allowMultiple must be boolean');
    }
    if (!this.filePicker) return { files: [] };
    const selected = await this.filePicker({ extensions, allowMultiple });
    authorize('files.pick',params,this.permissions,this.approvals);
    if(this.lifecycleState !== 'running') throw new StudioBridgeError('PERMISSION_DENIED','NOT_FOREGROUND');
    if (!Array.isArray(selected)) {
      throw new StudioBridgeError('INTERNAL_ERROR', 'Studio file picker returned invalid data');
    }
    const bounded = allowMultiple
      ? selected.slice(0, FILE_PROFILE.maxPickFiles)
      : selected.slice(0, 1);
    const files = [];
    for (const item of bounded) {
      const name = requirePickedFileName(item?.name);
      const bytes = normalizePickedBytes(item?.bytes);
      const extension = fileExtension(name);
      if (extensions.length && (!extension || !extensions.includes(extension))) continue;
      const totalBytes = [...this.fileStore.values()]
        .reduce((total, entry) => total + entry.bytes.length, 0);
      if (totalBytes + bytes.length > FILE_PROFILE.maxTotalBytes) {
        throw new StudioBridgeError('QUOTA_EXCEEDED', 'User file storage quota exceeded');
      }
      let fileId;
      do {
        fileId = (++this.fileSequence).toString(16).padStart(32, '0');
      } while (this.fileStore.has(fileId));
      const metadata = {
        fileId,
        name,
        size: bytes.length,
        importedAt: new Date().toISOString(),
        ...(extension ? { extension } : {}),
      };
      this.fileStore.set(fileId, { metadata, bytes });
      files.push({ ...metadata });
    }
    return { files };
  }

  listFiles() {
    return [...this.fileStore.values()]
      .map((entry) => ({ ...entry.metadata }))
      .sort((left, right) => right.importedAt.localeCompare(left.importedAt));
  }

  requireFile(fileId) {
    const normalized = requireFileId(fileId);
    const entry = this.fileStore.get(normalized);
    if (!entry) throw new StudioBridgeError('FILE_NOT_FOUND', 'fileId does not exist');
    return entry;
  }

  async openFileRead(params) {
    const entry = this.requireFile(params.fileId);
    const offset = optionalInteger(params, 'offset', 0);
    if (offset < 0 || offset > entry.bytes.length) {
      throw new StudioBridgeError('INVALID_REQUEST', 'offset is outside the file');
    }
    const available = entry.bytes.length - offset;
    const requestedLength = params.length === undefined
      ? available
      : optionalInteger(params, 'length', available);
    if (params.length !== undefined && requestedLength < 1) {
      throw new StudioBridgeError('INVALID_REQUEST', 'length must be greater than zero');
    }
    const length = Math.min(requestedLength, available);
    const bytes = entry.bytes.subarray(offset, offset + length);
    const streamId = `stream-${++this.fileStreamSequence}`;
    const resource = await this.fileStreamFactory({
      fileId: entry.metadata.fileId,
      bytes,
      size: entry.bytes.length,
      offset,
      length,
      sessionToken: this.sessionToken,
      runtimeGeneration: this.runtimeGeneration,
      onClose: () => this.releaseFileStream(streamId),
    });
    if (!resource?.streamPort?.postMessage || typeof resource.cancel !== 'function') {
      throw new StudioBridgeError('INTERNAL_ERROR', 'Studio file stream factory failed');
    }
    const timer = setTimeout(() => this.cancelFileStream(streamId), 60_000);
    timer.unref?.();
    this.fileStreams.set(streamId, {
      fileId: entry.metadata.fileId,
      cancel: resource.cancel,
      timer,
    });
    return {
      streamPort: resource.streamPort,
      fileId: entry.metadata.fileId,
      size: entry.bytes.length,
      offset,
      length,
    };
  }

  cancelFileStream(streamId) {
    const resource = this.fileStreams.get(streamId);
    if (!resource) return;
    this.releaseFileStream(streamId);
    resource.cancel(new StudioBridgeError('FILE_NOT_FOUND', 'File read stream is no longer valid'));
  }

  releaseFileStream(streamId) {
    const resource = this.fileStreams.get(streamId);
    if (resource?.timer) clearTimeout(resource.timer);
    this.fileStreams.delete(streamId);
  }

  invalidateFileStreams(fileId) {
    for (const [streamId, resource] of [...this.fileStreams.entries()]) {
      if (fileId !== undefined && resource.fileId !== fileId) continue;
      this.cancelFileStream(streamId);
    }
  }

  deleteFile(fileId) {
    const normalized = requireFileId(fileId);
    const deleted = this.fileStore.delete(normalized);
    if (deleted) this.invalidateFileStreams(normalized);
    return { deleted };
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
    if (name.startsWith('device.') && subscriptionIds.length === 0) return;
    if (name !== 'runtime.lifecycleChanged' && this.lifecycleState !== 'running') return;
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
    if(state !== 'running') {this.location.dispose();this.invalidateFileStreams();}
    const allowed = new Set(['starting', 'running', 'suspended', 'stopped', 'failed']);
    if (!allowed.has(state)) throw new RangeError(`Unknown lifecycle state: ${state}`);
    this.lifecycleState = state;
    if (state !== 'running') this.invalidateFileStreams();
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
    if (this.lifecycleState !== 'running' || !this.connected) return;
    try { authorize('plugin.sendMessage',{channel},this.permissions,this.approvals); } catch (_) { return; }
    if (!(data instanceof Uint8Array) || !data.length || data.length > PLUGIN_MESSAGE_PROFILE.maxPayloadBytes) return;
    let binary=''; for(const byte of data) binary+=String.fromCharCode(byte);
    this.emit('plugin.message',{channel,dataBase64:btoa(binary)});
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

function normalizeExtensions(value) {
  if (value === undefined) return [];
  if (!Array.isArray(value) || value.length > 32 ||
      value.some((extension) => typeof extension !== 'string')) {
    throw new StudioBridgeError(
      'INVALID_REQUEST',
      'extensions must be an array of at most 32 strings',
    );
  }
  return [...new Set(value
    .map((extension) => extension.trim().replace(/^\./u, '').toLowerCase())
    .filter(Boolean))];
}

function requireFileId(value) {
  if (typeof value !== 'string' || !/^[0-9a-f]{32}$/u.test(value)) {
    throw new StudioBridgeError(
      'INVALID_REQUEST',
      'fileId must be a 32-character lowercase hexadecimal string',
    );
  }
  return value;
}

function requirePickedFileName(value) {
  if (typeof value !== 'string' || value.trim().length === 0) {
    throw new StudioBridgeError('INTERNAL_ERROR', 'Picked file name is invalid');
  }
  return value.trim();
}

function normalizePickedBytes(value) {
  if (value instanceof Uint8Array) return Uint8Array.from(value);
  if (ArrayBuffer.isView(value)) {
    return Uint8Array.from(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
  }
  if (value instanceof ArrayBuffer) return new Uint8Array(value.slice(0));
  throw new StudioBridgeError('INTERNAL_ERROR', 'Picked file bytes are invalid');
}

function fileExtension(name) {
  const match = /\.([^.]+)$/u.exec(name);
  return match?.[1]?.toLowerCase();
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

function optionalInteger(values, key, fallback) {
  if (values[key] === undefined) return fallback;
  return requireInteger(values, key);
}

function createMessagePortResource({ bytes, onClose }) {
  if (typeof globalThis.MessageChannel !== 'function') {
    throw new StudioBridgeError('CAPABILITY_UNAVAILABLE', 'Binary file streaming is unavailable');
  }
  const channel = new globalThis.MessageChannel();
  let offset = 0;
  let finished = false;
  channel.port1.addEventListener('message', (event) => {
    if (finished) return;
    if (event.data?.type === 'cancel') {
      finished = true;
      channel.port1.close();
      onClose?.();
      return;
    }
    if (event.data?.type !== 'pull') return;
    if (offset >= bytes.length) {
      finished = true;
      channel.port1.postMessage({ type: 'end' });
      channel.port1.close();
      onClose?.();
      return;
    }
    const end = Math.min(offset + 256 * 1024, bytes.length);
    const buffer = bytes.slice(offset, end).buffer;
    offset = end;
    channel.port1.postMessage({ type: 'chunk', buffer }, [buffer]);
  });
  channel.port1.addEventListener('messageerror', () => {
    if (!finished) {
      finished = true;
      channel.port1.close();
      onClose?.();
    }
  });
  channel.port1.start();
  return {
    streamPort: channel.port2,
    cancel(reason) {
      if (finished) return;
      finished = true;
      channel.port1.postMessage({
        type: 'error',
        code: reason?.code ?? 'INTERNAL_ERROR',
        message: reason?.message ?? 'File read stream was cancelled',
      });
      channel.port1.close();
      onClose?.();
    },
  };
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
  if (typeof error?.code === 'string') return new StudioBridgeError(error.code,error.message);
  if (error instanceof RangeError || error instanceof TypeError) return new StudioBridgeError('INVALID_REQUEST', error.message);
  return new StudioBridgeError('INTERNAL_ERROR', 'Studio operation failed');
}

function randomToken() {
  return globalThis.crypto?.randomUUID?.() ?? `studio-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}
