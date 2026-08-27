export const PET_STATE_CHANNEL = 0x4d50;

const moodIds = Object.freeze({
  idle: 0,
  happy: 1,
  eating: 2,
  playing: 3,
  sleeping: 4,
  listening: 5,
  talking: 6,
});

const byte = (value) => Math.max(0, Math.min(100, Math.round(value)));

export function encodePetState(mood, state) {
  const level = Math.max(1, Math.min(0xffff, Math.floor(state.xp / 100) + 1));
  return new Uint8Array([
    1,
    moodIds[mood] ?? moodIds.idle,
    byte(state.happy),
    byte(state.food),
    byte(state.energy),
    level >> 8,
    level & 0xff,
  ]);
}
