import test from 'node:test';
import assert from 'node:assert/strict';

import {
  chooseDevicePlugin,
  chooseWebPlugin,
  describeWorkspace,
  evaluateCompatibility,
  WEB_BRIDGE_PLUGIN_ID,
} from '../src/workspace-mode.js';

const plugins = [
  { id: 'com.gm.example.bluetooth', path: '/build/bluetooth/bluetooth.gmp' },
  { id: WEB_BRIDGE_PLUGIN_ID, version: '1', path: '/build/web_bridge/web_bridge.gmp', provides: [
    { id: 'gm.scene', version: '1.0' }, { id: 'gm.device-events', version: '1.0' },
  ] },
  { id: 'com.gm.game.fighter', version: '29', path: '/build/game/fighter.gmp', provides: [
    { id: 'gm.fighter-control', version: '2.0' },
  ] },
];

const sceneWebPlugin = {
  deviceRequirements: {
    preferredPluginId: WEB_BRIDGE_PLUGIN_ID,
    protocols: [{ id: 'gm.scene', minVersion: '1.0' }],
  },
};

test('Web development defaults to web_bridge when device selection is implicit', () => {
  assert.equal(chooseDevicePlugin(plugins, {
    previousPath: plugins[0].path,
    webEnabled: true,
    webPlugin: sceneWebPlugin,
  }), plugins[1].path);
});

test('Glass selection chooses the newest phone plugin from a many-to-one pairing', () => {
  const device = plugins[1];
  const webPlugins = [
    { path: '/web/older', updatedAtMs: 100, deviceRequirements: {
      preferredPluginId: WEB_BRIDGE_PLUGIN_ID,
      protocols: [{ id: 'gm.scene', minVersion: '1.0' }],
    } },
    { path: '/web/newer', updatedAtMs: 200, deviceRequirements: {
      preferredPluginId: WEB_BRIDGE_PLUGIN_ID,
      protocols: [{ id: 'gm.scene', minVersion: '1.0' }],
    } },
  ];
  assert.equal(chooseWebPlugin(webPlugins, device), '/web/newer');
});

test('An exact glass plugin ID match outranks a newer protocol-only phone plugin', () => {
  const device = plugins[2];
  const webPlugins = [
    { path: '/web/protocol-newer', updatedAtMs: 300, deviceRequirements: {
      protocols: [{ id: 'gm.fighter-control', minVersion: '2.0' }],
    } },
    { path: '/web/exact-older', updatedAtMs: 100, deviceRequirements: {
      requiredPluginId: device.id,
      protocols: [{ id: 'gm.fighter-control', minVersion: '2.0' }],
    } },
  ];
  assert.equal(chooseWebPlugin(webPlugins, device), '/web/exact-older');
});

test('A glass plugin without a related phone plugin resolves to glass-only mode', () => {
  assert.equal(chooseWebPlugin([sceneWebPlugin], plugins[2]), '');
});

test('protocol metadata distinguishes recommended and incompatible devices', () => {
  assert.equal(evaluateCompatibility(sceneWebPlugin, plugins[1]).status, 'recommended');
  assert.deepEqual(evaluateCompatibility(sceneWebPlugin, plugins[0]), {
    compatible: false,
    preferred: false,
    status: 'incompatible',
    reasons: ['Missing protocol gm.scene'],
  });
});

test('a standalone GMP without sidecar metadata is unknown rather than incompatible', () => {
  assert.equal(evaluateCompatibility(sceneWebPlugin, {
    id: 'external.gmp', path: '/tmp/external.gmp', metadataAvailable: false, provides: [],
  }).status, 'unknown');
});

test('Fighter Controller requires fighter_arena v29 and control protocol v2', () => {
  const fighterWeb = { deviceRequirements: {
    requiredPluginId: 'com.gm.game.fighter', minPluginVersion: '29',
    protocols: [{ id: 'gm.fighter-control', minVersion: '2.0' }],
  } };
  assert.equal(evaluateCompatibility(fighterWeb, plugins[2]).compatible, true);
  assert.equal(evaluateCompatibility(fighterWeb, plugins[1]).compatible, false);
});

test('Studio startup still selects web_bridge while Web is disabled', () => {
  assert.equal(chooseDevicePlugin(plugins), plugins[1].path);
});

test('an explicit custom device selection is preserved for paired plugins', () => {
  assert.equal(chooseDevicePlugin(plugins, {
    previousPath: plugins[2].path,
    explicitSelection: true,
    webEnabled: true,
  }), plugins[2].path);
});

test('glass-only mode remains ready without a phone plugin', () => {
  assert.deepEqual(describeWorkspace({ webEnabled: false, deviceRunning: true }), {
    text: 'Glass-only debugging · Glass plugin running',
    ready: true,
    bridgeLabel: 'Glass events',
  });
});
