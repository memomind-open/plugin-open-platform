export class StreamMetrics {
  constructor(now = () => performance.now()) {
    this.now = now;
    this.reset();
  }

  reset() {
    this.startedAt = this.now();
    this.chunkCount = 0;
    this.frameCount = 0;
    this.byteCount = 0;
    this.droppedFrameCount = 0;
    this.discontinuityCount = 0;
    this.latestSequence = null;
    this.latestTimestampUs = null;
    this.latestDurationMs = 0;
    this.latestFrameLengths = [];
    this.queueLatencyMs = 0;
    this.maxQueueLatencyMs = 0;
  }

  add(chunk) {
    this.chunkCount += 1;
    this.frameCount += chunk.frameCount;
    this.byteCount += chunk.data.byteLength;
    this.droppedFrameCount += chunk.droppedFrameCount;
    if (chunk.discontinuity) this.discontinuityCount += 1;
    this.latestSequence = chunk.sequence;
    this.latestTimestampUs = chunk.timestampUs;
    this.latestDurationMs = chunk.durationMs;
    this.latestFrameLengths = [...chunk.frameLengths];
    this.queueLatencyMs = chunk.queueLatencyMs;
    this.maxQueueLatencyMs = Math.max(this.maxQueueLatencyMs, chunk.queueLatencyMs);
    return this.snapshot();
  }

  snapshot() {
    const elapsedMs = Math.max(0, this.now() - this.startedAt);
    const bitrateKbps = elapsedMs > 0 ? this.byteCount * 8 / elapsedMs : 0;
    return {
      elapsedMs,
      chunkCount: this.chunkCount,
      frameCount: this.frameCount,
      byteCount: this.byteCount,
      bitrateKbps,
      droppedFrameCount: this.droppedFrameCount,
      discontinuityCount: this.discontinuityCount,
      latestSequence: this.latestSequence,
      latestTimestampUs: this.latestTimestampUs,
      latestDurationMs: this.latestDurationMs,
      latestFrameLengths: [...this.latestFrameLengths],
      queueLatencyMs: this.queueLatencyMs,
      maxQueueLatencyMs: this.maxQueueLatencyMs,
    };
  }
}
