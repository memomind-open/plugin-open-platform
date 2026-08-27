import { describe, expect, it } from 'vitest';
import { MemoBridgeClient, type BridgeGlobalTarget } from './bridge';
import { newGame, parseStoredGame, placeAtCursor } from './game';

describe('plugin minimum happy path', () => {
  it('places a mark, receives a computer response, saves, and restores in a new Runtime after bootstrap', async () => {
    const storage = new Map<string, unknown>();
    const target: BridgeGlobalTarget = {};
    target.MemoPluginBridge = {
      postMessage(raw) {
        const request = JSON.parse(raw) as {
          requestId: string;
          method: string;
          params: Record<string, unknown>;
          runtimeGeneration: number;
        };
        let result: unknown = {};
        if (request.method === 'storage.get') {
          result = { value: storage.get(request.params.key as string) ?? null };
        }
        if (request.method === 'storage.set') {
          storage.set(request.params.key as string, request.params.value);
          result = { stored: true };
        }
        queueMicrotask(() => target.__memoPluginResolve?.({
          requestId: request.requestId,
          ok: true,
          result,
          runtimeGeneration: request.runtimeGeneration,
        }));
      },
    };

    const firstRuntime = new MemoBridgeClient(target);
    firstRuntime.install();
    target.__memoPluginBootstrap?.('first-token', 1);
    await firstRuntime.call('runtime.ready');

    const moved = placeAtCursor(newGame());
    expect(moved.board.filter((cell) => cell === 'X')).toHaveLength(1);
    expect(moved.board.filter((cell) => cell === 'O')).toHaveLength(1);
    await firstRuntime.call('storage.set', {
      key: 'game-state-v1',
      value: JSON.stringify(moved),
    });

    const secondRuntime = new MemoBridgeClient(target);
    secondRuntime.install();
    target.__memoPluginBootstrap?.('second-token', 2);
    const saved = await secondRuntime.call<{ value: unknown }>('storage.get', { key: 'game-state-v1' });
    expect(parseStoredGame(saved.value)).toEqual(moved);
  });
});
