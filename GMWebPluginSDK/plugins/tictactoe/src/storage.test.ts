import { describe, expect, it } from 'vitest';
import { newGame, placeAtCursor } from './game';
import { GamePersistence } from './storage';

describe('井字棋存档队列', () => {
  it('按动作顺序串行写入，较旧状态不会后完成', async () => {
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

  it('加载时解析 App storage 返回值', async () => {
    const saved = placeAtCursor(newGame());
    const bridge = {
      call: async <T>(): Promise<T> => ({ value: JSON.stringify(saved) }) as T,
    };
    await expect(new GamePersistence(bridge, 'game-state-v1').load()).resolves.toEqual(saved);
  });
});
