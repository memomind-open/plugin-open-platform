import test from 'node:test';
import assert from 'node:assert/strict';
import {StudioRuntime} from '../src/index.js';
const renderer={clear(){}};
const call=(r,method,params={})=>r.handle({version:'2.0',...r.bootstrap,requestId:'test',method,params});
test('dispatcher denies undeclared and refused calls without side effects',async()=>{
 const r=new StudioRuntime({renderer,permissions:[{name:'storage',required:false}]});
 assert.deepEqual((await call(r,'storage.set',{key:'a',value:1})).error,{code:'PERMISSION_DENIED',message:'NOT_GRANTED'});
 assert.equal(r.storage.size,0);
 assert.equal((await call(r,'location.getCurrentPosition')).error.message,'UNDECLARED');
 assert.equal((await call(r,'private.method')).error.code,'METHOD_NOT_FOUND');
});
test('location is simulated and visibility invalidates watches',async()=>{
 const r=new StudioRuntime({renderer,permissions:[{name:'location.foreground',required:false}],approvals:{'location.foreground':null}});
 const position=(await call(r,'location.getCurrentPosition')).result;
 assert.equal(position.coordinateSystem,'WGS84');assert.equal(position.latitude,31.2304);
 const watch=await call(r,'location.watchPosition');assert.ok(watch.result.watchId);
 assert.equal((await call(r,'location.watchPosition')).error.code,'BUSY');
 r.setLifecycle('suspended');assert.equal(r.location.watchId,null);
 assert.equal((await call(r,'location.getCurrentPosition')).error.message,'NOT_FOREGROUND');
});
test('late file-picker result cannot import after revoke',async()=>{
 let finish;
 const r=new StudioRuntime({renderer,permissions:[{name:'files.user-selected',required:false}],approvals:{'files.user-selected':null},filePicker:()=>new Promise(resolve=>{finish=resolve;})});
 const pending=call(r,'files.pick');
 r.setLifecycle('stopped');r.approvals={};finish([{name:'example.txt',bytes:new Uint8Array([1])}]);
 assert.equal((await pending).error.code,'PERMISSION_DENIED');assert.equal(r.fileStore.size,0);
});
test('message events require an approved channel',()=>{
 const r=new StudioRuntime({renderer});const events=[];r.onEvent(e=>events.push(e));
 r.emitPluginMessage(1000,new Uint8Array([1]));assert.equal(events.length,0);
});
