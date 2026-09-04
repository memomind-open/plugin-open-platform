import { describe, expect, it } from 'vitest';
import {
  chooseComputerMove,
  DeviceInputFilter,
  evaluate,
  mapDeviceEvent,
  moveCursor,
  newGame,
  parseStoredGame,
  placeAtCursor,
  reduceGame,
  resultText,
  type GameState,
} from './game';

function state(board: GameState['board'], cursor = 0): GameState {
  return { version: 1, board, cursor, result: evaluate(board) };
}

describe('tic-tac-toe domain logic', () => {
  it('starts with player X and selects the center by default', () => {
    expect(newGame()).toEqual({ version: 1, board: Array(9).fill(null), cursor: 4, result: 'playing' });
  });

  it('allows at most one computer move after a valid player move', () => {
    const next = placeAtCursor(newGame());
    expect(next.board.filter((cell) => cell === 'X')).toHaveLength(1);
    expect(next.board.filter((cell) => cell === 'O')).toHaveLength(1);
  });

  it('detects player wins, computer wins, and draws', () => {
    expect(evaluate(['X', 'X', 'X', null, 'O', null, 'O', null, null])).toBe('playerWon');
    expect(evaluate(['O', 'X', 'X', 'O', 'X', null, 'O', null, null])).toBe('computerWon');
    expect(evaluate(['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'])).toBe('draw');
  });

  it('rejects moves after game end and in occupied cells', () => {
    const won = state(['X', 'X', 'X', null, 'O', null, 'O', null, null], 3);
    expect(placeAtCursor(won)).toBe(won);
    const occupied = state(['X', null, null, null, null, null, null, null, null], 0);
    expect(placeAtCursor(occupied)).toBe(occupied);
  });

  it('lets the computer win first and block the player second', () => {
    expect(chooseComputerMove(['O', 'O', null, 'X', 'X', null, null, null, null])).toBe(2);
    expect(chooseComputerMove(['X', 'X', null, 'O', null, null, null, null, null])).toBe(2);
  });

  it('uses deterministic cell order without a winning or blocking move', () => {
    expect(chooseComputerMove(Array(9).fill(null))).toBe(4);
    expect(chooseComputerMove([null, null, null, null, 'X', null, null, null, null])).toBe(0);
  });

  it('clears the board, result, and cursor on restart', () => {
    expect(reduceGame(state(['X', 'X', 'X', null, 'O', null, 'O', null, null], 8), { type: 'restart' })).toEqual(newGame());
  });

  it('reports a win, loss, or draw clearly at game end', () => {
    expect(resultText('playerWon')).toBe('You won!');
    expect(resultText('computerWon')).toBe('You lost!');
    expect(resultText('draw')).toBe('Draw!');
  });
});

describe('device event mapping and cursor', () => {
  it('maps single to place and double to restart while nod and shake do nothing', () => {
    expect(mapDeviceEvent({ name: 'device.button', data: { action: 'single' } })).toEqual({ type: 'place' });
    expect(mapDeviceEvent({ name: 'device.button', data: { action: 'double' } })).toEqual({ type: 'restart' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'nod', active: true } })).toBeNull();
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'shake', active: true } })).toBeNull();
  });

  it('maps look-up, look-down, matching timeouts, and head turns to two-dimensional directions', () => {
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headRaise', active: true } })).toEqual({ type: 'up' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headRaiseTimeout', active: true } })).toEqual({ type: 'up' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headLower', active: true } })).toEqual({ type: 'down' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headLowerTimeout', active: true } })).toEqual({ type: 'down' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'left', active: true } })).toEqual({ type: 'left' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'right', active: true } })).toEqual({ type: 'right' });
  });

  it('ignores inactive and unknown events', () => {
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headRaise', active: false } })).toBeNull();
    expect(mapDeviceEvent({ name: 'device.button', data: { action: 'release' } })).toBeNull();
  });

  it('moves the cursor in four directions within its row and column', () => {
    const current = newGame();
    expect(moveCursor(current, 'left').cursor).toBe(3);
    expect(moveCursor(current, 'right').cursor).toBe(5);
    expect(moveCursor(current, 'up').cursor).toBe(1);
    expect(moveCursor(current, 'down').cursor).toBe(7);
  });

  it('debounces the same direction for 300 ms, executes another direction immediately, and does not extend the window for ignored packets', () => {
    let now = 1000;
    const filter = new DeviceInputFilter(() => now, 300, 600);
    const right = { name: 'device.imuGesture', data: { gesture: 'right', active: true } };
    const left = { name: 'device.imuGesture', data: { gesture: 'left', active: true } };

    expect(filter.map(right)).toEqual({ type: 'right' });
    now = 1100;
    expect(filter.map(right)).toBeNull();
    now = 1250;
    expect(filter.map(right)).toBeNull();
    now = 1299;
    expect(filter.map(left)).toEqual({ type: 'left' });
    now = 1300;
    expect(filter.map(right)).toEqual({ type: 'right' });
  });

  it('filters opposite vertical events within 600 ms as recentering and executes outside the window', () => {
    let now = 1000;
    const filter = new DeviceInputFilter(() => now, 300, 600);
    const lower = { name: 'device.imuGesture', data: { gesture: 'headLower', active: true } };
    const raise = { name: 'device.imuGesture', data: { gesture: 'headRaise', active: true } };

    expect(filter.decide(lower)).toEqual({ action: { type: 'down' }, reason: 'accepted' });
    now = 1599;
    expect(filter.decide(raise)).toEqual({ action: null, reason: 'verticalRebound' });
    now = 1600;
    expect(filter.decide(raise)).toEqual({ action: { type: 'up' }, reason: 'accepted' });
  });

  it('uses a timeout only when the matching regular event is missing', () => {
    let now = 1000;
    const filter = new DeviceInputFilter(() => now, 300, 600, 10000);
    const lower = { name: 'device.imuGesture', data: { gesture: 'headLower', active: true } };

    expect(filter.decide(lower)).toEqual({ action: { type: 'down' }, reason: 'accepted' });
    now = 6000;
    expect(filter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headLowerTimeout', active: true },
    })).toEqual({ action: null, reason: 'timeoutAlreadySeen' });

    const missingNormalFilter = new DeviceInputFilter(() => now, 300, 600, 10000);
    expect(missingNormalFilter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headLowerTimeout', active: true },
    })).toEqual({ action: { type: 'down' }, reason: 'accepted' });
  });

  it('lets a regular event classified as recentering prevent duplicate movement from a later timeout', () => {
    let now = 1000;
    const filter = new DeviceInputFilter(() => now, 300, 600, 10000);
    expect(filter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headRaise', active: true },
    })).toEqual({ action: { type: 'up' }, reason: 'accepted' });
    now = 1200;
    expect(filter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headLower', active: true },
    })).toEqual({ action: null, reason: 'verticalRebound' });
    now = 6200;
    expect(filter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headLowerTimeout', active: true },
    })).toEqual({ action: null, reason: 'timeoutAlreadySeen' });
  });

  it('continues to filter inactive events in input diagnostics', () => {
    const filter = new DeviceInputFilter();
    expect(filter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headLower', active: false },
    })).toEqual({ action: null, reason: 'inactive' });
  });

  it('cycles on the same axis and skips occupied cells without falling back to other rows or columns', () => {
    const horizontal = state([null, null, null, 'X', null, null, null, null, null], 4);
    expect(moveCursor(horizontal, 'left').cursor).toBe(5);
    const vertical = state([null, 'O', null, null, null, null, null, null, null], 4);
    expect(moveCursor(vertical, 'up').cursor).toBe(7);
    const blockedRow = state([null, null, null, 'X', null, 'O', null, null, null], 4);
    expect(moveCursor(blockedRow, 'left')).toBe(blockedRow);
  });

  it('does not move the cursor when no cells are empty or the game is over', () => {
    const draw = state(['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'], 8);
    expect(moveCursor(draw, 'right')).toBe(draw);
  });

  it('selects the first later empty cell after player and computer moves', () => {
    expect(placeAtCursor(newGame()).cursor).toBe(5);
  });
});

describe('saved-game restoration', () => {
  it('restores a valid v1 save', () => {
    const saved = state(['X', 'O', null, null, null, null, null, null, null], 2);
    expect(parseStoredGame(JSON.stringify(saved))).toEqual(saved);
  });

  it('falls back to a new game for invalid JSON, version, board, or result', () => {
    expect(parseStoredGame('{')).toEqual(newGame());
    expect(parseStoredGame(JSON.stringify({ ...newGame(), version: 2 }))).toEqual(newGame());
    expect(parseStoredGame(JSON.stringify({ ...newGame(), board: ['Z'] }))).toEqual(newGame());
    expect(parseStoredGame(JSON.stringify({ ...newGame(), result: 'playerWon' }))).toEqual(newGame());
  });
});
