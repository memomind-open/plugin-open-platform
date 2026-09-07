import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import {
  DISPLAY_CONTROL_COMMAND_CHANNEL,
  DISPLAY_CONTROL_STATE_CHANNEL,
  decodeDisplayControlState,
  displayControlOperation,
  encodeDisplayControlCommand,
} from '../../examples/display-control-lab/display-control-protocol.js';
import { createI18n } from '../../examples/display-control-lab/i18n.js';

const root = fileURLToPath(new URL('../..', import.meta.url));

test('Display Control Lab encodes bounded big-endian commands', () => {
  const packet = encodeDisplayControlCommand({
    requestId: 0x12345678,
    operation: displayControlOperation.setBrightness,
    value: 9,
  });
  assert.equal(DISPLAY_CONTROL_COMMAND_CHANNEL, 0x4443);
  assert.deepEqual([...packet], [1, 3, 9, 0, 0x12, 0x34, 0x56, 0x78]);
  assert.throws(() => encodeDisplayControlCommand({
    requestId: 1, operation: displayControlOperation.setBrightness, value: 0,
  }), /Brightness/);
  assert.throws(() => encodeDisplayControlCommand({
    requestId: 1, operation: displayControlOperation.setHeight, value: 9,
  }), /Optical level/);
});

test('Display Control Lab decodes complete state and preview flags', () => {
  const data = Uint8Array.from([
    1, 1, 0, 0x0f, 7, 3, 6, displayControlOperation.setScreen,
    0xde, 0xad, 0xbe, 0xef,
  ]);
  const state = decodeDisplayControlState({ channel: DISPLAY_CONTROL_STATE_CHANNEL, data });
  assert.deepEqual(state, {
    statusCode: 0,
    status: 'OK',
    screenOn: true,
    autoBrightnessBlocked: true,
    previewOnly: true,
    requestedScreenOn: true,
    brightness: 7,
    distance: 3,
    height: 6,
    lastOperation: displayControlOperation.setScreen,
    requestId: 0xdeadbeef,
  });
  assert.equal(decodeDisplayControlState({ channel: 1, data }), null);
});

test('Display Control Lab pairs one Web plugin with one GMP protocol', async () => {
  const manifest = JSON.parse(await readFile(
    `${root}/examples/display-control-lab/manifest.json`, 'utf8'));
  const glassManifest = JSON.parse(await readFile(
    `${root}/../GlassSDK/examples/display_control_lab/manifest.json`, 'utf8'));
  assert.deepEqual(manifest.permissions, ['device.events']);
  assert.equal(manifest.version, '0.1.1');
  assert.equal(manifest.deviceRequirements.requiredPluginId, manifest.id);
  assert.equal(manifest.deviceRequirements.minPluginVersion, '2');
  assert.equal(glassManifest.id, manifest.id);
  assert.equal(glassManifest.version, 2);
  assert.equal(glassManifest.provides.protocols[0].id, 'gm.display-control-lab');
});

test('Display Control Lab covers visible copy in English and Chinese', async () => {
  const html = await readFile(`${root}/examples/display-control-lab/index.html`, 'utf8');
  const keys = [...html.matchAll(/data-i18n="([^"]+)"/g)].map((match) => match[1]);
  const environment = (language) => ({
    navigator: { language },
    localStorage: { getItem: () => null, setItem() {} },
  });
  const english = createI18n(environment('en-US'));
  const chinese = createI18n(environment('zh-CN'));
  assert.ok(keys.length >= 20);
  for (const key of keys) {
    assert.notEqual(english.t(key), key, `missing English translation: ${key}`);
    assert.notEqual(chinese.t(key), key, `missing Chinese translation: ${key}`);
  }
});

test('Display Control Lab preserves the Studio display and physical recovery path', async () => {
  const plugin = await readFile(`${root}/examples/display-control-lab/plugin.js`, 'utf8');
  const gmp = await readFile(
    `${root}/../GlassSDK/examples/display_control_lab/display_control_lab.c`, 'utf8');
  assert.match(plugin, /transport === 'desktop-simulator'/);
  assert.match(plugin, /previewOnly: desktopPreview && operation === displayControlOperation\.setScreen/);
  assert.match(gmp, /GM_PLUGIN_CAP_DISPLAY_CONTROL/);
  assert.match(gmp, /GM_PLUGIN_CAP_IMU_EVENTS/);
  assert.match(gmp, /imu_enable\(GM_PLUGIN_IMU_ENABLE_GESTURES\)/);
  assert.match(gmp, /GM_PLUGIN_IMU_GESTURE_HEAD_RAISE/);
  assert.match(gmp, /wake_display_if_needed/);
  assert.match(gmp, /send_state\(self, 0U\)/);
  assert.match(gmp, /GM_PLUGIN_BUTTON_ACTION_LONG\s*\|\|[\s\S]*GM_PLUGIN_BUTTON_ACTION_VERY_LONG/);
  assert.match(gmp, /screen_turn_on\(true\)/);
  assert.match(gmp, /auto_brightness_block\(false\)/);
});

test('Display Control Lab treats restore as one guarded long-running operation', async () => {
  const plugin = await readFile(`${root}/examples/display-control-lab/plugin.js`, 'utf8');
  const gmp = await readFile(
    `${root}/../GlassSDK/examples/display_control_lab/display_control_lab.c`, 'utf8');
  assert.match(plugin, /const RESTORE_TIMEOUT_MS = 12000/);
  assert.match(plugin, /if \(commandRunning\) return/);
  assert.match(plugin, /operation === displayControlOperation\.restore[\s\S]*RESTORE_TIMEOUT_MS/);
  assert.match(plugin, /notice\.restoring/);
  assert.match(gmp, /self->brightness != self->initial_brightness/);
  assert.match(gmp, /self->distance != self->initial_distance/);
  assert.match(gmp, /self->height != self->initial_height/);
});

test('Display Control Lab wakes and synchronizes power from glasses input', async () => {
  const plugin = await readFile(`${root}/examples/display-control-lab/plugin.js`, 'utf8');
  const gmp = await readFile(
    `${root}/../GlassSDK/examples/display_control_lab/display_control_lab.c`, 'utf8');
  assert.match(plugin, /next\.requestId === 0/);
  assert.match(plugin, /notice\.woke/);
  assert.match(gmp, /event->type == GM_PLUGIN_EVENT_BUTTON[\s\S]*wake_display_if_needed/);
  assert.match(gmp, /GM_PLUGIN_IMU_GESTURE_HEAD_RAISE[\s\S]*wake_display_if_needed/);
});
