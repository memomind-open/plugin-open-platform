export type BridgeErrorCode =
  | 'INVALID_REQUEST' | 'PAYLOAD_TOO_LARGE' | 'UNAUTHORIZED' | 'PERMISSION_DENIED'
  | 'FILE_NOT_FOUND' | 'SYSTEM_PERMISSION_DENIED' | 'LOCATION_SERVICE_DISABLED' | 'POSITION_UNAVAILABLE'
  | 'STALE_RUNTIME' | 'METHOD_NOT_FOUND' | 'RATE_LIMITED' | 'BUSY' | 'QUOTA_EXCEEDED'
  | 'AUDIO_BUSY' | 'NO_AUDIO' | 'BUFFER_OVERFLOW'
  | 'TIMEOUT' | 'DEVICE_DISCONNECTED' | 'CAPABILITY_UNAVAILABLE'
  | 'RUNTIME_CLOSED' | 'RUNTIME_REPLACED' | 'INTERNAL_ERROR';

export interface BridgeTransport {
  waitForBootstrap(): Promise<{ sessionToken: string; runtimeGeneration: number }>;
  currentBootstrap?(): { sessionToken: string; runtimeGeneration: number } | undefined;
  subscribeBootstrap?(listener: (bootstrap: { sessionToken: string; runtimeGeneration: number }) => void): () => void;
  send(request: Record<string, unknown>): Promise<unknown>;
  subscribe(listener: (event: PluginEvent) => void): () => void;
  close?(): void;
}

export interface PluginEvent {
  name: string;
  data: Record<string, unknown>;
  runtimeGeneration: number;
  subscriptionIds?: string[];
}

export interface TextOptions {
  id: number; x: number; y: number; width: number; height: number;
  border: number; radius: number; text: string;
}

export interface ImageOptions {
  x: number; y: number; width: number; height: number; stride: number;
  dataBase64: string;
}

export interface PluginMessageResult { sent: boolean; channel: number; payloadBytes: number; }
export interface PluginMessage { channel: number; data: Uint8Array; }

export interface UserFile {
  fileId: string;
  name: string;
  size: number;
  importedAt: string;
  extension?: string;
}

export interface FileReadOptions {
  offset?: number;
  length?: number;
  signal?: AbortSignal;
}

export interface FileReadStream {
  fileId: string;
  size: number;
  offset: number;
  length: number;
  stream: ReadableStream<Uint8Array>;
}

export interface FileUsage {
  fileCount: number;
  totalBytes: number;
  maxTotalBytes: number;
}

export type AudioPickupMode =
  | 'unchanged' | 'frontFixed' | 'meetingAuto' | 'nonWearerFocus'
  | 'frontBalanced' | 'frontFocus';
export type AudioCaptureMode = 'recording' | 'stream';
export type AudioCaptureProfile = 'interactive' | 'balanced' | 'reliable' | 'custom';
export type AudioOverflowStrategy = 'drop-oldest' | 'drop-newest' | 'error';

export interface AudioCaptureCommonOptions {
  pickupMode?: AudioPickupMode;
  noiseReduction?: boolean;
  codec?: 'opus';
  sampleRate?: 16000;
  channels?: 1;
  signal?: AbortSignal;
}

export interface AudioRecordingCaptureOptions extends AudioCaptureCommonOptions {
  mode: 'recording';
  maxDurationMs?: number;
}

export interface AudioStreamCaptureOptions extends AudioCaptureCommonOptions {
  mode: 'stream';
  profile?: AudioCaptureProfile;
  chunkDurationMs?: number;
  maxQueueMs?: number;
  overflowStrategy?: AudioOverflowStrategy;
  maxDurationMs?: number | null;
}

export interface ResolvedAudioRecordingCaptureOptions {
  mode: 'recording';
  pickupMode: AudioPickupMode;
  noiseReduction: boolean;
  codec: 'opus';
  sampleRate: 16000;
  channels: 1;
  maxDurationMs: number;
  delivery: {
    chunkDurationMs: number;
    maxQueueMs: number;
    overflowStrategy: 'error';
  };
}

export interface ResolvedAudioStreamCaptureOptions {
  mode: 'stream';
  profile: AudioCaptureProfile;
  chunkDurationMs: number;
  maxQueueMs: number;
  overflowStrategy: AudioOverflowStrategy;
  pickupMode: AudioPickupMode;
  noiseReduction: boolean;
  codec: 'opus';
  sampleRate: 16000;
  channels: 1;
  maxDurationMs: number | null;
}

export interface AudioChunk {
  sessionId: string;
  sequence: number;
  timestampUs: number;
  durationMs: number;
  frameCount: number;
  frameLengths: number[];
  droppedFrameCount: number;
  discontinuity: boolean;
  queueLatencyMs: number;
  data: Uint8Array;
}

export interface AudioRecordingStopResult {
  mode: 'recording';
  sessionId: string;
  durationMs: number;
  frameCount: number;
  opusBytes: number;
  deliveredFrameCount: number;
  droppedFrameCount: number;
}

export interface AudioRecordingCaptureResult extends AudioRecordingStopResult {
  data: Uint8Array;
  frameLengths: number[];
}

export interface AudioStreamCaptureResult {
  mode: 'stream';
  sessionId: string;
  durationMs: number;
  deliveredFrameCount: number;
  droppedFrameCount: number;
}

export type AudioCaptureResult = AudioRecordingCaptureResult | AudioStreamCaptureResult;
export type AudioCaptureStopResult = AudioRecordingStopResult | AudioStreamCaptureResult;

export interface AudioRecordingCaptureSession {
  mode: 'recording';
  sessionId: string;
  resolvedOptions: ResolvedAudioRecordingCaptureOptions;
  stream: null;
  stop(): Promise<AudioRecordingCaptureResult>;
}

export interface AudioStreamCaptureSession {
  mode: 'stream';
  sessionId: string;
  resolvedOptions: ResolvedAudioStreamCaptureOptions;
  stream: ReadableStream<AudioChunk>;
  stop(): Promise<AudioStreamCaptureResult>;
}

export type AudioCaptureSession = AudioRecordingCaptureSession | AudioStreamCaptureSession;

export interface AudioCaptureState {
  state: 'starting' | 'capturing' | 'stopping' | 'stopped' | 'error';
  mode: AudioCaptureMode;
  sessionId: string;
  result?: AudioCaptureStopResult;
  errorCode?: BridgeErrorCode;
  message?: string;
}

export interface FrameBeginOptions {
  frameId: number;
  tileCount: number;
}

export interface FrameImageOptions extends ImageOptions {
  frameId: number;
  tileIndex: number;
  decodedSize: number;
}

export class GMPluginError extends Error {
  code: BridgeErrorCode;
}

export function opusRecordingToOgg(recording: AudioRecordingCaptureResult): Blob;

export class ParentFrameTransport implements BridgeTransport {
  constructor(options?: { windowObject?: Window; timeoutMs?: number; parentOrigin?: string });
  waitForBootstrap(): Promise<{ sessionToken: string; runtimeGeneration: number }>;
  currentBootstrap(): { sessionToken: string; runtimeGeneration: number } | undefined;
  subscribeBootstrap(listener: (bootstrap: { sessionToken: string; runtimeGeneration: number }) => void): () => void;
  send(request: Record<string, unknown>): Promise<unknown>;
  subscribe(listener: (event: PluginEvent) => void): () => void;
  close(): void;
}

export class AppWebViewTransport implements BridgeTransport {
  constructor(options?: { globalObject?: typeof globalThis; timeoutMs?: number });
  waitForBootstrap(): Promise<{ sessionToken: string; runtimeGeneration: number }>;
  currentBootstrap(): { sessionToken: string; runtimeGeneration: number } | undefined;
  subscribeBootstrap(listener: (bootstrap: { sessionToken: string; runtimeGeneration: number }) => void): () => void;
  send(request: Record<string, unknown>): Promise<unknown>;
  subscribe(listener: (event: PluginEvent) => void): () => void;
  close(): void;
}

export function createGMPlugin(options?: {
  transport?: BridgeTransport;
  timeoutMs?: number;
  fetchImpl?: typeof fetch;
}): {
  ready(): Promise<unknown>;
  call(method: string, params?: Record<string, unknown>): Promise<unknown>;
  on(eventName: string, listener: (data: Record<string, unknown>, event: PluginEvent) => void): () => void;
  runtime: Record<string, (...args: never[]) => Promise<unknown>>;
  storage: Record<string, (...args: any[]) => Promise<unknown>>;
  files: {
    pick(options?: { extensions?: string[]; allowMultiple?: boolean }): Promise<{ files: UserFile[] }>;
    list(): Promise<{ files: UserFile[] }>;
    stat(fileId: string): Promise<{ file: UserFile }>;
    openRead(fileId: string, options?: FileReadOptions): Promise<FileReadStream>;
    getUsage(): Promise<FileUsage>;
    delete(fileId: string): Promise<{ deleted: boolean }>;
  };
  display: Record<string, (...args: any[]) => Promise<unknown>>;
  device: Record<string, (...args: any[]) => Promise<unknown> | (() => void)>;
  plugin: {
    sendMessage(channel: number, data: Uint8Array): Promise<PluginMessageResult>;
    onMessage(listener: (message: PluginMessage, event: PluginEvent) => void): () => void;
  };
  audio: {
    openCapture(options: AudioRecordingCaptureOptions): Promise<AudioRecordingCaptureSession>;
    openCapture(options: AudioStreamCaptureOptions): Promise<AudioStreamCaptureSession>;
    stopCapture(sessionId: string): Promise<AudioCaptureStopResult>;
    onCaptureState(listener: (state: AudioCaptureState, event: PluginEvent) => void): () => void;
  };
  location: {
    getCurrentPosition(options?: {timeoutMs?: number}): Promise<PluginPosition>;
    watchPosition(options?: {timeoutMs?: number}): Promise<{watchId: string}>;
    clearWatch(watchId: string): Promise<{released: true}>;
    onPosition(listener: (position: PluginPosition & {watchId: string}, event: PluginEvent) => void): () => void;
    onError(listener: (data: {watchId: string; error: {code: string; message: string}}, event: PluginEvent) => void): () => void;
  };
  close(): void;
};
export interface PluginPosition {
  latitude: number; longitude: number; accuracy: number; timestamp: number;
  coordinateSystem: 'WGS84'; precision: 'precise' | 'reduced' | 'unknown';
}
