import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const gm = createGMPlugin();
const pet = document.querySelector('#pet');
const petImage = document.querySelector('#pet-image');
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
  idle: './assets/momo-festive.png',
  listening: './assets/momo-listening-v2.png',
  talking: './assets/momo-talking-v2.png',
};
Object.values(petVisuals).forEach((source) => {
  const image = new Image();
  image.src = source;
});
deviceCanvas.width = DEVICE_WIDTH;
deviceCanvas.height = DEVICE_HEIGHT;

const state = {
  happy: 78,
  food: 62,
  energy: 84,
  xp: 0,
};

const lines = {
  pet: ['嘿嘿，好痒！', '再摸一下嘛～', 'Momo 最喜欢你啦！', '呼噜呼噜…'],
  feed: ['曲奇真香！', '嗷呜！还想吃一块。', '谢谢你的投喂！'],
  play: ['接住啦！', '再来一局！', '看我超级弹跳！'],
  sleep: ['晚安，做个甜甜的梦…', 'Zzz…云朵是棉花糖。'],
};

let recorder;
let recordingStream;
let recordingChunks = [];
let recordingTimer;
let decayTimer;
let talkInputActive = false;
let glassesConnected = false;
let glassesSyncTimer;
let glassesSyncInFlight = false;
let glassesSyncPending = false;
let devicePageCreated = false;
let deviceMood = 'idle';
let deviceMoodTimer;
let presentedDeviceTiles = new Map();
let mouthAnimationTimer;
let nativeAudioAvailable = false;
let nativeAudioState = 'idle';
let nativeAudioConfigurePromise;
let nativeStartRequestInFlight = false;
let nativePressGeneration = 0;
let nativeRecordingId;
let observedNativeFrames = 0;

gm.audio.onFrames((batch) => {
  observedNativeFrames += batch.frames.length;
});

gm.audio.onState((audioState) => {
  nativeAudioState = audioState.state;
  if (audioState.state === 'recording') {
    nativeRecordingId = audioState.recordingId;
    observedNativeFrames = 0;
    showListeningState();
  } else if (audioState.state === 'stopped') {
    nativeRecordingId = audioState.latestRecordingId ?? audioState.recordingId;
    resetTalkButton();
    if (nativeRecordingId) {
      void gm.audio.playRecording({ recordingId: nativeRecordingId, voice: 'cute' }).catch(showNativeAudioError);
    }
  } else if (audioState.state === 'error') {
    resetTalkButton();
    stopMouthAnimation();
    const detail = audioState.errorCode === 'NO_AUDIO'
      ? '没有收到声音，再试一次吧。'
      : `眼镜录音失败：${audioState.message ?? audioState.errorCode ?? '未知错误'}`;
    say(detail);
  }
});

gm.audio.onPlaybackState((playback) => {
  if (playback.state === 'playing') {
    say('Momo 用眼镜听到后学你说：');
    startMouthAnimation();
    setDeviceMood('talking', 4000);
    burst('♪', 6);
    change({ happy: 10, energy: -2 }, 15);
  } else if (playback.state === 'completed' || playback.state === 'stopped') {
    stopMouthAnimation();
  } else if (playback.state === 'error') {
    stopMouthAnimation();
    showNativeAudioError(playback);
  }
});

function resetTalkButton() {
  talkButton.classList.remove('recording');
  talkButton.querySelector('strong').textContent = '按住说话';
  pet.classList.remove('listening');
}

function showListeningState() {
  talkButton.classList.add('recording');
  talkButton.querySelector('strong').textContent = '松开让我学';
  stopMouthAnimation('listening');
  pet.classList.add('listening');
  setDeviceMood('listening', 15000);
  say('眼镜耳朵竖起来啦，我在认真听…');
}

function showNativeAudioError(error) {
  resetTalkButton();
  stopMouthAnimation();
  say(`眼镜音频暂时用不了：${error?.message ?? error?.errorCode ?? '未知错误'}`);
}

function ensureNativeAudioConfigured() {
  if (!nativeAudioConfigurePromise) {
    nativeAudioConfigurePromise = gm.audio
      .configure({ noiseReduction: true, pickupMode: 'frontFocus' })
      .catch((error) => {
        nativeAudioConfigurePromise = undefined;
        throw error;
      });
  }
  return nativeAudioConfigurePromise;
}

function requestNativeRecordingStop() {
  if (nativeAudioState === 'stopping') return;
  nativeAudioState = 'stopping';
  void gm.audio.stopRecording().catch((error) => {
    nativeAudioState = 'error';
    showNativeAudioError(error);
  });
}

function setPetVisual(stateName) {
  petImage.src = petVisuals[stateName] ?? petVisuals.idle;
}

function stopMouthAnimation(nextState = 'idle') {
  window.clearInterval(mouthAnimationTimer);
  mouthAnimationTimer = undefined;
  pet.classList.remove('listening');
  setPetVisual(nextState);
}

function startMouthAnimation() {
  stopMouthAnimation('talking');
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

function animate(name, duration = 1000) {
  pet.classList.remove('happy', 'eating', 'playing', 'sleeping', 'talking');
  pet.classList.add(name);
  window.setTimeout(() => pet.classList.remove(name), duration);
}

function burst(symbol, count = 7) {
  const petBox = pet.getBoundingClientRect();
  const roomBox = particles.getBoundingClientRect();
  for (let index = 0; index < count; index += 1) {
    const particle = document.createElement('span');
    particle.className = 'particle';
    particle.textContent = symbol;
    particle.style.left = `${petBox.left - roomBox.left + petBox.width * (.25 + Math.random() * .5)}px`;
    particle.style.top = `${petBox.top - roomBox.top + petBox.height * (.25 + Math.random() * .35)}px`;
    particle.style.animationDelay = `${index * 45}ms`;
    particles.append(particle);
    window.setTimeout(() => particle.remove(), 1500);
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

function perform(action) {
  if (action === 'pet') {
    setDeviceMood('happy');
    change({ happy: 7, energy: -1 });
    animate('happy', 700);
    burst('♥', 6);
  } else if (action === 'feed') {
    setDeviceMood('eating');
    change({ food: 18, happy: 3, energy: 2 }, 12);
    animate('eating', 1100);
    burst('🍪', 5);
  } else if (action === 'play') {
    if (state.energy < 12) {
      setDeviceMood('sleeping');
      say('有点累啦，让我先睡一会儿。');
      animate('sleeping', 1500);
      return;
    }
    setDeviceMood('playing');
    change({ happy: 14, food: -5, energy: -10 }, 16);
    animate('playing', 1800);
    burst('★', 8);
  } else if (action === 'sleep') {
    setDeviceMood('sleeping', 2300);
    change({ energy: 22, food: -3, happy: 2 }, 10);
    animate('sleeping', 2300);
    burst('Z', 5);
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

function drawDeviceMomo() {
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
  deviceText('MOMO TALKING PET', 20, 22, 16, 13, 750);
  deviceText(`LV.${Math.floor(state.xp / 100) + 1}`, 578, 22, 16, 15, 750, 'right');
  deviceContext.strokeStyle = gray(4);
  deviceContext.lineWidth = 1;
  deviceContext.beginPath(); deviceContext.moveTo(18, 40); deviceContext.lineTo(582, 40); deviceContext.stroke();
  drawDeviceMomo();
  deviceText(deviceMood === 'idle' ? 'Momo 正在等你' : {
    happy: '好舒服！', eating: '正在吃曲奇', playing: '一起玩吧！', sleeping: '正在做美梦', talking: 'Momo 学你说话', listening: 'Momo 在听…'
  }[deviceMood] ?? 'Momo 很开心', 448, 78, 21, 15, 700, 'center');
  drawDeviceBar('快乐', state.happy, 130, 15);
  drawDeviceBar('饱腹', state.food, 180, 11);
  drawDeviceBar('能量', state.energy, 230, 8);
  deviceRoundRect(310, 267, 272, 45, 12, 6, 2);
  deviceText('抬头 玩耍   ·   低头 喂食', 446, 290, 14, 11, 600, 'center');
  deviceContext.strokeStyle = gray(4);
  deviceContext.beginPath(); deviceContext.moveTo(18, 324); deviceContext.lineTo(582, 324); deviceContext.stroke();
  deviceText('单击 摸摸   双击 喂食', 300, 338, 11, 7, 500, 'center');
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

function deviceTilesEqual(left, right) {
  if (!left || left.length !== right.length) return false;
  for (let index = 0; index < left.length; index += 1) if (left[index] !== right[index]) return false;
  return true;
}

async function syncToGlasses(announce = false) {
  if (!glassesConnected) {
    if (announce) say('请先加载并运行眼镜端 Web Bridge。');
    return;
  }
  if (glassesSyncInFlight) {
    glassesSyncPending = true;
    return;
  }
  glassesSyncInFlight = true;
  syncButton.disabled = true;
  try {
    if (!devicePageCreated) {
      await gm.display.createPage();
      devicePageCreated = true;
      presentedDeviceTiles = new Map();
    }
    renderDeviceFrame();
    const frame = deviceGray4Bytes();
    for (let y = 0; y < DEVICE_HEIGHT; y += 175) {
      for (let x = 0; x < DEVICE_WIDTH; x += 200) {
        const key = `${x}:${y}`;
        const tile = extractDeviceTile(frame, x, y, 200, 175);
        if (deviceTilesEqual(presentedDeviceTiles.get(key), tile)) continue;
        await gm.display.updateImage({
          x, y, width: 200, height: 175, stride: 100,
          dataBase64: deviceBytesToBase64(tile),
        });
        presentedDeviceTiles.set(key, tile);
      }
    }
    if (announce) say('状态已经同步到眼镜啦！');
  } catch (error) {
    status.textContent = `眼镜同步失败 · ${error.message}`;
    if (announce) say(`同步失败：${error.message}`);
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
  talkInputActive = true;
  if (event?.pointerId !== undefined) talkButton.setPointerCapture?.(event.pointerId);
  if (nativeAudioAvailable) {
    if (nativeAudioState !== 'idle' && nativeAudioState !== 'stopped' && nativeAudioState !== 'error') return;
    const pressGeneration = ++nativePressGeneration;
    try {
      nativeAudioState = 'starting';
      observedNativeFrames = 0;
      await ensureNativeAudioConfigured();
      if (!talkInputActive || pressGeneration !== nativePressGeneration) {
        if (pressGeneration === nativePressGeneration) {
          nativeAudioState = 'idle';
          resetTalkButton();
        }
        return;
      }
      nativeStartRequestInFlight = true;
      await gm.audio.startRecording();
      nativeStartRequestInFlight = false;
      if (!talkInputActive || pressGeneration !== nativePressGeneration) requestNativeRecordingStop();
    } catch (error) {
      nativeStartRequestInFlight = false;
      nativeAudioState = 'error';
      showNativeAudioError(error);
    }
    return;
  }
  if (recorder?.state === 'recording') return;
  if (!navigator.mediaDevices?.getUserMedia || typeof MediaRecorder === 'undefined') {
    say('当前浏览器不支持录音，可以摸摸我！');
    return;
  }
  try {
    recordingStream = await navigator.mediaDevices.getUserMedia({ audio: true });
    if (!talkInputActive) {
      recordingStream.getTracks().forEach((track) => track.stop());
      return;
    }
    recordingChunks = [];
    recorder = new MediaRecorder(recordingStream);
    recorder.addEventListener('dataavailable', (chunk) => {
      if (chunk.data.size) recordingChunks.push(chunk.data);
    });
    recorder.addEventListener('stop', repeatRecording, { once: true });
    recorder.start();
    showListeningState();
    setDeviceMood('listening', 6000);
    say('耳朵竖起来啦，我在认真听…');
    recordingTimer = window.setTimeout(stopRecording, 5000);
  } catch (error) {
    stopMouthAnimation();
    say(error.name === 'NotAllowedError' ? '需要麦克风权限才能学你说话。' : '麦克风暂时用不了。');
  }
}

function stopRecording(event) {
  event?.preventDefault();
  talkInputActive = false;
  window.clearTimeout(recordingTimer);
  if (nativeAudioAvailable) {
    nativePressGeneration += 1;
    resetTalkButton();
    if (nativeAudioState === 'recording') {
      requestNativeRecordingStop();
    } else if (nativeAudioState === 'starting') {
      if (nativeStartRequestInFlight) {
        // The bridge has accepted START (or is about to); stop it as soon as
        // that request settles so a quick release cannot leave capture active.
        return;
      }
      // The user released while only local configuration was pending. No
      // native recording exists yet, so restore the state for the next press.
      nativeAudioState = 'idle';
    }
    return;
  }
  if (recorder?.state === 'recording') recorder.stop();
  resetTalkButton();
}

function repeatRecording() {
  recordingStream?.getTracks().forEach((track) => track.stop());
  if (!recordingChunks.length) {
    stopMouthAnimation();
    return;
  }
  const audioUrl = URL.createObjectURL(new Blob(recordingChunks, { type: recorder.mimeType }));
  const audio = new Audio(audioUrl);
  audio.playbackRate = 1.3;
  audio.preservesPitch = false;
  audio.addEventListener('play', () => {
    say('Momo 学你说：');
    startMouthAnimation();
    animate('talking', Math.max(800, (audio.duration || 2) * 770));
    setDeviceMood('talking', Math.max(800, (audio.duration || 2) * 770));
    burst('♪', 6);
  });
  audio.addEventListener('ended', () => {
    stopMouthAnimation();
    URL.revokeObjectURL(audioUrl);
  }, { once: true });
  audio.play().catch(() => {
    stopMouthAnimation();
    say('点一下屏幕后再让我学说话吧。');
  });
  change({ happy: 10, energy: -2 }, 15);
}

pet.addEventListener('click', () => perform('pet'));
document.querySelectorAll('[data-action]:not([data-action="talk"])').forEach((button) => {
  button.addEventListener('click', () => perform(button.dataset.action));
});

talkButton.addEventListener('pointerdown', startRecording);
talkButton.addEventListener('pointerup', stopRecording);
talkButton.addEventListener('pointercancel', stopRecording);
talkButton.addEventListener('pointerleave', (event) => {
  if (event.buttons) stopRecording(event);
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
    nativeAudioAvailable = Boolean(capabilities?.audio);
    if (nativeAudioAvailable) await ensureNativeAudioConfigured();
    const saved = await gm.storage.get('talking-pet-state');
    if (saved.value && typeof saved.value === 'object') Object.assign(state, saved.value);
    render();
    gm.device.onButton((event) => {
      if (event.action === 'single') perform('pet');
      if (event.action === 'double') perform('feed');
    });
    gm.device.onGesture((event) => {
      if (!event.active) return;
      if (event.gesture === 'headRaise' || event.gesture === 'right') perform('play');
      if (event.gesture === 'headLower' || event.gesture === 'left') perform('feed');
    });
    gm.device.onConnection((event) => {
      glassesConnected = event.connected;
      devicePageCreated = false;
      presentedDeviceTiles = new Map();
      connectionDot.classList.toggle('connected', event.connected);
      status.textContent = event.connected ? '眼镜已连接 · 正在同步画面' : '眼镜已断开 · 手机端仍可玩';
      if (event.connected) scheduleGlassesSync();
    });
    await gm.device.subscribeEvents(['button', 'imuGesture', 'connection']);
    const deviceInfo = await gm.device.getInfo();
    glassesConnected = deviceInfo.connected;
    connectionDot.classList.toggle('connected', glassesConnected);
    const audioMode = nativeAudioAvailable ? '眼镜麦克风已就绪' : '使用手机麦克风';
    status.textContent = glassesConnected
      ? `Bridge v1 已就绪 · ${audioMode} · 正在同步画面`
      : `Bridge v1 已就绪 · ${audioMode} · 等待眼镜连接`;
    if (glassesConnected) await syncToGlasses();
  } catch (error) {
    status.textContent = `独立试玩模式 · ${error.code ?? 'Bridge 未连接'}`;
  }

  decayTimer = window.setInterval(() => {
    change({ food: -1, energy: -1 }, 0);
  }, 45000);
}

window.addEventListener('pagehide', () => {
  window.clearInterval(decayTimer);
  window.clearTimeout(glassesSyncTimer);
  window.clearTimeout(deviceMoodTimer);
  stopMouthAnimation();
  stopRecording();
  if (nativeAudioAvailable) void gm.audio.stopPlayback();
  recordingStream?.getTracks().forEach((track) => track.stop());
});

initialize();
