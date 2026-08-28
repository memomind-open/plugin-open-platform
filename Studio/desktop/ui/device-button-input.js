export const DEVICE_BUTTON_ACTION = Object.freeze({
  CLICK: 1,
  DOUBLE_CLICK: 2,
  LONG_PRESS: 3,
});
export const DEVICE_BUTTON_LONG_PRESS_MS = 1000;

export function createDeviceButtonInput({
  emit,
  setTimer = setTimeout,
  clearTimer = clearTimeout,
  now = () => performance.now(),
  doubleClickMs = 300,
  longPressMs = DEVICE_BUTTON_LONG_PRESS_MS,
}) {
  let pressed = false;
  let longPressTriggered = false;
  let pressStartedAt = null;
  let pressTimer = null;
  let clickTimer = null;

  function clearPressTimer() {
    if (pressTimer === null) return;
    clearTimer(pressTimer);
    pressTimer = null;
  }

  function press() {
    if (pressed) return false;
    pressed = true;
    longPressTriggered = false;
    pressStartedAt = now();
    pressTimer = setTimer(() => {
      pressTimer = null;
      if (!pressed) return;
      longPressTriggered = true;
      emit(DEVICE_BUTTON_ACTION.LONG_PRESS);
    }, longPressMs);
    return true;
  }

  function release() {
    if (!pressed) return;
    const heldMs = Math.max(0, now() - pressStartedAt);
    pressed = false;
    pressStartedAt = null;
    clearPressTimer();
    if (longPressTriggered) {
      longPressTriggered = false;
      return;
    }
    if (heldMs >= longPressMs) {
      emit(DEVICE_BUTTON_ACTION.LONG_PRESS);
      return;
    }
    if (clickTimer !== null) {
      clearTimer(clickTimer);
      clickTimer = null;
      emit(DEVICE_BUTTON_ACTION.DOUBLE_CLICK);
      return;
    }
    clickTimer = setTimer(() => {
      clickTimer = null;
      emit(DEVICE_BUTTON_ACTION.CLICK);
    }, doubleClickMs);
  }

  function cancelPress() {
    if (!pressed) return;
    pressed = false;
    longPressTriggered = false;
    pressStartedAt = null;
    clearPressTimer();
  }

  function reset() {
    cancelPress();
    if (clickTimer === null) return;
    clearTimer(clickTimer);
    clickTimer = null;
  }

  return { press, release, cancelPress, reset };
}
