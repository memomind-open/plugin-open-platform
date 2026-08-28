const PERMISSION_BY_METHOD = new Map([
  ['device.subscribeEvents', 'device.events'],
  ['device.unsubscribeEvents', 'device.events'],
]);

export function requiredPermission(method) {
  if (method.startsWith('display.')) return 'display';
  if (method.startsWith('storage.')) return 'storage';
  if (method.startsWith('audio.')) return 'audio.capture';
  return PERMISSION_BY_METHOD.get(method) ?? null;
}

export function hasBridgePermission(method, permissions = []) {
  const required = requiredPermission(method);
  return required === null || permissions.includes(required);
}

export function isTrustedPluginMessage(message, frameWindow, frameOrigin) {
  return Boolean(frameWindow) && Boolean(frameOrigin) &&
    message.source === frameWindow && message.origin === frameOrigin;
}

export function storageNamespace(namespaces, pluginId) {
  if (!pluginId) throw new Error('An active plugin is required for storage access');
  if (!namespaces.has(pluginId)) namespaces.set(pluginId, new Map());
  return namespaces.get(pluginId);
}
