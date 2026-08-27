export type BridgeErrorCode =
  | 'INVALID_REQUEST' | 'PAYLOAD_TOO_LARGE' | 'UNAUTHORIZED'
  | 'STALE_RUNTIME' | 'METHOD_NOT_FOUND' | 'RATE_LIMITED' | 'BUSY' | 'QUOTA_EXCEEDED'
  | 'AUDIO_BUSY' | 'NO_AUDIO'
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

export interface PluginMessageResult {
  sent: boolean;
  channel: number;
  payloadBytes: number;
}

export interface PluginMessage {
  channel: number;
  data: Uint8Array;
}

export type AudioPickupMode =
  | 'unchanged' | 'frontFixed' | 'meetingAuto' | 'nonWearerFocus'
  | 'frontBalanced' | 'frontFocus';
export type AudioVoice = 'original' | 'cute' | 'deep' | 'overlord';

export interface AudioFrameBatch {
  recordingId?: string;
  firstSequence: number;
  frameCount: number;
  droppedFrameCount: number;
  frames: Uint8Array[];
}

export interface AudioState {
  state: 'starting' | 'recording' | 'stopping' | 'stopped' | 'error';
  recordingId?: string;
  latestRecordingId?: string;
  errorCode?: BridgeErrorCode;
  message?: string;
  frameCount?: number;
  opusBytes?: number;
  durationMs?: number;
}

export interface AudioPlaybackState {
  state: 'preparing' | 'playing' | 'stopped' | 'completed' | 'error';
  playbackId: string;
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
}

export function createGMPlugin(options?: { transport?: BridgeTransport; timeoutMs?: number }): {
  ready(): Promise<unknown>;
  call(method: string, params?: Record<string, unknown>): Promise<unknown>;
  on(eventName: string, listener: (data: Record<string, unknown>, event: PluginEvent) => void): () => void;
  runtime: Record<string, (...args: never[]) => Promise<unknown>>;
  storage: Record<string, (...args: any[]) => Promise<unknown>>;
  display: Record<string, (...args: any[]) => Promise<unknown>>;
  device: Record<string, (...args: any[]) => Promise<unknown> | (() => void)>;
  plugin: {
    sendMessage(channel: number, data: Uint8Array): Promise<PluginMessageResult>;
    onMessage(listener: (message: PluginMessage, event: PluginEvent) => void): () => void;
  };
  audio: {
    configure(options?: { noiseReduction?: boolean; pickupMode?: AudioPickupMode }): Promise<unknown>;
    startRecording(): Promise<unknown>;
    stopRecording(): Promise<unknown>;
    playRecording(options: { recordingId: string; voice?: AudioVoice }): Promise<unknown>;
    stopPlayback(): Promise<unknown>;
    onFrames(listener: (batch: AudioFrameBatch, event: PluginEvent) => void): () => void;
    onState(listener: (state: AudioState, event: PluginEvent) => void): () => void;
    onPlaybackState(listener: (state: AudioPlaybackState, event: PluginEvent) => void): () => void;
  };
  close(): void;
};
