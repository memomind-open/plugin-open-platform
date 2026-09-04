(() => {
  if (globalThis.MemoPluginBridge?.postMessage) return;

  let bootstrap;
  let bootstrapHandler;

  Object.defineProperty(globalThis, '__memoPluginBootstrap', {
    configurable: true,
    get: () => bootstrapHandler,
    set: (handler) => {
      bootstrapHandler = handler;
      deliverBootstrap();
    },
  });

  globalThis.MemoPluginBridge = {
    postMessage(payload) {
      let request;
      try {
        request = typeof payload === 'string' ? JSON.parse(payload) : payload;
      } catch {
        throw new TypeError('MemoPluginBridge payload must be valid JSON');
      }
      globalThis.parent.postMessage({ type: 'gm-plugin:request', request }, '*');
    },
  };

  globalThis.addEventListener('message', (message) => {
    if (message.source !== globalThis.parent) return;
    const envelope = message.data;
    if (!envelope || typeof envelope !== 'object') return;

    if (envelope.type === 'gm-plugin:bootstrap') {
      bootstrap = envelope.bootstrap;
      deliverBootstrap();
    } else if (envelope.type === 'gm-plugin:response') {
      globalThis.__memoPluginResolve?.(envelope.response);
    } else if (envelope.type === 'gm-plugin:event') {
      globalThis.__memoPluginEmit?.(envelope.event);
    }
  });

  globalThis.parent.postMessage({ type: 'gm-plugin:bootstrap-request' }, '*');

  function deliverBootstrap() {
    if (!bootstrap || typeof bootstrapHandler !== 'function') return;
    const handler = bootstrapHandler;
    const value = bootstrap;
    bootstrap = undefined;
    queueMicrotask(() => handler(value.sessionToken, value.runtimeGeneration));
  }
})();
