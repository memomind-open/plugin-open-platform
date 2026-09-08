import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {normalizePermissions,authorize,METHOD_PERMISSIONS,VERSIONS} from '../src/permission-policy.js';
const vectors = JSON.parse(readFileSync(new URL('./permission-vectors.json',import.meta.url)));
test('App contract method map and versions',()=>{assert.deepEqual(METHOD_PERMISSIONS,vectors.methodPermissions);assert.deepEqual(VERSIONS,vectors.versions);});
for (const c of vectors.declarationCases) test('declaration: '+c.id,()=>{
 if(c.error) assert.throws(()=>normalizePermissions(c.input),c.error);
 else assert.deepEqual(normalizePermissions(c.input),c.normalized);
});
for (const c of vectors.authorizationCases) test('authorization: '+c.id,()=>{
 const run=()=>authorize(c.method,c.params,c.declarations,c.approvals);
 if(c.error) assert.throws(run,c.error); else assert.doesNotThrow(run);
});
test('corrupt approval never silently intersects',()=>assert.throws(()=>authorize('runtime.ping',{},[],{storage:null}),{code:'INVALID_REQUEST'}));
