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
  it('installs global bootstrap, resolve, emit, and heartbeat entry points', () => {
    const { target } = setup();
    expect(target.__memoPluginBootstrap).toBeTypeOf('function');
    expect(target.__memoPluginResolve).toBeTypeOf('function');
    expect(target.__memoPluginEmit).toBeTypeOf('function');
    expect(target.__memoPluginHeartbeat?.()).toBe(false);
    target.__memoPluginBootstrap?.('token-a', 1);
    expect(target.__memoPluginHeartbeat?.()).toBe(true);
  });

  it('sends a complete request and resolves with result', async () => {
    const { target, messages, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 3);
    const promise = client.call<{ ready: boolean }>('runtime.ready');
    expect(messages[0]).toMatchObject({
      version: '2.0', sessionToken: 'token-a', method: 'runtime.ready', params: {}, runtimeGeneration: 3,
    });
    const requestId = messages[0].requestId as string;
    target.__memoPluginResolve?.({ requestId, ok: true, result: { ready: true }, runtimeGeneration: 3 });
    await expect(promise).resolves.toEqual({ ready: true });
  });

  it('rejects with the error code and message', async () => {
    const { target, messages, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 1);
    const promise = client.call('display.updateText', { text: 'English' });
    target.__memoPluginResolve?.({
      requestId: messages[0].requestId as string,
      ok: false,
      error: { code: 'DEVICE_DISCONNECTED', message: 'Device disconnected' },
      runtimeGeneration: 1,
    });
    await expect(promise).rejects.toMatchObject({ code: 'DEVICE_DISCONNECTED', message: 'Device disconnected' });
  });

  it('times out after seven seconds and clears pending so a late response is ignored', async () => {
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

  it('rejects old pending calls on generation change and drops old responses', async () => {
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

  it('dispatches device events by name and drops old generations', () => {
    const { target, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 2);
    const listener = vi.fn();
    client.on('device.button', listener);
    target.__memoPluginEmit?.({ name: 'device.button', subscriptionIds: ['sub-1'], data: { action: 'single' }, runtimeGeneration: 1 });
    target.__memoPluginEmit?.({ name: 'device.button', subscriptionIds: ['sub-1'], data: { action: 'single' }, runtimeGeneration: 2 });
    expect(listener).toHaveBeenCalledTimes(1);
  });

  it('dispatches lifecycle events even with empty subscriptionIds', () => {
    const { target, client } = setup();
    target.__memoPluginBootstrap?.('token-a', 1);
    const listener = vi.fn();
    client.on('runtime.lifecycleChanged', listener);
    target.__memoPluginEmit?.({
      name: 'runtime.lifecycleChanged', subscriptionIds: [], data: { state: 'running' }, runtimeGeneration: 1,
    });
    expect(listener).toHaveBeenCalledOnce();
  });

  it('correlates event subscription, unsubscription, and local listener cleanup', async () => {
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

  it('fails explicitly when not configured', async () => {
    const { client } = setup();
    await expect(client.call('runtime.ready')).rejects.toEqual(
      new MemoBridgeError('BRIDGE_UNAVAILABLE', 'App Bridge is not ready'),
    );
  });
});
