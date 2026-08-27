import { describe, expect, it, vi } from 'vitest';
import {
  DevicePresenter,
  PresentationGeneration,
  renderDeviceText,
  shouldUseLz4,
} from './device';
import { newGame, type GameState } from './game';

function stateAt(cursor: number): GameState {
  return { ...newGame(), cursor };
}

function deferred(): { promise: Promise<unknown>; resolve: () => void; reject: (error: Error) => void } {
  let resolve!: () => void;
  let reject!: (error: Error) => void;
  const promise = new Promise<unknown>((onResolve, onReject) => {
    resolve = () => onResolve({});
    reject = onReject;
  });
  return { promise, resolve, reject };
}

async function tick(): Promise<void> {
  await new Promise((resolve) => setTimeout(resolve, 0));
}

describe('device rendering', () => {
  it('renders the complete rules and current status in the left pane', () => {
    const text = renderDeviceText(newGame());
    expect(text).toContain('TIC-TAC-TOE');
    expect(text).toContain('You are X; computer is O; you move first');
    expect(text).toContain('Goal: connect three in any direction');
    expect(text).toContain('Look up / down: move up / down');
    expect(text).toContain('Turn head left / right: move left / right');
    expect(text).toContain('Single click: place mark');
    expect(text).toContain('Double click: restart');
    expect(text).toContain('The glowing frame is the selected cell');
    expect(text).toContain('Selected: center');
  });

  it.each([
    ['playerWon', ['X', 'X', 'X', 'O', 'O', null, null, null, null], 'You won!'],
    ['computerWon', ['O', 'X', 'X', 'O', 'X', null, 'O', null, null], 'You lost!'],
    ['draw', ['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'], 'Draw!'],
  ] as const)('shows a clear %s result in the device left pane', (result, board, message) => {
    const text = renderDeviceText({ version: 1, board: [...board], cursor: 8, result });
    const lines = text.split('\n');
    expect(lines[0]).toBe(`【${message}】`);
    expect(lines[1]).toBe('Double-click the main button to restart');
  });

  it('updates the rules before sending one complete LZ4 board as the final commit', async () => {
    const calls: Array<{ method: string; params?: Record<string, unknown> }> = [];
    const bridge = {
      call: vi.fn(async (method: string, params?: Record<string, unknown>) => {
        calls.push({ method, params });
        return {};
      }),
    };
    await new DevicePresenter(bridge).present(newGame());
    expect(calls.map((call) => call.method)).toEqual([
      'display.updateText',
      'display.updateImageLz4',
    ]);
    expect(calls[0].params).toMatchObject({ id: 1, x: 10, y: 10, width: 278, height: 268 });
    expect(calls[1].params).toMatchObject({
      x: 304,
      y: 16,
      width: 256,
      height: 256,
      stride: 128,
      decodedSize: 32768,
    });
  });

  it('selects the compressed channel using the full protocol payload length', () => {
    expect(shouldUseLz4(100, 95)).toBe(true);
    expect(shouldUseLz4(100, 96)).toBe(false);
    expect(shouldUseLz4(100, 100)).toBe(false);
  });

  it('sends only in-flight A and latest C for A/B/C and shares the drain Promise', async () => {
    const images: Array<Record<string, unknown>> = [];
    const gates: ReturnType<typeof deferred>[] = [];
    const bridge = {
      call: vi.fn((method: string, params?: Record<string, unknown>) => {
        if (!method.includes('updateImage')) return Promise.resolve({});
        images.push(params ?? {});
        const gate = deferred();
        gates.push(gate);
        return gate.promise;
      }),
    };
    const presenter = new DevicePresenter(bridge);
    const first = presenter.present(stateAt(0));
    await tick();
    const second = presenter.present(stateAt(1));
    const third = presenter.present(stateAt(2));
    expect(second).toBe(first);
    expect(third).toBe(first);
    gates[0].resolve();
    await tick();
    expect(images).toHaveLength(2);
    gates[1].resolve();
    await expect(first).resolves.toBeUndefined();
  });

  it('drops intermediate B for A/B/A and does not resend A after success', async () => {
    const gates: ReturnType<typeof deferred>[] = [];
    const imageCall = vi.fn(() => {
      const gate = deferred();
      gates.push(gate);
      return gate.promise;
    });
    const bridge = {
      call: vi.fn((method: string) => method.includes('updateImage') ? imageCall() : Promise.resolve({})),
    };
    const presenter = new DevicePresenter(bridge);
    const stateA = stateAt(0);
    const done = presenter.present(stateA);
    await tick();
    presenter.present(stateAt(1));
    presenter.present(stateA);
    gates[0].resolve();
    await expect(done).resolves.toBeUndefined();
    expect(imageCall).toHaveBeenCalledTimes(1);
  });

  it('continues with the latest frame after an in-flight failure and resolves drain when it succeeds', async () => {
    const gates: ReturnType<typeof deferred>[] = [];
    const bridge = {
      call: vi.fn((method: string) => {
        if (!method.includes('updateImage')) return Promise.resolve({});
        const gate = deferred();
        gates.push(gate);
        return gate.promise;
      }),
    };
    const presenter = new DevicePresenter(bridge);
    const done = presenter.present(stateAt(0));
    await tick();
    presenter.present(stateAt(1));
    gates[0].reject(new Error('timeout'));
    await tick();
    gates[1].resolve();
    await expect(done).resolves.toBeUndefined();
  });

  it('rejects a failure without an update and allows an explicit retry of the same state', async () => {
    let fail = true;
    const bridge = {
      call: vi.fn(async (method: string) => {
        if (method.includes('updateImage') && fail) throw new Error('operation timed out');
        return {};
      }),
    };
    const presenter = new DevicePresenter(bridge);
    await expect(presenter.present(newGame())).rejects.toThrow('board transfer failed: operation timed out');
    fail = false;
    await expect(presenter.present(newGame())).resolves.toBeUndefined();
  });

  it('fully resends successful A when requested again after a partial B failure', async () => {
    let failImage = false;
    const bridge = {
      call: vi.fn(async (method: string) => {
        if (method.includes('updateImage') && failImage) throw new Error('timeout');
        return {};
      }),
    };
    const presenter = new DevicePresenter(bridge);
    const stateA = stateAt(0);
    await presenter.present(stateA);
    failImage = true;
    await expect(presenter.present(stateAt(1))).rejects.toThrow('timeout');
    failImage = false;
    await presenter.present(stateA);
    expect(bridge.call).toHaveBeenCalledTimes(6);
  });

  it('invalidates the in-flight draw generation immediately on disconnect', () => {
    const generations = new PresentationGeneration();
    const inFlight = generations.begin();
    expect(generations.isCurrent(inFlight)).toBe(true);
    generations.invalidate();
    expect(generations.isCurrent(inFlight)).toBe(false);
  });
});
