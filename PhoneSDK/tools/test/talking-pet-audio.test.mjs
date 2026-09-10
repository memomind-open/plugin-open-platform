import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

import {
  createOpeningCaptureTerminalTracker,
  openedCaptureDisposition,
} from '../../examples/talking-pet/recording-lifecycle.js';

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

test('talking-pet discards a session that failed before openRecording returned', () => {
  const terminals = createOpeningCaptureTerminalTracker();
  assert.equal(terminals.remember({
    state: 'error', sessionId: 'capture-opening-1', errorCode: 'NO_AUDIO',
  }), true);

  const failed = terminals.take('capture-opening-1');
  assert.equal(openedCaptureDisposition({
    terminal: failed, inputActive: true, generationMatches: true,
  }), 'discard');
  assert.equal(terminals.take('capture-opening-1'), undefined);
  assert.equal(openedCaptureDisposition({
    terminal: terminals.take('capture-opening-2'), inputActive: true, generationMatches: true,
  }), 'keep');
});

test('talking-pet stops a returned session after release or an early stopped event', () => {
  assert.equal(openedCaptureDisposition({
    terminal: { state: 'stopped' }, inputActive: true, generationMatches: true,
  }), 'stop');
  assert.equal(openedCaptureDisposition({
    terminal: undefined, inputActive: false, generationMatches: true,
  }), 'stop');
  assert.equal(openedCaptureDisposition({
    terminal: undefined, inputActive: true, generationMatches: false,
  }), 'stop');
});
