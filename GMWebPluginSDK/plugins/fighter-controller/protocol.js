export const INPUT_CHANNEL = 0x4647;
export const INPUT_VERSION = 2;
export const PAUSE_BIT = 1 << 9;

export function encodeInput(sequence, buttons, paused = false) {
  const mask = buttons | (paused ? PAUSE_BIT : 0);
  return Uint8Array.of(INPUT_VERSION, sequence & 0xff,
    (mask >> 8) & 0xff, mask & 0xff);
}

export function bytesToBase64(bytes) {
  if (!(bytes instanceof Uint8Array)) throw new TypeError('bytes must be Uint8Array');
  if (globalThis.btoa) return globalThis.btoa(String.fromCharCode(...bytes));
  return Buffer.from(bytes).toString('base64');
}
