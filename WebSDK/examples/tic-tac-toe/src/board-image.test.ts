import { describe, expect, it } from 'vitest';
import {
  BOARD_IMAGE_HEIGHT,
  BOARD_IMAGE_STRIDE,
  BOARD_IMAGE_WIDTH,
  bytesToBase64,
  renderBoardImage,
} from './board-image';
import { evaluate, newGame, type GameState } from './game';

function grayAt(
  pixels: Uint8Array,
  x: number,
  y: number,
  stride = BOARD_IMAGE_STRIDE,
): number {
  const packed = pixels[y * stride + Math.floor(x / 2)];
  return x % 2 === 0 ? packed >> 4 : packed & 0x0f;
}

describe('GRAY_4 board rendering', () => {
  it('generates the exact stride and pixel length required by the device right pane', () => {
    const image = renderBoardImage(newGame());
    expect(image).toMatchObject({
      width: BOARD_IMAGE_WIDTH,
      height: BOARD_IMAGE_HEIGHT,
      stride: BOARD_IMAGE_STRIDE,
    });
    expect(image.pixels).toHaveLength(BOARD_IMAGE_STRIDE * BOARD_IMAGE_HEIGHT);
  });

  it('draws the border, grid, and center selection state', () => {
    const image = renderBoardImage(newGame());
    expect(grayAt(image.pixels, 8, 8)).toBe(0);
    expect(grayAt(image.pixels, 128, 128)).toBe(11);
    expect(grayAt(image.pixels, 88, 40)).toBe(0);
  });

  it('draws X and O and adds the winning line at game end', () => {
    const board: GameState['board'] = ['X', 'X', 'X', 'O', 'O', null, null, null, null];
    const state: GameState = { version: 1, board, cursor: 8, result: evaluate(board) };
    const image = renderBoardImage(state);
    expect(grayAt(image.pixels, 48, 48)).toBe(5);
    expect(grayAt(image.pixels, 48, 128)).toBe(14);
    expect(grayAt(image.pixels, 48, 99)).toBe(0);
  });

  it('encodes the bitmap as Base64 for App display.updateImage', () => {
    const image = renderBoardImage(newGame());
    const encoded = bytesToBase64(image.pixels);
    expect(atob(encoded)).toHaveLength(image.pixels.length);
  });
});
