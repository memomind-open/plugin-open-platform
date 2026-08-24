import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  MemoBridgeClient,
  MemoBridgeError,
  type BridgeGlobalTarget,
} from './bridge';

function setup(timeoutMs = 7000): {
  target: BridgeGlobalTarget;
  messages: Array<Record<string, unknown>>;
  client: MemoBridgeClient;
} {
  const messages: Array<Record<string, unknown>> = [];
  const target: BridgeGlobalTarget = {
    MemoPluginBridge: {
      postMessage(message) { messages.push(JSON.parse(message) as Record<string, unknown>); },
    },
  };
  const client = new MemoBridgeClient(target, timeoutMs);
  client.install();
  return { target, messages, client };
}

afterEach(() => vi.useRealTimers());

describe('Aphrodite Bridge v1 SDK', () => {
  it('安装 bootstrap、resolve、emit 与 heartbeat 全局入口', () => {
    const { target } = setup();
    expect(target.__memoPluginBootstrap).toBeTypeOf('function');
    expect(target.__memoPluginResolve).toBeTypeOf('function');
    expect(target.__memoPluginEmit).toBeTypeOf('function');
    expect(target.__memoPluginHeartbeat?.()).toBe(false);
    target.__memoPluginBootstrap?.('token-a', 1);
    expect(target.__memoPluginHeartbeat?.()).toBe(true);
  });

  it('发送完整请求并使用 result resolve', async () => {
    const { target, messages, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 3);
    const promise = client.call<{ ready: boolean }>('runtime.ready');
    expect(messages[0]).toMatchObject({
      version: '1.0', sessionToken: 'token-a', method: 'runtime.ready', params: {}, runtimeGeneration: 3,
    });
    const requestId = messages[0].requestId as string;
    target.__memoPluginResolve?.({ requestId, ok: true, result: { ready: true }, runtimeGeneration: 3 });
    await expect(promise).resolves.toEqual({ ready: true });
  });

  it('使用 error code 与 message reject', async () => {
    const { target, messages, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 1);
    const promise = client.call('display.updateText', { text: '中文' });
    target.__memoPluginResolve?.({
      requestId: messages[0].requestId as string,
      ok: false,
      error: { code: 'DEVICE_DISCONNECTED', message: '设备已断开' },
      runtimeGeneration: 1,
    });
    await expect(promise).rejects.toMatchObject({ code: 'DEVICE_DISCONNECTED', message: '设备已断开' });
  });

  it('7 秒超时并清理 pending，使迟到响应无效', async () => {
    vi.useFakeTimers();
    const { target, messages, client } = setup(7000);
    target.__memoPluginBootstrap?.('token-a', 1);
    const promise = client.call('runtime.ping');
    const rejection = expect(promise).rejects.toMatchObject({ code: 'TIMEOUT' });
    await vi.advanceTimersByTimeAsync(7000);
    await rejection;
    expect(() => target.__memoPluginResolve?.({
      requestId: messages[0].requestId as string, ok: true, result: {}, runtimeGeneration: 1,
    })).not.toThrow();
  });

  it('generation 切换会拒绝旧 pending，旧响应被丢弃', async () => {
    const { target, messages, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 1);
    const oldPromise = client.call('runtime.ping');
    target.__memoPluginBootstrap?.('token-b', 2);
    await expect(oldPromise).rejects.toMatchObject({ code: 'STALE_RUNTIME' });
    target.__memoPluginResolve?.({
      requestId: messages[0].requestId as string, ok: true, result: {}, runtimeGeneration: 1,
    });
    expect(target.__memoPluginHeartbeat?.()).toBe(true);
  });

  it('按事件名分发设备事件并丢弃旧 generation', () => {
    const { target, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 2);
    const listener = vi.fn();
    client.on('device.button', listener);
    target.__memoPluginEmit?.({ name: 'device.button', subscriptionIds: ['sub-1'], data: { action: 'single' }, runtimeGeneration: 1 });
    target.__memoPluginEmit?.({ name: 'device.button', subscriptionIds: ['sub-1'], data: { action: 'single' }, runtimeGeneration: 2 });
    expect(listener).toHaveBeenCalledTimes(1);
  });

  it('空 subscriptionIds 的 lifecycle 事件仍会分发', () => {
    const { target, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 1);
    const listener = vi.fn();
    client.on('runtime.lifecycleChanged', listener);
    target.__memoPluginEmit?.({
      name: 'runtime.lifecycleChanged', subscriptionIds: [], data: { state: 'running' }, runtimeGeneration: 1,
    });
    expect(listener).toHaveBeenCalledOnce();
  });

  it('事件订阅、取消订阅与本地 listener 清理均可关联', async () => {
    const { target, messages, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 1);
    const listener = vi.fn();
    const removeListener = client.on('device.button', listener);
    const subscribe = client.call<{ subscriptionId: string }>('device.subscribeEvents', {
      types: ['button', 'connection'],
    });
    target.__memoPluginResolve?.({
      requestId: messages[0].requestId as string,
      ok: true,
      result: { subscriptionId: 'sub-1' },
      runtimeGeneration: 1,
    });
    await expect(subscribe).resolves.toEqual({ subscriptionId: 'sub-1' });
    const unsubscribe = client.call('device.unsubscribeEvents', { subscriptionId: 'sub-1' });
    target.__memoPluginResolve?.({
      requestId: messages[1].requestId as string,
      ok: true,
      result: { removed: true },
      runtimeGeneration: 1,
    });
    await unsubscribe;
    removeListener();
    target.__memoPluginEmit?.({
      name: 'device.button', subscriptionIds: ['sub-1'], data: { action: 'single' }, runtimeGeneration: 1,
    });
    expect(listener).not.toHaveBeenCalled();
  });

  it('未配置时调用明确失败', async () => {
    const { client } = setup();
    await expect(client.call('runtime.ready')).rejects.toEqual(
      new MemoBridgeError('BRIDGE_UNAVAILABLE', 'App Bridge 尚未就绪'),
    );
  });
});
