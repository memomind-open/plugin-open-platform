export type Cell = 'X' | 'O' | null;
export type GameResult = 'playing' | 'playerWon' | 'computerWon' | 'draw';

export interface GameState {
  version: 1;
  board: Cell[];
  cursor: number;
  result: GameResult;
}

export type GameAction =
  | { type: 'up' }
  | { type: 'down' }
  | { type: 'left' }
  | { type: 'right' }
  | { type: 'place' }
  | { type: 'restart' };

export type CursorDirection = 'up' | 'down' | 'left' | 'right';

export interface DeviceInputEvent {
  name: string;
  data: Record<string, unknown>;
}

export type DeviceInputDecisionReason =
  | 'accepted'
  | 'debounced'
  | 'verticalRebound'
  | 'timeoutAlreadySeen'
  | 'inactive'
  | 'unsupported';

export interface DeviceInputDecision {
  action: GameAction | null;
  reason: DeviceInputDecisionReason;
}

export class DeviceInputFilter {
  private readonly lastAcceptedAt = new Map<CursorDirection, number>();
  private readonly lastNormalVerticalSeenAt = new Map<'up' | 'down', number>();
  private lastAcceptedVertical: 'up' | 'down' | null = null;
  private lastAcceptedVerticalAt = 0;

  constructor(
    private readonly nowMs: () => number = Date.now,
    private readonly debouncePeriodMs = 300,
    private readonly verticalReboundPeriodMs = 600,
    private readonly timeoutPairPeriodMs = 10000,
  ) {}

  map(event: DeviceInputEvent): GameAction | null {
    return this.decide(event).action;
  }

  decide(event: DeviceInputEvent): DeviceInputDecision {
    const action = mapDeviceEvent(event);
    if (!action) {
      return { action: null, reason: ignoredEventReason(event) };
    }
    if (isDirectionAction(action)) {
      const now = this.nowMs();
      const vertical = verticalDirection(action);
      if (vertical && isNormalVerticalGesture(event)) {
        this.lastNormalVerticalSeenAt.set(vertical, now);
      }
      if (vertical && isVerticalTimeoutGesture(event)) {
        const normalSeenAt = this.lastNormalVerticalSeenAt.get(vertical);
        if (
          normalSeenAt !== undefined &&
          now - normalSeenAt <= this.timeoutPairPeriodMs
        ) {
          this.lastNormalVerticalSeenAt.delete(vertical);
          return { action: null, reason: 'timeoutAlreadySeen' };
        }
      }
      const previous = this.lastAcceptedAt.get(action.type);
      if (previous !== undefined && now - previous < this.debouncePeriodMs) {
        return { action: null, reason: 'debounced' };
      }
      if (
        vertical &&
        this.lastAcceptedVertical !== null &&
        vertical !== this.lastAcceptedVertical &&
        now - this.lastAcceptedVerticalAt < this.verticalReboundPeriodMs
      ) {
        return { action: null, reason: 'verticalRebound' };
      }
      this.lastAcceptedAt.set(action.type, now);
      if (vertical) {
        this.lastAcceptedVertical = vertical;
        this.lastAcceptedVerticalAt = now;
      }
    }
    return { action, reason: 'accepted' };
  }
}

const WINNING_LINES = [
  [0, 1, 2], [3, 4, 5], [6, 7, 8],
  [0, 3, 6], [1, 4, 7], [2, 5, 8],
  [0, 4, 8], [2, 4, 6],
] as const;

const COMPUTER_PREFERENCE = [4, 0, 2, 6, 8, 1, 3, 5, 7] as const;

export function newGame(): GameState {
  return {
    version: 1,
    board: Array<Cell>(9).fill(null),
    cursor: 4,
    result: 'playing',
  };
}

export function winner(board: Cell[]): Cell {
  const line = winningCells(board);
  return line ? board[line[0]] : null;
}

export function winningCells(board: Cell[]): readonly [number, number, number] | null {
  for (const line of WINNING_LINES) {
    const [a, b, c] = line;
    if (board[a] && board[a] === board[b] && board[a] === board[c]) return line;
  }
  return null;
}

export function evaluate(board: Cell[]): GameResult {
  const mark = winner(board);
  if (mark === 'X') return 'playerWon';
  if (mark === 'O') return 'computerWon';
  return board.every((cell) => cell !== null) ? 'draw' : 'playing';
}

export function chooseComputerMove(board: Cell[]): number | null {
  const empty = COMPUTER_PREFERENCE.filter((index) => board[index] === null);
  if (empty.length === 0) return null;
  for (const mark of ['O', 'X'] as const) {
    for (const index of empty) {
      const candidate = [...board];
      candidate[index] = mark;
      if (winner(candidate) === mark) return index;
    }
  }
  return empty[0] ?? null;
}

export function moveCursor(state: GameState, direction: CursorDirection): GameState {
  if (state.result !== 'playing' || state.board.every((cell) => cell !== null)) return state;
  const row = Math.floor(state.cursor / 3);
  const column = state.cursor % 3;
  for (let step = 1; step < 3; step += 1) {
    const targetRow = direction === 'up'
      ? (row - step + 3) % 3
      : direction === 'down'
        ? (row + step) % 3
        : row;
    const targetColumn = direction === 'left'
      ? (column - step + 3) % 3
      : direction === 'right'
        ? (column + step) % 3
        : column;
    const index = targetRow * 3 + targetColumn;
    if (state.board[index] === null) return { ...state, cursor: index };
  }
  return state;
}

export function placeAtCursor(state: GameState): GameState {
  if (state.result !== 'playing' || state.board[state.cursor] !== null) return state;
  const board = [...state.board];
  board[state.cursor] = 'X';
  let result = evaluate(board);
  if (result === 'playing') {
    const computerMove = chooseComputerMove(board);
    if (computerMove !== null) board[computerMove] = 'O';
    result = evaluate(board);
  }
  const nextState: GameState = { version: 1, board, cursor: state.cursor, result };
  return result === 'playing' ? selectNextEmpty(nextState) : nextState;
}

function selectNextEmpty(state: GameState): GameState {
  for (let step = 1; step <= state.board.length; step += 1) {
    const index = (state.cursor + step) % state.board.length;
    if (state.board[index] === null) return { ...state, cursor: index };
  }
  return state;
}

export function reduceGame(state: GameState, action: GameAction): GameState {
  switch (action.type) {
    case 'up':
    case 'down':
    case 'left':
    case 'right':
      return moveCursor(state, action.type);
    case 'place':
      return placeAtCursor(state);
    case 'restart':
      return newGame();
  }
}

export function mapDeviceEvent(event: DeviceInputEvent): GameAction | null {
  if (event.name === 'device.button') {
    if (event.data.action === 'single') return { type: 'place' };
    if (event.data.action === 'double') return { type: 'restart' };
    return null;
  }
  if (event.name !== 'device.imuGesture' || event.data.active !== true) return null;
  switch (event.data.gesture) {
    case 'headRaise':
    case 'headRaiseTimeout':
      return { type: 'up' };
    case 'headLower':
    case 'headLowerTimeout':
      return { type: 'down' };
    case 'left':
      return { type: 'left' };
    case 'right':
      return { type: 'right' };
    default:
      return null;
  }
}

function isDirectionAction(
  action: GameAction,
): action is { type: CursorDirection } {
  return ['up', 'down', 'left', 'right'].includes(action.type);
}

function verticalDirection(
  action: { type: CursorDirection },
): 'up' | 'down' | null {
  return action.type === 'up' || action.type === 'down' ? action.type : null;
}

function isNormalVerticalGesture(event: DeviceInputEvent): boolean {
  return event.name === 'device.imuGesture' &&
    (event.data.gesture === 'headRaise' || event.data.gesture === 'headLower');
}

function isVerticalTimeoutGesture(event: DeviceInputEvent): boolean {
  return event.name === 'device.imuGesture' &&
    (event.data.gesture === 'headRaiseTimeout' ||
      event.data.gesture === 'headLowerTimeout');
}

function ignoredEventReason(event: DeviceInputEvent): DeviceInputDecisionReason {
  if (event.name === 'device.imuGesture') {
    if (event.data.active !== true) return 'inactive';
  }
  return 'unsupported';
}

export function parseStoredGame(value: unknown): GameState {
  if (typeof value !== 'string') return newGame();
  try {
    const decoded: unknown = JSON.parse(value);
    if (!decoded || typeof decoded !== 'object') return newGame();
    const candidate = decoded as Partial<GameState>;
    if (candidate.version !== 1 ||
        !Array.isArray(candidate.board) ||
        candidate.board.length !== 9 ||
        candidate.board.some((cell) => cell !== null && cell !== 'X' && cell !== 'O') ||
        !Number.isInteger(candidate.cursor) ||
        (candidate.cursor ?? -1) < 0 ||
        (candidate.cursor ?? 9) > 8 ||
        !['playing', 'playerWon', 'computerWon', 'draw'].includes(candidate.result ?? '')) {
      return newGame();
    }
    const board = [...candidate.board] as Cell[];
    if (evaluate(board) !== candidate.result) return newGame();
    return { version: 1, board, cursor: candidate.cursor as number, result: candidate.result as GameResult };
  } catch {
    return newGame();
  }
}

export function resultText(result: GameResult): string {
  switch (result) {
    case 'playing': return 'Your turn';
    case 'playerWon': return 'You won!';
    case 'computerWon': return 'You lost!';
    case 'draw': return 'Draw!';
  }
}

export function positionName(index: number): string {
  return ['top left', 'top center', 'top right', 'middle left', 'center', 'middle right', 'bottom left', 'bottom center', 'bottom right'][index] ?? 'unknown';
}
