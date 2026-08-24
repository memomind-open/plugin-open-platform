export const WEB_BRIDGE_PLUGIN_ID = 'com.gm.example.web-bridge';

export function chooseDevicePlugin(plugins, options = {}) {
  const {
    previousPath = '', explicitSelection = false, webEnabled = false, webPlugin = null,
  } = options;
  const previous = plugins.find((plugin) => plugin.path === previousPath);
  if (explicitSelection && previous) return previous.path;

  if (webPlugin?.deviceRequirements) {
    const compatible = plugins
      .map((plugin) => ({ plugin, result: evaluateCompatibility(webPlugin, plugin) }))
      .filter(({ result }) => result.compatible)
      .sort((left, right) => Number(right.result.preferred) - Number(left.result.preferred));
    if (compatible[0]) return compatible[0].plugin.path;
  }

  const bridge = plugins.find((plugin) =>
    plugin.id === WEB_BRIDGE_PLUGIN_ID || /(?:^|[\\/])web_bridge\.gmp$/iu.test(plugin.path));
  if (webEnabled && bridge) return bridge.path;
  if (previous) return previous.path;
  if (bridge) return bridge.path;
  return plugins[0]?.path ?? '';
}

export function evaluateCompatibility(webPlugin, devicePlugin) {
  const requirements = webPlugin?.deviceRequirements;
  if (!requirements) {
    return { compatible: true, preferred: false, status: 'unknown', reasons: [] };
  }
  if (devicePlugin?.metadataAvailable === false) {
    return {
      compatible: true,
      preferred: false,
      status: 'unknown',
      reasons: ['设备包没有可读取的 manifest 元数据'],
    };
  }
  const reasons = [];
  if (requirements.requiredPluginId && requirements.requiredPluginId !== devicePlugin.id) {
    reasons.push(`需要设备插件 ${requirements.requiredPluginId}`);
  }
  if (requirements.requiredPluginId === devicePlugin.id && requirements.minPluginVersion &&
      compareVersions(devicePlugin.version, requirements.minPluginVersion) < 0) {
    reasons.push(`设备插件版本需不低于 ${requirements.minPluginVersion}`);
  }
  const provided = new Map((devicePlugin.provides ?? []).map((protocol) => [protocol.id, protocol.version]));
  for (const protocol of requirements.protocols ?? []) {
    const version = provided.get(protocol.id);
    if (!version) reasons.push(`缺少协议 ${protocol.id}`);
    else if (compareVersions(version, protocol.minVersion) < 0) {
      reasons.push(`${protocol.id} 需要 ${protocol.minVersion}，当前为 ${version}`);
    }
  }
  const compatible = reasons.length === 0;
  const preferred = compatible && requirements.preferredPluginId === devicePlugin.id;
  return {
    compatible,
    preferred,
    status: compatible ? (preferred ? 'recommended' : 'compatible') : 'incompatible',
    reasons,
  };
}

export function compareVersions(left, right) {
  const normalize = (value) => String(value ?? '').split('.').map((part) => Number(part) || 0);
  const leftParts = normalize(left);
  const rightParts = normalize(right);
  for (let index = 0; index < Math.max(leftParts.length, rightParts.length); index += 1) {
    const difference = (leftParts[index] ?? 0) - (rightParts[index] ?? 0);
    if (difference !== 0) return Math.sign(difference);
  }
  return 0;
}

export function describeWorkspace({ webEnabled, deviceRunning }) {
  if (!deviceRunning) {
    return {
      text: webEnabled ? 'Web 已选择 · 设备插件未运行' : '仅设备调试 · 设备插件未运行',
      ready: false,
      bridgeLabel: webEnabled ? 'Web ↔ Device' : 'Device events',
    };
  }
  return {
    text: webEnabled ? '联合调试 · Web 与设备插件运行中' : '仅设备调试 · 设备插件运行中',
    ready: true,
    bridgeLabel: webEnabled ? 'Web ↔ Device' : 'Device events',
  };
}
