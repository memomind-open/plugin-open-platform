(() => {
  let token = '';
  let generation = 0;
  let sequence = 0;
  let seconds = 0;
  const pending = new Map();
  const log = (value) => {
    document.getElementById('log').textContent = `${new Date().toLocaleTimeString()} ${value}\n` + document.getElementById('log').textContent.slice(0, 1200);
  };
  const call = (method, params = {}) => new Promise((resolve, reject) => {
    const requestId = `req-${Date.now()}-${++sequence}`;
    pending.set(requestId, { resolve, reject });
    MemoPluginBridge.postMessage(JSON.stringify({
      version: '1.0', sessionToken: token, requestId, method, params,
      runtimeGeneration: generation
    }));
  });

  window.__memoPluginBootstrap = async (sessionToken, runtimeGeneration) => {
    token = sessionToken;
    generation = runtimeGeneration;
    document.getElementById('runtime').textContent = `Bridge v1 / Generation ${generation}`;
    try {
      await call('runtime.ready');
      const saved = await call('storage.get', { key: 'seconds' });
      seconds = Number(saved.value || 0);
      log('Runtime ready');
    } catch (error) { log(`启动失败：${error.message || error}`); }
  };
  window.__memoPluginResolve = (response) => {
    if (response.runtimeGeneration !== generation) return;
    const item = pending.get(response.requestId);
    if (!item) return;
    pending.delete(response.requestId);
    response.ok ? item.resolve(response.result) : item.reject(response.error);
  };
  window.__memoPluginEmit = (event) => {
    if (event.runtimeGeneration !== generation) return;
    log(`${event.name}: ${JSON.stringify(event.data)}`);
    if (event.name === 'device.button' && event.data.action === 'double') seconds += 10;
  };
  window.__memoPluginHeartbeat = () => true;

  setInterval(() => {
    seconds += 1;
    document.getElementById('counter').textContent = String(seconds);
  }, 1000);
  document.getElementById('subscribe').onclick = () => call('device.subscribeEvents', { types: ['button', 'imuGesture', 'connection'] }).then(() => log('已订阅事件')).catch(log);
  document.getElementById('persist').onclick = () => call('storage.set', { key: 'seconds', value: seconds }).then(() => log('已保存')).catch(log);
  document.getElementById('draw').onclick = () => call('display.updateText', {
    id: 1, x: 20, y: 20, width: 520, height: 80, border: 1, radius: 8,
    text: `插件运行 ${seconds} 秒`
  }).then(() => log('设备已确认绘制')).catch((error) => log(`绘制失败：${error.message || JSON.stringify(error)}`));
})();
