import { winningCells, type GameState } from './game';

export const BOARD_IMAGE_WIDTH = 256;
export const BOARD_IMAGE_HEIGHT = 256;
export const BOARD_IMAGE_STRIDE = BOARD_IMAGE_WIDTH / 2;

const WHITE = 15;
const PANEL = 14;
const SELECTED = 11;
const INK = 0;
const WIN_LINE = 5;
const BOARD_MARGIN = 8;
const BOARD_SIZE = 240;
const CELL_SIZE = BOARD_SIZE / 3;

export interface Gray4Image {
  width: number;
  height: number;
  stride: number;
  pixels: Uint8Array;
}

export function renderBoardImage(state: GameState): Gray4Image {
  const gray = new Uint8Array(BOARD_IMAGE_WIDTH * BOARD_IMAGE_HEIGHT);
  gray.fill(WHITE);
  fillRect(gray, 2, 2, 252, 252, PANEL);

  if (state.result === 'playing' && state.board[state.cursor] === null) {
    const column = state.cursor % 3;
    const row = Math.floor(state.cursor / 3);
    fillRect(
      gray,
      BOARD_MARGIN + column * CELL_SIZE + 4,
      BOARD_MARGIN + row * CELL_SIZE + 4,
      CELL_SIZE - 8,
      CELL_SIZE - 8,
      SELECTED,
    );
    drawRect(
      gray,
      BOARD_MARGIN + column * CELL_SIZE + 7,
      BOARD_MARGIN + row * CELL_SIZE + 7,
      CELL_SIZE - 14,
      CELL_SIZE - 14,
      2,
      INK,
    );
  }

  drawRect(gray, BOARD_MARGIN, BOARD_MARGIN, BOARD_SIZE, BOARD_SIZE, 4, INK);
  for (const offset of [CELL_SIZE, CELL_SIZE * 2]) {
    fillRect(gray, BOARD_MARGIN + offset - 2, BOARD_MARGIN, 4, BOARD_SIZE, INK);
    fillRect(gray, BOARD_MARGIN, BOARD_MARGIN + offset - 2, BOARD_SIZE, 4, INK);
  }

  state.board.forEach((cell, index) => {
    if (!cell) return;
    const column = index % 3;
    const row = Math.floor(index / 3);
    const centerX = BOARD_MARGIN + column * CELL_SIZE + CELL_SIZE / 2;
    const centerY = BOARD_MARGIN + row * CELL_SIZE + CELL_SIZE / 2;
    if (cell === 'X') drawX(gray, centerX, centerY);
    if (cell === 'O') drawO(gray, centerX, centerY);
  });

  const line = winningCells(state.board);
  if (line) {
    const start = cellCenter(line[0]);
    const end = cellCenter(line[2]);
    drawLine(gray, start.x, start.y, end.x, end.y, 4, WIN_LINE);
  }

  return {
    width: BOARD_IMAGE_WIDTH,
    height: BOARD_IMAGE_HEIGHT,
    stride: BOARD_IMAGE_STRIDE,
    pixels: packGray4(gray),
  };
}

export function bytesToBase64(bytes: Uint8Array): string {
  let binary = '';
  const chunkSize = 0x4000;
  for (let offset = 0; offset < bytes.length; offset += chunkSize) {
    binary += String.fromCharCode(...bytes.subarray(offset, offset + chunkSize));
  }
  return btoa(binary);
}

function packGray4(gray: Uint8Array): Uint8Array {
  const packed = new Uint8Array(BOARD_IMAGE_STRIDE * BOARD_IMAGE_HEIGHT);
  for (let y = 0; y < BOARD_IMAGE_HEIGHT; y += 1) {
    for (let x = 0; x < BOARD_IMAGE_WIDTH; x += 2) {
      const high = gray[y * BOARD_IMAGE_WIDTH + x] & 0x0f;
      const low = gray[y * BOARD_IMAGE_WIDTH + x + 1] & 0x0f;
      packed[y * BOARD_IMAGE_STRIDE + x / 2] = (high << 4) | low;
    }
  }
  return packed;
}

function drawX(gray: Uint8Array, centerX: number, centerY: number): void {
  const radius = 27;
  drawLine(gray, centerX - radius, centerY - radius, centerX + radius, centerY + radius, 5, INK);
  drawLine(gray, centerX + radius, centerY - radius, centerX - radius, centerY + radius, 5, INK);
}

function drawO(gray: Uint8Array, centerX: number, centerY: number): void {
  const radius = 29;
  const thickness = 6;
  for (let y = Math.floor(centerY - radius - thickness); y <= centerY + radius + thickness; y += 1) {
    for (let x = Math.floor(centerX - radius - thickness); x <= centerX + radius + thickness; x += 1) {
      const distance = Math.hypot(x - centerX, y - centerY);
      if (Math.abs(distance - radius) <= thickness / 2) setPixel(gray, x, y, INK);
    }
  }
}

function cellCenter(index: number): { x: number; y: number } {
  return {
    x: BOARD_MARGIN + (index % 3) * CELL_SIZE + CELL_SIZE / 2,
    y: BOARD_MARGIN + Math.floor(index / 3) * CELL_SIZE + CELL_SIZE / 2,
  };
}

function drawLine(
  gray: Uint8Array,
  startX: number,
  startY: number,
  endX: number,
  endY: number,
  thickness: number,
  color: number,
): void {
  const distance = Math.max(Math.abs(endX - startX), Math.abs(endY - startY));
  const radius = Math.floor(thickness / 2);
  for (let step = 0; step <= distance; step += 1) {
    const progress = distance === 0 ? 0 : step / distance;
    const x = Math.round(startX + (endX - startX) * progress);
    const y = Math.round(startY + (endY - startY) * progress);
    fillRect(gray, x - radius, y - radius, radius * 2 + 1, radius * 2 + 1, color);
  }
}

function drawRect(
  gray: Uint8Array,
  x: number,
  y: number,
  width: number,
  height: number,
  thickness: number,
  color: number,
): void {
  fillRect(gray, x, y, width, thickness, color);
  fillRect(gray, x, y + height - thickness, width, thickness, color);
  fillRect(gray, x, y, thickness, height, color);
  fillRect(gray, x + width - thickness, y, thickness, height, color);
}

function fillRect(
  gray: Uint8Array,
  x: number,
  y: number,
  width: number,
  height: number,
  color: number,
): void {
  const startX = Math.max(0, Math.floor(x));
  const startY = Math.max(0, Math.floor(y));
  const endX = Math.min(BOARD_IMAGE_WIDTH, Math.ceil(x + width));
  const endY = Math.min(BOARD_IMAGE_HEIGHT, Math.ceil(y + height));
  for (let row = startY; row < endY; row += 1) {
    gray.fill(color, row * BOARD_IMAGE_WIDTH + startX, row * BOARD_IMAGE_WIDTH + endX);
  }
}

function setPixel(gray: Uint8Array, x: number, y: number, color: number): void {
  if (x < 0 || y < 0 || x >= BOARD_IMAGE_WIDTH || y >= BOARD_IMAGE_HEIGHT) return;
  gray[y * BOARD_IMAGE_WIDTH + x] = color;
}
