const TERMINAL_CAPTURE_STATES = new Set(['stopped', 'error']);

export function createOpeningCaptureTerminalTracker(maxEntries = 8) {
  const states = new Map();
  return {
    remember(event) {
      if (!event || typeof event.sessionId !== 'string' || !event.sessionId ||
          !TERMINAL_CAPTURE_STATES.has(event.state)) return false;
      states.delete(event.sessionId);
      states.set(event.sessionId, { ...event });
      while (states.size > maxEntries) states.delete(states.keys().next().value);
      return true;
    },
    take(sessionId) {
      const event = states.get(sessionId);
      states.delete(sessionId);
      return event;
    },
  };
}

export function openedCaptureDisposition({ terminal, inputActive, generationMatches }) {
  if (terminal?.state === 'error') return 'discard';
  if (terminal?.state === 'stopped' || !inputActive || !generationMatches) return 'stop';
  return 'keep';
}
