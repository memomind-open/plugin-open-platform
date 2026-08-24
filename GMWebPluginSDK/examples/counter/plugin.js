import { createGMPlugin } from '@memomind/gm-plugin-web-sdk';

const gm = createGMPlugin();
const counter = document.querySelector('#counter');
const status = document.querySelector('#status');
const events = document.querySelector('#events');
const draw = document.querySelector('#draw');
const save = document.querySelector('#save');
let value = 0;

const report = (message) => {
  events.textContent = `${new Date().toLocaleTimeString()} ${message}\n${events.textContent}`.slice(0, 1400);
};

const setValue = (next) => {
  value = next;
  counter.textContent = String(value);
};

try {
  await gm.ready();
  const saved = await gm.storage.get('counter');
  setValue(Number(saved.value ?? 0));
  gm.device.onButton((event) => {
    if (event.action === 'single') setValue(value + 1);
    if (event.action === 'double') setValue(0);
    report(`button.${event.action}`);
  });
  gm.device.onGesture((event) => {
    if (!event.active) return;
    if (event.gesture === 'headRaise' || event.gesture === 'right') setValue(value + 1);
    if (event.gesture === 'headLower' || event.gesture === 'left') setValue(Math.max(0, value - 1));
    report(`gesture.${event.gesture}`);
  });
  gm.device.onConnection((event) => {
    status.textContent = event.connected ? 'Studio 已连接 · 可绘制设备画面' : '设备已断开';
    report(`connection.${event.connected}`);
  });
  await gm.device.subscribeEvents(['button', 'imuGesture', 'connection']);
  status.textContent = 'Bridge v1 ready';
  draw.disabled = false;
  save.disabled = false;
} catch (error) {
  status.textContent = `${error.code ?? 'ERROR'}: ${error.message}`;
}

draw.addEventListener('click', async () => {
  try {
    await gm.display.updateText({ id: 1, x: 40, y: 80, width: 520, height: 120, border: 2, radius: 12, text: `计数器 ${value}` });
    report('display.updateText presented');
  } catch (error) {
    report(`${error.code ?? 'ERROR'}: ${error.message}`);
  }
});

save.addEventListener('click', async () => {
  await gm.storage.set('counter', value);
  report('storage.set counter');
});
