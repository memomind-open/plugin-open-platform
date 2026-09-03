import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AppWebViewTransport,
  createGMPlugin,
  GMPluginError,
  ParentFrameTransport,
} from '../src/index.js';

class FakeTransport {
  constructor() {
    this.requests = [];
    this.listeners = new Set();
    this.bootstrapListeners = new Set();
    this.bootstrap = { sessionToken: 'test-session', runtimeGeneration: 7 };
  }

  async waitForBootstrap() {
    return this.bootstrap;
  }

  async send(request) {
    this.requests.push(request);
    if (request.method === 'runtime.ready') return { ready: true, generation: 7 };
    return { echoed: request.params };
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  currentBootstrap() { return this.bootstrap; }

  subscribeBootstrap(listener) {
    this.bootstrapListeners.add(listener);
    return () => this.bootstrapListeners.delete(listener);
  }

  replaceBootstrap(bootstrap) {
    this.bootstrap = bootstrap;
    for (const listener of this.bootstrapListeners) listener(bootstrap);
  }

  emit(event) {
    for (const listener of this.listeners) listener(event);
  }
}

test('SDK adds the Bridge v1 runtime envelope', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  await gm.storage.set('score', 42);

  assert.equal(transport.requests[0].version, '1.0');
  assert.equal(transport.requests[0].sessionToken, 'test-session');
  assert.equal(transport.requests[0].runtimeGeneration, 7);
  assert.equal(transport.requests[1].method, 'storage.set');
  assert.deepEqual(transport.requests[1].params, { key: 'score', value: 42 });
});

test('SDK exposes persistent user-file helpers with the formal request shapes', async () => {
  const transport = new FakeTransport();
  transport.send = async function send(request) {
    this.requests.push(request);
    if (request.method === 'files.openRead') {
      return {
        resourceUrl: 'https://plugin-files.invalid/read-token',
        fileId: request.params.fileId,
        size: 100_000,
        offset: request.params.offset,
        length: request.params.length,
      };
    }
    return { echoed: request.params };
  };
  const body = Uint8Array.of(1, 2, 3, 4);
  const fetchCalls = [];
  const gm = createGMPlugin({
    transport,
    fetchImpl: async (url, options) => {
      fetchCalls.push({ url, options });
      return new Response(body, { status: 200 });
    },
  });
  await gm.files.pick({ extensions: ['txt'], allowMultiple: false });
  await gm.files.list();
  await gm.files.stat('a'.repeat(32));
  const opened = await gm.files.openRead('a'.repeat(32), { offset: 65_536, length: 4096 });
  const received = new Uint8Array(await new Response(opened.stream).arrayBuffer());
  await gm.files.getUsage();
  await gm.files.delete('a'.repeat(32));

  await assert.rejects(
    () => gm.files.openRead('a'.repeat(32), { offset: -1 }),
    (error) => error instanceof GMPluginError && error.code === 'INVALID_REQUEST',
  );
  await assert.rejects(
    () => gm.files.openRead('a'.repeat(32), { length: 0 }),
    (error) => error instanceof GMPluginError && error.code === 'INVALID_REQUEST',
  );

  assert.deepEqual(received, body);
  assert.equal(opened.size, 100_000);
  assert.deepEqual(fetchCalls.map(({ url, options }) => ({
    url,
    method: options.method,
    cache: options.cache,
  })), [{
    url: 'https://plugin-files.invalid/read-token',
    method: 'GET',
    cache: 'no-store',
  }]);

  assert.deepEqual(transport.requests.map(({ method, params }) => ({ method, params })), [
    { method: 'files.pick', params: { extensions: ['txt'], allowMultiple: false } },
    { method: 'files.list', params: {} },
    { method: 'files.stat', params: { fileId: 'a'.repeat(32) } },
    { method: 'files.openRead', params: { fileId: 'a'.repeat(32), offset: 65_536, length: 4096 } },
    { method: 'files.getUsage', params: {} },
    { method: 'files.delete', params: { fileId: 'a'.repeat(32) } },
  ]);
});

test('SDK accepts a Host-controlled transferable binary stream', async () => {
  const transport = new FakeTransport();
  const bytes = Uint8Array.of(4, 3, 2, 1);
  let hostPort;
  transport.send = async (request) => {
    const channel = new MessageChannel();
    hostPort = channel.port1;
    let sent = false;
    channel.port1.onmessage = (event) => {
      if (event.data?.type !== 'pull') return;
      if (!sent) {
        sent = true;
        const buffer = bytes.slice().buffer;
        channel.port1.postMessage({ type: 'chunk', buffer }, [buffer]);
      } else {
        channel.port1.postMessage({ type: 'end' });
      }
    };
    return {
      streamPort: channel.port2,
      fileId: request.params.fileId,
      size: bytes.length,
      offset: 0,
      length: bytes.length,
    };
  };
  const gm = createGMPlugin({
    transport,
    fetchImpl: () => {
      throw new Error('direct Host streams must not use fetch');
    },
  });

  const opened = await gm.files.openRead('a'.repeat(32));
  const reader = opened.stream.getReader();
  assert.deepEqual((await reader.read()).value, bytes);
  hostPort.postMessage({
    type: 'error',
    code: 'FILE_NOT_FOUND',
    message: 'File read stream is no longer valid',
  });

  await assert.rejects(
    () => reader.read(),
    (error) => error instanceof GMPluginError && error.code === 'FILE_NOT_FOUND',
  );
});

test('SDK rejects malformed or unauthorized binary stream tickets', async () => {
  const transport = new FakeTransport();
  transport.send = async () => ({
    resourceUrl: 'https://plugin-files.invalid/read-token',
    fileId: 'b'.repeat(32),
    size: 10,
    offset: 0,
    length: 10,
  });
  let fetchCalled = false;
  const gm = createGMPlugin({
    transport,
    fetchImpl: async () => {
      fetchCalled = true;
      return new Response(null, { status: 403 });
    },
  });
  await assert.rejects(
    () => gm.files.openRead('a'.repeat(32)),
    (error) => error instanceof GMPluginError && error.code === 'INTERNAL_ERROR',
  );
  assert.equal(fetchCalled, false);

  transport.send = async (request) => ({
    resourceUrl: 'https://plugin-files.invalid/read-token',
    fileId: request.params.fileId,
    size: 10,
    offset: 0,
    length: 10,
  });
  await assert.rejects(
    () => gm.files.openRead('a'.repeat(32)),
    (error) => error instanceof GMPluginError && error.code === 'UNAUTHORIZED',
  );
});

test('SDK does not apply the ordinary short timeout to the interactive file picker', async () => {
  const transport = new FakeTransport();
  transport.send = async function send(request) {
    this.requests.push(request);
    await new Promise((resolve) => setTimeout(resolve, 20));
    return { files: [] };
  };
  const gm = createGMPlugin({ transport, timeoutMs: 5 });
  await assert.doesNotReject(() => gm.files.pick());
});

test('SDK filters stale events and exposes typed helpers', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  const gestures = [];
  gm.device.onGesture((event) => gestures.push(event.gesture));
  transport.emit({ name: 'device.imuGesture', data: { gesture: 'headRaise' }, runtimeGeneration: 6 });
  transport.emit({ name: 'device.imuGesture', data: { gesture: 'headLower' }, runtimeGeneration: 7 });
  assert.deepEqual(gestures, ['headLower']);
});

test('SDK exposes glasses-to-Web plugin messages as Uint8Array values', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  const messages = [];
  const offMessage = gm.plugin.onMessage((message) => messages.push(message));

  transport.emit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'AQIDBA==' },
    runtimeGeneration: 6,
  });
  transport.emit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'AQIDBA==' },
    runtimeGeneration: 7,
  });

  assert.equal(messages.length, 1);
  assert.equal(messages[0].channel, 0x4648);
  assert.deepEqual(messages[0].data, Uint8Array.of(1, 2, 3, 4));

  offMessage();
  transport.emit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'BQ==' },
    runtimeGeneration: 7,
  });
  assert.equal(messages.length, 1);
});

test('SDK isolates invalid plugin message event payloads', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  const messages = [];
  gm.plugin.onMessage((message) => messages.push(message));

  const invalidEvents = [
    { channel: 0x10000, dataBase64: 'AQ==' },
    { channel: 0x4648, dataBase64: 'not-base64!' },
    { channel: 0x4648, dataBase64: '' },
  ];
  for (const data of invalidEvents) {
    assert.doesNotThrow(() => transport.emit({
      name: 'plugin.message',
      data,
      runtimeGeneration: 7,
    }));
  }

  const oversizedBase64 = Buffer.alloc(81902).toString('base64');
  const originalBuffer = globalThis.Buffer;
  let decodeAttempted = false;
  globalThis.Buffer = { from: () => { decodeAttempted = true; return Uint8Array.of(); } };
  try {
    assert.doesNotThrow(() => transport.emit({
      name: 'plugin.message',
      data: { channel: 0x4648, dataBase64: oversizedBase64 },
      runtimeGeneration: 7,
    }));
  } finally {
    globalThis.Buffer = originalBuffer;
  }
  assert.equal(decodeAttempted, false);
  assert.equal(messages.length, 0);

  const boundaryPayload = Uint8Array.from(
    { length: 81901 },
    (_, index) => (index * 37 + 11) & 0xff,
  );
  transport.emit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: Buffer.from(boundaryPayload).toString('base64') },
    runtimeGeneration: 7,
  });
  assert.equal(messages.length, 1);
  assert.deepEqual(messages[0].data, boundaryPayload);

  assert.throws(
    () => gm.plugin.onMessage(null),
    (error) => error instanceof GMPluginError && error.code === 'INVALID_REQUEST',
  );
});

test('App WebView bridge delivers native plugin.message events', () => {
  const globalObject = {
    MemoPluginBridge: { postMessage: () => {} },
  };
  const transport = new AppWebViewTransport({ globalObject });
  const gm = createGMPlugin({ transport });
  const messages = [];
  gm.plugin.onMessage((message) => messages.push(message));

  globalObject.__memoPluginEmit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'AQ==' },
    runtimeGeneration: 3,
  });
  assert.equal(messages.length, 0);

  globalObject.__memoPluginBootstrap('app-session', 3);
  globalObject.__memoPluginEmit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'CQgHBg==' },
    runtimeGeneration: 2,
  });
  globalObject.__memoPluginEmit({
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'CQgHBg==' },
    runtimeGeneration: 3,
  });

  assert.equal(messages.length, 1);
  assert.deepEqual(messages[0], {
    channel: 0x4648,
    data: Uint8Array.of(9, 8, 7, 6),
  });
});

test('SDK uses refreshed bootstrap credentials after a runtime replacement', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  transport.replaceBootstrap({ sessionToken: 'replacement', runtimeGeneration: 8 });
  await gm.runtime.ping();
  assert.equal(transport.requests[1].sessionToken, 'replacement');
  assert.equal(transport.requests[1].runtimeGeneration, 8);
});

test('parent transport rejects spoofed windows and origins', async () => {
  let receive;
  const posted = [];
  const parent = { postMessage: (...args) => posted.push(args) };
  const windowObject = {
    parent,
    addEventListener: (_name, listener) => { receive = listener; },
    removeEventListener: () => {},
  };
  const transport = new ParentFrameTransport({
    windowObject,
    parentOrigin: 'https://studio.example',
    timeoutMs: 1000,
  });
  const bootstrap = transport.waitForBootstrap();
  assert.equal(posted[0][1], 'https://studio.example');
  receive({ source: {}, origin: 'https://studio.example', data: {
    type: 'gm-plugin:bootstrap', bootstrap: { sessionToken: 'spoof', runtimeGeneration: 1 },
  } });
  receive({ source: parent, origin: 'https://attacker.example', data: {
    type: 'gm-plugin:bootstrap', bootstrap: { sessionToken: 'spoof', runtimeGeneration: 1 },
  } });
  assert.equal(transport.currentBootstrap(), undefined);
  receive({ source: parent, origin: 'https://studio.example', data: {
    type: 'gm-plugin:bootstrap', bootstrap: { sessionToken: 'trusted', runtimeGeneration: 2 },
  } });
  assert.deepEqual(await bootstrap, { sessionToken: 'trusted', runtimeGeneration: 2 });
  transport.close();
});

test('parent transport rejects requests pending across bootstrap replacement', async () => {
  let receive;
  const parent = { postMessage: () => {} };
  const windowObject = {
    parent,
    addEventListener: (_name, listener) => { receive = listener; },
    removeEventListener: () => {},
  };
  const transport = new ParentFrameTransport({
    windowObject,
    parentOrigin: 'https://studio.example',
    timeoutMs: 1000,
  });
  receive({ source: parent, origin: 'https://studio.example', data: {
    type: 'gm-plugin:bootstrap', bootstrap: { sessionToken: 'first', runtimeGeneration: 1 },
  } });
  const pending = transport.send({ requestId: 'pending', method: 'runtime.ping' });
  receive({ source: parent, origin: 'https://studio.example', data: {
    type: 'gm-plugin:bootstrap', bootstrap: { sessionToken: 'second', runtimeGeneration: 2 },
  } });
  await assert.rejects(pending, { name: 'GMPluginError', code: 'RUNTIME_REPLACED' });
  transport.close();
});

test('SDK rejects unknown event names', () => {
  const gm = createGMPlugin({ transport: new FakeTransport() });
  assert.throws(
    () => gm.on('device.private', () => {}),
    (error) => error instanceof GMPluginError && error.code === 'INVALID_REQUEST',
  );
});

test('SDK encodes plugin messages for the App bridge', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.plugin.sendMessage(0x4647, Uint8Array.of(2, 9, 0, 0xa1));
  assert.equal(transport.requests[0].method, 'plugin.sendMessage');
  assert.deepEqual(transport.requests[0].params, {
    channel: 0x4647,
    dataBase64: 'AgkAoQ==',
  });
  await assert.rejects(
    () => gm.plugin.sendMessage(1, [1, 2, 3]),
    (error) => error instanceof GMPluginError && error.code === 'INVALID_REQUEST',
  );
});

test('SDK exposes atomic framed LZ4 display helpers', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });

  await gm.display.beginFrame({ frameId: 17, tileCount: 2 });
  await gm.display.updateFrameImageLz4({
    frameId: 17,
    tileIndex: 0,
    x: 0,
    y: 0,
    width: 2,
    height: 1,
    stride: 1,
    decodedSize: 1,
    dataBase64: 'EAE=',
  });

  assert.equal(transport.requests[0].method, 'display.beginFrame');
  assert.equal(transport.requests[1].method, 'display.updateFrameImageLz4');
});

test('SDK encodes binary plugin messages for the Bridge transport', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });

  await gm.plugin.sendMessage(0x4647, Uint8Array.of(2, 7, 0x12, 0x34));

  assert.equal(transport.requests[0].method, 'plugin.sendMessage');
  assert.deepEqual(transport.requests[0].params, {
    channel: 0x4647,
    dataBase64: 'AgcSNA==',
  });
});

test('SDK rejects invalid plugin message inputs before transport', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });

  await assert.rejects(() => gm.plugin.sendMessage(-1, Uint8Array.of(1)), {
    name: 'GMPluginError',
    code: 'INVALID_REQUEST',
  });
  await assert.rejects(() => gm.plugin.sendMessage(1, new Uint8Array()), {
    name: 'GMPluginError',
    code: 'INVALID_REQUEST',
  });
  await assert.rejects(() => gm.plugin.sendMessage(1, new Uint8Array(81902)), {
    name: 'GMPluginError',
    code: 'PAYLOAD_TOO_LARGE',
  });
  assert.equal(transport.requests.length, 0);
});

test('SDK exposes native audio calls and decodes Opus frame batches', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  const batches = [];
  gm.audio.onFrames((batch) => batches.push(batch));

  await gm.audio.configure({ noiseReduction: true, pickupMode: 'frontFocus' });
  await gm.audio.startRecording();
  transport.emit({
    name: 'audio.frames',
    data: {
      recordingId: 'recording-7-1',
      firstSequence: 0,
      frameCount: 2,
      droppedFrameCount: 0,
      framesBase64: ['AQID', 'BAUG'],
    },
    runtimeGeneration: 7,
  });

  assert.equal(transport.requests[0].method, 'audio.configure');
  assert.deepEqual(transport.requests[0].params, {
    noiseReduction: true,
    pickupMode: 'frontFocus',
  });
  assert.equal(transport.requests[1].method, 'audio.startRecording');
  assert.equal(batches.length, 1);
  assert.deepEqual(batches[0].frames, [
    Uint8Array.of(1, 2, 3),
    Uint8Array.of(4, 5, 6),
  ]);
});

test('SDK isolates malformed native audio frame events', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  const batches = [];
  gm.audio.onFrames((batch) => batches.push(batch));

  assert.doesNotThrow(() => transport.emit({
    name: 'audio.frames',
    data: { framesBase64: ['not-base64!'] },
    runtimeGeneration: 7,
  }));
  assert.equal(batches.length, 0);
});
