import {requestDebugApprovals} from '/contract/debug-consent.js';
import {validateManifestPolicy} from '/contract/permission-policy.js';
import { CanvasDeviceRenderer } from '@memomind/gm-plugin-device-renderer';
import { StudioRuntime } from '@memomind/gm-plugin-studio-runtime';

const frame = document.querySelector('#plugin-frame');
const renderer = new CanvasDeviceRenderer(document.querySelector('#device-canvas'));
let runtime = new StudioRuntime({ renderer, filePicker: pickLocalFiles });
let launchGeneration = 0;
const logs = document.querySelector('#logs');
const transportStatus = document.querySelector('#transport-status');

const log = (kind, value) => {
  const item = document.createElement('li');
  item.textContent = `${new Date().toLocaleTimeString()}  ${kind}  ${typeof value === 'string' ? value : JSON.stringify(value)}`;
  logs.prepend(item);
  while (logs.children.length > 100) logs.lastElementChild.remove();
};

const postBootstrap = () => runtime.lifecycleState === 'running' && frame.contentWindow?.postMessage({ type: 'gm-plugin:bootstrap', bootstrap: runtime.bootstrap }, '*');

window.addEventListener('message', async (message) => {
  if (message.source !== frame.contentWindow) return;
  if (message.data?.type === 'gm-plugin:bootstrap-request') {
    postBootstrap();
    return;
  }
  if (message.data?.type !== 'gm-plugin:request') return;
  const request = message.data.request;
  log('REQUEST', { method: request?.method, params: summarizeParams(request?.params) });
  const current = runtime;
  const response = await current.handle(request);
  if (current !== runtime || ['stopped','failed'].includes(current.lifecycleState)) return;
  log(response.ok ? 'RESPONSE' : 'ERROR', response.ok ? response.result : response.error);
  updateTransportStatus(response);
  const transfer = response.ok && response.result?.streamPort?.postMessage
    ? [response.result.streamPort]
    : [];
  frame.contentWindow?.postMessage({ type: 'gm-plugin:response', response }, '*', transfer);
});

function subscribeRuntime() { runtime.onEvent((event) => {
  log('EVENT', { name: event.name, data: event.data });
  frame.contentWindow?.postMessage({ type: 'gm-plugin:event', event }, '*');
});

}
frame.addEventListener('load', postBootstrap);
// The iframe can finish loading before this module installs its load listener.
// Sending once immediately makes bootstrap delivery independent of load order.
postBootstrap();
async function launch() {
  const generation = ++launchGeneration;
  runtime.setLifecycle('stopped');
  frame.src = 'about:blank';
  const manifest = await (await fetch('/plugin/manifest.json')).json();
  const permissions = validateManifestPolicy(manifest);
  const approvals = await requestDebugApprovals(permissions);
  if (approvals === null || generation !== launchGeneration) return;
  // Reloading this fixed workspace keeps its data, but never reuses its grants.
  runtime = new StudioRuntime({renderer,filePicker:pickLocalFiles,permissions,approvals,runtimeGeneration:generation,
    storage:runtime.storage,fileStore:runtime.fileStore});
  subscribeRuntime();
  frame.src = '/plugin/' + manifest.entry;
}
document.querySelector('#reload').addEventListener('click',()=>void launch().catch(e=>log('ERROR',e.message)));
const revoke=document.createElement('button'); revoke.textContent='撤销授权并停止';
revoke.onclick=()=>{++launchGeneration;runtime.setLifecycle('stopped');runtime.approvals={};frame.src='about:blank';log('AUTH','已撤销，下次启动重新授权');};
document.querySelector('#reload').after(revoke);
void launch().catch(e=>log('ERROR',e.message));
document.querySelector('#clear').addEventListener('click', () => renderer.clear());
document.querySelector('#clear-log').addEventListener('click', () => logs.replaceChildren());

for (const button of document.querySelectorAll('[data-gesture]')) {
  button.addEventListener('click', () => runtime.emitGesture(button.dataset.gesture));
}
for (const button of document.querySelectorAll('[data-button]')) {
  button.addEventListener('click', () => runtime.emitButton(button.dataset.button));
}

document.querySelector('#lifecycle').addEventListener('change', (event) => runtime.setLifecycle(event.target.value));
document.querySelector('#connected').addEventListener('change', (event) => {
  runtime.setConnected(event.target.checked);
  document.querySelector('#connection-dot').style.background = event.target.checked ? '#5bf0ba' : '#ff657a';
  document.querySelector('#connection-label').textContent = event.target.checked ? 'Device connected' : 'Device disconnected';
});

window.addEventListener('keydown', (event) => {
  const mapping = { ArrowUp: 'headRaise', ArrowDown: 'headLower', ArrowLeft: 'left', ArrowRight: 'right' };
  const gesture = mapping[event.key];
  if (!gesture) return;
  event.preventDefault();
  runtime.emitGesture(gesture);
});

function summarizeParams(params) {
  if (!params || typeof params !== 'object') return params;
  const summary = { ...params };
  if (typeof summary.dataBase64 === 'string') {
    summary.dataBase64 = summarizePluginMessage(summary.channel, summary.dataBase64)
      ?? `<base64 ${summary.dataBase64.length} chars>`;
  }
  if (Array.isArray(summary.operations)) summary.operations = `<${summary.operations.length} operations>`;
  return summary;
}

function summarizePluginMessage(channel, encoded) {
  if (channel !== 0x4647) return null;
  try {
    const bytes = Uint8Array.from(atob(encoded), (character) => character.charCodeAt(0));
    if (bytes.length !== 4 || bytes[0] !== 2) return null;
    const mask = (bytes[2] << 8) | bytes[3];
    const names = ['LEFT', 'RIGHT', 'UP', 'DOWN', 'LIGHT', 'HEAVY', 'KICK',
      'BLOCK', 'START', 'PAUSE', 'SKILL_1', 'SKILL_2'];
    const pressed = names.filter((_, bit) => mask & (1 << bit));
    return `<fighter-v2 seq=${bytes[1]} buttons=${pressed.join('|') || 'NONE'}>`;
  } catch {
    return null;
  }
}

function updateTransportStatus(response) {
  const transfer = response.ok
    ? response.result?.transport ?? response.result?.transports?.at?.(-1)
    : null;
  transportStatus.classList.toggle('error', response.error?.code === 'PAYLOAD_TOO_LARGE');
  if (transfer) {
    const decoded = transfer.codec === 'gray4-lz4' ? ` · decoded ${formatBytes(transfer.decodedBytes)}` : '';
    transportStatus.textContent = `CH${transfer.channel} · ${formatBytes(transfer.payloadBytes)} / ${formatBytes(transfer.maxPayloadBytes)}${decoded}`;
  } else if (response.error?.code === 'PAYLOAD_TOO_LARGE') {
    transportStatus.textContent = `Packet limit exceeded · Split into tiles${response.error.message.includes('LZ4') ? ' and compress each tile' : ' or consider LZ4'}`;
  }
}

function formatBytes(value) {
  return Number.isInteger(value) ? `${value.toLocaleString()} B` : '-';
}

function pickLocalFiles({ extensions, allowMultiple }) {
  const input = document.createElement('input');
  input.type = 'file';
  input.multiple = allowMultiple;
  input.accept = extensions.map((extension) => `.${extension}`).join(',');
  input.hidden = true;
  document.body.append(input);
  return new Promise((resolve, reject) => {
    let settled = false;
    const finish = (value, error) => {
      if (settled) return;
      settled = true;
      input.remove();
      if (error) reject(error);
      else resolve(value);
    };
    input.addEventListener('cancel', () => finish([]), { once: true });
    input.addEventListener('change', async () => {
      try {
        const selected = await Promise.all([...input.files].map(async (file) => ({
          name: file.name,
          bytes: new Uint8Array(await file.arrayBuffer()),
        })));
        finish(selected);
      } catch (error) {
        finish(undefined, error);
      }
    }, { once: true });
    input.click();
  });
}

document.addEventListener('visibilitychange',()=>{ if(document.hidden) runtime.setLifecycle('suspended'); else if(runtime.lifecycleState==='suspended') runtime.setLifecycle('running'); });
