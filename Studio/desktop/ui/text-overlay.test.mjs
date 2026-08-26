import test from 'node:test';
import assert from 'node:assert/strict';

import { layoutOverlayText, wrapOverlayText } from './text-overlay.js';

const context = {
  measureText(text) {
    return { width: Array.from(text).length * 10 };
  },
};

test('wraps CJK text at the device label width', () => {
  assert.deepEqual(wrapOverlayText(context, '眼镜文字', 20, true), ['眼镜', '文字']);
});

test('preserves explicit line breaks when wrapping is disabled', () => {
  assert.deepEqual(wrapOverlayText(context, 'first\nsecond', 20, false), ['first', 'second']);
});

test('includes LVGL letter spacing while wrapping', () => {
  assert.deepEqual(wrapOverlayText(context, 'ABC', 22, true, 2), ['AB', 'C']);
});

test('anchors auto-sized labels with the measured host-font width', () => {
  const layout = layoutOverlayText(context, {
    x: 40,
    y: 0,
    width: 100,
    height: 34,
    fontHeight: 34,
    alignment: 1,
    objectAlignment: 3,
    autoSize: true,
    wrap: true,
    text: 'ABC',
  });
  assert.equal(layout.x, 110);
  assert.equal(layout.width, 30);
});

test('omits lines that do not fully fit in the label bounds', () => {
  const layout = layoutOverlayText(context, {
    x: 0,
    y: 0,
    width: 220,
    height: 70,
    fontHeight: 34,
    alignment: 2,
    objectAlignment: 9,
    autoSize: false,
    wrap: true,
    text: 'GAME OVER\nClick: restart\nHold: exit',
  });
  assert.deepEqual(layout.visibleLines.map((line) => line.text), [
    'GAME OVER',
    'Click: restart',
  ]);
});
