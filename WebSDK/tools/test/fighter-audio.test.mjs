import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

import { FighterAudio } from '../../plugins/fighter-controller/audio.js';

class FakeAudio {
  constructor(source) {
    this.source = source;
    this.paused = true;
    this.ended = false;
    this.currentTime = 0;
    this.loop = false;
    this.volume = 1;
    this.playCount = 0;
    this.pauseCount = 0;
  }

  play() {
    this.paused = false;
    this.playCount += 1;
    return Promise.resolve();
  }

  pause() {
    this.paused = true;
    this.pauseCount += 1;
  }
}

function harness() {
  const tracks = [];
  let now = 1000;
  const audio = new FighterAudio({
    audioFactory: (source) => {
      const track = new FakeAudio(source);
      tracks.push(track);
      return track;
    },
    now: () => now,
  });
  return { audio, tracks, advance: (milliseconds) => { now += milliseconds; } };
}

test('Fighter audio maps combat and match-flow events to effects', () => {
  const { audio, tracks } = harness();
  const cases = [
    [{ type: 'hit', value: 1 }, 'impact_light.wav'],
    [{ type: 'hit', value: 6 }, 'impact_sweep.wav'],
    [{ type: 'block', value: 0 }, 'block.wav'],
    [{ type: 'guardBreak', value: 0 }, 'guard_break.wav'],
    [{ type: 'specialLaunch', value: 0 }, 'special_launch.wav'],
    [{ type: 'roundEnd', value: 0 }, 'round_draw.wav'],
    [{ type: 'roundEnd', value: 1 }, 'round_win.wav'],
    [{ type: 'roundEnd', value: 2 }, 'round_loss.wav'],
    [{ type: 'jump', value: 0 }, 'jump.wav'],
    [{ type: 'roundStart', value: 0 }, 'round_start.wav'],
    [{ type: 'menu', value: 1 }, 'menu_confirm.wav'],
    [{ type: 'ko', value: 1 }, 'ko.wav'],
  ];

  for (const [event, file] of cases) {
    assert.equal(audio.playGameEvent(event), true);
    assert.equal(tracks.at(-4).source, `assets/sfx/${file}`);
  }
  assert.equal(audio.playGameEvent({ type: 'future', value: 0 }), false);
});

test('Fighter audio suppresses the matching game attack after local input', () => {
  const { audio, tracks, advance } = harness();
  audio.playInput(4);
  const playCount = () => tracks
    .filter((track) => track.source.endsWith('/swing_light.wav'))
    .reduce((sum, track) => sum + track.playCount, 0);
  assert.equal(playCount(), 1);

  assert.equal(audio.playGameEvent({ type: 'attack', value: 1 }), true);
  assert.equal(playCount(), 1);

  advance(351);
  assert.equal(audio.playGameEvent({ type: 'attack', value: 1 }), true);
  assert.equal(playCount(), 2);
});

test('Fighter audio switches, pauses, and resumes looping music', () => {
  const { audio, tracks } = harness();
  assert.equal(audio.playGameEvent({ type: 'music', value: 2 }), true);
  const fight = tracks.find((track) => track.source.endsWith('/music_fight.wav'));
  assert.equal(fight.loop, true);
  assert.equal(fight.playCount, 1);

  audio.setMusicPaused(true);
  assert.equal(fight.paused, true);
  audio.setMusicPaused(false);
  assert.equal(fight.playCount, 2);

  assert.equal(audio.playGameEvent({ type: 'music', value: 3 }), true);
  const victory = tracks.find((track) => track.source.endsWith('/music_victory.wav'));
  assert.equal(fight.paused, true);
  assert.equal(victory.loop, true);
  assert.equal(victory.playCount, 1);
  assert.equal(audio.playGameEvent({ type: 'music', value: 99 }), false);
});

test('Fighter generated audio assets are PCM mono WAV files', async () => {
  const names = [
    'swing_sweep.wav',
    'impact_light.wav', 'impact_heavy.wav', 'impact_kick.wav',
    'impact_special.wav', 'impact_sweep.wav', 'block.wav', 'guard_break.wav',
    'special_launch.wav', 'jump.wav', 'round_start.wav', 'round_win.wav',
    'round_loss.wav', 'round_draw.wav', 'ko.wav',
    'music_title.wav', 'music_select.wav', 'music_fight.wav',
    'music_victory.wav', 'music_defeat.wav',
  ];
  for (const name of names) {
    const data = await readFile(new URL(`../../plugins/fighter-controller/assets/sfx/${name}`, import.meta.url));
    assert.equal(data.toString('ascii', 0, 4), 'RIFF', name);
    assert.equal(data.toString('ascii', 8, 12), 'WAVE', name);
    assert.equal(data.readUInt16LE(20), 1, name);
    assert.equal(data.readUInt16LE(22), 1, name);
    assert.equal(data.readUInt32LE(24), 22050, name);
    assert.equal(data.readUInt16LE(34), 16, name);
    assert.equal(data.readUInt32LE(40), data.length - 44, name);
  }
});
