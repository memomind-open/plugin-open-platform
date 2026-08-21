import assert from 'node:assert/strict';
import test from 'node:test';

import { createGMPlugin, GMPluginError } from '../src/index.js';

class FakeTransport {
  constructor() {
    this.requests = [];
    this.listeners = new Set();
  }

  async waitForBootstrap() {
    return { sessionToken: 'test-session', runtimeGeneration: 7 };
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
