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

const ATTACK_BY_INPUT_BIT = new Map([
  [4, 1],
  [5, 2],
  [6, 3],
  [7, 6],
  [10, 4],
  [11, 5],
]);

const ATTACK_SOUND = new Map([
  [1, ['swing_light.wav', 0.48]],
  [2, ['swing_heavy.wav', 0.58]],
  [3, ['swing_kick.wav', 0.58]],
  [4, ['charge.wav', 0.62]],
  [5, ['special.wav', 0.78]],
  [6, ['swing_sweep.wav', 0.62]],
]);

const HIT_SOUND = new Map([
  [1, ['impact_light.wav', 0.68]],
  [2, ['impact_heavy.wav', 0.82]],
  [3, ['impact_kick.wav', 0.82]],
  [4, ['impact_special.wav', 0.92]],
  [5, ['impact_heavy.wav', 0.86]],
  [6, ['impact_sweep.wav', 0.86]],
]);

const MUSIC_SOUND = new Map([
  [0, ['music_title.wav', 0.26]],
  [1, ['music_select.wav', 0.25]],
  [2, ['music_fight.wav', 0.28]],
  [3, ['music_victory.wav', 0.30]],
  [4, ['music_defeat.wav', 0.27]],
]);

const POOL_SIZE = 4;
const LOCAL_ATTACK_DEDUP_MS = 350;

export class FighterAudio {
  constructor({ audioFactory = (source) => new Audio(source), now = () => Date.now() } = {}) {
    this.audioFactory = audioFactory;
    this.now = now;
    this.pools = new Map();
    this.musicTracks = new Map();
    this.currentMusic = null;
    this.currentMusicState = null;
    this.musicPaused = false;
    this.pendingMusic = false;
    this.recentAttack = null;
  }

  playInput(bit) {
    const sound = SOUND_BY_INPUT_BIT.get(bit);
    if (sound) this.play(...sound);
    const attack = ATTACK_BY_INPUT_BIT.get(bit);
    if (attack) this.recentAttack = { attack, timestampMs: this.now() };
  }

  playPause() {
    this.play('menu_move.wav', 0.48);
  }

  playGameEvent(event) {
    if (!event || typeof event.type !== 'string') return false;
    switch (event.type) {
      case 'hit': return this.playMapped(HIT_SOUND, event.value);
      case 'block': this.play('block.wav', 0.72); return true;
      case 'guardBreak': this.play('guard_break.wav', 0.88); return true;
      case 'specialLaunch': this.play('special_launch.wav', 0.88); return true;
      case 'roundEnd':
        this.play(event.value === 1 ? 'round_win.wav' :
          (event.value === 2 ? 'round_loss.wav' : 'round_draw.wav'), 0.86);
        return true;
      case 'attack':
        if (this.isRecentLocalAttack(event.value)) return true;
        return this.playMapped(ATTACK_SOUND, event.value);
      case 'jump': this.play('jump.wav', 0.50); return true;
      case 'roundStart': this.play('round_start.wav', 0.78); return true;
      case 'menu': this.play(event.value === 1 ? 'menu_confirm.wav' : 'menu_move.wav', 0.60); return true;
      case 'ko': this.play('ko.wav', 0.94); return true;
      case 'music': return this.setMusic(event.value);
      default: return false;
    }
  }

  playMapped(sounds, value) {
    const sound = sounds.get(value);
    if (!sound) return false;
    this.play(...sound);
    return true;
  }

  isRecentLocalAttack(attack) {
    const recent = this.recentAttack;
    if (!recent || recent.attack !== attack || this.now() - recent.timestampMs > LOCAL_ATTACK_DEDUP_MS) {
      return false;
    }
    this.recentAttack = null;
    return true;
  }

  play(name, volume) {
    const pool = this.pool(name, volume);
    const audio = pool.find((candidate) => candidate.paused || candidate.ended) ?? pool[0];
    audio.pause();
    audio.currentTime = 0;
    this.start(audio, false);
    this.resumeMusic();
  }

  pool(name, volume) {
    let pool = this.pools.get(name);
    if (pool) return pool;
    pool = Array.from({ length: POOL_SIZE }, () => {
      const audio = this.audioFactory(`assets/sfx/${name}`);
      audio.preload = 'auto';
      audio.volume = volume;
      return audio;
    });
    this.pools.set(name, pool);
    return pool;
  }

  setMusic(state) {
    const definition = MUSIC_SOUND.get(state);
    if (!definition) return false;
    if (this.currentMusicState === state) {
      this.resumeMusic();
      return true;
    }
    this.currentMusic?.pause();
    const [name, volume] = definition;
    let audio = this.musicTracks.get(name);
    if (!audio) {
      audio = this.audioFactory(`assets/sfx/${name}`);
      audio.preload = 'auto';
      audio.loop = true;
      audio.volume = volume;
      this.musicTracks.set(name, audio);
    }
    audio.currentTime = 0;
    this.currentMusic = audio;
    this.currentMusicState = state;
    this.pendingMusic = true;
    this.resumeMusic();
    return true;
  }

  setMusicPaused(paused) {
    this.musicPaused = Boolean(paused);
    if (this.musicPaused) this.currentMusic?.pause();
    else this.resumeMusic();
  }

  resumeMusic() {
    if (!this.currentMusic || this.musicPaused ||
        (!this.pendingMusic && !this.currentMusic.paused)) return;
    this.start(this.currentMusic, true);
  }

  start(audio, music) {
    try {
      const started = audio.play();
      if (music) this.pendingMusic = false;
      started?.catch(() => {
        if (music && audio === this.currentMusic) this.pendingMusic = true;
      });
    } catch {
      if (music && audio === this.currentMusic) this.pendingMusic = true;
    }
  }

  destroy() {
    for (const pool of this.pools.values()) {
      for (const audio of pool) audio.pause();
    }
    for (const audio of this.musicTracks.values()) audio.pause();
    this.pools.clear();
    this.musicTracks.clear();
    this.currentMusic = null;
    this.currentMusicState = null;
  }
}

export { FighterAudio as FighterInputAudio };
