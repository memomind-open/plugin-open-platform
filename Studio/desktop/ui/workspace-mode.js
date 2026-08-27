export const WEB_BRIDGE_PLUGIN_ID = 'com.gm.example.web-bridge';

export function chooseWebPlugin(plugins, devicePlugin) {
  if (!devicePlugin) return '';
  const matches = plugins
    .map((plugin) => {
      const requirements = plugin.deviceRequirements ?? {};
      const result = evaluateCompatibility(plugin, devicePlugin);
      const exactRequired = requirements.requiredPluginId === devicePlugin.id;
      const exactPreferred = requirements.preferredPluginId === devicePlugin.id;
      const protocolMatch = (requirements.protocols?.length ?? 0) > 0 && result.compatible;
      const score = Number(exactRequired) * 4 + Number(exactPreferred) * 2 + Number(protocolMatch);
      return { plugin, result, score };
    })
    .filter(({ result, score }) => result.compatible && score > 0)
    .sort((left, right) => right.score - left.score ||
      Number(right.plugin.updatedAtMs ?? 0) - Number(left.plugin.updatedAtMs ?? 0));
  return matches[0]?.plugin.path ?? '';
}

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
      reasons: ['The glass package has no readable manifest metadata'],
    };
  }
  const reasons = [];
  if (requirements.requiredPluginId && requirements.requiredPluginId !== devicePlugin.id) {
    reasons.push(`Requires glass plugin ${requirements.requiredPluginId}`);
  }
  if (requirements.requiredPluginId === devicePlugin.id && requirements.minPluginVersion &&
      compareVersions(devicePlugin.version, requirements.minPluginVersion) < 0) {
    reasons.push(`Glass plugin version must be at least ${requirements.minPluginVersion}`);
  }
  const provided = new Map((devicePlugin.provides ?? []).map((protocol) => [protocol.id, protocol.version]));
  for (const protocol of requirements.protocols ?? []) {
    const version = provided.get(protocol.id);
    if (!version) reasons.push(`Missing protocol ${protocol.id}`);
    else if (compareVersions(version, protocol.minVersion) < 0) {
      reasons.push(`${protocol.id} requires ${protocol.minVersion}; current version is ${version}`);
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
      text: webEnabled ? 'Phone plugin selected · Glass plugin not running' : 'Glass-only debugging · Glass plugin not running',
      ready: false,
      bridgeLabel: webEnabled ? 'Phone ↔ Glass' : 'Glass events',
    };
  }
  return {
    text: webEnabled ? 'Paired debugging · Phone and glass plugins running' : 'Glass-only debugging · Glass plugin running',
    ready: true,
    bridgeLabel: webEnabled ? 'Phone ↔ Glass' : 'Glass events',
  };
}
