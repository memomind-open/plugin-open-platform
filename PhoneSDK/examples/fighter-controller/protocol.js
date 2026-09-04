export const INPUT_CHANNEL = 0x4647;
export const INPUT_VERSION = 2;
export const PAUSE_BIT = 1 << 9;
export const EVENT_CHANNEL = 0x4648;
export const EVENT_VERSION = 1;

export const FIGHT_EVENT_TYPES = Object.freeze({
  1: 'hit',
  2: 'block',
  3: 'guardBreak',
  4: 'specialLaunch',
  5: 'roundEnd',
  6: 'attack',
  7: 'jump',
  8: 'roundStart',
  9: 'menu',
  10: 'ko',
  11: 'music',
});

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

export function decodeFightEvent(message) {
  if (message?.channel !== EVENT_CHANNEL) return null;
  const data = message.data;
  if (!(data instanceof Uint8Array) || data.length !== 4 || data[0] !== EVENT_VERSION) {
    return null;
  }
  const type = FIGHT_EVENT_TYPES[data[2]];
  if (!type) return null;
  return { sequence: data[1], type, value: data[3] };
}
