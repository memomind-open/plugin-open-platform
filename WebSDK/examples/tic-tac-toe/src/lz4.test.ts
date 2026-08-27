import { describe, expect, it } from 'vitest';
import { renderBoardImage } from './board-image';
import { newGame } from './game';
import { compressLz4 } from './lz4';

interface SequenceInfo {
  literalLength: number;
  matchLength: number | null;
  matchStart: number | null;
}

function decodeStrict(block: Uint8Array, expectedLength: number): Uint8Array {
  const output: number[] = [];
  const sequences: SequenceInfo[] = [];
  let offset = 0;
  while (offset < block.length) {
    const token = block[offset++];
    let literalLength = token >>> 4;
    if (literalLength === 15) {
      let extension = 255;
      while (extension === 255) {
        if (offset >= block.length) throw new Error('truncated literal extension');
        extension = block[offset++];
        literalLength += extension;
      }
    }
    if (offset + literalLength > block.length) throw new Error('truncated literals');
    output.push(...block.subarray(offset, offset + literalLength));
    offset += literalLength;
    if (offset === block.length) {
      sequences.push({ literalLength, matchLength: null, matchStart: null });
      break;
    }
    if (offset + 2 > block.length) throw new Error('truncated match offset');
    const matchOffset = block[offset] | (block[offset + 1] << 8);
    offset += 2;
    if (matchOffset === 0 || matchOffset > output.length) throw new Error('invalid match offset');
    let matchLength = (token & 0x0f) + 4;
    if ((token & 0x0f) === 15) {
      let extension = 255;
      while (extension === 255) {
        if (offset >= block.length) throw new Error('truncated match extension');
        extension = block[offset++];
        matchLength += extension;
      }
    }
    const matchStart = output.length;
    for (let index = 0; index < matchLength; index += 1) {
      output.push(output[output.length - matchOffset]);
    }
    sequences.push({ literalLength, matchLength, matchStart });
  }
  if (output.length !== expectedLength) throw new Error('decoded length mismatch');
  const lastSequence = sequences[sequences.length - 1];
  if (lastSequence?.matchLength !== null) throw new Error('last sequence contains a match');
  if (expectedLength >= 5 && (lastSequence?.literalLength ?? 0) < 5) {
    throw new Error('fewer than five final literals');
  }
  for (const sequence of sequences) {
    if (sequence.matchStart !== null && sequence.matchStart > expectedLength - 12) {
      throw new Error('last match starts too near source end');
    }
  }
  return Uint8Array.from(output);
}

function seededBytes(length: number): Uint8Array {
  let seed = 0x6d2b79f5;
  return Uint8Array.from({ length }, () => {
    seed ^= seed << 13;
    seed ^= seed >>> 17;
    seed ^= seed << 5;
    return seed & 0xff;
  });
}

describe('strict raw LZ4 block encoder', () => {
  it('matches frozen Dart SDK golden vectors', () => {
    expect(compressLz4(Uint8Array.of())).toEqual(Uint8Array.of(0));
    expect(compressLz4(Uint8Array.of(1, 2, 3, 4, 5))).toEqual(
      Uint8Array.of(0x50, 1, 2, 3, 4, 5),
    );
    expect(compressLz4(Uint8Array.from({ length: 13 }, () => 0x61))).toEqual(
      Uint8Array.of(0x13, 0x61, 1, 0, 0x50, 0x61, 0x61, 0x61, 0x61, 0x61),
    );
    expect(compressLz4(Uint8Array.from({ length: 30 }, () => 0x61))).toEqual(
      Uint8Array.of(0x1f, 0x61, 1, 0, 5, 0x50, 0x61, 0x61, 0x61, 0x61, 0x61),
    );
    const literal15 = Uint8Array.from({ length: 15 }, (_, index) => index);
    expect(compressLz4(literal15)).toEqual(Uint8Array.of(0xf0, 0, ...literal15));
    const literal270 = seededBytes(270);
    expect(compressLz4(literal270)).toEqual(Uint8Array.of(0xf0, 255, 0, ...literal270));
  });

  it.each([0, 1, 4, 5, 11, 12, 13, 270, 4096])(
    'strict decoder round-trips length %i',
    (length) => {
      const input = length === 4096
        ? Uint8Array.from({ length }, (_, index) => index % 32)
        : seededBytes(length);
      expect(decodeStrict(compressLz4(input), input.length)).toEqual(input);
    },
  );

  it('strict decoder rejects malformed offsets and decoded lengths', () => {
    expect(() => decodeStrict(Uint8Array.of(0, 0, 0), 4)).toThrow('invalid match offset');
    expect(() => decodeStrict(Uint8Array.of(0x10, 1), 2)).toThrow('decoded length mismatch');
  });

  it('strictly decodes the complete board and remains significantly smaller than the source', () => {
    const pixels = renderBoardImage(newGame()).pixels;
    const compressed = compressLz4(pixels);
    expect(compressed.length).toBeLessThan(pixels.length);
    expect(decodeStrict(compressed, pixels.length)).toEqual(pixels);
  });
});
