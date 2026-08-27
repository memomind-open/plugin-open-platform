import { describe, expect, it } from 'vitest';
import { newGame, placeAtCursor } from './game';
import { GamePersistence } from './storage';

describe('tic-tac-toe save queue', () => {
  it('writes serially in action order so an older state cannot finish later', async () => {
    const calls: string[] = [];
    const resolvers: Array<() => void> = [];
    const bridge = {
      call: async <T>(method: string, params?: Record<string, unknown>): Promise<T> => {
        expect(method).toBe('storage.set');
        calls.push(params?.value as string);
        await new Promise<void>((resolve) => resolvers.push(resolve));
        return {} as T;
      },
    };
    const persistence = new GamePersistence(bridge, 'game-state-v1');
    const first = newGame();
    const second = placeAtCursor(first);

    const firstSave = persistence.save(first);
    const secondSave = persistence.save(second);
    await Promise.resolve();
    expect(calls).toEqual([JSON.stringify(first)]);

    resolvers.shift()?.();
    await firstSave;
    await Promise.resolve();
    expect(calls).toEqual([JSON.stringify(first), JSON.stringify(second)]);

    resolvers.shift()?.();
    await secondSave;
  });

  it('parses the App storage value when loading', async () => {
    const saved = placeAtCursor(newGame());
    const bridge = {
      call: async <T>(): Promise<T> => ({ value: JSON.stringify(saved) }) as T,
    };
    await expect(new GamePersistence(bridge, 'game-state-v1').load()).resolves.toEqual(saved);
  });
});
