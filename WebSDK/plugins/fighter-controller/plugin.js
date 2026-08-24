import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';
import { FighterInputAudio } from './audio.js';
import { INPUT_CHANNEL, bytesToBase64, encodeInput } from './protocol.js';

const SEND_INTERVAL_MS = 50;

const gm = createGMPlugin();
const inputAudio = new FighterInputAudio();
const status = document.querySelector('#status');
const statusText = status.querySelector('span');
const telemetry = document.querySelector('#telemetry');
const pauseButton = document.querySelector('#pause');
const controls = [...document.querySelectorAll('[data-bit]')];
const joystick = document.querySelector('#joystick');
const joystickKnob = document.querySelector('#joystick-knob');
const pointers = new Map();
let buttons = 0;
let directionalButtons = 0;
let joystickPointer = null;
let paused = false;
let sequence = 0;
let connected = false;
let sending = false;
let pending = false;
let stopped = false;
const frameQueue = [];

async function requestLandscape() {
  try {
    await screen.orientation?.lock?.('landscape');
  } catch {
    // Mobile browsers commonly require the native WebView host to lock
    // orientation. The portrait overlay remains the deterministic fallback.
  }
}

void requestLandscape();
document.addEventListener('pointerdown', requestLandscape, { once: true });

function setStatus(text, state = '') {
  statusText.textContent = text;
  status.className = `status ${state}`.trim();
}

function recomputeButtons() {
  let nextButtons = directionalButtons;
  for (const bit of pointers.values()) nextButtons |= 1 << bit;
  const changed = nextButtons !== buttons;
  buttons = nextButtons;
  for (const control of controls) {
    control.classList.toggle('active', [...pointers.values()].includes(Number(control.dataset.bit)));
  }
  return changed;
}

function updateJoystick(event) {
  const rect = joystick.getBoundingClientRect();
  const centerX = rect.left + rect.width / 2;
  const centerY = rect.top + rect.height / 2;
  const limit = Math.min(rect.width, rect.height) * 0.32;
  const screenX = event.clientX - centerX;
  const screenY = event.clientY - centerY;
  const forcedLandscape = matchMedia('(orientation: portrait) and (pointer: coarse)').matches;
  // The portrait fallback rotates the complete controller 90 degrees
  // clockwise. Pointer coordinates stay in the unrotated viewport, so apply
  // the inverse rotation before interpreting joystick directions.
  let dx = forcedLandscape ? screenY : screenX;
  let dy = forcedLandscape ? -screenX : screenY;
  const distance = Math.hypot(dx, dy);
  if (distance > limit) {
    dx = dx / distance * limit;
    dy = dy / distance * limit;
  }
  joystickKnob.style.transform = `translate(${dx}px, ${dy}px)`;
  const normalizedX = dx / limit;
  const normalizedY = dy / limit;
  const deadzone = 0.28;
  directionalButtons = 0;
  if (normalizedX < -deadzone) directionalButtons |= 1 << 0;
  if (normalizedX > deadzone) directionalButtons |= 1 << 1;
  if (normalizedY < -deadzone) directionalButtons |= 1 << 2;
  if (normalizedY > deadzone) directionalButtons |= 1 << 3;
  const directions = [];
  if (directionalButtons & (1 << 0)) directions.push('left');
  if (directionalButtons & (1 << 1)) directions.push('right');
  if (directionalButtons & (1 << 2)) directions.push('up');
  if (directionalButtons & (1 << 3)) directions.push('down');
  joystick.setAttribute('aria-valuetext', directions.join(' ') || 'center');
  joystick.classList.toggle('active', directionalButtons !== 0);
  if (recomputeButtons()) queueCurrentFrame();
}

function resetJoystick() {
  joystickPointer = null;
  directionalButtons = 0;
  joystickKnob.style.transform = 'translate(0, 0)';
  joystick.setAttribute('aria-valuetext', 'center');
  joystick.classList.remove('active');
  if (recomputeButtons()) queueCurrentFrame();
}

joystick.addEventListener('pointerdown', (event) => {
  event.preventDefault();
  joystickPointer = event.pointerId;
  joystick.setPointerCapture(event.pointerId);
  updateJoystick(event);
});
joystick.addEventListener('pointermove', (event) => {
  if (event.pointerId === joystickPointer) updateJoystick(event);
});
for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) {
  joystick.addEventListener(name, (event) => {
    if (event.pointerId === joystickPointer) resetJoystick();
  });
}
joystick.addEventListener('contextmenu', (event) => event.preventDefault());

function releasePointer(pointerId) {
  pointers.delete(pointerId);
  if (recomputeButtons()) queueCurrentFrame();
}

function replayPressFeedback(control) {
  control.focus({ preventScroll: true });
  control.classList.remove('press-feedback');
  // Commit the previous animation state so every rapid tap starts a new pulse,
  // even when two taps occur between browser paint frames.
  void control.offsetWidth;
  control.classList.add('press-feedback');
}

for (const control of controls) {
  control.addEventListener('pointerdown', (event) => {
    event.preventDefault();
    replayPressFeedback(control);
    inputAudio.playInput(Number(control.dataset.bit));
    control.setPointerCapture(event.pointerId);
    pointers.set(event.pointerId, Number(control.dataset.bit));
    if (recomputeButtons()) queueCurrentFrame();
  });
  for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) {
    control.addEventListener(name, (event) => releasePointer(event.pointerId));
  }
  control.addEventListener('contextmenu', (event) => event.preventDefault());
  control.addEventListener('animationend', () => control.classList.remove('press-feedback'));
}

pauseButton.addEventListener('click', () => {
  inputAudio.playPause();
  paused = !paused;
  pointers.clear();
  resetJoystick();
  recomputeButtons();
  pauseButton.classList.toggle('active', paused);
  pauseButton.textContent = paused ? 'RESUME' : 'PAUSE';
  queueCurrentFrame();
});

function pauseForLifecycle() {
  pointers.clear();
  resetJoystick();
  paused = true;
  pauseButton.classList.add('active');
  pauseButton.textContent = 'RESUME';
  for (const control of controls) control.classList.remove('active');
  queueCurrentFrame();
}

window.addEventListener('blur', pauseForLifecycle);
window.addEventListener('pagehide', pauseForLifecycle);
document.addEventListener('visibilitychange', () => {
  if (document.hidden) pauseForLifecycle();
});

function inputFrame() {
  return encodeInput(sequence++, buttons, paused);
}

function queueCurrentFrame() {
  pending = true;
  if (!connected || stopped) return;
  // Capture the state now instead of reconstructing it after the asynchronous
  // Bridge request. This preserves short press/release edges between 20 Hz
  // keepalive ticks and makes rapid multi-touch button input deterministic.
  frameQueue.push(inputFrame());
  if (frameQueue.length > 32) frameQueue.splice(0, frameQueue.length - 32);
  void sendLatest();
}

async function sendLatest() {
  if (sending || stopped) {
    pending = true;
    return;
  }
  if (!connected) {
    frameQueue.length = 0;
    pending = true;
    return;
  }
  sending = true;
  pending = false;
  try {
    const frame = frameQueue.shift() ?? inputFrame();
    await gm.call('plugin.sendMessage', {
      channel: INPUT_CHANNEL,
      dataBase64: bytesToBase64(frame),
    });
    telemetry.textContent = `CH 0x4647 · 20 Hz · SEQ ${frame[1]}`;
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  } finally {
    sending = false;
    if ((frameQueue.length || pending) && !stopped) queueMicrotask(sendLatest);
  }
}

setInterval(() => {
  pending = true;
  void sendLatest();
}, SEND_INTERVAL_MS);

async function start() {
  try {
    await gm.ready();
    const info = await gm.device.getInfo();
    connected = Boolean(info.connected);
    setStatus(connected ? 'Glasses connected' : 'Glasses disconnected', connected ? 'ready' : 'error');
    const subscription = await gm.device.subscribeEvents(['connection']);
    gm.device.onConnection((event) => {
      connected = Boolean(event.connected);
      setStatus(connected ? 'Glasses connected' : 'Glasses disconnected', connected ? 'ready' : 'error');
      if (connected) queueCurrentFrame();
      else frameQueue.length = 0;
    });
    void subscription;
  } catch (error) {
    setStatus(error.message || String(error), 'error');
  }
}

window.addEventListener('pagehide', () => {
  stopped = true;
  gm.close();
});

void start();
