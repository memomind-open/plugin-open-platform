import test from 'node:test';
import assert from 'node:assert/strict';

import { wrapOverlayText } from './text-overlay.js';

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
