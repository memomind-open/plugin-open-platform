import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();
const countOutput = document.getElementById('count');
const status = document.getElementById('status');
const buttons = ['increment', 'sync'].map((id) => document.getElementById(id));
const storageKey = 'starter.count';
let count = 0;
let closed = false;

function setBusy(busy) {
  for (const button of buttons) button.disabled = busy || closed;
}

async function perform(action) {
  setBusy(true);
  try {
    await action();
  } catch (error) {
    status.textContent = `${error.code || 'ERROR'}: ${error.message || String(error)}`;
  } finally {
    setBusy(false);
  }
}

buttons[0].addEventListener('click', () => perform(async () => {
  const next = count + 1;
  await gm.storage.set(storageKey, next);
  count = next;
  countOutput.textContent = String(count);
  status.textContent = '已保存；重新加载可恢复计数。';
}));

buttons[1].addEventListener('click', () => perform(async () => {
  await gm.display.createPage();
  await gm.display.updateText({
    id: 1, x: 20, y: 20, width: 300, height: 60,
    border: 1, radius: 8, text: `Count: ${count}`,
  });
  status.textContent = '宿主已确认文字指令，请查看虚拟屏或眼镜。';
}));

async function start() {
  try {
    await gm.ready();
    await gm.runtime.getCapabilities();
    const saved = await gm.storage.get(storageKey);
    count = Number.isSafeInteger(saved.value) && saved.value >= 0 ? saved.value : 0;
    countOutput.textContent = String(count);
    status.textContent = '宿主已连接，可以开始操作。';
    setBusy(false);
  } catch (error) {
    status.textContent = `${error.code || 'ERROR'}: ${error.message || String(error)}；请检查宿主与权限后重新加载。`;
  }
}

window.addEventListener('pagehide', () => {
  closed = true;
  setBusy(true);
  gm.close();
});
void start();
