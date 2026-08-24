import assert from 'node:assert/strict';
import test from 'node:test';

import { createGMPlugin, GMPluginError, ParentFrameTransport } from '../src/index.js';

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
