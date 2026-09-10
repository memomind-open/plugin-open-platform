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
