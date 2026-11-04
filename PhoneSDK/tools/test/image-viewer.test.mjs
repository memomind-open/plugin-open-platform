import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
import { WIDTH, HEIGHT, FRAME_BYTES, ALIGNMENTS, placement, composeImage, rgbaToGray4, readImageBlob } from '../../examples/image-viewer/image.js';
import { pickImageFile, openImageRead, waitForFilePickerReturn, imageMime } from '../../examples/image-viewer/files.js';
import { ImageTransfer, OP, packet, parseAck, hash, RESPONSE_CHANNEL } from '../../examples/image-viewer/protocol.js';
import { compressLz4 } from '../../examples/image-viewer/lz4.js';
import { decodeRawLz4 } from '../../packages/device-renderer/src/index.js';
globalThis.crypto ??= webcrypto;

test('pixel placement: no scale; small/large/exact/mixed sizes and odd borders', () => {
  for (const [w,h,dx,dy,cw,ch] of [[100,50,250,150,100,50], [900,500,0,0,600,350], [600,350,0,0,600,350], [800,100,0,125,600,100], [100,800,250,0,100,350], [1,1,299,174,1,1], [599,349,0,0,599,349]]) {
    assert.deepEqual(placement(w,h), {sx:0,sy:0,width:cw,height:ch,dx,dy});
  }
  assert.throws(() => placement(0, 20));
});

test('Canvas receives identical integer source/destination extents; preview uses transmitted quantization', () => {
  let draw;
  const rgba = new Uint8ClampedArray(WIDTH * HEIGHT * 4);
  rgba.set([255,255,255,255, 0,0,0,255]);
  const context = { fillRect(x,y,w,h) { assert.deepEqual([x,y,w,h], [0,0,600,350]); },
    drawImage(...args) {draw=args;}, getImageData() {return {data:rgba};}, putImageData(image) { assert.deepEqual([...image.data.slice(0,8)], [0,255,0,255, 0,0,0,255]); } };
  const canvas = { getContext() {return context;} };
  const image = {};
  assert.equal(composeImage(image,123,41,canvas)[0], 0xf0);
  assert.equal(context.imageSmoothingEnabled,false);
  assert.deepEqual(draw,[image,0,0,123,41,238,154,123,41]);
  assert.equal(canvas.width,600); assert.equal(canvas.height,350);
});

test('GRAY4: black/white, alpha on black, high nibble first, exact frame size', () => {
  const rgba = new Uint8ClampedArray(WIDTH * HEIGHT * 4);
  rgba.set([255,255,255,255, 0,0,0,255, 255,255,255,0, 255,255,255,128]);
  const bytes = rgbaToGray4(rgba);
  assert.equal(bytes.length,105000); assert.equal(bytes[0],0xf0); assert.equal(bytes[1],0x08);
  assert.throws(() => rgbaToGray4(new Uint8Array(1)));
});

function fixture({ drop = false, failure, mutate, pending = false, lz4 = false } = {}) {
  let listener, count = 0;
  const calls = [];
  const gm = {plugin: {
    onMessage(cb) {listener=cb; return () => {listener=null;};},
    async sendMessage(channel, data) {
      calls.push(Uint8Array.from(data)); count++;
      if (failure) throw failure;
      if (pending || (drop && count === 1)) return;
      const request = parseAck(Uint8Array.from([...data.subarray(0,12),0,0,88,2,94,1,0,16]));
      const ack = new Uint8Array(20), v = new DataView(ack.buffer);
      ack.set(data.subarray(0,12)); ack[1] |= 128;
      v.setUint32(8, data[1] === OP.chunk ? request.offset + data.length - 12 : request.offset, true);
      v.setUint16(14,600,true); v.setUint16(16,350,true); v.setUint16(18,4096,true);
      ack[13] = lz4 ? 1 : 0;
      mutate?.(ack, data);
      listener?.({channel:RESPONSE_CHANNEL,data:ack});
    }
  }};
  return {gm,calls};
}

test('full transfer uses sequential chunks and business commit ACK; delayed ACK retries same session', async () => {
  const {gm,calls} = fixture({drop:true});
  const transfer = new ImageTransfer(gm,{timeout:10,retries:2});
  const frame = new Uint8Array(FRAME_BYTES).fill(0x5a);
  let progress=0;
  await transfer.send(frame, sent => {progress=sent;});
  assert.deepEqual(calls[0],calls[1]);
  assert.equal(progress,FRAME_BYTES);
  assert.equal(calls.at(-1)[1],OP.commit);
  const chunks = calls.filter(c=>c[1]===OP.chunk);
  assert.equal(chunks.length,26);
  assert.deepEqual(Uint8Array.from(chunks.flatMap(c=>[...c.subarray(12)])),frame);
  const begin = calls.find(c=>c[1]===OP.begin);
  assert.equal(new DataView(begin.buffer).getUint32(16,true),hash(frame));
  transfer.close();
});

test('permission errors are surfaced without retry', async () => {
  const {gm,calls} = fixture({failure:Object.assign(new Error('denied'),{code:'PERMISSION_DENIED'})});
  const transfer = new ImageTransfer(gm,{timeout:10});
  await assert.rejects(transfer.send(new Uint8Array(FRAME_BYTES)),/denied/);
  assert.equal(calls.length,1); transfer.close();
});

test('wrong geometry, corrupt ACK offset, device rejection, and lost ACK fail honestly', async () => {
  for (const mutate of [ack=>{ack[14]=0;}, (ack,data)=>{if(data[1]===OP.chunk) ack[8]^=1;}, ack=>{ack[12]=3;}]) {
    const {gm}=fixture({mutate}); const transfer=new ImageTransfer(gm,{timeout:10});
    await assert.rejects(transfer.send(new Uint8Array(FRAME_BYTES))); transfer.close();
  }
  const {gm,calls}=fixture({pending:true}); const transfer=new ImageTransfer(gm,{timeout:5,retries:1});
  await assert.rejects(transfer.send(new Uint8Array(FRAME_BYTES)),e=>e.code==='ACK_TIMEOUT');
  assert.equal(calls.length,2); transfer.close();
});

test('stale-session ACK ignored and abort stops a pending transfer', async () => {
  const {gm} = fixture({mutate:ack=>{ack[4]^=1;}});
  const transfer = new ImageTransfer(gm,{timeout:1000});
  const abort = new AbortController();
  const sending = transfer.send(new Uint8Array(FRAME_BYTES),()=>{},abort.signal);
  setTimeout(()=>abort.abort(new Error('disconnected')),5);
  await assert.rejects(sending,/disconnected/); assert.equal(transfer.busy,false); transfer.close();
});

test('call overlay is reported as deferred, never as already displayed', async () => {
  const {gm}=fixture({mutate:(ack,data)=>{if(data[1]===OP.commit) ack[12]=1;}});
  const transfer=new ImageTransfer(gm);
  assert.equal((await transfer.send(new Uint8Array(FRAME_BYTES))).status,1); transfer.close();
});

test('file stream validates actual byte count and cancels malformed streams', async () => {
  const file={fileId:'a',size:4};
  const mock=bytes=>({files:{async openRead(){return {fileId:'a',offset:0,length:4,stream:new ReadableStream({start(c){c.enqueue(bytes);c.close();}})};}}});
  const image = await readImageBlob(mock(Uint8Array.from([66,77,0,0])),file);
  assert.equal(image.size,4); assert.equal(image.type,'image/bmp');
  await assert.rejects(readImageBlob(mock(new Uint8Array(3)),file),e=>e.messageKey==='error.incompleteFile');
  await assert.rejects(readImageBlob(mock(new Uint8Array(5)),file),e=>e.messageKey==='error.length');
});

test('codec rejects malformed replies and uses little endian fields',()=>{
  assert.equal(parseAck(new Uint8Array(19)),null);
  assert.deepEqual([...packet(OP.chunk,0x1234,0x12345678,0x10203040)], [1,3,0x34,0x12,0x78,0x56,0x34,0x12,0x40,0x30,0x20,0x10]);
});


test('image selection and cancellation use the Host-managed files API', async () => {
  let params;
  const file={fileId:'photo',name:'photo.PNG',size:8};
  const gm={files:{async pick(options){params=options;return {files:[file]};}}};
  assert.equal(await pickImageFile(gm),file);
  assert.deepEqual(params,{extensions:['png','jpg','jpeg','bmp','webp'],allowMultiple:false});
  gm.files.pick=async()=>({files:[]});
  assert.equal(await pickImageFile(gm),null);
});

test('file signature determines MIME, independent of Android provider/extension', () => {
  assert.equal(imageMime(Uint8Array.from([137,80,78,71,13,10,26,10])),'image/png');
  assert.equal(imageMime(Uint8Array.from([255,216,255])),'image/jpeg');
  assert.equal(imageMime(Uint8Array.from([66,77])),'image/bmp');
  assert.equal(imageMime(Uint8Array.from([82,73,70,70,0,0,0,0,87,69,66,80])),'image/webp');
  assert.throws(()=>imageMime(new TextEncoder().encode('not a PNG')),e=>e.messageKey==='error.format');
});

test('Novel Reader ticket recovery requests a fresh stream up to twice', async () => {
  let attempts=0;
  const expected={stream:'fresh'};
  const gm={files:{async openRead(id){
    assert.equal(id,'image');attempts++;
    if(attempts<3) throw Object.assign(new Error('Host file stream failed: HTTP 404'),{code:'INTERNAL_ERROR'});
    return expected;
  }}};
  assert.equal(await openImageRead(gm,'image'),expected);assert.equal(attempts,3);
});

test('permanent file errors and permission failures are never retried', async () => {
  for(const [code,message] of [['FILE_NOT_FOUND','missing'],['UNAUTHORIZED','Host file stream failed: HTTP 403'],['PERMISSION_DENIED','NOT_GRANTED']]) {
    let attempts=0;const error=Object.assign(new Error(message),{code});
    const gm={files:{async openRead(){attempts++;throw error;}}};
    await assert.rejects(openImageRead(gm,'image'),e=>e===error);assert.equal(attempts,1);
  }
});

test('ticket recovery is bounded and abort cancels its wait without reopening', async () => {
  let attempts=0;
  const gm={files:{async openRead(){attempts++;throw Object.assign(new Error('Host file stream failed: HTTP 404'),{code:'INTERNAL_ERROR'});}}};
  await assert.rejects(openImageRead(gm,'image'),/HTTP 404/);assert.equal(attempts,3);
  attempts=0;const controller=new AbortController();
  const pending=openImageRead(gm,'image',controller.signal);
  setTimeout(()=>controller.abort(new Error('cancelled')),5);
  await assert.rejects(pending,/cancelled/);assert.equal(attempts,1);
});

test('picker return waits for foreground, but stopped/cancelled runtimes cannot continue', async () => {
  let reads=0;
  const gm={runtime:{async getLifecycleState(){return {state:reads++===0?'suspended':'running'};}}};
  await waitForFilePickerReturn(gm);assert.equal(reads,2);
  gm.runtime.getLifecycleState=async()=>({state:'stopped'});
  await assert.rejects(waitForFilePickerReturn(gm),e=>e.messageKey==='error.stopped');
  const controller=new AbortController();controller.abort(new Error('cancelled'));
  await assert.rejects(waitForFilePickerReturn(gm,controller.signal),/cancelled/);
});


test('LZ4 round trips repetitive, literal, boundary and random blocks without losing pixels', () => {
  let seed=1234567;
  for(const length of [0,1,5,12,13,19,270,65535,65536,FRAME_BYTES]) {
    const noise=Uint8Array.from({length},()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return seed&255;});
    for(const input of [noise,new Uint8Array(length),Uint8Array.from({length},(_,i)=>i%47)]) {
      assert.deepEqual(decodeRawLz4(compressLz4(input),length),input);
    }
  }
});

test('LZ4 sends only compressed bytes, checks decoded hash, and reports actual progress', async () => {
  const {gm,calls}=fixture({lz4:true});const transfer=new ImageTransfer(gm);
  const frame=new Uint8Array(FRAME_BYTES).fill(0x5a),compressed=compressLz4(frame);
  let totalBytes=0;
  const result=await transfer.send(frame,(sent,total,stats)=>{totalBytes=total;assert.equal(stats.encoding,'LZ4');},undefined,compressed);
  const begin=calls.find(c=>c[1]===OP.beginLz4),v=new DataView(begin.buffer);
  assert.equal(v.getUint32(16,true),hash(frame));assert.equal(v.getUint32(20,true),compressed.length);
  assert.equal(v.getUint32(24,true),FRAME_BYTES);
  const chunks=calls.filter(c=>c[1]===OP.chunk);
  assert.equal(chunks.length,1);assert.equal(totalBytes,compressed.length);
  assert.deepEqual(Uint8Array.from(chunks.flatMap(c=>[...c.subarray(12)])),compressed);
  assert.equal(result.wireBytes,compressed.length);assert.equal(result.encoding,'LZ4');transfer.close();
});

test('incompressible frames and legacy receivers use exact raw bytes with explicit reason', async () => {
  let seed=42;
  const noise=Uint8Array.from({length:FRAME_BYTES},()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return seed&255;});
  assert.ok(compressLz4(noise).length>=noise.length);
  for(const [lz4,frame] of [[true,noise],[false,new Uint8Array(FRAME_BYTES)]]) {
    const {gm,calls}=fixture({lz4});const transfer=new ImageTransfer(gm);
    const result=await transfer.send(frame);
    assert.equal(result.encoding,'GRAY4');assert.equal(result.wireBytes,FRAME_BYTES);assert.ok(result.reason);
    assert.ok(calls.some(c=>c[1]===OP.begin));assert.ok(!calls.some(c=>c[1]===OP.beginLz4));
    transfer.close();
  }
});


test('all nine alignments reach preview and compressed wire pixels without scaling', async () => {
  for (const [width, height] of [[101, 51], [800, 101], [101, 800], [900, 500]]) {
    for (const [index, alignment] of ALIGNMENTS.entries()) {
      const w = Math.min(width, WIDTH), h = Math.min(height, HEIGHT);
      const dx = [0, Math.floor((WIDTH-w)/2), WIDTH-w][index % 3];
      const dy = [0, Math.floor((HEIGHT-h)/2), HEIGHT-h][Math.floor(index/3)];
      assert.deepEqual(placement(width,height,alignment), {sx:0,sy:0,width:w,height:h,dx,dy});
      const rgba = new Uint8ClampedArray(WIDTH * HEIGHT * 4);
      let preview;
      const ctx = {
        fillRect() { rgba.fill(0); },
        drawImage(image,sx,sy,cw,ch,x,y,dw,dh) {
          assert.deepEqual([sx,sy,cw,ch,x,y,dw,dh], [0,0,w,h,dx,dy,w,h]);
          for(let row=y;row<y+dh;row++) for(let col=x;col<x+dw;col++) rgba.fill(255,(row*WIDTH+col)*4,(row*WIDTH+col+1)*4);
        },
        getImageData() {return {data:rgba};},
        putImageData(image) {preview=image.data;}
      };
      const bytes=composeImage({},width,height,{getContext:()=>ctx},alignment);
      const compressed=compressLz4(bytes);
      const {gm,calls}=fixture({lz4:true});
      const transfer=new ImageTransfer(gm);
      await transfer.send(bytes,()=>{},undefined,compressed);
      const wire=Uint8Array.from(calls.filter(c=>c[1]===OP.chunk).flatMap(c=>[...c.subarray(12)]));
      const decoded=decodeRawLz4(wire,FRAME_BYTES);
      assert.deepEqual(decoded,bytes);
      for(let y=0;y<HEIGHT;y++) for(let x=0;x<WIDTH;x++) {
        const i=y*WIDTH+x, expected=x>=dx&&x<dx+w&&y>=dy&&y<dy+h?15:0;
        assert.equal((decoded[i>>1]>>((i&1)?0:4))&15,expected);
        assert.equal(preview[i*4+1],expected*17);
      }
      transfer.close();
    }
  }
  assert.throws(()=>placement(100,50,'invalid'), /alignment/);
});
