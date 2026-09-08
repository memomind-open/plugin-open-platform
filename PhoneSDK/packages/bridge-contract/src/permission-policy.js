// App contract p7-handoff-r1. Studio enforces Bridge policy, not native network isolation.
export const VERSIONS = Object.freeze({"schemaVersion":2,"permissionPolicyVersion":1,"bridgeVersion":"2.0"});
export const PERMISSIONS = Object.freeze(["storage","files.user-selected","display","device.info","device.events","device.messaging","audio.capture","audio.playback","network","location.foreground"]);
export const METHOD_PERMISSIONS = Object.freeze({
  "runtime.ready": null,
  "runtime.ping": null,
  "runtime.getBridgeVersion": null,
  "runtime.getCapabilities": null,
  "runtime.getLifecycleState": null,
  "storage.get": "storage",
  "storage.set": "storage",
  "storage.remove": "storage",
  "storage.clear": "storage",
  "files.pick": "files.user-selected",
  "files.list": "files.user-selected",
  "files.stat": "files.user-selected",
  "files.openRead": "files.user-selected",
  "files.getUsage": "files.user-selected",
  "files.delete": "files.user-selected",
  "display.createPage": "display",
  "display.closePage": "display",
  "display.updateText": "display",
  "display.updateImage": "display",
  "display.updateImageLz4": "display",
  "display.rebuildPage": "display",
  "display.beginFrame": "display",
  "display.updateFrameImageLz4": "display",
  "device.getInfo": "device.info",
  "device.subscribeEvents": "device.events",
  "device.unsubscribeEvents": "device.events",
  "plugin.sendMessage": "device.messaging",
  "audio.openCapture": "audio.capture",
  "audio.stopCapture": "audio.capture",
  "location.getCurrentPosition": "location.foreground",
  "location.watchPosition": "location.foreground",
  "location.clearWatch": "location.foreground"
});
const own = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
const object = v => v !== null && typeof v === 'object' && !Array.isArray(v);
export function policyError(code, message) { return Object.assign(new Error(message), {code}); }
const invalid = () => { throw policyError('INVALID_REQUEST', 'INVALID_PERMISSION_CONFIG'); };
export function normalizePermissions(input) {
  if (!Array.isArray(input) || input.length > PERMISSIONS.length) invalid();
  const seen = new Set();
  return input.map(p => {
    if (!object(p) || Object.keys(p).some(k => !['name','required','reason','scope'].includes(k)) ||
        !PERMISSIONS.includes(p.name) || typeof p.required !== 'boolean' || seen.has(p.name)) invalid();
    seen.add(p.name);
    const result = {name:p.name, required:p.required};
    if (own(p,'reason')) {
      if (typeof p.reason !== 'string' || !p.reason.trim() || [...p.reason].length > 200) invalid();
      result.reason = p.reason;
    }
    const key = p.name === 'device.events' ? 'types' : p.name === 'device.messaging' ? 'channels' : null;
    if (!key) { if (own(p,'scope')) invalid(); }
    else {
      const values = p.scope?.[key];
      if (!object(p.scope) || Object.keys(p.scope).length !== 1 || !Array.isArray(values) ||
          !values.length || new Set(values).size !== values.length ||
          (key === 'channels' && values.length > 64) ||
          values.some(v => key === 'types' ? !['button','imuGesture','rawImu','connection'].includes(v) :
            !Number.isInteger(v) || v < 0 || v > 65535)) invalid();
      result.scope = {[key]: [...values].sort(key === 'channels' ? (a,b)=>a-b : undefined)};
    }
    return result;
  }).sort((a,b)=>a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
}
export function validateManifestPolicy(manifest) {
  if (!object(manifest) || Object.entries(VERSIONS).some(([k,v])=>manifest[k] !== v)) invalid();
  return normalizePermissions(manifest.permissions);
}
export function validateApprovals(declarations, approvals) {
  if (!object(approvals)) invalid();
  for (const [name,scope] of Object.entries(approvals)) {
    const p = declarations.find(p=>p.name===name);
    if (!p) invalid();
    if (!p.scope) { if (scope !== null) invalid(); }
    else {
      const [key] = Object.keys(p.scope);
      const normalized = normalizePermissions([{name,required:false,scope}])[0].scope[key];
      if (normalized.some(v=>!p.scope[key].includes(v))) invalid();
    }
  }
  return approvals;
}
export function authorize(method, params, declarations, approvals) {
  const ds = normalizePermissions(declarations);
  validateApprovals(ds, approvals);
  if (!own(METHOD_PERMISSIONS,method)) throw policyError('METHOD_NOT_FOUND','METHOD_NOT_FOUND');
  const requirePermission = name => {
    if (!ds.some(p=>p.name===name)) throw policyError('PERMISSION_DENIED','UNDECLARED');
    if (!own(approvals,name)) throw policyError('PERMISSION_DENIED','NOT_GRANTED');
  };
  const scopeDenied = () => { throw policyError('PERMISSION_DENIED','OUT_OF_SCOPE'); };
  const permission = METHOD_PERMISSIONS[method];
  if (permission) requirePermission(permission);
  for (const p of ds.filter(p=>p.required)) {
    requirePermission(p.name);
    if (p.scope) {
      const key = Object.keys(p.scope)[0];
      if (p.scope[key].some(v=>!approvals[p.name][key].includes(v))) scopeDenied();
    }
  }
  if (method === 'device.subscribeEvents') {
    if (!Array.isArray(params?.types) || !params.types.length || new Set(params.types).size !== params.types.length ||
        params.types.some(v=>!['button','imuGesture','rawImu','connection'].includes(v)))
      throw policyError('INVALID_REQUEST','INVALID_EVENT_TYPES');
    if (params.types.some(v=>!approvals['device.events'].types.includes(v))) scopeDenied();
  }
  if (method === 'plugin.sendMessage') {
    if (!Number.isInteger(params?.channel) || params.channel < 0 || params.channel > 65535)
      throw policyError('INVALID_REQUEST','INVALID_CHANNEL');
    if (!approvals['device.messaging'].channels.includes(params.channel)) scopeDenied();
    if ([1,2,6,7,8,9,260].includes(params.channel)) requirePermission('display');
    if (params.channel >= 256 && params.channel <= 259) {
      requirePermission('device.events');
      if (!approvals['device.events'].types.includes(['button','imuGesture','rawImu','connection'][params.channel-256])) scopeDenied();
    }
  }
}
export function grantedMethods(declarations, approvals, supported = Object.keys(METHOD_PERMISSIONS)) {
  return supported.filter(method => {
    const permission = METHOD_PERMISSIONS[method];
    return own(METHOD_PERMISSIONS,method) && (!permission || own(approvals,permission));
  });
}
