import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const source = await readFile(
  new URL('../../examples/talking-pet/plugin.js', import.meta.url),
  'utf8',
);

test('talking-pet uses the unified Host audio stream only', () => {
  assert.match(source, /gm\.audio\.openRecording\s*\(/);
  assert.match(source, /audio\.openCapture/);
  assert.match(source, /audio\.stopCapture/);
  assert.doesNotMatch(source, /getUserMedia|MediaRecorder/);
  assert.doesNotMatch(source, /modes\?\.recording/);
});

test('talking-pet serializes stop requests and restores a reusable capture state', () => {
  assert.match(source, /let nativeStopPromise;/);
  assert.match(source, /if \(nativeStopPromise\) return nativeStopPromise;/);
  assert.match(source, /nativeCapture && !nativeStopPromise/);
  assert.match(source, /nativeAudioState = 'stopped';/);
  assert.match(source, /audioState\.sessionId !== nativeCapture\.sessionId/);
  assert.match(source, /audioState\.state === 'stopped' && nativeStopPromise/);
  assert.match(source, /nativeStartRequestInFlight \|\| nativeCapture \|\| nativeStopPromise/);
});

test('talking-pet handles Android touch hold without waiting for a drag gesture', () => {
  assert.match(source, /addEventListener\('touchstart'/);
  assert.match(source, /addEventListener\('touchend'/);
  assert.match(source, /addEventListener\('touchcancel'/);
  assert.match(source, /\{ passive: false \}/);
  assert.match(source, /if \(talkInputActive\) return;/);
  assert.match(source, /if \(!talkInputActive\) return;/);
  assert.match(source, /event\.pointerType !== 'touch'/);
});
