import './style.css';
import { MemoBridgeClient, MemoBridgeError, type BridgeEvent, type BridgeGlobalTarget } from './bridge';
import {
  DeviceInputFilter,
  newGame,
  positionName,
  reduceGame,
  resultText,
  type GameAction,
} from './game';
import { DevicePresenter, PresentationGeneration } from './device';
import { GamePersistence } from './storage';

const STORAGE_KEY = 'game-state-v1';
const bridge = new MemoBridgeClient(window as unknown as BridgeGlobalTarget);
bridge.install();
const presenter = new DevicePresenter(bridge);
const persistence = new GamePersistence(bridge, STORAGE_KEY);
let state = newGame();
let subscriptionId: string | null = null;
let initialized = false;
const presentationGenerations = new PresentationGeneration();
const deviceInputFilter = new DeviceInputFilter();

const board = requiredElement('board');
const gameCard = requiredElement('game-card');
const resultOverlay = requiredElement('result-overlay');
const resultTitle = requiredElement('result-title');
const status = requiredElement('game-status');
const selection = requiredElement('selection');
const bridgeStatus = requiredElement('bridge-status');
const lastCommand = requiredElement('last-command');
const lastOperation = requiredElement('last-operation');
const logElement = requiredElement('log');

function requiredElement(id: string): HTMLElement {
  const element = document.getElementById(id);
  if (!element) throw new Error(`Missing page element: ${id}`);
  return element;
}

function log(message: string): void {
  logElement.textContent = `${new Date().toLocaleTimeString('zh-CN')} ${message}\n${logElement.textContent}`.slice(0, 1400);
}

function render(): void {
  status.textContent = resultText(state.result);
  gameCard.classList.toggle('finished', state.result !== 'playing');
  resultOverlay.hidden = state.result === 'playing';
  resultTitle.textContent = resultText(state.result);
  selection.textContent = state.result === 'playing'
    ? `Selected: ${positionName(state.cursor)}`
    : `${resultText(state.result)} Double-click the main button or select Restart to play again.`;
  board.replaceChildren(...state.board.map((cell, index) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = `cell${index === state.cursor && state.result === 'playing' ? ' selected' : ''}${cell ? ` ${cell.toLowerCase()}` : ''}`;
    button.textContent = cell ?? '';
    button.disabled = !initialized || state.result !== 'playing' || cell !== null;
    button.setAttribute('role', 'gridcell');
    button.setAttribute('aria-label', `${positionName(index)}: ${cell ?? 'empty'}`);
    button.addEventListener('click', () => {
      if (state.result !== 'playing' || state.board[index] !== null) return;
      state = { ...state, cursor: index };
      dispatch({ type: 'place' });
    });
    return button;
  }));
}

function dispatch(action: GameAction): void {
  if (!initialized) return;
  const previous = state;
  state = reduceGame(state, action);
  if (state === previous) return;
  render();
  void persist();
  void present();
}

async function persist(): Promise<void> {
  try {
    await persistence.save(state);
  } catch (error) {
    log(`Save failed: ${errorMessage(error)}`);
  }
}

async function present(): Promise<void> {
  const generation = presentationGenerations.begin();
  try {
    await presenter.present(state);
    if (!presentationGenerations.isCurrent(generation)) return;
    bridgeStatus.textContent = 'Device confirmed the current board';
  } catch (error) {
    if (!presentationGenerations.isCurrent(generation)) return;
    bridgeStatus.textContent = 'Device unavailable; the Web game can continue';
    log(`Device draw failed: ${errorMessage(error)}`);
  }
}

function onDeviceEvent(event: BridgeEvent): void {
  const received = describeDeviceEvent(event);
  lastCommand.textContent = received;
  const decision = deviceInputFilter.decide(event);
  if (!decision.action) {
    const ignored = describeIgnoredReason(decision.reason);
    lastOperation.textContent = ignored;
    log(`Received: ${received} → ${ignored}`);
    return;
  }

  const before = state;
  const actionName = describeAction(decision.action);
  dispatch(decision.action);
  const operation = state === before
    ? describeNoStateChange(decision.action)
    : describeStateChange(decision.action, before, state);
  lastOperation.textContent = operation;
  log(`Received: ${received} → ${actionName} → ${operation}`);
}

function describeDeviceEvent(event: BridgeEvent): string {
  const sequence = typeof event.data.sequence === 'number'
    ? ` #${event.data.sequence}`
    : '';
  if (event.name === 'device.button') {
    const action = event.data.action;
    const name = action === 'single'
      ? 'Main button single click'
      : action === 'double'
        ? 'Main button double click'
        : `Main button ${String(action)}`;
    return `${name}${sequence}`;
  }
  const gesture = String(event.data.gesture ?? 'unknown');
  const names: Record<string, string> = {
    headRaise: 'Look up',
    headLower: 'Look down',
    headRaiseTimeout: 'Sustained look-up timeout',
    headLowerTimeout: 'Sustained look-down timeout',
    left: 'Turn head left',
    right: 'Turn head right',
    nod: 'Nod',
    shake: 'Shake head',
  };
  return `${names[gesture] ?? gesture}${sequence}，active=${String(event.data.active)}`;
}

function describeIgnoredReason(reason: string): string {
  switch (reason) {
    case 'debounced': return 'Ignored: repeated direction within 300 ms';
    case 'verticalRebound': return 'Ignored: opposite vertical event within 600 ms treated as recentering';
    case 'timeoutAlreadySeen': return 'Ignored: matching regular posture event already received';
    case 'inactive': return 'Ignored: gesture is inactive';
    default: return 'Ignored: command is not bound in the current game';
  }
}

function describeAction(action: GameAction): string {
  const names: Record<GameAction['type'], string> = {
    up: 'Move up',
    down: 'Move down',
    left: 'Move left',
    right: 'Move right',
    place: 'Place mark',
    restart: 'Restart game',
  };
  return names[action.type];
}

function describeStateChange(
  action: GameAction,
  before: typeof state,
  after: typeof state,
): string {
  if (action.type === 'restart') return 'Game restarted; selected: center';
  if (action.type === 'place') {
    return `Placed a mark at ${positionName(before.cursor)}; status: ${resultText(after.result)}`;
  }
  return `Moved: ${positionName(before.cursor)} → ${positionName(after.cursor)}`;
}

function describeNoStateChange(action: GameAction): string {
  if (state.result !== 'playing') return `${describeAction(action)} not performed: game over; double-click to restart`;
  if (action.type === 'place') return 'Mark not placed: selected cell is occupied';
  return `${describeAction(action)} not performed: no empty cell in that direction`;
}

function errorMessage(error: unknown): string {
  if (error instanceof MemoBridgeError) return `${error.code}：${error.message}`;
  return error instanceof Error ? error.message : String(error);
}

document.getElementById('up')?.addEventListener('click', () => dispatch({ type: 'up' }));
document.getElementById('down')?.addEventListener('click', () => dispatch({ type: 'down' }));
document.getElementById('left')?.addEventListener('click', () => dispatch({ type: 'left' }));
document.getElementById('right')?.addEventListener('click', () => dispatch({ type: 'right' }));
document.getElementById('place')?.addEventListener('click', () => dispatch({ type: 'place' }));
document.getElementById('restart')?.addEventListener('click', () => dispatch({ type: 'restart' }));

window.addEventListener('pagehide', () => {
  presenter.closeBestEffort();
  if (subscriptionId) {
    void bridge.call('device.unsubscribeEvents', { subscriptionId }).catch(() => undefined);
  }
});

async function start(): Promise<void> {
  render();
  await bridge.whenConfigured();
  await bridge.call('runtime.ready');
  bridgeStatus.textContent = 'App Bridge connected; restoring game…';
  try {
    state = await persistence.load();
  } catch (error) {
    state = newGame();
    log(`Could not load saved game; started a new game: ${errorMessage(error)}`);
  }
  render();

  bridge.on('device.button', onDeviceEvent);
  bridge.on('device.imuGesture', onDeviceEvent);
  bridge.on('device.connection', (event) => {
    if (event.data.connected === true) void present();
    if (event.data.connected === false) {
      presentationGenerations.invalidate();
      bridgeStatus.textContent = 'Device disconnected; the Web game can continue';
    }
  });
  bridge.on('runtime.lifecycleChanged', (event) => {
    const lifecycle = event.data.state;
    if (lifecycle === 'running' || lifecycle === 'recovering') void present();
  });

  try {
    const subscription = await bridge.call<{ subscriptionId: string }>('device.subscribeEvents', {
      types: ['button', 'imuGesture', 'connection'],
    });
    subscriptionId = subscription.subscriptionId;
    log(`Device events subscribed: ${subscriptionId}`);
  } catch (error) {
    log(`Device event subscription failed; Web controls remain available: ${errorMessage(error)}`);
  }
  initialized = true;
  for (const id of ['up', 'down', 'left', 'right', 'place', 'restart']) {
    const control = document.getElementById(id);
    if (control instanceof HTMLButtonElement) control.disabled = false;
  }
  render();
  await present();
}

void start().catch((error) => {
  bridgeStatus.textContent = 'Plugin startup failed';
  log(errorMessage(error));
});
