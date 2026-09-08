export interface BridgeErrorShape {
  code?: string;
  message?: string;
}

export interface BridgeResponse {
  requestId: string;
  ok: boolean;
  result?: unknown;
  error?: BridgeErrorShape;
  runtimeGeneration: number;
}

export interface BridgeEvent {
  name: string;
  subscriptionIds: string[];
  data: Record<string, unknown>;
  runtimeGeneration: number;
}

export interface BridgeTransport {
  postMessage(message: string): void;
}

export interface BridgeGlobalTarget {
  MemoPluginBridge?: BridgeTransport;
  __memoPluginBootstrap?: (sessionToken: string, runtimeGeneration: number) => void;
  __memoPluginResolve?: (response: BridgeResponse) => void;
  __memoPluginEmit?: (event: BridgeEvent) => void;
  __memoPluginHeartbeat?: () => boolean;
}

interface PendingRequest {
  resolve(value: unknown): void;
  reject(reason: Error): void;
  timer: ReturnType<typeof setTimeout>;
}

interface Configuration {
  sessionToken: string;
  runtimeGeneration: number;
}

export class MemoBridgeError extends Error {
  constructor(
    public readonly code: string,
    message: string,
  ) {
    super(message);
    this.name = 'MemoBridgeError';
  }
}

type EventListener = (event: BridgeEvent) => void;

export class MemoBridgeClient {
  private configuration: Configuration | null = null;
  private requestSequence = 0;
  private readonly pending = new Map<string, PendingRequest>();
  private readonly listeners = new Map<string, Set<EventListener>>();
  private configuredPromise: Promise<void>;
  private resolveConfigured!: () => void;

  constructor(
    private readonly target: BridgeGlobalTarget,
    private readonly timeoutMs = 7000,
  ) {
    this.configuredPromise = this.newConfiguredPromise();
  }

  install(): void {
    this.target.__memoPluginBootstrap = (token, generation) => this.bootstrap(token, generation);
    this.target.__memoPluginResolve = (response) => this.resolveResponse(response);
    this.target.__memoPluginEmit = (event) => this.emitEvent(event);
    this.target.__memoPluginHeartbeat = () => this.configuration !== null;
  }

  whenConfigured(): Promise<void> {
    return this.configuredPromise;
  }

  call<T = unknown>(method: string, params: Record<string, unknown> = {}): Promise<T> {
    const configuration = this.configuration;
    const transport = this.target.MemoPluginBridge;
    if (!configuration || !transport) {
      return Promise.reject(new MemoBridgeError('BRIDGE_UNAVAILABLE', 'App Bridge is not ready'));
    }
    const requestId = `tictactoe-${Date.now()}-${++this.requestSequence}`;
    return new Promise<T>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(requestId);
        reject(new MemoBridgeError('TIMEOUT', `${method} timed out`));
      }, this.timeoutMs);
      this.pending.set(requestId, {
        resolve: (value) => resolve(value as T),
        reject,
        timer,
      });
      try {
        transport.postMessage(JSON.stringify({
          version: '2.0',
          sessionToken: configuration.sessionToken,
          requestId,
          method,
          params,
          runtimeGeneration: configuration.runtimeGeneration,
        }));
      } catch (error) {
        clearTimeout(timer);
        this.pending.delete(requestId);
        reject(new MemoBridgeError('BRIDGE_UNAVAILABLE', String(error)));
      }
    });
  }

  on(name: string, listener: EventListener): () => void {
    const bucket = this.listeners.get(name) ?? new Set<EventListener>();
    bucket.add(listener);
    this.listeners.set(name, bucket);
    return () => {
      bucket.delete(listener);
      if (bucket.size === 0) this.listeners.delete(name);
    };
  }

  private bootstrap(sessionToken: string, runtimeGeneration: number): void {
    if (!sessionToken || !Number.isInteger(runtimeGeneration) || runtimeGeneration < 0) return;
    const previous = this.configuration;
    if (previous &&
        (previous.sessionToken !== sessionToken || previous.runtimeGeneration !== runtimeGeneration)) {
      this.rejectAll(new MemoBridgeError('STALE_RUNTIME', 'Plugin Runtime was recreated'));
      this.configuredPromise = this.newConfiguredPromise();
    }
    this.configuration = { sessionToken, runtimeGeneration };
    this.resolveConfigured();
  }

  private resolveResponse(response: BridgeResponse): void {
    if (response.runtimeGeneration !== this.configuration?.runtimeGeneration) return;
    const item = this.pending.get(response.requestId);
    if (!item) return;
    clearTimeout(item.timer);
    this.pending.delete(response.requestId);
    if (response.ok) {
      item.resolve(response.result);
    } else {
      item.reject(new MemoBridgeError(
        response.error?.code ?? 'BRIDGE_ERROR',
        response.error?.message ?? 'Bridge call failed',
      ));
    }
  }

  private emitEvent(event: BridgeEvent): void {
    if (event.runtimeGeneration !== this.configuration?.runtimeGeneration) return;
    for (const listener of this.listeners.get(event.name) ?? []) listener(event);
  }

  private rejectAll(error: MemoBridgeError): void {
    for (const item of this.pending.values()) {
      clearTimeout(item.timer);
      item.reject(error);
    }
    this.pending.clear();
  }

  private newConfiguredPromise(): Promise<void> {
    return new Promise<void>((resolve) => {
      this.resolveConfigured = resolve;
    });
  }
}
