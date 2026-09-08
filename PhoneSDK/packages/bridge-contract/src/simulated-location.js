let nextWatchSequence = 0;
// Explicit Studio-only coordinates. Never requests the computer's physical location.
export class SimulatedLocation {
  constructor(emit, active = () => true) { this.emit = emit; this.active = active; this.watchId = null; this.timer = null; }
  sample() { return {latitude:31.2304,longitude:121.4737,accuracy:10,timestamp:Date.now(),coordinateSystem:'WGS84',precision:'precise'}; }
  validate(params) {
    if (!params || Array.isArray(params) || Object.keys(params).some(k=>k!=='timeoutMs') ||
      ('timeoutMs' in params && (!Number.isInteger(params.timeoutMs)||params.timeoutMs<1||params.timeoutMs>60000)))
      throw Object.assign(new Error('INVALID_LOCATION_OPTIONS'),{code:'INVALID_REQUEST'});
    if (!this.active()) throw Object.assign(new Error('NOT_FOREGROUND'),{code:'PERMISSION_DENIED'});
    if (this.watchId) throw Object.assign(new Error('LOCATION_BUSY'),{code:'BUSY'});
  }
  current(params) { this.validate(params); return this.sample(); }
  watch(params) {
    this.validate(params); this.watchId = 'studio-location-'+(++nextWatchSequence);
    this.timer = setInterval(()=>{
      if (!this.active()) { this.dispose(); return; }
      this.emit('location.position',{watchId:this.watchId,...this.sample()});
    },1000);
    return {watchId:this.watchId};
  }
  clear(params) {
    if (!params || Object.keys(params).length!==1 || typeof params.watchId!=='string' || !params.watchId)
      throw Object.assign(new Error('INVALID_WATCH_ID'),{code:'INVALID_REQUEST'});
    if (params.watchId===this.watchId) this.dispose();
    return {released:true};
  }
  dispose() { clearInterval(this.timer); this.timer=null; this.watchId=null; }
}
