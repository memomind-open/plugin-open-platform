import type { GameState } from './game';
import { parseStoredGame } from './game';

interface StorageBridge {
  call<T = unknown>(method: string, params?: Record<string, unknown>): Promise<T>;
}

export class GamePersistence {
  private saveChain: Promise<void> = Promise.resolve();

  constructor(
    private readonly bridge: StorageBridge,
    private readonly key: string,
  ) {}

  async load(): Promise<GameState> {
    const stored = await this.bridge.call<{ value: unknown }>('storage.get', { key: this.key });
    return parseStoredGame(stored.value);
  }

  save(state: GameState): Promise<void> {
    const value = JSON.stringify(state);
    const operation = async (): Promise<void> => {
      await this.bridge.call('storage.set', { key: this.key, value });
    };
    const next = this.saveChain.then(operation, operation);
    this.saveChain = next.catch(() => undefined);
    return next;
  }
}
