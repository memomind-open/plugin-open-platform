import assert from 'node:assert/strict';
import test from 'node:test';

import { StudioRuntime as BaseRuntime } from '../src/index.js';

// Existing functional tests explicitly approve their declared fixture permissions.
class StudioRuntime extends BaseRuntime {
  constructor(options) {
    const permissions=['storage','files.user-selected','display','device.info'].map(name=>({name,required:true}));
    permissions.push({name:'device.events',required:true,scope:{types:['button','imuGesture','rawImu','connection']}});
    super({permissions,approvals:Object.fromEntries(permissions.map(p=>[p.name,p.scope??null])),...options});
  }
}
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
    version: '2.0',
    sessionToken: runtime.sessionToken,
    requestId: `request-${method}`,
    method,
    params,
    runtimeGeneration: runtime.runtimeGeneration,
  });
}

async function readPort(port) {
  const chunks = [];
  port.start();
  while (true) {
    const message = new Promise((resolve) => port.addEventListener('message', resolve, { once: true }));
    port.postMessage({ type: 'pull' });
    const { data } = await message;
    if (data.type === 'end') break;
    if (data.type === 'error') throw Object.assign(new Error(data.message), { code: data.code });
    chunks.push(new Uint8Array(data.buffer));
  }
  const length = chunks.reduce((total, chunk) => total + chunk.length, 0);
  const bytes = new Uint8Array(length);
  let offset = 0;
  for (const chunk of chunks) {
    bytes.set(chunk, offset);
    offset += chunk.length;
  }
  return bytes;
}

test('Studio runtime handles every public runtime and storage method', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token' });
  assert.equal((await request(runtime, 'runtime.ready')).ok, true);
  assert.deepEqual((await request(runtime, 'runtime.getBridgeVersion')).result, { version: '2.0' });
  await request(runtime, 'storage.set', { key: 'score', value: 12 });
  assert.deepEqual((await request(runtime, 'storage.get', { key: 'score' })).result, { value: 12 });
  assert.deepEqual((await request(runtime, 'storage.remove', { key: 'score' })).result, { removed: true });
  assert.deepEqual((await request(runtime, 'storage.clear')).result, { cleared: true });
});

test('Studio runtime preserves picked files and opens binary range streams', async () => {
  const storage = new Map();
  const fileStore = new Map();
  const source = Uint8Array.from({ length: 700_000 }, (_, index) => index % 251);
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

  const restarted = new StudioRuntime({
    renderer: new FakeRenderer(),
    sessionToken: 'second',
    storage,
    fileStore,
  });
  assert.deepEqual((await request(restarted, 'files.list')).result.files, [file]);
  assert.deepEqual((await request(restarted, 'files.stat', { fileId: file.fileId })).result, { file });
  const opened = await request(restarted, 'files.openRead', {
    fileId: file.fileId,
    offset: 4096,
    length: 65_536,
  });
  assert.equal(opened.result.fileId, file.fileId);
  assert.equal(opened.result.size, source.length);
  assert.equal(opened.result.offset, 4096);
  assert.equal(opened.result.length, 65_536);
  assert.deepEqual(
    await readPort(opened.result.streamPort),
    source.slice(4096, 4096 + 65_536),
  );
  const tail = await request(restarted, 'files.openRead', {
    fileId: file.fileId,
    offset: source.length - 4,
    length: 100,
  });
  assert.equal(tail.result.length, 4);
  assert.deepEqual(
    await readPort(tail.result.streamPort),
    source.slice(source.length - 4),
  );
  assert.deepEqual((await request(restarted, 'files.getUsage')).result, {
    fileCount: 1,
    totalBytes: source.length,
    maxTotalBytes: 400 * 1024 * 1024,
  });
  const active = await request(restarted, 'files.openRead', { fileId: file.fileId });
  const clientPort = structuredClone(active.result.streamPort, { transfer: [active.result.streamPort] });
  clientPort.start();
  const firstChunk = new Promise((resolve) => clientPort.addEventListener('message', resolve, { once: true }));
  clientPort.postMessage({ type: 'pull' });
  assert.equal(new Uint8Array((await firstChunk).data.buffer).length, 256 * 1024);
  const invalidated = new Promise((resolve) => clientPort.addEventListener('message', resolve, { once: true }));
  assert.equal((await request(restarted, 'files.delete', { fileId: file.fileId })).result.deleted, true);
  assert.deepEqual((await invalidated).data, {
    type: 'error',
    code: 'FILE_NOT_FOUND',
    message: 'File read stream is no longer valid',
  });
  restarted.setLifecycle('suspended');
  assert.equal(restarted.fileStreams.size, 0);
  assert.equal((await request(restarted, 'files.list')).error.message, 'NOT_FOREGROUND');
  restarted.setLifecycle('running');
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

test('runtime rejects unapproved messages and drops uplink', async () => {
 const runtime=new StudioRuntime({renderer:new FakeRenderer(),sessionToken:'token'});
 const events=[]; runtime.eventListeners.add(e=>events.push(e));
 runtime.emitPluginMessage(32766,Uint8Array.of(1));
 const response=await request(runtime,'plugin.sendMessage',{channel:32766,dataBase64:'AQ=='});
 assert.equal(response.error.code,'PERMISSION_DENIED');
 assert.deepEqual(runtime.pluginMessages,[]); assert.deepEqual(events,[]);
});

test('scoped messages round trip and stop after approval or lifecycle changes',async()=>{
 const runtime=new StudioRuntime({renderer:new FakeRenderer(),sessionToken:'token',permissions:[{name:'device.messaging',required:false,scope:{channels:[500]}}],approvals:{'device.messaging':{channels:[500]}}});
 const events=[];runtime.onEvent(e=>events.push(e));
 assert.equal((await request(runtime,'plugin.sendMessage',{channel:500,dataBase64:'AQ=='})).ok,true);
 assert.equal((await request(runtime,'plugin.sendMessage',{channel:501,dataBase64:'AQ=='})).error.message,'OUT_OF_SCOPE');
 runtime.emitPluginMessage(500,Uint8Array.of(1));runtime.emitPluginMessage(501,Uint8Array.of(1));
 assert.equal(events.length,1);assert.equal(events[0].data.dataBase64,'AQ==');
 runtime.approvals={};runtime.emitPluginMessage(500,Uint8Array.of(1));assert.equal(events.length,1);
 runtime.approvals={'device.messaging':{channels:[500]}};runtime.lifecycleState='stopped';
 runtime.emitPluginMessage(500,Uint8Array.of(1));assert.equal(events.length,1);
 assert.equal((await request(runtime,'plugin.sendMessage',{channel:500,dataBase64:'AQ=='})).ok,false);
 assert.equal(runtime.pluginMessages.length,1);
});

test('capabilities omit reserved channels lacking their standard permission',async()=>{
 const runtime=new StudioRuntime({renderer:new FakeRenderer(),sessionToken:'token',permissions:[{name:'device.messaging',required:false,scope:{channels:[2,500]}}],approvals:{'device.messaging':{channels:[2,500]}}});
 const response=await request(runtime,'runtime.getCapabilities',{});
 assert.deepEqual(response.result.pluginMessaging.channels,[500]);
 assert.equal((await request(runtime,'plugin.sendMessage',{channel:2,dataBase64:'AQ=='})).ok,false);
});

test('Studio runtime rejects stale generations and unknown methods', async () => {
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token', runtimeGeneration: 3 });
  const stale = await runtime.handle({ version: '2.0', sessionToken: 'token', requestId: 'stale', method: 'runtime.ready', params: {}, runtimeGeneration: 2 });
  assert.equal(stale.error.code, 'STALE_RUNTIME');
  const unknown = await runtime.handle({ version: '2.0', sessionToken: 'token', requestId: 'unknown', method: 'private.method', params: {}, runtimeGeneration: 3 });
  assert.equal(unknown.error.code, 'METHOD_NOT_FOUND');
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
  const runtime = new StudioRuntime({ renderer: new FakeRenderer(), sessionToken: 'token', permissions:[{name:'device.messaging',required:false,scope:{channels:[0x4647]}}], approvals:{'device.messaging':{channels:[0x4647]}} });
  for (let index = 0; index <= 100; index += 1) {
    await request(runtime, 'plugin.sendMessage', {
      channel: 0x4647,
      dataBase64: Buffer.from([index]).toString('base64'),
    });
  }

  assert.equal(runtime.pluginMessages.length, 100);
});
