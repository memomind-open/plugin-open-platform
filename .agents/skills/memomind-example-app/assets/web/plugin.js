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
  status.textContent = 'Saved. Reload to restore the count.';
}));

buttons[1].addEventListener('click', () => perform(async () => {
  await gm.display.createPage();
  await gm.display.updateText({
    id: 1, x: 20, y: 20, width: 300, height: 60,
    border: 1, radius: 8, text: `Count: ${count}`,
  });
  status.textContent = 'The host confirmed the text command. Check the virtual display or glasses.';
}));

async function start() {
  try {
    await gm.ready();
    await gm.runtime.getCapabilities();
    const saved = await gm.storage.get(storageKey);
    count = Number.isSafeInteger(saved.value) && saved.value >= 0 ? saved.value : 0;
    countOutput.textContent = String(count);
    status.textContent = 'Connected to the host. Ready to use.';
    setBusy(false);
  } catch (error) {
    status.textContent = `${error.code || 'ERROR'}: ${error.message || String(error)}. Check the host and permissions, then reload.`;
  }
}

window.addEventListener('pagehide', () => {
  closed = true;
  setBusy(true);
  gm.close();
});
void start();
