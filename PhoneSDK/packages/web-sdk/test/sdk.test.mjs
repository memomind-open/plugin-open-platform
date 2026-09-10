import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AppWebViewTransport,
  createGMPlugin,
  GMPluginError,
  opusRecordingToOgg,
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

test('SDK wraps raw Opus frames as an H5-playable Ogg Blob', async () => {
  const blob = opusRecordingToOgg({ data: Uint8Array.of(1, 2, 3, 4, 5), frameLengths: [2, 3] });
  const bytes = new Uint8Array(await blob.arrayBuffer());
  assert.equal(blob.type, 'audio/ogg; codecs=opus');
  assert.equal(new TextDecoder().decode(bytes.slice(0, 4)), 'OggS');
  assert.equal(new TextDecoder().decode(bytes.slice(28, 36)), 'OpusHead');
  assert.throws(() => opusRecordingToOgg({ data: Uint8Array.of(1), frameLengths: [2] }), TypeError);
});

function encodeTestAudioChunk({
  sequence = 0,
  timestampUs = 0,
  durationMs = 40,
  frameLengths = [2, 3],
  droppedFrameCount = 0,
  discontinuity = false,
  queueLatencyMs = 4,
  data = Uint8Array.of(1, 2, 3, 4, 5),
} = {}) {
  const headerBytes = 40 + frameLengths.length * 2;
  const buffer = new ArrayBuffer(headerBytes + data.byteLength);
  const view = new DataView(buffer);
  view.setUint32(0, 0x474d4155, false);
  view.setUint8(4, 1);
  view.setUint8(5, 1);
  view.setUint16(6, discontinuity ? 1 : 0, false);
  view.setUint16(8, headerBytes, false);
  view.setUint16(10, frameLengths.length, false);
  view.setUint32(12, sequence, false);
  const timestampHigh = Math.floor(timestampUs / 0x100000000);
  view.setUint32(16, timestampHigh, false);
  view.setUint32(20, timestampUs - timestampHigh * 0x100000000, false);
  view.setUint32(24, durationMs, false);
  view.setUint32(28, droppedFrameCount, false);
  view.setUint32(32, queueLatencyMs, false);
  view.setUint32(36, data.byteLength, false);
  frameLengths.forEach((length, index) => view.setUint16(40 + index * 2, length, false));
  new Uint8Array(buffer, headerBytes).set(data);
  return buffer;
}

function recordingFixture(sessionId) {
  const channel = new MessageChannel();
  let sent = false;
  let finished = false;
  channel.port1.onmessage = (event) => {
    if (JSON.parse(event.data)?.type !== 'pull' || sent) return;
    sent = true;
    const buffer = encodeTestAudioChunk();
    channel.port1.postMessage(buffer, [buffer]);
    if (finished) channel.port1.postMessage(JSON.stringify({ type: 'end' }));
  };
  return {
    ticket: {
      sessionId, streamPort: channel.port2,
      resolvedOptions: {
        profile: 'reliable', chunkDurationMs: 100, maxQueueMs: 3000,
        overflowStrategy: 'error', pickupMode: 'frontFocus', noiseReduction: true,
        codec: 'opus', sampleRate: 16000, channels: 1, maxDurationMs: 5000,
      },
    },
    finish() {
      finished = true;
      if (sent) channel.port1.postMessage(JSON.stringify({ type: 'end' }));
      return {
        sessionId, durationMs: 40, frameCount: 2, opusBytes: 5,
        deliveredFrameCount: 2, droppedFrameCount: 0,
      };
    },
  };
}

test('SDK adds the Bridge v2 runtime envelope', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await gm.ready();
  await gm.storage.set('score', 42);

  assert.equal(transport.requests[0].version, '2.0');
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

test('SDK exposes bounded custom messaging with decoded uplink', async () => {
 const transport=new FakeTransport(); const gm=createGMPlugin({transport});
 await gm.ready(); await gm.plugin.sendMessage(500,Uint8Array.of(1));
 assert.equal(transport.requests.at(-1).method,'plugin.sendMessage');
 const received=[];gm.plugin.onMessage(m=>received.push(m));
 transport.emit({name:'plugin.message',runtimeGeneration:7,data:{channel:500,dataBase64:'AQ=='}});
 assert.equal(received.length,1);assert.deepEqual(received[0].data,Uint8Array.of(1));
 transport.emit({name:'plugin.message',runtimeGeneration:6,data:{channel:500,dataBase64:'AQ=='}});
 transport.emit({name:'plugin.message',runtimeGeneration:7,data:{channel:500,dataBase64:'_w=='}});
 assert.equal(received.length,1);
 await assert.rejects(gm.plugin.sendMessage(500,new Uint8Array()),{code:'INVALID_REQUEST'});
 await assert.rejects(gm.plugin.sendMessage(500,new Uint8Array(81902)),{code:'PAYLOAD_TOO_LARGE'});
  gm.close();
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





test('App WebView bridge joins an audio descriptor with event.ports[0]', async () => {
  let receiveWindowMessage;
  const globalObject = {
    MemoPluginBridge: { postMessage: () => {} },
    addEventListener: (name, listener) => {
      if (name === 'message') receiveWindowMessage = listener;
    },
    removeEventListener: () => {},
  };
  const transport = new AppWebViewTransport({ globalObject, timeoutMs: 1000 });
  globalObject.__memoPluginBootstrap('app-session', 3);
  const descriptor = {
    id: 'stream-token-1234567890',
    kind: 'audio.capture',
    sessionId: 'capture-app-1',
    runtimeGeneration: 3,
  };
  const responsePromise = transport.send({
    requestId: 'request-audio-1', method: 'audio.openCapture', runtimeGeneration: 3,
  });
  globalObject.__memoPluginResolve({
    requestId: 'request-audio-1', ok: true,
    runtimeGeneration: 3,
    result: { sessionId: 'capture-app-1', streamDescriptor: descriptor },
  });
  const channel = new MessageChannel();
  receiveWindowMessage({
    data: JSON.stringify({ type: 'gm-plugin:stream-port', descriptor }),
    ports: [channel.port2],
  });
  const result = await responsePromise;

  assert.equal(result.streamPort, channel.port2);
  assert.deepEqual(result.streamDescriptor, descriptor);
  channel.port1.close();
  channel.port2.close();

  const earlyDescriptor = {
    id: 'stream-token-0987654321',
    kind: 'audio.capture',
    sessionId: 'capture-app-2',
    runtimeGeneration: 3,
  };
  const earlyChannel = new MessageChannel();
  receiveWindowMessage({
    data: { type: 'gm-plugin:stream-port', descriptor: earlyDescriptor },
    ports: [earlyChannel.port2],
  });
  const earlyResponsePromise = transport.send({
    requestId: 'request-audio-2', method: 'audio.openCapture', runtimeGeneration: 3,
  });
  globalObject.__memoPluginResolve({
    requestId: 'request-audio-2', ok: true,
    runtimeGeneration: 3,
    result: { sessionId: 'capture-app-2', streamDescriptor: earlyDescriptor },
  });
  const earlyResult = await earlyResponsePromise;
  assert.equal(earlyResult.streamPort, earlyChannel.port2);
  earlyChannel.port1.close();
  earlyChannel.port2.close();
  transport.close();
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






test('SDK exposes Bridge 2.0 capture and foreground location without old aliases', async()=>{
  const transport=new FakeTransport();
  const fixture=recordingFixture('capture-7-1');
  transport.send=async function(request){
    this.requests.push(request);
    if(request.method==='audio.openCapture')return fixture.ticket;
    if(request.method==='audio.stopCapture')return fixture.finish();
    return {};
  };
  const gm=createGMPlugin({transport}); const states=[];
  gm.audio.onCaptureState(s=>states.push(s));
  await gm.audio.openRecording({
    noiseReduction:true,pickupMode:'frontFocus',maxDurationMs:5000,
  });
  await gm.audio.stopCapture('capture-7-1');
  assert.deepEqual(transport.requests.at(-1).params,{sessionId:'capture-7-1'});
  await gm.location.getCurrentPosition({timeoutMs:1000});
  await gm.location.watchPosition();
  await gm.location.clearWatch('watch-1');
  transport.emit({name:'audio.captureState',data:{state:'capturing'},runtimeGeneration:7});
  assert.deepEqual(transport.requests.map(r=>r.method),['audio.openCapture','audio.stopCapture','location.getCurrentPosition','location.watchPosition','location.clearWatch']);
  assert.equal(states.length,1);
  assert.equal(gm.audio.configure,undefined);
  assert.equal(gm.audio.onFrames,undefined);
});
test('SDK ignores old audio frames and stale generation location events',async()=>{
  const transport=new FakeTransport();const gm=createGMPlugin({transport});await gm.ready();
  const positions=[];gm.location.onPosition(p=>positions.push(p));
  transport.emit({name:'audio.frames',data:{framesBase64:['bad']},runtimeGeneration:7});
  transport.emit({name:'location.position',data:{latitude:1},runtimeGeneration:6});
  assert.equal(positions.length,0);
});

test('SDK rejects removed Host capture modes before sending', async () => {
  const transport = new FakeTransport();
  const gm = createGMPlugin({ transport });
  await assert.rejects(gm.audio.openCapture({ mode: 'stream' }), { code: 'INVALID_REQUEST' });
  await assert.rejects(gm.audio.openRecording({ mode: 'recording' }), { code: 'INVALID_REQUEST' });
  assert.equal(transport.requests.length, 0);
});

test('stopCapture requires an explicit capture ID before sending',async()=>{
 const transport=new FakeTransport();
 transport.send=async function(request){
  this.requests.push(request);
  return {
   sessionId:'capture-7-2',durationMs:20,frameCount:1,opusBytes:40,
   deliveredFrameCount:1,droppedFrameCount:0,
  };
 };
 const gm=createGMPlugin({transport});
 for(const id of [undefined,null,'',' ',{},1])await assert.rejects(gm.audio.stopCapture(id),{code:'INVALID_REQUEST'});
 assert.equal(transport.requests.length,0);
 await gm.audio.stopCapture('capture-7-2');
 assert.deepEqual(transport.requests[0].params,{sessionId:'capture-7-2'});
});
test('SDK returns H5-owned Opus bytes through the unified recording session', async () => {
  const transport = new FakeTransport();
  const fixture = recordingFixture('capture-recording-1');
  transport.send = async function send(request) {
    this.requests.push(request);
    if (request.method === 'audio.openCapture') return fixture.ticket;
    if (request.method === 'audio.stopCapture') return fixture.finish();
    throw new Error(`unexpected method ${request.method}`);
  };
  const gm = createGMPlugin({ transport });
  const capture = await gm.audio.openRecording({
    pickupMode: 'frontFocus', noiseReduction: true, maxDurationMs: 5000,
  });
  const result = await capture.stop();

  assert.equal('mode' in capture, false);
  assert.equal('stream' in capture, false);
  assert.equal(transport.requests[0].method, 'audio.openCapture');
  assert.deepEqual(transport.requests[0].params, {
    pickupMode: 'frontFocus', noiseReduction: true, profile: 'reliable', maxDurationMs: 5000,
  });
  assert.equal(transport.requests[1].method, 'audio.stopCapture');
  assert.deepEqual([...result.data], [1, 2, 3, 4, 5]);
  assert.deepEqual(result.frameLengths, [2, 3]);
  assert.equal('recordingId' in result, false);
});

test('aborting an H5-owned recording stops its capture session', async () => {
  const transport = new FakeTransport();
  const fixture = recordingFixture('capture-abort-1');
  transport.send = async function send(request) {
    this.requests.push(request);
    if (request.method === 'audio.openCapture') return {
      ...fixture.ticket,
      resolvedOptions: { ...fixture.ticket.resolvedOptions, pickupMode: 'unchanged', maxDurationMs: 15000 },
    };
    if (request.method === 'audio.stopCapture') return fixture.finish();
    throw new Error(`unexpected method ${request.method}`);
  };
  const controller = new AbortController();
  const gm = createGMPlugin({ transport });
  const capture = await gm.audio.openRecording({ signal: controller.signal });
  controller.abort();
  await assert.rejects(capture.stop(), { name: 'AbortError' });

  assert.deepEqual(transport.requests.map(({ method }) => method), [
    'audio.openCapture', 'audio.stopCapture',
  ]);
});

test('SDK exposes real-time audio only as a backpressured transferable binary stream', async () => {
  const transport = new FakeTransport();
  let hostPort;
  transport.send = async function send(request) {
    this.requests.push(request);
    if (request.method === 'audio.openCapture') {
      const channel = new MessageChannel();
      hostPort = channel.port1;
      hostPort.onmessage = (event) => {
        if (JSON.parse(event.data)?.type !== 'pull') return;
        const buffer = encodeTestAudioChunk();
        hostPort.postMessage(buffer, [buffer]);
      };
      return {
        sessionId: 'capture-stream-1',
        resolvedOptions: {
          profile: 'interactive', chunkDurationMs: 40, maxQueueMs: 200,
          overflowStrategy: 'drop-oldest', pickupMode: 'unchanged', noiseReduction: true,
          codec: 'opus', sampleRate: 16000, channels: 1, maxDurationMs: null,
        },
        streamPort: channel.port2,
      };
    }
    if (request.method === 'audio.stopCapture') {
      hostPort.onmessage = null;
      hostPort.postMessage(JSON.stringify({ type: 'end' }));
      return {
        sessionId: 'capture-stream-1', durationMs: 40, frameCount: 2, opusBytes: 5,
        deliveredFrameCount: 2, droppedFrameCount: 0,
      };
    }
    throw new Error(`unexpected method ${request.method}`);
  };
  const gm = createGMPlugin({ transport });
  const states = [];
  gm.audio.onCaptureState((state) => states.push(state.state));
  const capture = await gm.audio.openCapture({ profile: 'interactive' });
  const reader = capture.stream.getReader();
  const chunk = (await reader.read()).value;

  assert.deepEqual(chunk.data, Uint8Array.of(1, 2, 3, 4, 5));
  assert.deepEqual(chunk.frameLengths, [2, 3]);
  transport.emit({
    name: 'audio.captureState',
    data: { state: 'capturing', sessionId: capture.sessionId },
    runtimeGeneration: 7,
  });
  assert.deepEqual(states, ['capturing']);
  const result = await capture.stop();
  assert.equal(result.deliveredFrameCount, 2);
  assert.equal((await reader.read()).done, true);
  hostPort.close();
});

test('SDK rejects a malformed native audio binary envelope', async () => {
  const transport = new FakeTransport();
  let hostPort;
  transport.send = async function send(request) {
    this.requests.push(request);
    if (request.method !== 'audio.openCapture') throw new Error(`unexpected method ${request.method}`);
    const channel = new MessageChannel();
    hostPort = channel.port1;
    hostPort.onmessage = (event) => {
      if (JSON.parse(event.data)?.type !== 'pull') return;
      const invalid = new ArrayBuffer(40);
      hostPort.postMessage(invalid, [invalid]);
    };
    return {
      sessionId: 'capture-invalid-1',
      resolvedOptions: {
        profile: 'interactive', chunkDurationMs: 40, maxQueueMs: 200,
        overflowStrategy: 'drop-oldest', pickupMode: 'unchanged', noiseReduction: true,
        codec: 'opus', sampleRate: 16000, channels: 1, maxDurationMs: null,
      },
      streamPort: channel.port2,
    };
  };
  const gm = createGMPlugin({ transport });
  const capture = await gm.audio.openCapture({ profile: 'interactive' });
  await assert.rejects(() => capture.stream.getReader().read(), {
    name: 'GMPluginError', code: 'INTERNAL_ERROR',
  });
  hostPort.close();
});

for (const kind of ['app','parent']) test(kind+' rejects stale reply before requestId resolution',async()=>{
  let listener;
  const parent={postMessage(){}};
  const windowObject={parent,addEventListener(n,fn){listener=fn;},removeEventListener(){}};
  const globalObject={MemoPluginBridge:{postMessage(){}}};
  const transport=kind==='app'?new AppWebViewTransport({globalObject}):new ParentFrameTransport({windowObject,parentOrigin:'http://host.test'});
  transport.replaceBootstrap({sessionToken:'token',runtimeGeneration:2});
  const pending=transport.send({requestId:'same',runtimeGeneration:2,method:'runtime.ping'});
  const deliver=response=>kind==='app'?transport.resolve(response):listener({source:parent,origin:'http://host.test',data:{type:'gm-plugin:response',response}});
  deliver({requestId:'same',runtimeGeneration:1,ok:true,result:'stale'});
  assert.equal(transport.pending.size,1);
  deliver({requestId:'same',runtimeGeneration:2,ok:true,result:'current'});
  assert.equal(await pending,'current');
  transport.close?.();
});
