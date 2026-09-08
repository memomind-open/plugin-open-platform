import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {readBounded,makeToneWav,gray4Pattern,hexBytes,validateHttpUrl} from '../../examples/permission-debug/lab-core.js';
import {validateManifestPolicy} from '../../packages/bridge-contract/src/permission-policy.js';
import vm from 'node:vm';
test('lab declares all nine permissions optionally, reserving out-of-scope probes',async()=>{
 const manifest=JSON.parse(await readFile(new URL('../../examples/permission-debug/manifest.json',import.meta.url)));
 const p=validateManifestPolicy(manifest);assert.equal(p.length,9);assert.ok(p.every(x=>!x.required));
 assert.ok(!p.find(x=>x.name==='device.events').scope.types.includes('rawImu'));
 assert.ok(!p.some(x=>x.name==='device.messaging'));
});
test('WAV test sound has a valid 2-second PCM payload and nonzero samples',()=>{
 const bytes=makeToneWav(),view=new DataView(bytes.buffer);
 assert.equal(new TextDecoder().decode(bytes.subarray(0,4)),'RIFF');
 assert.equal(view.getUint32(24,true),16000);assert.equal(view.getUint32(40,true),64000);
 assert.ok(bytes.subarray(44).some(x=>x!==0));
});
test('GRAY4 preview has correct byte length and contrasting squares',()=>{
 const p=gray4Pattern();assert.equal(p.bytes.length,p.stride*p.height);
 assert.equal(p.bytes[0],0x33);assert.equal(p.bytes[5],0xff);
});
test('binary editor bounds and validates input',()=>{
 assert.deepEqual(hexBytes('48 65 6c 6c 6f'),new TextEncoder().encode('Hello'));
 for(const input of ['','g0','1','00'.repeat(1025)])assert.throws(()=>hexBytes(input));
});
test('HTTP probe rejects non-http protocols and embedded credentials',()=>{
 assert.equal(validateHttpUrl('https://example.com/health'),'https://example.com/health');
 for(const url of ['file:///a','javascript:alert(1)','https://u:p@example.com'])assert.throws(()=>validateHttpUrl(url));
});
test('stream previews truncate and cancel instead of loading an entire file',async()=>{
 let cancelled=false;
 const stream=new ReadableStream({start(c){c.enqueue(new Uint8Array([1,2,3,4,5]));},cancel(){cancelled=true;}});
 assert.deepEqual(await readBounded(stream,3),new Uint8Array([1,2,3]));assert.ok(cancelled);
});
test('all UI actions are wired and recording requests use the App recording mode',async()=>{
 const source=await readFile(new URL('../../examples/permission-debug/plugin.js',import.meta.url),'utf8');
 const html=await readFile(new URL('../../examples/permission-debug/index.html',import.meta.url),'utf8');
 const actions=[...html.matchAll(/data-action="([^"]+)"/g)].map(m=>m[1]);
 assert.equal(new Set(actions).size,actions.length);
 for(const name of actions)assert.ok(source.includes(name+':')||source.includes(name+'(')||source.includes(name+','),name);
 assert.ok(source.includes("mode:'recording'"));
 assert.ok(source.includes("pickupMode:'frontBalanced'"));
 assert.ok(!source.includes('getUserMedia'));
 assert.ok(source.includes("credentials:'omit'"));
});

test('lab receives Opus bytes through capture sessions and plays them in H5',async()=>{
 const source=(await readFile(new URL('../../examples/permission-debug/plugin.js',import.meta.url),'utf8')).replace(/^import .*;\n/gm,'');
 const nodes=new Map(),stops=[];let listener,resolveStart,failStop=false;
 const context2d={createImageData:()=>({data:new Uint8ClampedArray(4)}),putImageData(){}};
 const element=()=>({value:'',textContent:'',dataset:{},getContext:()=>context2d,addEventListener(){},pause(){},play:async()=>{},querySelectorAll:()=>[]});
 const buttons=['playCapturedAudio','playTone','delayTone','stopAudio'].map(action=>({...element(),dataset:{action}}));
 const document={getElementById:id=>{if(!nodes.has(id))nodes.set(id,element());return nodes.get(id);},querySelectorAll:()=>buttons,addEventListener(){}};
 const recording={sessionId:'capture-A',mode:'recording',frameCount:2,opusBytes:5,durationMs:40,data:new Uint8Array([1,2,3,4,5]),frameLengths:[2,3]};
 const makeSession=id=>({sessionId:id,state:'starting',stop:async()=>{stops.push(id);if(failStop)throw Error('failed');return {...recording,sessionId:id};}});
 const noop=()=>{},gm={audio:{openCapture:()=>new Promise(r=>resolveStart=r),stopCapture:async id=>{stops.push(id);return {...recording,sessionId:id};},onCaptureState:fn=>listener=fn},device:{onButton:noop,onGesture:noop,onConnection:noop},plugin:{onMessage:noop},location:{onPosition:noop,onError:noop},on:noop,ready:()=>new Promise(()=>{})};
 const context=vm.createContext({createGMPlugin:()=>gm,opusRecordingToOgg:()=>new Blob(),document,window:{addEventListener(){}},gray4Pattern:()=>({width:1,height:1,bytes:new Uint8Array([0])}),setTimeout,clearTimeout,URL,Blob,console});
 vm.runInContext(source+'\nglobalThis.lab={actions,state,cleanup};',context);
 const {actions,state,cleanup}=context.lab;state.ready=true;
 const pending=actions.startCapture();
 listener({state:'starting',sessionId:'capture-A'});
 resolveStart(makeSession('capture-A'));await pending;
 listener({state:'capturing',sessionId:'capture-A'});assert.equal(state.capture,true);
 failStop=true;await assert.rejects(actions.stopCapture());assert.equal(state.captureSession.sessionId,'capture-A');
 failStop=false;await actions.stopCapture();assert.equal(stops.at(-1),'capture-A');
 assert.equal(state.captureSession,null);assert.equal(state.capture,false);
 assert.match(nodes.get('capture-result').textContent,/2 帧.*5 B.*40 ms/);
 await actions.playCapturedAudio();assert.equal(state.playbackBusy,true);
 const late=actions.startCapture();cleanup();resolveStart(makeSession('capture-B'));await late;
 assert.equal(stops.at(-1),'capture-B');
});

test('network default uses an editable CORS probe without automatic fetching',async()=>{
 const html=await readFile(new URL('../../examples/permission-debug/index.html',import.meta.url),'utf8');
 assert.match(html,/id="request-url"[^>]*value="https:\/\/httpbin.org\/get"/);
 assert.match(html,/仅点击后请求/);
});
