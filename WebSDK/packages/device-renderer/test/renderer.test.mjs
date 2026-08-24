import assert from 'node:assert/strict';
import test from 'node:test';

import { CanvasDeviceRenderer, Gray4Framebuffer, decodeRawLz4 } from '../src/index.js';

test('GRAY_4 stores the even pixel in the high nibble', () => {
  const framebuffer = new Gray4Framebuffer({ width: 4, height: 2 });
  framebuffer.drawPacked({ x: 0, y: 0, width: 4, height: 1, stride: 2, pixels: Uint8Array.from([0xf1, 0x8a]) });
  assert.deepEqual(Array.from(framebuffer.pixels.slice(0, 4)), [15, 1, 8, 10]);
});

test('GRAY_4 accepts firmware-compatible row padding', () => {
  const framebuffer = new Gray4Framebuffer({ width: 4, height: 1 });
  framebuffer.drawPacked({ x: 0, y: 0, width: 3, height: 1, stride: 3, pixels: Uint8Array.from([0x12, 0x30, 0xff]) });
  assert.deepEqual(Array.from(framebuffer.pixels), [1, 2, 3, 0]);
});

test('GRAY_4 rejects stride below packed row width', () => {
  const framebuffer = new Gray4Framebuffer({ width: 4, height: 2 });
  assert.throws(() => framebuffer.drawPacked({ x: 0, y: 0, width: 4, height: 1, stride: 1, pixels: new Uint8Array(1) }), /stride/);
});

test('raw LZ4 decoder handles a literal-only terminal sequence', () => {
  const decoded = decodeRawLz4(Uint8Array.from([0x30, 1, 2, 3]), 3);
  assert.deepEqual(Array.from(decoded), [1, 2, 3]);
});

test('raw LZ4 decoder rejects a zero match offset', () => {
  assert.throws(() => decodeRawLz4(Uint8Array.from([0x10, 1, 0, 0]), 5), /offset/);
});

test('atomic frame does not render intermediate tiles', () => {
  const canvas = fakeCanvas();
  const renderer = new CanvasDeviceRenderer(canvas, {
    width: 2, height: 2, defaultFontPx: 1,
  });
  const initialRenders = canvas.context.renderCount;

  renderer.beginFrame();
  renderer.updateFrameImage({
    x: 0, y: 0, width: 2, height: 1, stride: 1,
    pixels: Uint8Array.from([0xf0]),
  }, false);
  assert.equal(canvas.context.renderCount, initialRenders);
  renderer.updateFrameImage({
    x: 0, y: 1, width: 2, height: 1, stride: 1,
    pixels: Uint8Array.from([0x0f]),
  }, true);
  assert.equal(canvas.context.renderCount, initialRenders + 1);
  assert.deepEqual(Array.from(renderer.framebuffer.pixels), [15, 0, 0, 15]);
});

function fakeCanvas() {
  const context = {
    renderCount: 0,
    createImageData(width, height) { return { data: new Uint8ClampedArray(width * height * 4) }; },
    putImageData() { this.renderCount += 1; },
    measureText() { return { width: 0 }; },
    beginPath() {}, roundRect() {}, stroke() {}, save() {}, rect() {}, clip() {}, fillText() {}, restore() {},
  };
  return { width: 0, height: 0, context, getContext: () => context };
}
