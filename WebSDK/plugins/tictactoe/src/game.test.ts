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

describe('井字棋领域逻辑', () => {
  it('玩家执 X 先手且默认选择中间', () => {
    expect(newGame()).toEqual({ version: 1, board: Array(9).fill(null), cursor: 4, result: 'playing' });
  });

  it('玩家合法落子后电脑至多落一步', () => {
    const next = placeAtCursor(newGame());
    expect(next.board.filter((cell) => cell === 'X')).toHaveLength(1);
    expect(next.board.filter((cell) => cell === 'O')).toHaveLength(1);
  });

  it('识别玩家胜利、电脑胜利和平局', () => {
    expect(evaluate(['X', 'X', 'X', null, 'O', null, 'O', null, null])).toBe('playerWon');
    expect(evaluate(['O', 'X', 'X', 'O', 'X', null, 'O', null, null])).toBe('computerWon');
    expect(evaluate(['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'])).toBe('draw');
  });

  it('终局和已占用格拒绝继续落子', () => {
    const won = state(['X', 'X', 'X', null, 'O', null, 'O', null, null], 3);
    expect(placeAtCursor(won)).toBe(won);
    const occupied = state(['X', null, null, null, null, null, null, null, null], 0);
    expect(placeAtCursor(occupied)).toBe(occupied);
  });

  it('电脑优先取胜，其次拦截玩家', () => {
    expect(chooseComputerMove(['O', 'O', null, 'X', 'X', null, null, null, null])).toBe(2);
    expect(chooseComputerMove(['X', 'X', null, 'O', null, null, null, null, null])).toBe(2);
  });

  it('没有胜招或拦截时使用确定性格序', () => {
    expect(chooseComputerMove(Array(9).fill(null))).toBe(4);
    expect(chooseComputerMove([null, null, null, null, 'X', null, null, null, null])).toBe(0);
  });

  it('重新开始清空棋盘、结果和光标', () => {
    expect(reduceGame(state(['X', 'X', 'X', null, 'O', null, 'O', null, null], 8), { type: 'restart' })).toEqual(newGame());
  });

  it('终局明确提示你赢了、你输了或打平了', () => {
    expect(resultText('playerWon')).toBe('你赢了！');
    expect(resultText('computerWon')).toBe('你输了！');
    expect(resultText('draw')).toBe('打平了！');
  });
});

describe('设备事件映射与光标', () => {
  it('single 落子，double 重开，nod 与 shake 不再触发动作', () => {
    expect(mapDeviceEvent({ name: 'device.button', data: { action: 'single' } })).toEqual({ type: 'place' });
    expect(mapDeviceEvent({ name: 'device.button', data: { action: 'double' } })).toEqual({ type: 'restart' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'nod', active: true } })).toBeNull();
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'shake', active: true } })).toBeNull();
  });

  it('抬头低头、对应 timeout 和左右转头映射到二维方向', () => {
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headRaise', active: true } })).toEqual({ type: 'up' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headRaiseTimeout', active: true } })).toEqual({ type: 'up' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headLower', active: true } })).toEqual({ type: 'down' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headLowerTimeout', active: true } })).toEqual({ type: 'down' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'left', active: true } })).toEqual({ type: 'left' });
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'right', active: true } })).toEqual({ type: 'right' });
  });

  it('active=false 和未知事件不触发动作', () => {
    expect(mapDeviceEvent({ name: 'device.imuGesture', data: { gesture: 'headRaise', active: false } })).toBeNull();
    expect(mapDeviceEvent({ name: 'device.button', data: { action: 'release' } })).toBeNull();
  });

  it('光标在同行同列按四个方向移动', () => {
    const current = newGame();
    expect(moveCursor(current, 'left').cursor).toBe(3);
    expect(moveCursor(current, 'right').cursor).toBe(5);
    expect(moveCursor(current, 'up').cursor).toBe(1);
    expect(moveCursor(current, 'down').cursor).toBe(7);
  });

  it('同方向 300ms 固定防抖，不同方向立即执行且忽略包不延长窗口', () => {
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

  it('600ms 内上下反向事件按回正过滤，窗口外正常执行', () => {
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

  it('timeout 仅在对应普通事件缺失时补偿执行', () => {
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

  it('被判定为回正的普通事件也能阻止后续 timeout 重复移动', () => {
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

  it('输入诊断仍过滤未激活事件', () => {
    const filter = new DeviceInputFilter();
    expect(filter.decide({
      name: 'device.imuGesture',
      data: { gesture: 'headLower', active: false },
    })).toEqual({ action: null, reason: 'inactive' });
  });

  it('同轴循环并跳过已占用格，不回退其它行列', () => {
    const horizontal = state([null, null, null, 'X', null, null, null, null, null], 4);
    expect(moveCursor(horizontal, 'left').cursor).toBe(5);
    const vertical = state([null, 'O', null, null, null, null, null, null, null], 4);
    expect(moveCursor(vertical, 'up').cursor).toBe(7);
    const blockedRow = state([null, null, null, 'X', null, 'O', null, null, null], 4);
    expect(moveCursor(blockedRow, 'left')).toBe(blockedRow);
  });

  it('没有空格或已经终局时光标不移动', () => {
    const draw = state(['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'], 8);
    expect(moveCursor(draw, 'right')).toBe(draw);
  });

  it('玩家和电脑落子后从原位置向后选择第一个空格', () => {
    expect(placeAtCursor(newGame()).cursor).toBe(5);
  });
});

describe('存档恢复', () => {
  it('恢复合法 v1 存档', () => {
    const saved = state(['X', 'O', null, null, null, null, null, null, null], 2);
    expect(parseStoredGame(JSON.stringify(saved))).toEqual(saved);
  });

  it('非法 JSON、版本、棋盘和结果回退新局', () => {
    expect(parseStoredGame('{')).toEqual(newGame());
    expect(parseStoredGame(JSON.stringify({ ...newGame(), version: 2 }))).toEqual(newGame());
    expect(parseStoredGame(JSON.stringify({ ...newGame(), board: ['Z'] }))).toEqual(newGame());
    expect(parseStoredGame(JSON.stringify({ ...newGame(), result: 'playerWon' }))).toEqual(newGame());
  });
});
