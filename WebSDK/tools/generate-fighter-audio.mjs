#!/usr/bin/env node
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const SAMPLE_RATE = 22050;
const repositoryRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const outputDirectory = resolve(repositoryRoot, 'plugins/fighter-controller/assets/sfx');
let noiseState = 0x4648a11d;

await mkdir(outputDirectory, { recursive: true });

const effects = new Map([
  ['swing_sweep.wav', sweepEffect(0.34, 520, 105, 0.72)],
  ['impact_light.wav', impactEffect(0.18, 170, 0.55)],
  ['impact_heavy.wav', impactEffect(0.30, 105, 0.92)],
  ['impact_kick.wav', impactEffect(0.27, 135, 0.82)],
  ['impact_special.wav', specialImpact()],
  ['impact_sweep.wav', sweepImpact()],
  ['block.wav', blockEffect()],
  ['guard_break.wav', guardBreakEffect()],
  ['special_launch.wav', specialLaunchEffect()],
  ['jump.wav', sweepEffect(0.25, 180, 680, 0.42)],
  ['round_start.wav', cue([60, 67, 72], 0.18, 0.96, 'square')],
  ['round_win.wav', cue([60, 64, 67, 72], 0.20, 1.24, 'triangle')],
  ['round_loss.wav', cue([55, 51, 48, 43], 0.23, 1.38, 'triangle')],
  ['round_draw.wav', cue([55, 60, 55], 0.24, 1.15, 'square')],
  ['ko.wav', koEffect()],
]);

for (const [name, samples] of effects) await writeWav(name, samples);

const music = [
  ['music_title.wav', 102, [48, null, 55, null, 60, null, 55, null, 51, null, 58, null, 63, null, 58, null], [36, 36, 39, 39]],
  ['music_select.wav', 124, [60, 64, 67, 72, 67, 64, 62, 67, 65, 69, 72, 77, 72, 69, 67, 71], [48, 50, 53, 55]],
  ['music_fight.wav', 152, [60, 63, 67, 70, 67, 72, 70, 67, 58, 62, 65, 70, 65, 74, 70, 65], [36, 43, 34, 41]],
  ['music_victory.wav', 132, [60, 64, 67, 72, 76, 79, 84, 79, 65, 69, 72, 77, 81, 84, 89, 84], [48, 53, 55, 60]],
  ['music_defeat.wav', 88, [60, null, 59, null, 55, null, 51, null, 48, null, 47, null, 43, null, 36, null], [36, 35, 31, 24]],
];

for (const [name, bpm, melody, bass] of music) {
  await writeWav(name, musicLoop(bpm, melody, bass, name === 'music_fight.wav'));
}

async function writeWav(name, samples) {
  await writeFile(resolve(outputDirectory, name), encodeWav(normalize(samples)));
}

function samplesFor(seconds) {
  return new Float64Array(Math.ceil(seconds * SAMPLE_RATE));
}

function addTone(target, {
  start = 0, duration, frequency, endFrequency = frequency, gain = 0.3,
  wave = 'sine', attack = 0.006, release = 0.08,
}) {
  const first = Math.floor(start * SAMPLE_RATE);
  const count = Math.min(Math.floor(duration * SAMPLE_RATE), target.length - first);
  let phase = 0;
  for (let index = 0; index < count; index += 1) {
    const elapsed = index / SAMPLE_RATE;
    const progress = index / Math.max(1, count - 1);
    const frequencyAtSample = frequency * ((endFrequency / frequency) ** progress);
    phase += 2 * Math.PI * frequencyAtSample / SAMPLE_RATE;
    const envelope = Math.min(1, elapsed / attack) *
      Math.min(1, (duration - elapsed) / release);
    target[first + index] += oscillator(wave, phase) * gain * Math.max(0, envelope);
  }
}

function addNoise(target, { start = 0, duration, gain = 0.3, release = duration, color = 0.5 }) {
  const first = Math.floor(start * SAMPLE_RATE);
  const count = Math.min(Math.floor(duration * SAMPLE_RATE), target.length - first);
  let filtered = 0;
  for (let index = 0; index < count; index += 1) {
    const white = seededNoise();
    filtered += color * (white - filtered);
    const progress = index / Math.max(1, count - 1);
    const envelope = Math.min(1, index / (SAMPLE_RATE * 0.004)) *
      Math.min(1, (duration * (1 - progress)) / release);
    target[first + index] += filtered * gain * Math.max(0, envelope);
  }
}

function oscillator(wave, phase) {
  const sine = Math.sin(phase);
  if (wave === 'square') return sine >= 0 ? 1 : -1;
  if (wave === 'triangle') return 2 / Math.PI * Math.asin(sine);
  if (wave === 'saw') return 2 * ((phase / (2 * Math.PI)) % 1) - 1;
  return sine;
}

function seededNoise() {
  noiseState ^= noiseState << 13;
  noiseState ^= noiseState >>> 17;
  noiseState ^= noiseState << 5;
  return ((noiseState >>> 0) / 0xffffffff) * 2 - 1;
}

function sweepEffect(duration, from, to, gain) {
  const output = samplesFor(duration);
  addNoise(output, { duration, gain: gain * 0.36, release: duration, color: 0.36 });
  addTone(output, { duration, frequency: from, endFrequency: to, gain, wave: 'saw', release: duration });
  return output;
}

function impactEffect(duration, frequency, strength) {
  const output = samplesFor(duration);
  addNoise(output, { duration: duration * 0.72, gain: strength, release: duration * 0.55, color: 0.62 });
  addTone(output, { duration, frequency: frequency * 1.7, endFrequency: frequency,
    gain: strength * 0.72, wave: 'sine', release: duration * 0.72 });
  addTone(output, { duration: duration * 0.75, frequency: frequency * 0.55,
    gain: strength * 0.62, wave: 'triangle', release: duration * 0.65 });
  return output;
}

function specialImpact() {
  const output = impactEffect(0.48, 82, 0.86);
  addTone(output, { duration: 0.44, frequency: 260, endFrequency: 980,
    gain: 0.46, wave: 'square', release: 0.16 });
  return output;
}

function sweepImpact() {
  const output = impactEffect(0.38, 92, 0.90);
  addNoise(output, { start: 0.06, duration: 0.30, gain: 0.58, release: 0.26, color: 0.24 });
  return output;
}

function blockEffect() {
  const output = samplesFor(0.30);
  addNoise(output, { duration: 0.15, gain: 0.66, release: 0.12, color: 0.78 });
  for (const frequency of [420, 690, 1040]) {
    addTone(output, { duration: 0.26, frequency, gain: 0.28, wave: 'square', release: 0.22 });
  }
  return output;
}

function guardBreakEffect() {
  const output = blockEffect();
  const extended = samplesFor(0.62);
  extended.set(output);
  for (let index = 0; index < 5; index += 1) {
    addTone(extended, { start: 0.12 + index * 0.055, duration: 0.20,
      frequency: 940 - index * 125, gain: 0.28, wave: 'square', release: 0.17 });
  }
  addNoise(extended, { start: 0.16, duration: 0.38, gain: 0.44, release: 0.34, color: 0.72 });
  return extended;
}

function specialLaunchEffect() {
  const output = samplesFor(0.58);
  addTone(output, { duration: 0.52, frequency: 145, endFrequency: 1160,
    gain: 0.54, wave: 'saw', release: 0.15 });
  addTone(output, { start: 0.12, duration: 0.42, frequency: 290, endFrequency: 1740,
    gain: 0.34, wave: 'square', release: 0.12 });
  addNoise(output, { start: 0.30, duration: 0.25, gain: 0.24, release: 0.20, color: 0.28 });
  return output;
}

function cue(notes, spacing, length, wave) {
  const output = samplesFor(length);
  notes.forEach((note, index) => {
    addTone(output, { start: index * spacing, duration: spacing * 1.8,
      frequency: noteFrequency(note), gain: 0.43, wave, release: spacing * 1.1 });
    addTone(output, { start: index * spacing, duration: spacing * 1.5,
      frequency: noteFrequency(note + 12), gain: 0.18, wave: 'sine', release: spacing });
  });
  return output;
}

function koEffect() {
  const output = samplesFor(1.28);
  addNoise(output, { duration: 0.34, gain: 0.82, release: 0.28, color: 0.66 });
  addTone(output, { duration: 0.66, frequency: 150, endFrequency: 52,
    gain: 0.88, wave: 'saw', release: 0.58 });
  [48, 43, 36].forEach((note, index) => {
    addTone(output, { start: 0.32 + index * 0.21, duration: 0.48,
      frequency: noteFrequency(note), gain: 0.42, wave: 'square', release: 0.35 });
  });
  return output;
}

function musicLoop(bpm, melody, bass, aggressive) {
  const step = 60 / bpm / 2;
  const steps = melody.length * 2;
  const output = samplesFor(steps * step);
  for (let index = 0; index < steps; index += 1) {
    const note = melody[index % melody.length];
    if (note != null) {
      addTone(output, { start: index * step, duration: step * 0.86,
        frequency: noteFrequency(note), gain: aggressive ? 0.18 : 0.15,
        wave: aggressive ? 'square' : 'triangle', release: step * 0.30 });
    }
    if (index % 4 === 0) {
      const bassNote = bass[(index / 4) % bass.length];
      addTone(output, { start: index * step, duration: step * 3.5,
        frequency: noteFrequency(bassNote), gain: 0.22, wave: 'triangle', release: step });
      addTone(output, { start: index * step, duration: step * 0.7,
        frequency: 105, endFrequency: 48, gain: aggressive ? 0.42 : 0.25,
        wave: 'sine', release: step * 0.55 });
    }
    if (index % 2 === 1) {
      addNoise(output, { start: index * step, duration: step * 0.20,
        gain: aggressive ? 0.11 : 0.065, release: step * 0.18, color: 0.76 });
    }
  }
  return output;
}

function noteFrequency(note) {
  return 440 * (2 ** ((note - 69) / 12));
}

function normalize(samples) {
  let maximum = 0;
  for (const sample of samples) maximum = Math.max(maximum, Math.abs(sample));
  const scale = maximum > 0.94 ? 0.94 / maximum : 1;
  return Float64Array.from(samples, (sample) => Math.tanh(sample * scale * 1.18) / Math.tanh(1.18));
}

function encodeWav(samples) {
  const dataBytes = samples.length * 2;
  const output = Buffer.alloc(44 + dataBytes);
  output.write('RIFF', 0);
  output.writeUInt32LE(36 + dataBytes, 4);
  output.write('WAVE', 8);
  output.write('fmt ', 12);
  output.writeUInt32LE(16, 16);
  output.writeUInt16LE(1, 20);
  output.writeUInt16LE(1, 22);
  output.writeUInt32LE(SAMPLE_RATE, 24);
  output.writeUInt32LE(SAMPLE_RATE * 2, 28);
  output.writeUInt16LE(2, 32);
  output.writeUInt16LE(16, 34);
  output.write('data', 36);
  output.writeUInt32LE(dataBytes, 40);
  samples.forEach((sample, index) => {
    output.writeInt16LE(Math.round(Math.max(-1, Math.min(1, sample)) * 32767), 44 + index * 2);
  });
  return output;
}
