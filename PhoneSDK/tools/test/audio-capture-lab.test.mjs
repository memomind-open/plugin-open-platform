import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  AUDIO_LAB_STATE_CHANNEL,
  encodeAudioLabState,
} from '../../examples/audio-capture-lab/device-protocol.js';
import { StreamMetrics } from '../../examples/audio-capture-lab/stream-metrics.js';
import { createI18n } from '../../examples/audio-capture-lab/i18n.js';

const root = fileURLToPath(new URL('../..', import.meta.url));

test('Audio Capture Lab encodes one bounded big-endian GMP state packet', () => {
  const packet = encodeAudioLabState({
    state: 'streaming', mode: 'stream', pickupMode: 'frontFocus',
    profile: 'interactive', noiseReduction: true, voice: 'deep',
    elapsedMs: 65_432, chunkCount: 123, droppedFrameCount: 7,
    queueLatencyMs: 250, errorCode: 'none', language: 'zh',
  });
  assert.equal(AUDIO_LAB_STATE_CHANNEL, 0x414c);
  assert.equal(packet.byteLength, 23);
  assert.deepEqual([...packet.slice(0, 8)], [1, 3, 2, 5, 1, 1, 2, 0]);
  const view = new DataView(packet.buffer);
  assert.equal(view.getUint32(8, false), 65_432);
  assert.equal(view.getUint32(12, false), 123);
  assert.equal(view.getUint32(16, false), 7);
  assert.equal(view.getUint16(20, false), 250);
  assert.equal(packet[22], 1);
});

test('Audio Capture Lab stream metrics aggregate chunk and drop metadata', () => {
  let now = 1000;
  const metrics = new StreamMetrics(() => now);
  now = 1200;
  metrics.add({
    sequence: 4, timestampUs: 80_000, durationMs: 40, frameCount: 2,
    frameLengths: [40, 42], droppedFrameCount: 3, discontinuity: true,
    queueLatencyMs: 18, data: new Uint8Array(82),
  });
  now = 1400;
  const snapshot = metrics.add({
    sequence: 5, timestampUs: 120_000, durationMs: 40, frameCount: 2,
    frameLengths: [41, 41], droppedFrameCount: 0, discontinuity: false,
    queueLatencyMs: 9, data: new Uint8Array(82),
  });
  assert.equal(snapshot.chunkCount, 2);
  assert.equal(snapshot.frameCount, 4);
  assert.equal(snapshot.byteCount, 164);
  assert.equal(snapshot.droppedFrameCount, 3);
  assert.equal(snapshot.discontinuityCount, 1);
  assert.equal(snapshot.latestSequence, 5);
  assert.equal(snapshot.maxQueueLatencyMs, 18);
});

test('Audio Capture Lab stays on the native binary audio path', async () => {
  const plugin = await readFile(`${root}/examples/audio-capture-lab/plugin.js`, 'utf8');
  const manifest = JSON.parse(await readFile(`${root}/examples/audio-capture-lab/manifest.json`, 'utf8'));
  const gmp = await readFile(`${root}/../GlassSDK/examples/audio_capture_lab/audio_capture_lab.c`, 'utf8');
  assert.deepEqual(manifest.permissions, [
    { name: 'audio.capture', required: true },
    { name: 'audio.playback', required: false },
    { name: 'device.events', required: true, scope: { types: ['button', 'connection'] } },
    { name: 'device.info', required: true },
    { name: 'device.messaging', required: false, scope: { channels: [0x414c] } },
  ]);
  assert.equal(manifest.deviceRequirements.requiredPluginId, 'com.memomind.demo.audio-capture-lab');
  assert.match(plugin, /session\.stream\.getReader\(\)/);
  assert.match(plugin, /await reader\.read\(\)/);
  assert.doesNotMatch(plugin, /for await\s*\(/);
  assert.doesNotMatch(plugin, /dataBase64|\bbtoa\s*\(/);
  assert.match(gmp, /AUDIO_LAB_STATE_CHANNEL UINT16_C\(0x414C\)/);
  assert.doesNotMatch(gmp, /AudioChunk|openCapture|opus/i);
});

test('Audio Capture Lab does not expose transport implementation copy in the UI', async () => {
  const html = await readFile(`${root}/examples/audio-capture-lab/index.html`, 'utf8');
  assert.doesNotMatch(html, /No Base64|不使用 Base64|data-i18n="stream\.chip"/);
});

test('Audio Capture Lab keeps advanced controls collapsed by default', async () => {
  const html = await readFile(`${root}/examples/audio-capture-lab/index.html`, 'utf8');
  assert.match(html, /<details class="advanced-section">/);
  assert.match(html, /<details class="advanced-section" id="stream-advanced">/);
  assert.match(html, /<details class="panel diagnostics-panel">/);
  assert.doesNotMatch(html, /<details[^>]*\sopen(?:\s|>)/);
});

test('Audio Capture Lab covers every visible label in English and Chinese', async () => {
  const html = await readFile(`${root}/examples/audio-capture-lab/index.html`, 'utf8');
  const keys = [...html.matchAll(/data-i18n="([^"]+)"/g)].map((match) => match[1]);
  const environment = (language) => ({
    navigator: { language },
  });
  const english = createI18n(environment('en-US'));
  const chinese = createI18n(environment('zh-CN'));
  assert.ok(keys.length > 40);
  for (const key of keys) {
    assert.notEqual(english.t(key), key, `missing English translation: ${key}`);
    assert.notEqual(chinese.t(key), key, `missing Chinese translation: ${key}`);
  }
  assert.equal(english.language, 'en');
  assert.equal(chinese.language, 'zh');
});

test('Audio Capture Lab starts and switches language without browser storage', () => {
  const translatedNodes = [
    { dataset: { i18n: 'language.switch' }, textContent: '' },
  ];
  const document = {
    documentElement: { lang: '' },
    querySelectorAll: () => translatedNodes,
  };
  const environment = {
    navigator: { language: 'en-US' },
    document,
    get localStorage() {
      throw new DOMException('Blocked by the plugin sandbox', 'SecurityError');
    },
  };

  const i18n = createI18n(environment);
  i18n.apply();
  assert.equal(i18n.language, 'en');
  assert.equal(document.documentElement.lang, 'en');
  assert.equal(translatedNodes[0].textContent, '中文');

  i18n.toggle();
  assert.equal(i18n.language, 'zh');
  assert.equal(document.documentElement.lang, 'zh-CN');
  assert.equal(translatedNodes[0].textContent, 'EN');
});

test('Audio Capture Lab exposes only the five active pickup modes', async () => {
  const html = await readFile(`${root}/examples/audio-capture-lab/index.html`, 'utf8');
  const values = [...html.matchAll(/<option value="(frontFixed|meetingAuto|nonWearerFocus|frontBalanced|frontFocus|unchanged)"/g)]
    .map((match) => match[1]);
  assert.deepEqual(values, [
    'frontFixed', 'meetingAuto', 'nonWearerFocus', 'frontBalanced', 'frontFocus',
  ]);
  assert.match(html, /<option value="frontBalanced"[^>]* selected>/);
});

test('Audio Capture Lab plays transferred Opus in H5 without native playback methods', async () => {
  const html = await readFile(`${root}/examples/audio-capture-lab/index.html`, 'utf8');
  const plugin = await readFile(`${root}/examples/audio-capture-lab/plugin.js`, 'utf8');
  assert.doesNotMatch(html, /voice-effect|playback\.voice|voice\.(?:original|cute|deep|overlord)/);
  assert.doesNotMatch(plugin, /voiceEffect|voice\.(?:cute|deep|overlord)/);
  assert.match(plugin, /opusRecordingToOgg\(recordingResult\)/);
  assert.match(plugin, /recordingPlayer\.play\(\)/);
  assert.doesNotMatch(plugin, /gm\.audio\.(?:playRecording|stopPlayback)/);
});

test('Audio Capture Lab separates recording results from stream-only metrics', async () => {
  const html = await readFile(`${root}/examples/audio-capture-lab/index.html`, 'utf8');
  const plugin = await readFile(`${root}/examples/audio-capture-lab/plugin.js`, 'utf8');
  assert.match(html, /id="recording-metrics"/);
  assert.match(html, /id="recording-metric-duration"/);
  assert.match(html, /id="recording-metric-frames"/);
  assert.match(html, /id="recording-metric-bytes"/);
  assert.match(html, /id="stream-metrics"[^>]*class="[^"]*hidden|class="[^"]*hidden[^"]*"[^>]*id="stream-metrics"/);
  assert.match(plugin, /renderRecordingMetrics\(result\)/);
  assert.match(plugin, /result\.frameCount\.toLocaleString\(\)/);
  assert.match(plugin, /formatBytes\(result\.opusBytes\)/);
  assert.match(plugin, /streamMetrics\.classList\.toggle\('hidden', mode !== 'stream'\)/);
});

test('Audio Capture Lab does not stop capture when the page becomes hidden', async () => {
  const plugin = await readFile(`${root}/examples/audio-capture-lab/plugin.js`, 'utf8');
  assert.doesNotMatch(plugin, /document\.visibilityState/);
  assert.doesNotMatch(plugin, /visibilitychange/);
  assert.doesNotMatch(plugin, /pagehide/);
  assert.doesNotMatch(plugin, /Audio Capture Lab lifecycle ended/);
  assert.match(plugin, /ui\.stopCapture\.addEventListener\('click', \(\) => void stopCapture\('phone-button'\)\)/);
  assert.match(plugin, /event\.action === 'single'\) void stopActiveOperation\('glasses-button'\)/);
});

test('Audio Capture Lab exits the GMP on primary LONG and VERY_LONG actions', async () => {
  const gmp = await readFile(`${root}/../GlassSDK/examples/audio_capture_lab/audio_capture_lab.c`, 'utf8');
  assert.match(gmp, /GM_PLUGIN_BUTTON_ACTION_LONG\s*\|\|[\s\S]*GM_PLUGIN_BUTTON_ACTION_VERY_LONG/);
  assert.match(gmp, /self->host->app_exit\(\)/);
});
