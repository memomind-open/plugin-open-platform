import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';
import { ALIGNMENTS } from '../../examples/image-viewer/image.js';

const source = (await readFile(new URL('../../examples/image-viewer/plugin.js', import.meta.url), 'utf8')).replace(/^import .*;\r?\n/gm, '');
const slotKey = 'pixel-image-viewer.slots.v1', alignmentKey = 'pixel-image-viewer.alignments.v1';
function boot(storage) {
  const elements = new Map(), buttons = [], sent = [];
  let failSave = false, failRead = false;
  function element() {
    return { hidden:false, value:'', classList:{toggle(){},contains(){return false;}}, handlers:{},
      addEventListener(name,fn){this.handlers[name]=fn;}, setAttribute(){},
      querySelector: get, querySelectorAll:()=>buttons,
      insertBefore(button){buttons.push(button);},
      getContext:()=>({fillRect(){},drawImage(){}}) };
  }
  function get(selector) { if(!elements.has(selector))elements.set(selector,element()); return elements.get(selector); }
  const gm = { ready:async()=>{}, on:()=>()=>{},
    device:{onConnection:()=>()=>{},subscribeEvents:async()=>({subscriptionId:'s'}),getInfo:async()=>({connected:true})},
    runtime:{getLifecycleState:async()=>({state:'running'})},
    storage:{get:async key=>({value:storage.get(key)}),set:async(key,value)=>{if(failSave)throw Error('save failed');storage.set(key,value);}},
    files:{list:async()=>({files:[{fileId:'a',name:'a.png'}]})}
  };
  const context = vm.createContext({ document:{querySelector:get,createElement:element,documentElement:{style:{setProperty(){}}}},
    window:{addEventListener(){}}, AbortController,setTimeout,clearTimeout,
    WIDTH:600,HEIGHT:350,ALIGNMENTS,i18n:{apply(){},number:String},t:key=>key,localizedError:message=>Error(message),
    createGMPlugin:()=>gm,readImageBlob:async()=>{if(failRead)throw Error('read failed');return {};},
    decodeImage:async()=>({image:{},width:100,height:50,close(){}}),
    composeImage:(image,w,h,canvas,alignment)=>Uint8Array.of(ALIGNMENTS.indexOf(alignment)),
    compressLz4:bytes=>bytes.slice(),throwIfAborted:signal=>signal.throwIfAborted(),
    ImageTransfer:class {async send(bytes,progress,signal,compressed){assert.deepEqual(bytes,compressed);sent.push(bytes[0]);return {status:0,elapsedMs:1};}}
  });
  vm.runInContext(source,context);
  async function idle(){for(let n=0;n<100;n++){await new Promise(resolve=>setImmediate(resolve));if(vm.runInContext('ready && !initializing && !operation',context))return;}throw Error('plugin did not become idle');}
  return {get,buttons,sent,idle,setFailSave:value=>{failSave=value;},setFailRead:value=>{failRead=value;},
    async change(value){get('#alignment').value=value;get('#alignment').handlers.change();await idle();},
    async send(){get('.send').handlers.click();await idle();}};
}

test('alignment UI rebuilds send cache, persists per slot, restores and rolls back failures',async()=>{
  const storage=new Map([[slotKey,['a','a']]]);
  let app=boot(storage);await app.idle();
  assert.equal(app.get('#alignment').value,'center');
  await app.change('bottom-right');await app.send();assert.equal(app.sent.at(-1),8);
  app.buttons[1].handlers.click();assert.equal(app.get('#alignment').value,'center');
  await app.change('top-left');await app.send();assert.equal(app.sent.at(-1),0);
  app.buttons[0].handlers.click();assert.equal(app.get('#alignment').value,'bottom-right');
  app.setFailSave(true);await app.change('middle-left');
  assert.equal(app.get('#alignment').value,'bottom-right');await app.send();assert.equal(app.sent.at(-1),8);
  app.setFailSave(false);app.setFailRead(true);await app.change('top-center');
  assert.equal(app.get('#alignment').value,'bottom-right');await app.send();assert.equal(app.sent.at(-1),8);
  app=boot(storage);await app.idle();assert.equal(app.get('#alignment').value,'bottom-right');
  await app.send();assert.equal(app.sent.at(-1),8);
  app.buttons[1].handlers.click();assert.equal(app.get('#alignment').value,'top-left');
  app.buttons[2].handlers.click();await app.change('bottom-left');
  app=boot(storage);await app.idle();app.buttons[2].handlers.click();assert.equal(app.get('#alignment').value,'bottom-left');
});
