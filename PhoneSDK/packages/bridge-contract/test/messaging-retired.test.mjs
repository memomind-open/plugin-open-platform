import test from 'node:test';
import assert from 'node:assert/strict';
import {normalizePermissions, authorize, grantedMethods} from '../src/permission-policy.js';
test('custom messaging requires declared and approved channels', () => {
  const declarations=normalizePermissions([{name:'device.messaging',required:false,scope:{channels:[32766]}}]);
  authorize('plugin.sendMessage',{channel:32766},declarations,{'device.messaging':{channels:[32766]}});
  assert.throws(() => authorize('plugin.sendMessage',{channel:32766},[],{}), {code:'PERMISSION_DENIED'});
  assert.throws(() => authorize('plugin.sendMessage',{channel:32766},declarations,{}), {code:'PERMISSION_DENIED'});
  assert.throws(() => authorize('plugin.sendMessage',{channel:32767},declarations,{'device.messaging':{channels:[32766]}}), /OUT_OF_SCOPE/);
  assert.ok(!grantedMethods([],{}).includes('plugin.sendMessage'));
});
