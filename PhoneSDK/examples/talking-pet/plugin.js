import { createGMPlugin, opusRecordingToOgg } from './vendor/gm-plugin-web-sdk.esm.js';
import { encodePetState, PET_STATE_CHANNEL } from './device-protocol.js';
import {
  createOpeningCaptureTerminalTracker,
  openedCaptureDisposition,
} from './recording-lifecycle.js';

const gm = createGMPlugin();
const pet = document.querySelector('#pet');
const petImage = document.querySelector('#pet-image');
const room = document.querySelector('#room');
const speech = document.querySelector('#speech');
const status = document.querySelector('#status');
const connectionDot = document.querySelector('#connection-dot');
const talkButton = document.querySelector('#talk');
const syncButton = document.querySelector('#sync');
const particles = document.querySelector('#particles');
const deviceCanvas = document.createElement('canvas');
const deviceContext = deviceCanvas.getContext('2d', { willReadFrequently: true });
const DEVICE_WIDTH = 600;
const DEVICE_HEIGHT = 350;
const petVisuals = {
  idle: './assets/memo-idle-v2.png',
  blink: './assets/memo-blink-v2.webp',
  eating: './assets/memo-eating-v2.png',
  playing: './assets/memo-playing-v2.png',
  sleeping: './assets/memo-sleeping-v2.png',
  listening: './assets/memo-listening-v2.png',
  talking: './assets/memo-talking-v2.png',
};
function preloadActionVisuals() {
  Object.entries(petVisuals).forEach(([state, source]) => {
    if (state === 'idle' || state === 'blink') return;
    const image = new Image();
    image.decoding = 'async';
    image.src = source;
  });
}

window.addEventListener('load', () => {
  if (typeof requestIdleCallback === 'function') {
    requestIdleCallback(preloadActionVisuals, { timeout: 3000 });
  } else {
    setTimeout(preloadActionVisuals, 1000);
  }
}, { once: true });
deviceCanvas.width = DEVICE_WIDTH;
deviceCanvas.height = DEVICE_HEIGHT;

const state = {
  happy: 78,
  food: 62,
  energy: 84,
  xp: 0,
};

const lines = {
  pet: ['Hehe, that tickles!', 'One more pat, please!', 'Memo likes you best!', 'Purr, purr…'],
  feed: ['That cookie smells amazing!', 'Nom! Can I have another?', 'Thanks for the treat!'],
  play: ['Caught it!', 'One more round!', 'Watch my super jump!'],
  sleep: ['Good night and sweet dreams…', 'Zzz… the clouds are cotton candy.'],
};
const ACTION_TIMING = Object.freeze({
  happy: 1600,
  feed: 2200,
  feedBite: 480,
  play: 2400,
  playCatch: 520,
  sleep: 3000,
});

let decayTimer;
let talkInputActive = false;
let glassesConnected = false;
let glassesSyncTimer;
let glassesSyncInFlight = false;
let glassesSyncPending = false;
let deviceMood = 'idle';
let deviceMoodTimer;
let mouthAnimationTimer;
let petAnimationTimer;
let petAnimationGeneration = 0;
let idleBlinkTimer;
let idleBlinkFrameTimer;
let idleBlinkGeneration = 0;
let nativeAudioAvailable = false;
let nativeAudioState = 'idle';
let nativeCapture;
let nativeStopPromise;
let nativeStartRequestInFlight = false;
let nativePressGeneration = 0;
const openingCaptureTerminals = createOpeningCaptureTerminalTracker();

// Bridge 2.0 capture data uses the native binary stream, not JSON audio.frames.
gm.audio.onCaptureState((audioState) => {
  if (nativeStartRequestInFlight && !nativeCapture &&
      openingCaptureTerminals.remember(audioState)) {
    return;
  }
  if (audioState.sessionId && nativeCapture?.sessionId &&
      audioState.sessionId !== nativeCapture.sessionId) return;
  if (audioState.state === 'stopped' && nativeStopPromise) return;
  nativeAudioState = audioState.state;
  if (audioState.state === 'capturing') {
    showListeningState();
    if (!talkInputActive) requestNativeRecordingStop();
  } else if (audioState.state === 'stopped') {
    // A timeout can stop the Host before the page asks for the result. Manual
    // stops already have a shared promise and must not be issued a second time.
    if (nativeCapture && !nativeStopPromise) requestNativeRecordingStop();
  } else if (audioState.state === 'error') {
    if (nativeCapture && !nativeStopPromise) discardNativeCapture(nativeCapture);
    resetTalkButton();
    stopMouthAnimation();
    const detail = audioState.errorCode === 'NO_AUDIO'
      ? 'No audio was captured. Please try again.'
      : `Glasses recording failed: ${audioState.message ?? audioState.errorCode ?? 'Unknown error'}`;
    say(detail);
  }
});

function resetTalkButton() {
  talkButton.classList.remove('recording', 'processing');
  talkButton.querySelector('strong').textContent = 'Hold to talk';
  pet.classList.remove('listening');
  room.classList.remove('is-listening');
}

function showListeningState() {
  talkButton.classList.add('recording');
  talkButton.querySelector('strong').textContent = 'Release to repeat';
  stopMouthAnimation('listening');
  animate('listening');
  setDeviceMood('listening', 15000);
  say('My glasses ears are up. I am listening…');
}

function showProcessingState() {
  resetTalkButton();
  talkButton.classList.add('processing');
  talkButton.querySelector('strong').textContent = 'Changing voice…';
  stopMouthAnimation('idle');
  animate('processing');
  resetDeviceMood();
  say('Memo is making your voice extra cute…');
}

function showNativeAudioError(error) {
  resetTalkButton();
  stopMouthAnimation();
  say(`Glasses audio is temporarily unavailable: ${error?.message ?? error?.errorCode ?? 'Unknown error'}`);
}

function requestNativeRecordingStop() {
  const capture = nativeCapture;
  if (nativeStopPromise) return nativeStopPromise;
  if (!capture) {
    if (nativeAudioState === 'stopping') nativeAudioState = 'stopped';
    return Promise.resolve();
  }
  nativeAudioState = 'stopping';
  const stopPromise = capture.stop().then((recording) => {
    if (nativeCapture === capture) nativeCapture = undefined;
    nativeAudioState = 'stopped';
    return playGlassesRecording(recording);
  }).catch((error) => {
    nativeAudioState = 'error';
    if (nativeCapture === capture) nativeCapture = undefined;
    showNativeAudioError(error);
  }).finally(() => {
    if (nativeStopPromise === stopPromise) nativeStopPromise = undefined;
  });
  nativeStopPromise = stopPromise;
  return stopPromise;
}

function discardNativeCapture(capture) {
  if (nativeCapture === capture) nativeCapture = undefined;
  if (nativeStopPromise) return nativeStopPromise;
  const discardPromise = capture.stop().catch(() => undefined).finally(() => {
    if (nativeStopPromise === discardPromise) nativeStopPromise = undefined;
  });
  nativeStopPromise = discardPromise;
  return discardPromise;
}

async function playGlassesRecording(recording) {
  showProcessingState();
  const audioUrl = URL.createObjectURL(opusRecordingToOgg(recording));
  const audio = new Audio(audioUrl);
  audio.playbackRate = 1.3;
  audio.preservesPitch = false;
  audio.addEventListener('play', () => {
    resetTalkButton();
    say('Memo heard you through the glasses and says:');
    startMouthAnimation();
    setDeviceMood('talking', 15000);
    burst('♪', 9, 'note');
    change({ happy: 10, energy: -2 }, 15);
  });
  audio.addEventListener('ended', () => {
    stopMouthAnimation(); resetDeviceMood(); URL.revokeObjectURL(audioUrl);
  }, { once: true });
  try { await audio.play(); } catch (error) { URL.revokeObjectURL(audioUrl); throw error; }
}

function setPetVisual(stateName) {
  petImage.src = petVisuals[stateName] ?? petVisuals.idle;
}

function canIdleBlink() {
  const activeActions = ['happy', 'eating', 'playing', 'sleeping', 'listening', 'processing', 'talking'];
  return deviceMood === 'idle'
    && !talkInputActive
    && (nativeAudioState === 'idle' || nativeAudioState === 'stopped' || nativeAudioState === 'error')
    && !activeActions.some((action) => pet.classList.contains(action));
}

function scheduleIdleBlink(delay = 2500 + Math.random() * 2500) {
  window.clearTimeout(idleBlinkTimer);
  idleBlinkTimer = window.setTimeout(() => {
    if (!canIdleBlink()) {
      scheduleIdleBlink(900);
      return;
    }
    const generation = ++idleBlinkGeneration;
    const doubleBlink = Math.random() < .35;
    setPetVisual('blink');
    idleBlinkFrameTimer = window.setTimeout(() => {
      if (generation !== idleBlinkGeneration || !canIdleBlink()) {
        scheduleIdleBlink();
        return;
      }
      setPetVisual('idle');
      if (!doubleBlink) {
        scheduleIdleBlink();
        return;
      }
      idleBlinkFrameTimer = window.setTimeout(() => {
        if (generation !== idleBlinkGeneration || !canIdleBlink()) {
          scheduleIdleBlink();
          return;
        }
        setPetVisual('blink');
        idleBlinkFrameTimer = window.setTimeout(() => {
          if (generation === idleBlinkGeneration && canIdleBlink()) setPetVisual('idle');
          scheduleIdleBlink();
        }, 95);
      }, 115);
    }, 95);
  }, delay);
}

function stopMouthAnimation(nextState = 'idle') {
  window.clearInterval(mouthAnimationTimer);
  mouthAnimationTimer = undefined;
  pet.classList.remove('listening', 'talking', 'processing');
  room.classList.remove('is-listening', 'is-talking', 'is-processing');
  setPetVisual(nextState);
}

function startMouthAnimation() {
  stopMouthAnimation('talking');
  animate('talking');
  let mouthOpen = true;
  mouthAnimationTimer = window.setInterval(() => {
    mouthOpen = !mouthOpen;
    setPetVisual(mouthOpen ? 'talking' : 'listening');
  }, 135);
}

const clamp = (value) => Math.max(0, Math.min(100, Math.round(value)));
const randomLine = (type) => lines[type][Math.floor(Math.random() * lines[type].length)];

function render() {
  for (const key of ['happy', 'food', 'energy']) {
    document.querySelector(`#${key}-bar`).style.width = `${state[key]}%`;
    document.querySelector(`#${key}-value`).value = state[key];
    document.querySelector(`#${key}-value`).textContent = state[key];
  }
  document.querySelector('#level').textContent = Math.floor(state.xp / 100) + 1;
}

function say(message) {
  speech.textContent = message;
  speech.classList.remove('pop');
  requestAnimationFrame(() => speech.classList.add('pop'));
  window.setTimeout(() => speech.classList.remove('pop'), 240);
}

function animate(name, duration = 0) {
  const actions = ['happy', 'eating', 'playing', 'sleeping', 'listening', 'processing', 'talking'];
  const generation = ++petAnimationGeneration;
  window.clearTimeout(petAnimationTimer);
  pet.classList.remove(...actions);
  room.classList.remove(...actions.map((action) => `is-${action}`));
  room.classList.remove('is-biting', 'is-play-caught');
  void pet.offsetWidth;
  pet.classList.add(name);
  room.classList.add(`is-${name}`);
  if (duration > 0) {
    petAnimationTimer = window.setTimeout(() => {
      if (generation !== petAnimationGeneration) return;
      pet.classList.remove(name);
      room.classList.remove(`is-${name}`, 'is-biting', 'is-play-caught');
      setPetVisual('idle');
    }, duration);
  }
  return generation;
}

function burst(symbol, count = 7, kind = 'default') {
  const petBox = pet.getBoundingClientRect();
  const roomBox = particles.getBoundingClientRect();
  for (let index = 0; index < count; index += 1) {
    const particle = document.createElement('span');
    particle.className = `particle particle-${kind}`;
    particle.textContent = symbol;
    particle.style.left = `${petBox.left - roomBox.left + petBox.width * (.25 + Math.random() * .5)}px`;
    particle.style.top = `${petBox.top - roomBox.top + petBox.height * (.25 + Math.random() * .35)}px`;
    particle.style.setProperty('--drift-x', `${Math.round(-90 + Math.random() * 180)}px`);
    particle.style.setProperty('--rise', `${Math.round(80 + Math.random() * 90)}px`);
    particle.style.setProperty('--spin', `${Math.round(-160 + Math.random() * 320)}deg`);
    particle.style.setProperty('--particle-size', `${Math.round(18 + Math.random() * 15)}px`);
    particle.style.setProperty('--particle-duration', `${Math.round(950 + Math.random() * 550)}ms`);
    particle.style.animationDelay = `${index * 45}ms`;
    particles.append(particle);
    window.setTimeout(() => particle.remove(), 2100);
  }
}

function change(values, xp = 8) {
  for (const [key, amount] of Object.entries(values)) state[key] = clamp(state[key] + amount);
  state.xp += xp;
  render();
  void saveState();
  scheduleGlassesSync();
}

function setDeviceMood(mood, duration = 1400) {
  deviceMood = mood;
  window.clearTimeout(deviceMoodTimer);
  scheduleGlassesSync();
  deviceMoodTimer = window.setTimeout(() => {
    deviceMood = 'idle';
    scheduleGlassesSync();
  }, duration);
}

function resetDeviceMood() {
  window.clearTimeout(deviceMoodTimer);
  deviceMoodTimer = undefined;
  if (deviceMood === 'idle') return;
  deviceMood = 'idle';
  scheduleGlassesSync();
}

function perform(action) {
  if (action === 'pet') {
    setPetVisual('idle');
    setDeviceMood('happy', ACTION_TIMING.happy);
    change({ happy: 7, energy: -1 });
    animate('happy', ACTION_TIMING.happy);
    burst('♥', 9, 'heart');
  } else if (action === 'feed') {
    setPetVisual('idle');
    setDeviceMood('eating', ACTION_TIMING.feed);
    change({ food: 18, happy: 3, energy: 2 }, 12);
    const generation = animate('eating', ACTION_TIMING.feed);
    window.setTimeout(() => {
      if (generation !== petAnimationGeneration) return;
      setPetVisual('eating');
      room.classList.add('is-biting');
      burst('•', 11, 'feed');
    }, ACTION_TIMING.feedBite);
  } else if (action === 'play') {
    const tired = state.energy < 12;
    setPetVisual('idle');
    setDeviceMood('playing', ACTION_TIMING.play);
    change({ happy: tired ? 8 : 14, food: -5, energy: tired ? -3 : -10 }, 16);
    const generation = animate('playing', ACTION_TIMING.play);
    window.setTimeout(() => {
      if (generation !== petAnimationGeneration) return;
      setPetVisual('playing');
      room.classList.add('is-play-caught');
      burst('★', tired ? 7 : 12, 'play');
    }, ACTION_TIMING.playCatch);
    say(tired ? 'I am a little tired, but I will still catch it! Whew!' : randomLine(action));
    return;
  } else if (action === 'sleep') {
    setPetVisual('sleeping');
    setDeviceMood('sleeping', ACTION_TIMING.sleep);
    change({ energy: 22, food: -3, happy: 2 }, 10);
    animate('sleeping', ACTION_TIMING.sleep);
    burst('Z', 7, 'sleep');
  }
  say(randomLine(action));
}

async function saveState() {
  try {
    await gm.storage.set('talking-pet-state', state);
  } catch {
    // The game remains playable when opened outside a connected host.
  }
}

function scheduleGlassesSync() {
  if (!glassesConnected) return;
  window.clearTimeout(glassesSyncTimer);
  glassesSyncTimer = window.setTimeout(() => void syncToGlasses(), 120);
}

function gray(level) {
  const value = Math.max(0, Math.min(15, Math.round(level))) * 17;
  return `rgb(${value},${value},${value})`;
}

function deviceText(value, x, y, size = 18, level = 15, weight = 600, align = 'left') {
  deviceContext.font = `${weight} ${size}px "PingFang SC","Microsoft YaHei",system-ui,sans-serif`;
  deviceContext.textAlign = align;
  deviceContext.textBaseline = 'middle';
  deviceContext.fillStyle = gray(level);
  deviceContext.fillText(String(value), x, y);
}

function deviceRoundRectPath(context, x, y, width, height, radius) {
  const safeRadius = Math.max(0, Math.min(radius, width / 2, height / 2));
  context.beginPath();
  context.moveTo(x + safeRadius, y);
  context.arcTo(x + width, y, x + width, y + height, safeRadius);
  context.arcTo(x + width, y + height, x, y + height, safeRadius);
  context.arcTo(x, y + height, x, y, safeRadius);
  context.arcTo(x, y, x + width, y, safeRadius);
  context.closePath();
}

function deviceRoundRect(x, y, width, height, radius, level, lineWidth = 2) {
  deviceRoundRectPath(deviceContext, x, y, width, height, radius);
  deviceContext.strokeStyle = gray(level);
  deviceContext.lineWidth = lineWidth;
  deviceContext.stroke();
}

function deviceStar(cx, cy, outerRadius, innerRadius, level = 15) {
  deviceContext.beginPath();
  for (let point = 0; point < 8; point += 1) {
    const angle = -Math.PI / 2 + point * Math.PI / 4;
    const radius = point % 2 === 0 ? outerRadius : innerRadius;
    const x = cx + Math.cos(angle) * radius;
    const y = cy + Math.sin(angle) * radius;
    if (point === 0) deviceContext.moveTo(x, y);
    else deviceContext.lineTo(x, y);
  }
  deviceContext.closePath();
  deviceContext.fillStyle = gray(level);
  deviceContext.fill();
}

function drawDeviceMemo() {
  const ctx = deviceContext;
  ctx.lineJoin = 'round';
  ctx.lineCap = 'round';

  ctx.fillStyle = gray(3);
  ctx.beginPath();
  ctx.ellipse(157, 236, 69, 73, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = gray(11);
  ctx.lineWidth = 3;
  ctx.stroke();

  ctx.fillStyle = gray(5);
  ctx.beginPath();
  ctx.moveTo(84, 112); ctx.lineTo(105, 55); ctx.lineTo(133, 102);
  ctx.moveTo(181, 102); ctx.lineTo(211, 55); ctx.lineTo(229, 114);
  ctx.fill();
  ctx.strokeStyle = gray(13);
  ctx.stroke();

  ctx.fillStyle = gray(4);
  ctx.beginPath();
  ctx.ellipse(157, 132, 91, 67, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = gray(14);
  ctx.lineWidth = 4;
  ctx.stroke();

  if (deviceMood === 'sleeping') {
    ctx.strokeStyle = gray(15);
    ctx.lineWidth = 5;
    ctx.beginPath(); ctx.moveTo(103, 132); ctx.quadraticCurveTo(119, 145, 135, 132); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(179, 132); ctx.quadraticCurveTo(195, 145, 211, 132); ctx.stroke();
    deviceText('Z', 237, 88, 23, 13, 700);
    deviceText('Z', 260, 64, 16, 8, 700);
  } else {
    deviceStar(120, 130, deviceMood === 'happy' ? 25 : 21, 7);
    deviceStar(194, 130, deviceMood === 'happy' ? 25 : 21, 7);
  }

  ctx.strokeStyle = gray(15);
  ctx.lineWidth = 4;
  ctx.beginPath();
  if (deviceMood === 'talking') ctx.ellipse(157, 169, 9, 14, 0, 0, Math.PI * 2);
  else { ctx.moveTo(142, 168); ctx.quadraticCurveTo(157, 181, 172, 168); }
  ctx.stroke();

  ctx.fillStyle = gray(9);
  deviceRoundRectPath(ctx, 102, 201, 110, 77, 18);
  ctx.fill();
  ctx.strokeStyle = gray(15);
  ctx.lineWidth = 3;
  ctx.stroke();
  ctx.strokeStyle = gray(3);
  ctx.lineWidth = 5;
  for (const y of [220, 240, 260]) {
    ctx.beginPath(); ctx.moveTo(145, y); ctx.lineTo(169, y); ctx.stroke();
    ctx.fillStyle = gray(15); ctx.beginPath(); ctx.arc(145, y, 4, 0, Math.PI * 2); ctx.fill();
    ctx.beginPath(); ctx.arc(169, y, 4, 0, Math.PI * 2); ctx.fill();
  }

  ctx.fillStyle = gray(4);
  ctx.beginPath(); ctx.ellipse(112, 296, 39, 16, 0, 0, Math.PI * 2); ctx.fill(); ctx.strokeStyle = gray(12); ctx.stroke();
  ctx.beginPath(); ctx.ellipse(202, 296, 39, 16, 0, 0, Math.PI * 2); ctx.fill(); ctx.stroke();

  if (deviceMood === 'eating') {
    ctx.fillStyle = gray(12); ctx.beginPath(); ctx.arc(246, 210, 18, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = gray(2);
    for (const [x, y] of [[238, 204], [252, 211], [241, 220]]) { ctx.beginPath(); ctx.arc(x, y, 2, 0, Math.PI * 2); ctx.fill(); }
  } else if (deviceMood === 'playing') {
    ctx.strokeStyle = gray(14); ctx.lineWidth = 3; ctx.beginPath(); ctx.arc(250, 239, 21, 0, Math.PI * 2); ctx.stroke();
    deviceStar(250, 239, 10, 4, 14);
  }
}

function drawDeviceBar(label, value, y, level) {
  deviceText(label, 322, y, 15, 10, 600);
  deviceRoundRect(392, y - 10, 154, 20, 10, 5, 2);
  const width = Math.max(8, Math.round(146 * value / 100));
  deviceContext.fillStyle = gray(level);
  deviceRoundRectPath(deviceContext, 396, y - 6, width, 12, 6);
  deviceContext.fill();
  deviceText(value, 566, y, 14, 15, 700, 'right');
}

function renderDeviceFrame() {
  deviceContext.fillStyle = gray(0);
  deviceContext.fillRect(0, 0, DEVICE_WIDTH, DEVICE_HEIGHT);
  deviceText('AUDIO TALKING PET', 20, 22, 16, 13, 750);
  deviceText(`LV.${Math.floor(state.xp / 100) + 1}`, 578, 22, 16, 15, 750, 'right');
  deviceContext.strokeStyle = gray(4);
  deviceContext.lineWidth = 1;
  deviceContext.beginPath(); deviceContext.moveTo(18, 40); deviceContext.lineTo(582, 40); deviceContext.stroke();
  drawDeviceMemo();
  deviceText(deviceMood === 'idle' ? 'Memo is waiting' : {
    happy: 'That feels great!', eating: 'Eating a cookie', playing: 'Let us play!', sleeping: 'Sweet dreams', talking: 'Memo repeats you', listening: 'Memo is listening…'
  }[deviceMood] ?? 'Memo is happy', 448, 78, 21, 15, 700, 'center');
  drawDeviceBar('HAPPY', state.happy, 130, 15);
  drawDeviceBar('FULL', state.food, 180, 11);
  drawDeviceBar('ENERGY', state.energy, 230, 8);
  deviceRoundRect(310, 267, 272, 45, 12, 6, 2);
  deviceText('PHONE: FEED · PLAY · SLEEP', 446, 290, 14, 11, 600, 'center');
  deviceContext.strokeStyle = gray(4);
  deviceContext.beginPath(); deviceContext.moveTo(18, 324); deviceContext.lineTo(582, 324); deviceContext.stroke();
  deviceText('SINGLE: PET   DOUBLE: FEED', 300, 338, 11, 7, 500, 'center');
}

function deviceGray4Bytes() {
  const pixels = deviceContext.getImageData(0, 0, DEVICE_WIDTH, DEVICE_HEIGHT).data;
  const bytes = new Uint8Array((DEVICE_WIDTH * DEVICE_HEIGHT) / 2);
  for (let source = 0, target = 0; source < pixels.length; source += 8, target += 1) {
    const left = Math.round((pixels[source] * .2126 + pixels[source + 1] * .7152 + pixels[source + 2] * .0722) / 17);
    const right = Math.round((pixels[source + 4] * .2126 + pixels[source + 5] * .7152 + pixels[source + 6] * .0722) / 17);
    bytes[target] = (Math.min(15, left) << 4) | Math.min(15, right);
  }
  return bytes;
}

function extractDeviceTile(frame, x, y, width, height) {
  const sourceStride = DEVICE_WIDTH / 2;
  const tileStride = width / 2;
  const tile = new Uint8Array(tileStride * height);
  for (let row = 0; row < height; row += 1) {
    const start = (y + row) * sourceStride + x / 2;
    tile.set(frame.subarray(start, start + tileStride), row * tileStride);
  }
  return tile;
}

function deviceBytesToBase64(bytes) {
  let binary = '';
  for (let offset = 0; offset < bytes.length; offset += 8192) {
    binary += String.fromCharCode(...bytes.subarray(offset, offset + 8192));
  }
  return btoa(binary);
}

async function syncToGlasses(announce = false) {
  if (!glassesConnected) {
    if (announce) say('Connect the glasses and run the native Audio Talking Pet plugin first.');
    return;
  }
  if (glassesSyncInFlight) {
    glassesSyncPending = true;
    return;
  }
  glassesSyncInFlight = true;
  syncButton.disabled = true;
  try {
    await gm.plugin.sendMessage(PET_STATE_CHANNEL, encodePetState(deviceMood, state));
    if (announce) say("Memo's status is now synced to the glasses!");
  } catch (error) {
    status.textContent = `Glasses sync failed · ${error.message}`;
    if (announce) say(`Sync failed: ${error.message}`);
  } finally {
    glassesSyncInFlight = false;
    syncButton.disabled = false;
    if (glassesSyncPending) {
      glassesSyncPending = false;
      scheduleGlassesSync();
    }
  }
}

async function startRecording(event) {
  event?.preventDefault();
  if (talkInputActive) return;
  talkInputActive = true;
  if (event?.pointerId !== undefined && event.pointerType !== 'touch') {
    try { talkButton.setPointerCapture?.(event.pointerId); } catch { /* optional enhancement */ }
  }
  if (!nativeAudioAvailable) {
    talkInputActive = false;
    resetTalkButton();
    say('Glasses audio capture is unavailable. Check the connected device and audio permission.');
    return;
  }
  if (nativeStartRequestInFlight || nativeCapture || nativeStopPromise ||
      (nativeAudioState !== 'idle' && nativeAudioState !== 'stopped' && nativeAudioState !== 'error')) return;
  const pressGeneration = ++nativePressGeneration;
  try {
    nativeAudioState = 'starting';
    nativeStartRequestInFlight = true;
    const capture = await gm.audio.openRecording({
      pickupMode: 'frontFocus',
      noiseReduction: true,
      maxDurationMs: 5000,
    });
    nativeStartRequestInFlight = false;
    const terminal = openingCaptureTerminals.take(capture.sessionId);
    const disposition = openedCaptureDisposition({
      terminal,
      inputActive: talkInputActive,
      generationMatches: pressGeneration === nativePressGeneration,
    });
    if (disposition === 'discard') {
      nativeAudioState = 'error';
      showNativeAudioError(terminal);
      discardNativeCapture(capture);
      return;
    }
    nativeCapture = capture;
    if (disposition === 'stop') requestNativeRecordingStop();
  } catch (error) {
    nativeStartRequestInFlight = false;
    nativeAudioState = 'error';
    showNativeAudioError(error);
  }
}

function stopRecording(event) {
  event?.preventDefault();
  if (!talkInputActive) return;
  talkInputActive = false;
  nativePressGeneration += 1;
  if (!nativeAudioAvailable) {
    resetTalkButton();
    return;
  }
  if (nativeAudioState === 'capturing') {
    showProcessingState();
    requestNativeRecordingStop();
  } else if (nativeAudioState === 'starting') {
    showProcessingState();
    // If the descriptor is already available, stop now. Otherwise
    // startRecording() observes talkInputActive=false when openRecording()
    // resolves and performs the same stop.
    if (!nativeStartRequestInFlight && nativeCapture) requestNativeRecordingStop();
  } else {
    resetTalkButton();
  }
}

pet.addEventListener('click', () => perform('pet'));
document.querySelectorAll('[data-action]:not([data-action="talk"])').forEach((button) => {
  button.addEventListener('click', () => perform(button.dataset.action));
});

talkButton.addEventListener('pointerdown', (event) => {
  if (event.pointerType !== 'touch') void startRecording(event);
});
talkButton.addEventListener('pointerup', (event) => {
  if (event.pointerType !== 'touch') stopRecording(event);
});
talkButton.addEventListener('pointercancel', (event) => {
  if (event.pointerType !== 'touch') stopRecording(event);
});
talkButton.addEventListener('touchstart', (event) => {
  if (event.touches.length === 1) void startRecording(event);
}, { passive: false });
talkButton.addEventListener('touchend', stopRecording, { passive: false });
talkButton.addEventListener('touchcancel', stopRecording, { passive: false });
talkButton.addEventListener('contextmenu', (event) => event.preventDefault());
talkButton.addEventListener('selectstart', (event) => event.preventDefault());
talkButton.addEventListener('dragstart', (event) => event.preventDefault());
talkButton.addEventListener('pointerleave', (event) => {
  if (event.pointerType !== 'touch' && event.buttons) stopRecording(event);
});
talkButton.addEventListener('keydown', (event) => {
  if ((event.key === ' ' || event.key === 'Enter') && !event.repeat) void startRecording(event);
});
talkButton.addEventListener('keyup', (event) => {
  if (event.key === ' ' || event.key === 'Enter') stopRecording(event);
});
syncButton.addEventListener('click', () => void syncToGlasses(true));

async function initialize() {
  render();
  try {
    await gm.ready();
    const capabilities = await gm.runtime.getCapabilities();
    nativeAudioAvailable = Boolean(
      capabilities?.audio
      && capabilities?.methods?.includes('audio.openCapture')
      && capabilities?.methods?.includes('audio.stopCapture'),
    );
    const saved = await gm.storage.get('talking-pet-state');
    if (saved.value && typeof saved.value === 'object') Object.assign(state, saved.value);
    render();
    gm.device.onButton((event) => {
      if (event.action === 'single') perform('pet');
      if (event.action === 'double') perform('feed');
      if (event.action === 'long') perform('sleep');
    });
    gm.device.onConnection((event) => {
      glassesConnected = event.connected;
      connectionDot.classList.toggle('connected', event.connected);
      status.textContent = event.connected ? 'Glasses connected · Native Memo animation ready' : 'Glasses disconnected · Phone play remains available';
      if (event.connected) scheduleGlassesSync();
    });
    await gm.device.subscribeEvents(['button', 'connection']);
    const deviceInfo = await gm.device.getInfo();
    glassesConnected = deviceInfo.connected;
    connectionDot.classList.toggle('connected', glassesConnected);
    const audioMode = nativeAudioAvailable ? 'Glasses microphone ready' : 'Glasses microphone unavailable';
    status.textContent = glassesConnected
      ? `Audio Talking Pet plugin ready · ${audioMode}`
      : `Audio Talking Pet plugin ready · ${audioMode} · Waiting for glasses`;
    if (glassesConnected) await syncToGlasses();
  } catch (error) {
    status.textContent = `Standalone demo mode · ${error.code ?? 'Bridge disconnected'}`;
  }

  decayTimer = window.setInterval(() => {
    change({ happy: -1, food: -1, energy: -1 }, 0);
  }, 3000);
  scheduleIdleBlink(1400);
}

window.addEventListener('pagehide', () => {
  window.clearInterval(decayTimer);
  window.clearTimeout(glassesSyncTimer);
  window.clearTimeout(deviceMoodTimer);
  window.clearTimeout(idleBlinkTimer);
  window.clearTimeout(idleBlinkFrameTimer);
  idleBlinkGeneration += 1;
  stopMouthAnimation();
  stopRecording();
});

initialize();
