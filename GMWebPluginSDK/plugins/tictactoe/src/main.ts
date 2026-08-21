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
  if (!element) throw new Error(`缺少页面元素：${id}`);
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
    ? `当前选择：${positionName(state.cursor)}`
    : `${resultText(state.result)} 双击主按钮或点击“重新开始”再来一局。`;
  board.replaceChildren(...state.board.map((cell, index) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = `cell${index === state.cursor && state.result === 'playing' ? ' selected' : ''}${cell ? ` ${cell.toLowerCase()}` : ''}`;
    button.textContent = cell ?? '';
    button.disabled = !initialized || state.result !== 'playing' || cell !== null;
    button.setAttribute('role', 'gridcell');
    button.setAttribute('aria-label', `${positionName(index)}：${cell ?? '空格'}`);
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
    log(`保存失败：${errorMessage(error)}`);
  }
}

async function present(): Promise<void> {
  const generation = presentationGenerations.begin();
  try {
    await presenter.present(state);
    if (!presentationGenerations.isCurrent(generation)) return;
    bridgeStatus.textContent = '设备已确认接收当前棋盘';
  } catch (error) {
    if (!presentationGenerations.isCurrent(generation)) return;
    bridgeStatus.textContent = '设备暂不可用，网页棋局仍可继续';
    log(`设备绘制失败：${errorMessage(error)}`);
  }
}

function onDeviceEvent(event: BridgeEvent): void {
  const received = describeDeviceEvent(event);
  lastCommand.textContent = received;
  const decision = deviceInputFilter.decide(event);
  if (!decision.action) {
    const ignored = describeIgnoredReason(decision.reason);
    lastOperation.textContent = ignored;
    log(`收到：${received} → ${ignored}`);
    return;
  }

  const before = state;
  const actionName = describeAction(decision.action);
  dispatch(decision.action);
  const operation = state === before
    ? describeNoStateChange(decision.action)
    : describeStateChange(decision.action, before, state);
  lastOperation.textContent = operation;
  log(`收到：${received} → ${actionName} → ${operation}`);
}

function describeDeviceEvent(event: BridgeEvent): string {
  const sequence = typeof event.data.sequence === 'number'
    ? ` #${event.data.sequence}`
    : '';
  if (event.name === 'device.button') {
    const action = event.data.action;
    const name = action === 'single'
      ? '主按钮单击'
      : action === 'double'
        ? '主按钮双击'
        : `主按钮 ${String(action)}`;
    return `${name}${sequence}`;
  }
  const gesture = String(event.data.gesture ?? '未知');
  const names: Record<string, string> = {
    headRaise: '抬头',
    headLower: '低头',
    headRaiseTimeout: '持续抬头超时',
    headLowerTimeout: '持续低头超时',
    left: '向左转头',
    right: '向右转头',
    nod: '点头',
    shake: '摇头',
  };
  return `${names[gesture] ?? gesture}${sequence}，active=${String(event.data.active)}`;
}

function describeIgnoredReason(reason: string): string {
  switch (reason) {
    case 'debounced': return '已忽略：300ms 内同方向重复上报';
    case 'verticalRebound': return '已忽略：600ms 内上下反向事件判定为回正';
    case 'timeoutAlreadySeen': return '已忽略：对应普通姿态事件已经收到';
    case 'inactive': return '已忽略：手势未激活';
    default: return '已忽略：当前游戏未绑定该指令';
  }
}

function describeAction(action: GameAction): string {
  const names: Record<GameAction['type'], string> = {
    up: '执行向上移动',
    down: '执行向下移动',
    left: '执行向左移动',
    right: '执行向右移动',
    place: '执行落子',
    restart: '执行重新开始',
  };
  return names[action.type];
}

function describeStateChange(
  action: GameAction,
  before: typeof state,
  after: typeof state,
): string {
  if (action.type === 'restart') return '已重新开始，当前选择：中间';
  if (action.type === 'place') {
    return `已在${positionName(before.cursor)}落子，当前状态：${resultText(after.result)}`;
  }
  return `已移动：${positionName(before.cursor)} → ${positionName(after.cursor)}`;
}

function describeNoStateChange(action: GameAction): string {
  if (state.result !== 'playing') return `未执行${describeAction(action).replace('执行', '')}：棋局已结束，请双击重开`;
  if (action.type === 'place') return '未落子：当前格已被占用';
  return `未执行${describeAction(action).replace('执行', '')}：该方向没有可选空格`;
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
  bridgeStatus.textContent = 'App Bridge 已连接，正在恢复棋局…';
  try {
    state = await persistence.load();
  } catch (error) {
    state = newGame();
    log(`读取存档失败，已开始新局：${errorMessage(error)}`);
  }
  render();

  bridge.on('device.button', onDeviceEvent);
  bridge.on('device.imuGesture', onDeviceEvent);
  bridge.on('device.connection', (event) => {
    if (event.data.connected === true) void present();
    if (event.data.connected === false) {
      presentationGenerations.invalidate();
      bridgeStatus.textContent = '设备已断开，网页棋局仍可继续';
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
    log(`设备事件已订阅：${subscriptionId}`);
  } catch (error) {
    log(`设备事件订阅失败，网页仍可操作：${errorMessage(error)}`);
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
  bridgeStatus.textContent = '插件启动失败';
  log(errorMessage(error));
});
