export function describeDeviceFrameTransition(wasRunning, running) {
  return {
    drawFrame: running,
    exited: wasRunning && !running,
  };
}
