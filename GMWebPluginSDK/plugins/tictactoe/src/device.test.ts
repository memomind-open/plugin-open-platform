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

describe('设备中文绘制', () => {
  it('左侧输出完整中文规则和当前状态', () => {
    const text = renderDeviceText(newGame());
    expect(text).toContain('井字棋');
    expect(text).toContain('你是 X，电脑是 O，你先手');
    expect(text).toContain('目标：横、竖或斜线连成三个');
    expect(text).toContain('抬头 / 低头：上 / 下');
    expect(text).toContain('向左转头 / 向右转头：左 / 右');
    expect(text).toContain('单击：确认落子');
    expect(text).toContain('双击：重新开始');
    expect(text).toContain('发亮方框是当前选中格');
    expect(text).toContain('已选择：中间');
  });

  it.each([
    ['playerWon', ['X', 'X', 'X', 'O', 'O', null, null, null, null], '你赢了！'],
    ['computerWon', ['O', 'X', 'X', 'O', 'X', null, 'O', null, null], '你输了！'],
    ['draw', ['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'], '打平了！'],
  ] as const)('终局 %s 在设备左侧明确提示结果', (result, board, message) => {
    const text = renderDeviceText({ version: 1, board: [...board], cursor: 8, result });
    const lines = text.split('\n');
    expect(lines[0]).toBe(`【${message}】`);
    expect(lines[1]).toBe('双击主按钮，重新开始');
  });

  it('先更新规则，再发送一张完整 LZ4 棋盘作为最终提交', async () => {
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

  it('按完整协议 payload 长度选择压缩通道', () => {
    expect(shouldUseLz4(100, 95)).toBe(true);
    expect(shouldUseLz4(100, 96)).toBe(false);
    expect(shouldUseLz4(100, 100)).toBe(false);
  });

  it('A/B/C 只发送在途 A 和最新 C，且共享 drain Promise', async () => {
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

  it('A/B/A 清除中间 B，A 成功后不重复发送', async () => {
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

  it('在途失败但有更新时继续最新帧，最新成功则 drain 成功', async () => {
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

  it('无更新的失败会拒绝，且同状态可显式重试', async () => {
    let fail = true;
    const bridge = {
      call: vi.fn(async (method: string) => {
        if (method.includes('updateImage') && fail) throw new Error('operation timed out');
        return {};
      }),
    };
    const presenter = new DevicePresenter(bridge);
    await expect(presenter.present(newGame())).rejects.toThrow('棋盘发送失败：operation timed out');
    fail = false;
    await expect(presenter.present(newGame())).resolves.toBeUndefined();
  });

  it('B 部分绘制失败后再次请求已成功的 A 仍会完整重发', async () => {
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

  it('断连使在途绘制 generation 立即失效', () => {
    const generations = new PresentationGeneration();
    const inFlight = generations.begin();
    expect(generations.isCurrent(inFlight)).toBe(true);
    generations.invalidate();
    expect(generations.isCurrent(inFlight)).toBe(false);
  });
});
