import assert from 'node:assert/strict';
import test from 'node:test';

import { StudioRuntime } from '../src/index.js';

class FakeRenderer {
  constructor() {
    this.operations = [];
  }
  clear() { this.operations.push(['clear']); }
  updateText(params) { this.operations.push(['text', params]); }
  updateImage(params) { this.operations.push(['image', params]); }
  beginFrame(params) { this.operations.push(['frame-begin', params]); }
  updateFrameImage(params, complete) { this.operations.push(['frame-image', params, complete]); }
}

function request(runtime, method, params = {}) {
  return runtime.handle({
    version: '1.0',
    sessionToken: runtime.sessionToken,
    requestId: `request-${method}`,
    method,
    params,
    runtimeGeneration: runtime.runtimeGeneration,
  });
}

test('Studio runtime handles every public runtime and storage method', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  assert.equal((await request(runtime, 'runtime.ready')).ok, true);
  assert.deepEqual((await request(runtime, 'runtime.getBridgeVersion')).result, { version: '1.0' });
  await request(runtime, 'storage.set', { key: 'score', value: 12 });
  assert.deepEqual((await request(runtime, 'storage.get', { key: 'score' })).result, { value: 12 });
  assert.deepEqual((await request(runtime, 'storage.remove', { key: 'score' })).result, { removed: true });
  assert.deepEqual((await request(runtime, 'storage.clear')).result, { cleared: true });
});

test('Studio runtime preserves picked files and opens binary range streams', async () => {
  const storage = new Map();
  const fileStore = new Map();
  const source = Uint8Array.from({ length: 70_000 }, (_, index) => index % 251);
  const firstRun = new StudioRuntime({
    renderer: new FakeRenderer(),
    sessionToken: 'first',
    storage,
    fileStore,
    filePicker: async () => [{ name: 'book.txt', bytes: source }],
  });
  const picked = await request(firstRun, 'files.pick', {
    extensions: ['txt'],
    allowMultiple: false,
  });
  assert.equal(picked.ok, true);
  const [file] = picked.result.files;
  assert.match(file.fileId, /^[0-9a-f]{32}$/u);
  assert.equal(file.size, source.length);

  const revoked = [];
  const openedRanges = [];
  const restarted = new StudioRuntime({
    renderer: new FakeRenderer(),
    sessionToken: 'second',
    storage,
    fileStore,
    fileResourceFactory: ({ bytes, offset, length }) => {
      assert.deepEqual(bytes, source.subarray(offset, offset + length));
      openedRanges.push({ offset, length });
      return `blob:https://studio.invalid/read-token-${openedRanges.length}`;
    },
    fileResourceRevoke: (resourceUrl) => revoked.push(resourceUrl),
  });
  assert.deepEqual((await request(restarted, 'files.list')).result.files, [file]);
  assert.deepEqual((await request(restarted, 'files.stat', { fileId: file.fileId })).result, { file });
  const opened = await request(restarted, 'files.openRead', {
    fileId: file.fileId,
    offset: 4096,
    length: 65_536,
  });
  assert.deepEqual(opened.result, {
    resourceUrl: 'blob:https://studio.invalid/read-token-1',
    fileId: file.fileId,
    size: source.length,
    offset: 4096,
    length: 65_536,
  });
  const tail = await request(restarted, 'files.openRead', {
    fileId: file.fileId,
    offset: source.length - 4,
    length: 100,
  });
  assert.equal(tail.result.length, 4);
  assert.deepEqual(openedRanges, [
    { offset: 4096, length: 65_536 },
    { offset: source.length - 4, length: 4 },
  ]);
  restarted.setLifecycle('suspended');
  assert.deepEqual(revoked, [
    'blob:https://studio.invalid/read-token-1',
    'blob:https://studio.invalid/read-token-2',
  ]);
  assert.deepEqual((await request(restarted, 'files.getUsage')).result, {
    fileCount: 1,
    totalBytes: source.length,
    maxTotalBytes: 400 * 1024 * 1024,
  });
  assert.equal((await request(restarted, 'files.delete', { fileId: file.fileId })).result.deleted, true);
  assert.equal((await request(restarted, 'files.list')).result.files.length, 0);
});

test('Studio runtime renders text and rejects drawing while disconnected', async () => {
  const renderer = new FakeRenderer();
  const runtime = new StudioRuntime({ renderer, sessionToken: 'token' });
  const params = { id: 1, x: 1, y: 2, width: 100, height: 40, border: 1, radius: 4, text: 'Hello' };
  assert.equal((await request(runtime, 'display.updateText', params)).ok, true);
  assert.equal(renderer.operations[0][0], 'text');
  runtime.setConnected(false);
  const response = await request(runtime, 'display.updateText', params);
  assert.equal(response.ok, false);
  assert.equal(response.error.code, 'DEVICE_DISCONNECTED');
});

test('Studio runtime only emits subscribed device events', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  const events = [];
  runtime.onEvent((event) => events.push(event));
  const result = await request(runtime, 'device.subscribeEvents', { types: ['button', 'imuGesture'] });
  await new Promise((resolve) => setImmediate(resolve));
  runtime.emitButton('single');
  runtime.emitGesture('headRaise');
  runtime.setConnected(false);
  assert.equal(result.result.subscriptionId, 'sub-1');
  assert.deepEqual(events.map((event) => event.name), ['device.button', 'device.imuGesture']);
});

test('Studio runtime emits plugin messages without a device event subscription', () => {
  const runtime = new StudioRuntime({
    renderer: new FakeRenderer(),
    sessionToken: 'token',
    runtimeGeneration: 4,
  });
  const events = [];
  runtime.onEvent((event) => events.push(event));

  runtime.emitPluginMessage(0x4648, Uint8Array.of(1, 2, 3, 4));

  assert.deepEqual(events, [{
    name: 'plugin.message',
    data: { channel: 0x4648, dataBase64: 'AQIDBA==' },
    runtimeGeneration: 4,
  }]);
  assert.throws(
    () => runtime.emitPluginMessage(0x10000, Uint8Array.of(1)),
    { code: 'INVALID_REQUEST' },
  );
});

test('Studio runtime rejects stale generations and unknown methods', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token', runtimeGeneration: 3 });
  const stale = await runtime.handle({ version: '1.0', sessionToken: 'token', requestId: 'stale', method: 'runtime.ready', params: {}, runtimeGeneration: 2 });
  assert.equal(stale.error.code, 'STALE_RUNTIME');
  const unknown = await runtime.handle({ version: '1.0', sessionToken: 'token', requestId: 'unknown', method: 'private.method', params: {}, runtimeGeneration: 3 });
  assert.equal(unknown.error.code, 'METHOD_NOT_FOUND');
});

test('Studio validates and records plugin messages', async () => {
  const handled = [];
  const runtime = new StudioRuntime({
    renderer: new FakeRenderer(),
    sessionToken: 'token',
    pluginMessageHandler: (message) => handled.push(message),
  });
  const response = await request(runtime, 'plugin.sendMessage', {
    channel: 0x4647,
    dataBase64: Buffer.from([2, 7, 0, 0xa1]).toString('base64'),
  });
  assert.equal(response.ok, true);
  assert.deepEqual(response.result, { sent: true, channel: 0x4647, payloadBytes: 4 });
  assert.deepEqual([...runtime.pluginMessages[0].data], [2, 7, 0, 0xa1]);
  assert.equal(typeof runtime.pluginMessages[0].timestampMs, 'number');
  assert.equal(handled[0], runtime.pluginMessages[0]);

  const invalid = await request(runtime, 'plugin.sendMessage', {
    channel: 0x10000,
    dataBase64: 'AQ==',
  });
  assert.equal(invalid.error.code, 'INVALID_REQUEST');
});

test('Studio rejects oversized Channel 6 images and accepts vertical tiles', async () => {
  const renderer = new FakeRenderer();
  const runtime = new StudioRuntime({ renderer, sessionToken: 'token' });
  const stride = 300;
  const oversized = new Uint8Array(stride * 300);
  const rejected = await request(runtime, 'display.updateImage', {
    x: 0, y: 0, width: 600, height: 300, stride,
    dataBase64: Buffer.from(oversized).toString('base64'),
  });
  assert.equal(rejected.ok, false);
  assert.equal(rejected.error.code, 'PAYLOAD_TOO_LARGE');
  assert.match(rejected.error.message, /at most 272 rows/);

  const tile = new Uint8Array(stride * 272);
  const accepted = await request(runtime, 'display.updateImage', {
    x: 0, y: 0, width: 600, height: 272, stride,
    dataBase64: Buffer.from(tile).toString('base64'),
  });
  assert.equal(accepted.ok, true);
  assert.equal(accepted.result.transport.channel, 6);
  assert.equal(accepted.result.transport.payloadBytes, 81610);
});

test('Studio applies Channel 7 decoded-size limit before LZ4 rendering', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  const rejected = await request(runtime, 'display.updateImageLz4', {
    x: 0, y: 0, width: 600, height: 350, stride: 300,
    decodedSize: 105000,
    dataBase64: Buffer.from([0x10, 0]).toString('base64'),
  });
  assert.equal(rejected.ok, false);
  assert.equal(rejected.error.code, 'PAYLOAD_TOO_LARGE');
  assert.match(rejected.error.message, /at most 273 rows/);
  assert.match(rejected.error.message, /compress every tile independently/);
});

test('Studio validates atomic framed LZ4 order and commits only the final tile', async () => {
  const renderer = new FakeRenderer();
  const runtime = new StudioRuntime({ renderer, sessionToken: 'token' });
  const begin = await request(runtime, 'display.beginFrame', { frameId: 9, tileCount: 2 });
  assert.deepEqual(begin.result, { frameId: 9, tileIndex: 0xffff, nextIndex: 0, complete: false });

  const tile = (tileIndex, y) => request(runtime, 'display.updateFrameImageLz4', {
    frameId: 9, tileIndex, x: 0, y, width: 2, height: 1, stride: 1,
    decodedSize: 1, dataBase64: Buffer.from([0x10, tileIndex + 1]).toString('base64'),
  });
  const first = await tile(0, 0);
  assert.equal(first.result.complete, false);
  assert.equal(renderer.operations.at(-1)[2], false);
  const skipped = await tile(2, 1);
  assert.equal(skipped.ok, false);
  assert.equal(skipped.error.code, 'INVALID_REQUEST');
  const last = await tile(1, 1);
  assert.equal(last.result.complete, true);
  assert.equal(renderer.operations.at(-1)[2], true);
});

test('Studio bounds generic plugin message history', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  for (let index = 0; index <= 100; index += 1) {
    await request(runtime, 'plugin.sendMessage', {
      channel: 0x4647,
      dataBase64: Buffer.from([index]).toString('base64'),
    });
  }

  assert.equal(runtime.pluginMessages.length, 100);
  assert.deepEqual([...runtime.pluginMessages[0].data], [1]);
  assert.deepEqual([...runtime.pluginMessages.at(-1).data], [100]);
});

test('Studio rejects invalid generic plugin messages without recording them', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  const invalidChannel = await request(runtime, 'plugin.sendMessage', {
    channel: 0x10000,
    dataBase64: 'AQ==',
  });
  const empty = await request(runtime, 'plugin.sendMessage', {
    channel: 1,
    dataBase64: '',
  });
  const oversized = await request(runtime, 'plugin.sendMessage', {
    channel: 1,
    dataBase64: Buffer.alloc(81902).toString('base64'),
  });

  assert.equal(invalidChannel.error.code, 'INVALID_REQUEST');
  assert.equal(empty.error.code, 'INVALID_REQUEST');
  assert.equal(oversized.error.code, 'PAYLOAD_TOO_LARGE');
  assert.deepEqual(runtime.pluginMessages, []);
});

test('Studio rejects generic plugin messages while disconnected', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  runtime.setConnected(false);
  const response = await request(runtime, 'plugin.sendMessage', {
    channel: 1,
    dataBase64: 'AQ==',
  });
  assert.equal(response.error.code, 'DEVICE_DISCONNECTED');
  assert.deepEqual(runtime.pluginMessages, []);
});
