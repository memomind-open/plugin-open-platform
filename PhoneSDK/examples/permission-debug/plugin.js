import {createGMPlugin,opusRecordingToOgg} from './vendor/gm-plugin-web-sdk.esm.js';
import {readBounded,makeToneWav,gray4Pattern,hexBytes,validateHttpUrl,PREVIEW_LIMIT} from './lab-core.js';

const gm=createGMPlugin({timeoutMs:20000}), $=id=>document.getElementById(id);
const player=$('player'), pattern=gray4Pattern();
const state={ready:false,capabilities:{},subscription:null,watch:null,request:null,previewAbort:null,timer:null,
  capture:false,captureSession:null,captureStarting:false,playbackBusy:false,presses:0,points:[],urls:new Set(),recording:null,revision:0};
const noteKey='permission-lab.note.v1';
const base64=bytes=>{let s='';for(const b of bytes)s+=String.fromCharCode(b);return btoa(s);};
function log(label,value){
 $('log').textContent=(new Date().toLocaleTimeString()+' '+label+' '+JSON.stringify(value)+'\n'+$('log').textContent).slice(0,12000);
}
function result(id,text,error=false){$(id).textContent=text;$(id).dataset.state=error?'error':'ok';}
function describe(error){return (error.code??error.name??'ERROR')+' · '+error.message;}
function reportFailure(name,error){
 const detail={code:error?.code??error?.name??'ERROR',message:error?.message??String(error)};
 log(name,detail);console.error('[PermissionLab]',name,detail);
}
function fields(id,values){
 const list=$(id);list.replaceChildren();
 for(const [key,value]of Object.entries(values)){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=key;dd.textContent=typeof value==='object'?JSON.stringify(value):String(value??'—');list.append(dt,dd);}
}
function objectUrl(blob){const url=URL.createObjectURL(blob);state.urls.add(url);return url;}
function releaseUrls(){for(const url of state.urls)URL.revokeObjectURL(url);state.urls.clear();}
function captureSummary(data){
 const r=data?.result??data;
 if(!r||!Number.isFinite(r.frameCount))return;
 result('capture-result',`收到 ${r.frameCount} 帧 · ${r.opusBytes??0} B Opus · 采集 ${r.durationMs??0} ms。收到数据不等于有效人声，请回放确认。`,r.frameCount===0);
 log('capture.summary',{frameCount:r.frameCount,opusBytes:r.opusBytes,durationMs:r.durationMs});
}
function updateAudioButtons(){
 for(const button of document.querySelectorAll('[data-action]')){
  if(['playCapturedAudio','playTone','delayTone'].includes(button.dataset.action))button.disabled=state.playbackBusy;
 }
}
const outputs={saveNote:'storage-result',loadNote:'storage-result',deleteNote:'storage-result',
 pickFiles:'files-result',listFiles:'files-result',showText:'display-result',showPattern:'display-result',closePage:'display-result',
 deviceInfo:'device-result',subscribe:'events-result',unsubscribe:'events-result',
 startCapture:'capture-result',stopCapture:'capture-result',playTone:'playback-result',delayTone:'playback-result',playCapturedAudio:'playback-result',stopAudio:'playback-result',
 request:'network-result',cancelRequest:'network-result',locate:'location-result',watch:'location-result',clearWatch:'location-result',
 badEvent:'negative-result',badMethod:'negative-result'};
async function run(name,button){
 const output=outputs[name];button.disabled=true;result(output,'正在执行…');
 try{
  if(!state.ready)throw new Error('宿主尚未就绪，请先完成启动授权');
  await actions[name]();
 }catch(e){result(output,describe(e),true);reportFailure(name,e);}
 finally{button.disabled=false;updateAudioButtons();}
}
for(const button of document.querySelectorAll('[data-action]'))button.onclick=()=>run(button.dataset.action,button);

async function refreshFiles(){
 const [{files},usage]=await Promise.all([gm.files.list(),gm.files.getUsage()]);
 $('file-list').replaceChildren();
 for(const file of files){
  const row=document.createElement('article'),title=document.createElement('strong'),open=document.createElement('button'),remove=document.createElement('button');
  title.textContent=file.name+' · '+file.size.toLocaleString()+' B';
  open.textContent='读取预览';open.onclick=async()=>{open.disabled=true;try{await previewFile(file);}catch(e){result('files-result',describe(e),true);}finally{open.disabled=false;}};
  remove.textContent='删除此文件';remove.onclick=async()=>{
   if(!confirm('从此插件的文件空间删除“'+file.name+'”？不会删除系统原文件。'))return;
   remove.disabled=true;
   try{await gm.files.delete(file.fileId);$('file-preview').replaceChildren();await refreshFiles();}
   catch(e){result('files-result',describe(e),true);}finally{remove.disabled=false;}
  };
  row.append(title,open,remove);$('file-list').append(row);
 }
 result('files-result',files.length+' 个文件 / 已使用 '+usage.totalBytes.toLocaleString()+' B');
}
async function previewFile(file){
 state.previewAbort?.abort();const controller=new AbortController();state.previewAbort=controller;
 $('file-preview').replaceChildren();
 if(file.size===0){result('files-result','空文件，无内容可预览');return;}
 const extension=file.name.split('.').pop().toLowerCase();
 const media={png:'image/png',jpg:'image/jpeg',jpeg:'image/jpeg',gif:'image/gif',wav:'audio/wav',mp3:'audio/mpeg',ogg:'audio/ogg'};
 if(media[extension]&&file.size>PREVIEW_LIMIT)throw new Error('媒体文件超过 1 MiB 预览上限，请选择较小的测试文件');
 const opened=await gm.files.openRead(file.fileId,{offset:0,length:Math.min(file.size,PREVIEW_LIMIT),signal:controller.signal});
 const bytes=await readBounded(opened.stream);
 if(controller.signal.aborted)return;
 if(media[extension]){
  const url=objectUrl(new Blob([bytes],{type:media[extension]}));
  const element=document.createElement(media[extension].startsWith('image/')?'img':'audio');
  if(element.tagName==='AUDIO')element.controls=true;else element.alt=file.name;
  element.src=url;$('file-preview').append(element);
 }else{
  const preview=document.createElement('pre');preview.textContent=new TextDecoder().decode(bytes);$('file-preview').append(preview);
 }
 result('files-result','已从宿主文件流读取 '+bytes.length+' B'+(file.size>bytes.length?'（仅预览文件开头）':''));
}
function drawPattern(){
 const ctx=$('pattern').getContext('2d'),image=ctx.createImageData(pattern.width,pattern.height);
 for(let i=0;i<pattern.width*pattern.height;i++){const nibble=(pattern.bytes[i>>1]>>(i%2===0?4:0))&15;image.data.set([nibble*17,nibble*17,nibble*17,255],i*4);}
 ctx.putImageData(image,0,0);
}
function updatePosition(p){
 if(!Number.isFinite(p.latitude)||!Number.isFinite(p.longitude))throw new Error('宿主返回的位置无效');
 state.points.push(p);if(state.points.length>120)state.points.shift();
 fields('position',{'纬度':p.latitude.toFixed(6),'经度':p.longitude.toFixed(6),'精度':p.accuracy+' m','时间':new Date(p.timestamp).toLocaleTimeString(),'坐标系':p.coordinateSystem,'样本':state.points.length});
 const ctx=$('track').getContext('2d'),w=400,h=180;
 ctx.clearRect(0,0,w,h);ctx.strokeStyle='#d1ded5';ctx.lineWidth=1;
 for(let x=20;x<w;x+=40){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke();}
 for(let y=20;y<h;y+=40){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();}
 const lats=state.points.map(p=>p.latitude),lons=state.points.map(p=>p.longitude);
 const minLat=Math.min(...lats),maxLat=Math.max(...lats),minLon=Math.min(...lons),maxLon=Math.max(...lons);
 const point=p=>[maxLon===minLon?w/2:20+(p.longitude-minLon)/(maxLon-minLon)*(w-40),maxLat===minLat?h/2:h-20-(p.latitude-minLat)/(maxLat-minLat)*(h-40)];
 ctx.strokeStyle='#1b6754';ctx.lineWidth=3;ctx.beginPath();
 state.points.forEach((p,i)=>{const [x,y]=point(p);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});ctx.stroke();
 const [x,y]=point(p);ctx.fillStyle='#16a47a';ctx.beginPath();ctx.arc(x,y,6,0,Math.PI*2);ctx.fill();
 result('location-result',(state.capabilities.location?.simulated?'已收到模拟位置':'已收到宿主位置')+'；轨迹仅保存在本页');
}
async function playTone(){
 clearTimeout(state.timer);player.pause();player.src=objectUrl(new Blob([makeToneWav()],{type:'audio/wav'}));
 await player.play();
 result('playback-result','H5 测试音正在播放。若启动时拒绝 audio.playback，此结果表示当前宿主未阻止播放，不算权限验收通过。');
}
const actions={
 async saveNote(){await gm.storage.set(noteKey,{text:$('note').value,savedAt:Date.now()});result('storage-result','便签已保存到插件 Bridge 存储');},
 async loadNote(){const {value}=await gm.storage.get(noteKey);$('note').value=value?.text??'';result('storage-result',value?'已读取保存的便签 · '+new Date(value.savedAt).toLocaleString():'尚无保存的便签');},
 async deleteNote(){await gm.storage.remove(noteKey);$('note').value='';result('storage-result','仅已删除实验室便签，未清空插件其他存储');},
 async pickFiles(){await gm.files.pick({extensions:['txt','json','md','png','jpg','jpeg','gif','wav','mp3','ogg'],allowMultiple:true});await refreshFiles();},
 listFiles:refreshFiles,
 async showText(){await gm.display.createPage();await gm.display.updateText({id:1,x:20,y:20,width:560,height:100,border:1,radius:8,text:$('display-text').value});result('display-result','宿主已确认文字指令，请查看眼镜或虚拟显示屏');},
 async showPattern(){await gm.display.createPage();await gm.display.updateImage({x:20,y:140,width:pattern.width,height:pattern.height,stride:pattern.stride,dataBase64:base64(pattern.bytes)});result('display-result','宿主已确认 GRAY_4 棋盘图，请查看眼镜或虚拟显示屏');},
 async closePage(){await gm.display.closePage();result('display-result','已关闭眼镜显示页');},
 async deviceInfo(){const info=await gm.device.getInfo();fields('device-info',info);result('device-result','已读取宿主设备信息');},
 async subscribe(){if(state.subscription)throw new Error('正在监听，请先停止');const revision=state.revision;const r=await gm.device.subscribeEvents(['button','imuGesture','connection']);if(revision!==state.revision){await gm.device.unsubscribeEvents(r.subscriptionId);return;}state.subscription=r.subscriptionId;result('events-result','已订阅，等待实际按键和头部动作');},
 async unsubscribe(){if(state.subscription){await gm.device.unsubscribeEvents(state.subscription);state.subscription=null;}result('events-result','监听已停止');},
 async startCapture(){
  if(state.capture||state.captureSession||state.captureStarting)throw new Error('录音已经开始或正在启动');
  const revision=state.revision;state.captureStarting=true;
  try{
   const r=await gm.audio.openCapture({mode:'recording',noiseReduction:true,pickupMode:'frontBalanced',maxDurationMs:15000});
   if(revision!==state.revision){await gm.audio.stopCapture(r.sessionId);return;}
   state.recording=null;state.captureSession=r;
   // captureState can arrive before this response; never overwrite its terminal state.
   result('capture-result','原生采集调用成功；状态以下方事件为准');
  }finally{state.captureStarting=false;}
 },
 async stopCapture(){
  if(state.captureStarting)throw new Error('采集正在启动，请稍后停止');
  const capture=state.captureSession;if(!capture)throw new Error('尚未取得录音会话，请先开始录音');
  const r=await capture.stop();finishCapture(capture,r);
  result('capture-result','停止采集成功');captureSummary(r);
 },
 playTone,
 async delayTone(){clearTimeout(state.timer);state.timer=setTimeout(()=>{if(!document.hidden)void playTone().catch(e=>result('playback-result',describe(e),true));},3000);result('playback-result','3 秒后直接调用 audio.play()；不自动解锁，不保证浏览器会允许');},
 async playCapturedAudio(){
  if(state.playbackBusy)throw new Error('正在回放，请等待结束或点击停止全部播放');
  if(!state.recording)throw new Error('请先完成一次眼镜录音');
  clearTimeout(state.timer);player.pause();state.playbackBusy=true;updateAudioButtons();
  result('playback-result','H5 正在播放收到的 Opus 录音');
  try{player.muted=false;player.volume=1;player.src=objectUrl(opusRecordingToOgg(state.recording));await player.play();}
  catch(e){state.playbackBusy=false;updateAudioButtons();throw e;}
 },
 async stopAudio(){clearTimeout(state.timer);player.pause();for(const audio of $('file-preview').querySelectorAll('audio'))audio.pause();state.playbackBusy=false;updateAudioButtons();result('playback-result','H5 播放已暂停');},
 async request(){
  if(state.request)throw new Error('已有请求，请先取消');const url=validateHttpUrl($('request-url').value.trim()),controller=new AbortController();
  state.request=controller;const timer=setTimeout(()=>controller.abort(),15000),started=performance.now();
  try{
   const response=await fetch(url,{signal:controller.signal,credentials:'omit',cache:'no-store',referrerPolicy:'no-referrer',redirect:'error'});
   const bytes=response.body?await readBounded(response.body,65536):new Uint8Array();
   $('response-body').textContent=new TextDecoder().decode(bytes);
   result('network-result','HTTP '+response.status+' · '+Math.round(performance.now()-started)+' ms · '+bytes.length+' B。实际 H5 请求已完成，不等于 network 权限隔离通过。',!response.ok);
  }catch(e){result('network-result',describe(e)+'。失败可能来自 CORS、网络或宿主策略，不能仅凭失败断言权限拦截成功。',true);}
  finally{clearTimeout(timer);if(state.request===controller)state.request=null;}
 },
 async cancelRequest(){state.request?.abort();result('network-result','已取消当前请求');},
 async locate(){const revision=state.revision;const p=await gm.location.getCurrentPosition({timeoutMs:15000});if(revision===state.revision)updatePosition(p);},
 async watch(){if(state.watch)throw new Error('正在跟踪，请先停止');const revision=state.revision;const {watchId}=await gm.location.watchPosition({timeoutMs:15000});if(revision!==state.revision){await gm.location.clearWatch(watchId);return;}state.watch=watchId;result('location-result','监听已建立，等待位置事件');},
 async clearWatch(){if(state.watch)await gm.location.clearWatch(state.watch);state.watch=null;result('location-result','已停止跟踪');},
 async badEvent(){await gm.device.subscribeEvents(['rawImu']);result('negative-result','异常：未声明范围 rawImu 被放行',true);},
 async badMethod(){await gm.call('network.request');result('negative-result','异常：未知方法被放行',true);},
};
function finishCapture(capture,recording){
 state.recording=recording;
 if(state.captureSession===capture){state.captureSession=null;state.capture=false;$('capture-state').textContent='采集已停止';}
}
gm.device.onButton(data=>{if(!state.subscription)return;state.presses++;$('press-count').textContent=state.presses;result('events-result','按键：'+(data.action??data.button??'button'));});
gm.device.onGesture(data=>{
 if(!state.subscription)return;
 const shift={left:[-30,0],right:[30,0],headRaise:[0,-22],headLower:[0,22],nod:[0,15],shake:[20,0]}[data.gesture]??[0,0];
 $('motion-dot').style.transform='translate('+shift[0]+'px,'+shift[1]+'px)';result('events-result','头部动作：'+data.gesture);
});
gm.device.onConnection(data=>{if(state.subscription)result('events-result',data.connected?'设备已连接':'设备已断开');});
gm.audio.onCaptureState(data=>{
 if(state.captureSession&&state.captureSession.sessionId!==data.sessionId&&!state.captureStarting)return;
 state.capture=['starting','capturing','recording','stopping'].includes(data.state);
 $('capture-state').textContent='眼镜状态：'+data.state;log('audio.captureState',data);
 captureSummary(data);
 if(data.state==='stopped'&&state.captureSession&&!state.recording){
  const capture=state.captureSession;
  void capture.stop().then(r=>{finishCapture(capture,r);captureSummary(r);}).catch(e=>{result('capture-result',describe(e),true);reportFailure('audio.captureState.stop',e);});
 }
 if(data.state==='error')result('capture-result',(data.errorCode??'ERROR')+' · '+(data.message??'采集失败'),true);
});
gm.location.onPosition(data=>{if(data.watchId===state.watch)updatePosition(data);});
gm.location.onError(data=>{if(data.watchId===state.watch)result('location-result',data.error.code+' · '+data.error.message,true);});
player.addEventListener('ended',()=>{state.playbackBusy=false;updateAudioButtons();result('playback-result','H5 音频已播放完成');});
player.addEventListener('playing',()=>log('audio.playing',{duration:player.duration,currentTime:player.currentTime,volume:player.volume,muted:player.muted,readyState:player.readyState}));
player.addEventListener('error',()=>{const e=player.error??new Error('HTML audio playback failed');state.playbackBusy=false;updateAudioButtons();result('playback-result',describe(e),true);reportFailure('audio.player.error',e);});
player.addEventListener('stalled',()=>log('audio.player.stalled',{currentTime:player.currentTime,readyState:player.readyState}));
function cleanup(){
 ++state.revision;clearTimeout(state.timer);state.request?.abort();state.previewAbort?.abort();player.pause();
 for(const audio of $('file-preview').querySelectorAll('audio'))audio.pause();
 if(!state.ready)return;
 if(state.watch)void gm.location.clearWatch(state.watch).catch(()=>{});
 if(state.subscription)void gm.device.unsubscribeEvents(state.subscription).catch(()=>{});
 if(state.capture&&state.captureSession)void state.captureSession.stop().catch(()=>{});
 state.watch=null;state.subscription=null;state.capture=false;releaseUrls();
}
document.addEventListener('visibilitychange',()=>{if(document.hidden)cleanup();});
window.addEventListener('pagehide',cleanup);
gm.on('runtime.lifecycleChanged',data=>{if(data.state!=='running')cleanup();});
drawPattern();
gm.ready().then(async()=>{
 const version=await gm.runtime.getBridgeVersion();state.capabilities=await gm.runtime.getCapabilities();state.ready=true;
 $('status').textContent='已连接 Bridge '+version.version+' · 共 9 个权限功能区';
 if(state.capabilities.location?.simulated){$('location-source').textContent='模拟位置：当前 Studio 返回固定坐标，不是设备真实 GPS。';$('location-source').classList.add('simulated');}
 else $('location-source').textContent=state.capabilities.location?'宿主位置：显示原生接口返回值，不使用插件伪造坐标。':'当前宿主未开放定位。仍可点击检查实际拒绝原因。';
 for(const section of document.querySelectorAll('[data-permission]')){
   const p=section.dataset.permission;
   if(p==='audio.capture'&&!state.capabilities.audio)result('capture-result','当前宿主未开放眼镜采集；点击后将显示真实错误，不使用电脑麦克风替代',true);
 }
 log('capabilities',state.capabilities);
}).catch(e=>{$('status').textContent='连接失败：'+describe(e);});
