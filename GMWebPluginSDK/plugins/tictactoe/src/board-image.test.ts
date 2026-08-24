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

describe('GRAY_4 棋盘渲染', () => {
  it('生成设备右半屏所需的严格 stride 和像素长度', () => {
    const image = renderBoardImage(newGame());
    expect(image).toMatchObject({
      width: BOARD_IMAGE_WIDTH,
      height: BOARD_IMAGE_HEIGHT,
      stride: BOARD_IMAGE_STRIDE,
    });
    expect(image.pixels).toHaveLength(BOARD_IMAGE_STRIDE * BOARD_IMAGE_HEIGHT);
  });

  it('绘制边框、网格和中间选中态', () => {
    const image = renderBoardImage(newGame());
    expect(grayAt(image.pixels, 8, 8)).toBe(0);
    expect(grayAt(image.pixels, 128, 128)).toBe(11);
    expect(grayAt(image.pixels, 88, 40)).toBe(0);
  });

  it('绘制 X、O，并在终局增加胜利连线', () => {
    const board: GameState['board'] = ['X', 'X', 'X', 'O', 'O', null, null, null, null];
    const state: GameState = { version: 1, board, cursor: 8, result: evaluate(board) };
    const image = renderBoardImage(state);
    expect(grayAt(image.pixels, 48, 48)).toBe(5);
    expect(grayAt(image.pixels, 48, 128)).toBe(14);
    expect(grayAt(image.pixels, 48, 99)).toBe(0);
  });

  it('位图可编码为 App display.updateImage 使用的 Base64', () => {
    const image = renderBoardImage(newGame());
    const encoded = bytesToBase64(image.pixels);
    expect(atob(encoded)).toHaveLength(image.pixels.length);
  });
});
