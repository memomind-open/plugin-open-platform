const SOUND_BY_INPUT_BIT = new Map([
  [4, ['swing_light.wav', 0.48]],
  [5, ['swing_heavy.wav', 0.58]],
  [6, ['swing_kick.wav', 0.58]],
  // Guard success depends on a game event; this is only a local input cue.
  [7, ['menu_move.wav', 0.48]],
  [8, ['menu_confirm.wav', 0.60]],
  [10, ['charge.wav', 0.62]],
  [11, ['special.wav', 0.92]],
]);

const POOL_SIZE = 4;

export class FighterInputAudio {
  constructor() {
    this.pools = new Map();
  }

  playInput(bit) {
    const sound = SOUND_BY_INPUT_BIT.get(bit);
    if (sound) this.play(...sound);
  }

  playPause() {
    this.play('menu_move.wav', 0.48);
  }

  play(name, volume) {
    const pool = this.pool(name, volume);
    const audio = pool.find((candidate) => candidate.paused || candidate.ended) ?? pool[0];
    audio.pause();
    audio.currentTime = 0;
    audio.play()?.catch(() => {
      // Some WebViews reject playback until the first user gesture. Calls to
      // this class originate from pointer/click handlers, so later taps work.
    });
  }

  pool(name, volume) {
    let pool = this.pools.get(name);
    if (pool) return pool;
    pool = Array.from({ length: POOL_SIZE }, () => {
      const audio = new Audio(`assets/sfx/${name}`);
      audio.preload = 'auto';
      audio.volume = volume;
      return audio;
    });
    this.pools.set(name, pool);
    return pool;
  }
}
