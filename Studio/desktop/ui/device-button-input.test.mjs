import assert from 'node:assert/strict';
import test from 'node:test';

import { createDeviceButtonInput, DEVICE_BUTTON_ACTION } from './device-button-input.js';

function createClock() {
  let now = 0;
  let sequence = 0;
  const timers = new Map();
  return {
    setTimer(callback, delay) {
      sequence += 1;
      timers.set(sequence, { callback, time: now + delay });
      return sequence;
    },
    clearTimer(id) {
      timers.delete(id);
    },
    now() {
      return now;
    },
    elapse(milliseconds) {
      now += milliseconds;
    },
    advance(milliseconds) {
      const target = now + milliseconds;
      while (true) {
        const next = [...timers.entries()]
          .filter(([, timer]) => timer.time <= target)
          .sort((left, right) => left[1].time - right[1].time || left[0] - right[0])[0];
        if (!next) break;
        const [id, timer] = next;
        timers.delete(id);
        now = timer.time;
        timer.callback();
      }
      now = target;
    },
  };
}

function createInput() {
  const actions = [];
  const clock = createClock();
  const input = createDeviceButtonInput({
    emit: (action) => actions.push(action),
    setTimer: clock.setTimer,
    clearTimer: clock.clearTimer,
    now: clock.now,
  });
  return { actions, clock, input };
}

test('one short press emits one click after the double-click window', () => {
  const { actions, clock, input } = createInput();
  input.press();
  input.release();
  clock.advance(299);
  assert.deepEqual(actions, []);
  clock.advance(1);
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.CLICK]);
});

test('two short presses emit one double click without a single click', () => {
  const { actions, clock, input } = createInput();
  input.press();
  input.release();
  clock.advance(100);
  input.press();
  input.release();
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.DOUBLE_CLICK]);
  clock.advance(300);
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.DOUBLE_CLICK]);
});

test('holding for one second emits one long press and release emits nothing else', () => {
  const { actions, clock, input } = createInput();
  input.press();
  clock.advance(999);
  assert.deepEqual(actions, []);
  clock.advance(1);
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.LONG_PRESS]);
  input.release();
  clock.advance(300);
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.LONG_PRESS]);
});

test('release recovers a long press when the host delayed the timer callback', () => {
  const { actions, clock, input } = createInput();
  input.press();
  clock.elapse(1000);
  input.release();
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.LONG_PRESS]);
  clock.advance(300);
  assert.deepEqual(actions, [DEVICE_BUTTON_ACTION.LONG_PRESS]);
});

test('cancelling an active press emits no action', () => {
  const { actions, clock, input } = createInput();
  input.press();
  input.cancelPress();
  clock.advance(1000);
  assert.deepEqual(actions, []);
});
