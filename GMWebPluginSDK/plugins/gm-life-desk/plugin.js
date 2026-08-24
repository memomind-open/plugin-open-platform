import { createGMPlugin } from './vendor/gm-plugin-web-sdk.esm.js';

const $ = (selector) => document.querySelector(selector);
const canvas = $('#deviceCanvas');
const ctx = canvas.getContext('2d', { willReadFrequently: true });
const W = 600;
const H = 350;
const APPS = ['weather', 'typhoon', 'calculator', 'focus', 'clock', 'memo'];
const APP_LABELS = { home: '桌面', weather: '天气', typhoon: '台风', calculator: '计算器', focus: '专注', clock: '世界时钟', memo: '便签' };
const WEATHER = {
  0: ['晴朗', 'sun'], 1: ['大部晴朗', 'sun'], 2: ['局部多云', 'cloud'], 3: ['阴天', 'cloud'],
  45: ['有雾', 'fog'], 48: ['雾凇', 'fog'], 51: ['小雨', 'rain'], 53: ['毛毛雨', 'rain'], 55: ['较强毛雨', 'rain'],
  61: ['小雨', 'rain'], 63: ['中雨', 'rain'], 65: ['大雨', 'rain'], 71: ['小雪', 'snow'], 73: ['中雪', 'snow'], 75: ['大雪', 'snow'],
  80: ['阵雨', 'rain'], 81: ['较强阵雨', 'rain'], 82: ['强阵雨', 'rain'], 95: ['雷雨', 'storm'], 96: ['雷雨冰雹', 'storm'], 99: ['强雷雨', 'storm']
};
const CALC_KEYS = ['7', '8', '9', '÷', '4', '5', '6', '×', '1', '2', '3', '−', 'C', '0', '.', '+', '⌫', '±', '%', '='];
const CLOCKS = [['上海', 'Asia/Shanghai'], ['东京', 'Asia/Tokyo'], ['伦敦', 'Europe/London'], ['纽约', 'America/New_York']];

let gm;
let bridgeReady = false;
let deviceConnected = true;
let pageCreated = false;
let presenting = false;
let pendingPresent = false;
let lz4Available = true;
let atomicFrameAvailable = false;
let nextFrameId = 1;
let presentedTiles = new Map();
let renderTimer = null;
let renderRevision = 0;
let screen = 'home';
let homeIndex = 0;
let weatherOffset = 0;
let stormIndex = 0;
let calcIndex = 0;
let clockIndex = 0;
let weather = null;
let storms = [];
let place = { name: '上海', latitude: 31.2304, longitude: 121.4737 };
let memo = '今天也要保持好奇。\n\n双击设备按键可随时返回桌面。';
let calc = { display: '0', left: null, op: null, waiting: false, expression: '' };
let focusMinutes = 25;
let focusRemaining = 25 * 60;
let focusRunning = false;
let focusEndsAt = 0;
let focusTimer = null;
let toastTimer;

const shade = (level) => {
  const value = Math.max(0, Math.min(15, Math.round(level))) * 17;
  return `rgb(${value},${value},${value})`;
};

function font(size, weight = 500) {
  ctx.font = `${weight} ${size}px "PingFang SC","SF Pro Display",system-ui,sans-serif`;
  ctx.textBaseline = 'middle';
}

function pathRoundRect(x, y, width, height, radius) {
  const r = Math.min(radius, width / 2, height / 2);
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + width, y, x + width, y + height, r);
  ctx.arcTo(x + width, y + height, x, y + height, r);
  ctx.arcTo(x, y + height, x, y, r);
  ctx.arcTo(x, y, x + width, y, r);
  ctx.closePath();
}

function fillRoundRect(x, y, width, height, radius, level) {
  // Low-luminance fills become broad green blocks on the optical display.
  // Keep panel interiors black and preserve hierarchy with outlines and text.
  ctx.fillStyle = shade(level <= 4 ? 0 : level);
  pathRoundRect(x, y, width, height, radius);
  ctx.fill();
}

function strokeRoundRect(x, y, width, height, radius, level, lineWidth = 1) {
  ctx.strokeStyle = shade(level);
  ctx.lineWidth = lineWidth;
  pathRoundRect(x, y, width, height, radius);
  ctx.stroke();
}

function line(x1, y1, x2, y2, level = 4, width = 1) {
  ctx.strokeStyle = shade(level);
  ctx.lineWidth = width;
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();
}

function text(value, x, y, size = 16, level = 15, weight = 500, align = 'left') {
  font(size, weight);
  ctx.fillStyle = shade(level);
  ctx.textAlign = align;
  ctx.fillText(String(value), x, y);
}

function fitText(value, x, y, maxWidth, size = 16, level = 15, weight = 500, align = 'left') {
  font(size, weight);
  let output = String(value ?? '');
  while (output.length > 1 && ctx.measureText(output).width > maxWidth) output = `${output.slice(0, -2)}…`;
  text(output, x, y, size, level, weight, align);
}

function drawIcon(name, cx, cy, size = 26, level = 13) {
  ctx.strokeStyle = shade(level);
  ctx.fillStyle = shade(level);
  ctx.lineWidth = 2;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  if (name === 'sun') {
    ctx.beginPath(); ctx.arc(cx, cy, size * .22, 0, Math.PI * 2); ctx.stroke();
    for (let i = 0; i < 8; i += 1) { const a = i * Math.PI / 4; line(cx + Math.cos(a) * size * .34, cy + Math.sin(a) * size * .34, cx + Math.cos(a) * size * .48, cy + Math.sin(a) * size * .48, level, 2); }
  } else if (name === 'cloud' || name === 'rain' || name === 'storm') {
    ctx.beginPath(); ctx.arc(cx - size * .15, cy, size * .2, Math.PI, 0); ctx.arc(cx + size * .08, cy - size * .08, size * .28, Math.PI, 0); ctx.arc(cx + size * .28, cy, size * .17, Math.PI, 0); ctx.stroke();
    line(cx - size * .35, cy, cx + size * .45, cy, level, 2);
    if (name === 'rain') for (let i = -1; i <= 1; i += 1) line(cx + i * size * .18, cy + size * .14, cx + i * size * .18 - 3, cy + size * .29, level, 2);
    if (name === 'storm') { line(cx + 2, cy + size * .09, cx - 4, cy + size * .28, level, 3); line(cx - 4, cy + size * .28, cx + 6, cy + size * .24, level, 3); }
  } else if (name === 'fog') {
    for (let i = -1; i <= 1; i += 1) line(cx - size * .42, cy + i * 7, cx + size * .42, cy + i * 7, level, 2);
  } else if (name === 'snow') {
    for (let i = 0; i < 3; i += 1) { const a = i * Math.PI / 3; line(cx - Math.cos(a) * size * .4, cy - Math.sin(a) * size * .4, cx + Math.cos(a) * size * .4, cy + Math.sin(a) * size * .4, level, 2); }
  } else if (name === 'typhoon') {
    ctx.beginPath(); ctx.arc(cx, cy, size * .42, -.5, Math.PI * 1.15); ctx.stroke();
    ctx.beginPath(); ctx.arc(cx, cy, size * .23, Math.PI * .5, Math.PI * 2.2); ctx.stroke();
    ctx.beginPath(); ctx.arc(cx, cy, 2, 0, Math.PI * 2); ctx.fill();
  } else if (name === 'calculator') {
    strokeRoundRect(cx - size * .38, cy - size * .45, size * .76, size * .9, 4, level, 2);
    line(cx - size * .25, cy - size * .15, cx + size * .25, cy - size * .15, level, 2);
    for (let row = 0; row < 2; row += 1) for (let col = 0; col < 2; col += 1) { ctx.beginPath(); ctx.arc(cx - 6 + col * 12, cy + 2 + row * 11, 1.5, 0, Math.PI * 2); ctx.fill(); }
  } else if (name === 'focus') {
    ctx.beginPath(); ctx.arc(cx, cy, size * .4, -.5 * Math.PI, Math.PI * 1.15); ctx.stroke();
    line(cx, cy, cx + size * .2, cy - size * .17, level, 2);
    ctx.beginPath(); ctx.arc(cx, cy, 2, 0, Math.PI * 2); ctx.fill();
  } else if (name === 'clock') {
    ctx.beginPath(); ctx.arc(cx, cy, size * .4, 0, Math.PI * 2); ctx.stroke();
    line(cx, cy, cx, cy - size * .23, level, 2); line(cx, cy, cx + size * .2, cy + size * .08, level, 2);
  } else if (name === 'memo') {
    strokeRoundRect(cx - size * .34, cy - size * .43, size * .68, size * .86, 3, level, 2);
    for (let i = -1; i <= 1; i += 1) line(cx - size * .2, cy + i * 9, cx + size * .2, cy + i * 9, level, 1);
  }
  ctx.lineCap = 'butt';
}

function clearScreen() {
  ctx.fillStyle = shade(0);
  ctx.fillRect(0, 0, W, H);
}

function drawHeader(title) {
  text('◌  浮光 OS', 18, 18, 13, 12, 650);
  text(title, 300, 18, 13, 8, 550, 'center');
  const now = new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', hour12: false });
  text(now, 554, 18, 13, 13, 650, 'right');
  strokeRoundRect(565, 12, 18, 11, 3, 8, 1);
  ctx.fillStyle = shade(deviceConnected ? 12 : 3); ctx.fillRect(568, 15, deviceConnected ? 11 : 4, 5);
  line(16, 36, 584, 36, 3, 1);
}

function drawFooter(hint = '← → 选择     单击 确认     双击 桌面') {
  line(16, 328, 584, 328, 3, 1);
  text(hint, 300, 340, 10, 7, 500, 'center');
}

function getWeatherInfo(code) { return WEATHER[code] || ['天气变化', 'cloud']; }

function renderHome() {
  drawHeader('桌面');
  fillRoundRect(18, 48, 156, 112, 16, 2);
  strokeRoundRect(18, 48, 156, 112, 16, 5);
  const now = new Date();
  text(now.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', hour12: false }), 34, 91, 38, 15, 650);
  text(now.toLocaleDateString('zh-CN', { month: 'long', day: 'numeric', weekday: 'short' }), 35, 132, 12, 8, 500);

  fillRoundRect(184, 48, 398, 112, 16, 1);
  strokeRoundRect(184, 48, 398, 112, 16, 4);
  if (weather?.current) {
    const [label, icon] = getWeatherInfo(weather.current.weather_code);
    drawIcon(icon, 222, 95, 45, 13);
    text(`${Math.round(weather.current.temperature_2m)}°`, 266, 86, 37, 15, 650);
    text(label, 269, 119, 13, 10, 550);
    text(place.name, 552, 70, 14, 12, 650, 'right');
    text(`体感 ${Math.round(weather.current.apparent_temperature)}°`, 552, 97, 12, 8, 500, 'right');
    text(`湿度 ${weather.current.relative_humidity_2m}%  风 ${Math.round(weather.current.wind_speed_10m)}km/h`, 552, 124, 11, 7, 500, 'right');
  } else {
    drawIcon('cloud', 225, 98, 42, 7);
    text('天气载入中', 268, 91, 22, 12, 600);
    text(place.name, 268, 121, 12, 7);
  }

  const gap = 7;
  const cardW = 88;
  for (let i = 0; i < APPS.length; i += 1) {
    const x = 18 + i * (cardW + gap);
    const selected = i === homeIndex;
    fillRoundRect(x, 176, cardW, 139, 14, selected ? 4 : 1);
    strokeRoundRect(x, 176, cardW, 139, 14, selected ? 15 : 4, selected ? 2 : 1);
    const appIcon = APPS[i] === 'weather'
      ? getWeatherInfo(weather?.current?.weather_code)[1]
      : APPS[i];
    drawIcon(appIcon, x + cardW / 2, 218, 35, selected ? 15 : 10);
    text(APP_LABELS[APPS[i]], x + cardW / 2, 263, 13, selected ? 15 : 10, 600, 'center');
    const sub = APPS[i] === 'weather' ? '预报' : APPS[i] === 'typhoon' ? `${storms.length} 活跃` : APPS[i] === 'calculator' ? '计算' : APPS[i] === 'focus' ? `${focusMinutes} 分` : APPS[i] === 'clock' ? '四地' : '速记';
    text(sub, x + cardW / 2, 286, 10, selected ? 11 : 6, 500, 'center');
    if (selected) { fillRoundRect(x + 31, 302, 26, 3, 2, 14); }
  }
  drawFooter('← → 选择     ↑ ↓ 跳转     单击 打开');
}

function renderWeather() {
  drawHeader('天气');
  const current = weather?.current;
  const [label, icon] = getWeatherInfo(current?.weather_code);
  fillRoundRect(18, 49, 188, 155, 16, 2);
  strokeRoundRect(18, 49, 188, 155, 16, 5);
  drawIcon(icon, 62, 91, 46, 13);
  text(current ? `${Math.round(current.temperature_2m)}°` : '--°', 102, 86, 43, 15, 650);
  text(label, 35, 139, 17, 12, 600);
  text(place.name, 35, 169, 12, 8);
  if (current) text(`体感 ${Math.round(current.apparent_temperature)}°  湿度 ${current.relative_humidity_2m}%`, 35, 190, 11, 7);

  fillRoundRect(218, 49, 364, 155, 16, 1);
  strokeRoundRect(218, 49, 364, 155, 16, 4);
  text('未来 6 小时', 238, 67, 11, 8, 600);
  if (weather?.hourly) {
    const start = Math.max(0, weather.hourly.time.findIndex((item) => new Date(item) >= new Date())) + weatherOffset;
    const temps = weather.hourly.temperature_2m.slice(start, start + 6);
    const min = Math.min(...temps) - 1;
    const max = Math.max(...temps) + 1;
    const points = [];
    temps.forEach((temp, i) => {
      const x = 244 + i * 61;
      const y = 142 - ((temp - min) / Math.max(1, max - min)) * 48;
      points.push([x, y]);
      text(`${Math.round(temp)}°`, x, y - 15, 11, 11, 600, 'center');
      text(new Date(weather.hourly.time[start + i]).toLocaleTimeString('zh-CN', { hour: '2-digit', hour12: false }), x, 181, 9, 6, 500, 'center');
      if (i) line(points[i - 1][0], points[i - 1][1], x, y, 10, 2);
      ctx.fillStyle = shade(i === 0 ? 15 : 10); ctx.beginPath(); ctx.arc(x, y, 3, 0, Math.PI * 2); ctx.fill();
    });
  } else text('正在获取预报…', 400, 126, 17, 8, 500, 'center');

  const metrics = [
    ['降水', weather?.hourly ? `${weather.hourly.precipitation_probability[Math.max(0, weather.hourly.time.findIndex((item) => new Date(item) >= new Date()))]}%` : '--'],
    ['风速', current ? `${Math.round(current.wind_speed_10m)} km/h` : '--'],
    ['湿度', current ? `${current.relative_humidity_2m}%` : '--']
  ];
  metrics.forEach(([name, value], i) => {
    const x = 18 + i * 194;
    fillRoundRect(x, 217, 182, 96, 13, 1);
    strokeRoundRect(x, 217, 182, 96, 13, 4);
    text(name, x + 16, 240, 11, 7, 500);
    text(value, x + 16, 276, 22, 14, 650);
    fillRoundRect(x + 16, 296, Math.min(145, i === 0 ? Number.parseInt(value, 10) * 1.45 || 10 : i === 1 ? 90 : 112), 4, 2, 9);
  });
  drawFooter('← → 时间轴     长按 刷新     双击 桌面');
}

function renderTyphoon() {
  drawHeader('全球台风');
  if (!storms.length) {
    drawIcon('typhoon', 300, 126, 84, 11);
    text('当前没有活跃热带气旋', 300, 209, 24, 14, 650, 'center');
    text('数据来源 GDACS · 长按刷新', 300, 246, 12, 7, 500, 'center');
    drawFooter('长按 刷新     双击 桌面');
    return;
  }
  const storm = storms[stormIndex % storms.length];
  fillRoundRect(18, 49, 340, 264, 16, 1);
  strokeRoundRect(18, 49, 340, 264, 16, 5);
  text(`活跃系统 ${stormIndex + 1} / ${storms.length}`, 38, 73, 11, 7, 600);
  fitText(storm.eventname || storm.name, 38, 109, 280, 28, 15, 650);
  fillRoundRect(38, 132, 88, 24, 12, storm.alertlevel === 'Red' ? 12 : storm.alertlevel === 'Orange' ? 8 : 5);
  text(`${storm.alertlevel || 'Green'} 警报`, 82, 144, 10, 15, 650, 'center');
  text(storm.country || '开放海域', 38, 181, 14, 11, 550);
  const point = Array.isArray(storm.coordinates) ? `${Number(storm.coordinates[1]).toFixed(1)}°, ${Number(storm.coordinates[0]).toFixed(1)}°` : '位置更新中';
  text(`坐标  ${point}`, 38, 214, 12, 8);
  text(`来源  ${storm.source || 'GDACS'}`, 38, 241, 12, 8);
  text(`更新  ${new Date(storm.datemodified).toLocaleString('zh-CN', { month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit' })}`, 38, 268, 12, 8);
  text('信息仅供参考，请以当地官方预警为准', 38, 296, 10, 6);

  fillRoundRect(370, 49, 212, 264, 16, 1);
  strokeRoundRect(370, 49, 212, 264, 16, 4);
  for (let r = 28; r <= 82; r += 27) { ctx.strokeStyle = shade(3); ctx.lineWidth = 1; ctx.beginPath(); ctx.arc(476, 170, r, 0, Math.PI * 2); ctx.stroke(); }
  line(390, 170, 562, 170, 3); line(476, 84, 476, 256, 3);
  drawIcon('typhoon', 476, 170, 70, 14);
  ctx.fillStyle = shade(14); ctx.beginPath(); ctx.arc(476, 170, 4, 0, Math.PI * 2); ctx.fill();
  text('热带气旋定位', 476, 281, 11, 8, 550, 'center');
  drawFooter('↑ ↓ 切换系统     长按 刷新     双击 桌面');
}

function renderCalculator() {
  drawHeader('计算器');
  fillRoundRect(18, 48, 564, 62, 14, 2);
  strokeRoundRect(18, 48, 564, 62, 14, 5);
  fitText(calc.expression, 36, 65, 300, 11, 7, 500);
  fitText(calc.display, 558, 86, 440, 30, 15, 650, 'right');
  const gap = 7;
  const keyW = 135.75;
  const keyH = 36;
  CALC_KEYS.forEach((key, index) => {
    const col = index % 4;
    const row = Math.floor(index / 4);
    const x = 18 + col * (keyW + gap);
    const y = 120 + row * (keyH + 5);
    const selected = index === calcIndex;
    fillRoundRect(x, y, keyW, keyH, 9, selected ? 13 : ['÷', '×', '−', '+', '='].includes(key) ? 3 : 1);
    strokeRoundRect(x, y, keyW, keyH, 9, selected ? 15 : 4, selected ? 2 : 1);
    text(key, x + keyW / 2, y + keyH / 2 + 1, 16, selected ? 0 : 13, 650, 'center');
  });
  drawFooter('← → ↑ ↓ 选键     单击 输入     长按 清除');
}

function renderFocus() {
  drawHeader('专注');
  const total = focusMinutes * 60;
  const visualElapsed = Math.floor((total - focusRemaining) / 5) * 5;
  const progress = total ? visualElapsed / total : 0;
  const focusCenterX = 145;
  ctx.strokeStyle = shade(3); ctx.lineWidth = 12; ctx.beginPath(); ctx.arc(focusCenterX, 177, 96, 0, Math.PI * 2); ctx.stroke();
  ctx.strokeStyle = shade(13); ctx.lineWidth = 12; ctx.lineCap = 'round'; ctx.beginPath(); ctx.arc(focusCenterX, 177, 96, -.5 * Math.PI, -.5 * Math.PI + Math.PI * 2 * progress); ctx.stroke(); ctx.lineCap = 'butt';
  const mm = String(Math.floor(focusRemaining / 60)).padStart(2, '0');
  const ss = String(focusRemaining % 60).padStart(2, '0');
  text(`${mm}:${ss}`, focusCenterX, 145, 36, 15, 650, 'center');
  text(focusRunning ? '正在专注' : '等待开始', focusCenterX, 205, 12, focusRunning ? 12 : 7, 550, 'center');
  fillRoundRect(314, 61, 268, 79, 14, 1); strokeRoundRect(314, 61, 268, 79, 14, 4);
  text('当前模式', 332, 82, 10, 7); text(`${focusMinutes} 分钟`, 332, 113, 22, 14, 650);
  fillRoundRect(314, 153, 268, 79, 14, focusRunning ? 4 : 1); strokeRoundRect(314, 153, 268, 79, 14, focusRunning ? 13 : 4);
  text(focusRunning ? '单击暂停' : '单击开始', 332, 177, 11, 8); text(focusRunning ? '保持节奏' : '进入心流', 332, 207, 21, 14, 650);
  fillRoundRect(314, 245, 268, 68, 14, 1); strokeRoundRect(314, 245, 268, 68, 14, 4);
  text('← → 切换  25 / 5 / 50 分钟', 332, 279, 12, 9, 550);
  drawFooter('← → 时长     单击 开始/暂停     长按 重置');
}

function renderClock() {
  drawHeader('世界时钟');
  CLOCKS.forEach(([name, zone], index) => {
    const col = index % 2;
    const row = Math.floor(index / 2);
    const x = 18 + col * 286;
    const y = 51 + row * 132;
    const selected = index === clockIndex;
    fillRoundRect(x, y, 278, 120, 15, selected ? 3 : 1);
    strokeRoundRect(x, y, 278, 120, 15, selected ? 14 : 4, selected ? 2 : 1);
    drawIcon('clock', x + 42, y + 40, 31, selected ? 14 : 8);
    text(name, x + 69, y + 38, 15, selected ? 15 : 10, 650);
    const time = new Date().toLocaleTimeString('zh-CN', { timeZone: zone, hour: '2-digit', minute: '2-digit', hour12: false });
    text(time, x + 18, y + 84, 30, selected ? 15 : 12, 650);
    text(new Date().toLocaleDateString('zh-CN', { timeZone: zone, month: 'numeric', day: 'numeric', weekday: 'short' }), x + 255, y + 87, 11, 7, 500, 'right');
  });
  drawFooter('← → ↑ ↓ 选择城市     双击 桌面');
}

function wrapText(value, maxWidth, maxLines = 7) {
  font(17, 500);
  const lines = [];
  for (const paragraph of String(value || '暂无便签').split('\n')) {
    let current = '';
    for (const char of paragraph || ' ') {
      if (ctx.measureText(current + char).width > maxWidth && current) { lines.push(current); current = char; } else current += char;
      if (lines.length >= maxLines) break;
    }
    if (lines.length < maxLines) lines.push(current);
    if (lines.length >= maxLines) break;
  }
  if (lines.length === maxLines && lines.join('').length < String(value).replace(/\n/g, '').length) lines[maxLines - 1] = `${lines[maxLines - 1].slice(0, -1)}…`;
  return lines;
}

function renderMemo() {
  drawHeader('便签');
  fillRoundRect(18, 49, 564, 264, 16, 1);
  strokeRoundRect(18, 49, 564, 264, 16, 5);
  drawIcon('memo', 52, 81, 28, 12);
  text('QUICK MEMO', 80, 80, 11, 7, 650);
  line(38, 104, 562, 104, 3);
  const lines = wrapText(memo, 505, 7);
  lines.forEach((item, index) => text(item, 43, 132 + index * 28, 17, index === 0 ? 15 : 11, index === 0 ? 600 : 500));
  drawFooter('在手机控制台编辑内容     双击 桌面');
}

function renderDevice() {
  clearScreen();
  if (screen === 'home') renderHome();
  else if (screen === 'weather') renderWeather();
  else if (screen === 'typhoon') renderTyphoon();
  else if (screen === 'calculator') renderCalculator();
  else if (screen === 'focus') renderFocus();
  else if (screen === 'clock') renderClock();
  else if (screen === 'memo') renderMemo();
  $('#screenName').textContent = APP_LABELS[screen];
}

function toGray4Bytes() {
  const pixels = ctx.getImageData(0, 0, W, H).data;
  const bytes = new Uint8Array((W * H) / 2);
  let output = 0;
  for (let i = 0; i < pixels.length; i += 8) {
    const left = Math.round((pixels[i] * .2126 + pixels[i + 1] * .7152 + pixels[i + 2] * .0722) / 17);
    const right = Math.round((pixels[i + 4] * .2126 + pixels[i + 5] * .7152 + pixels[i + 6] * .0722) / 17);
    bytes[output] = (Math.min(15, left) << 4) | Math.min(15, right);
    output += 1;
  }
  return bytes;
}

function bytesToBase64(bytes) {
  let binary = '';
  const chunk = 8192;
  for (let i = 0; i < bytes.length; i += chunk) binary += String.fromCharCode(...bytes.subarray(i, i + chunk));
  return btoa(binary);
}

function lz4CompressBlock(input) {
  const sourceLength = input.length;
  const hashTable = new Int32Array(65536);
  hashTable.fill(-1);
  const output = new Uint8Array(sourceLength + Math.ceil(sourceLength / 255) + 32);
  const matchFindLimit = sourceLength - 12;
  const matchCopyLimit = sourceLength - 5;
  let inputOffset = 0;
  let anchor = 0;
  let outputOffset = 0;

  const read32 = (offset) => (
    input[offset]
    | (input[offset + 1] << 8)
    | (input[offset + 2] << 16)
    | (input[offset + 3] << 24)
  ) >>> 0;
  const hash = (value) => (Math.imul(value, 2654435761) >>> 16) & 0xffff;
  const writeLength = (length) => {
    let remaining = length;
    while (remaining >= 255) { output[outputOffset] = 255; outputOffset += 1; remaining -= 255; }
    output[outputOffset] = remaining;
    outputOffset += 1;
  };

  while (inputOffset <= matchFindLimit) {
    const sequence = read32(inputOffset);
    const hashValue = hash(sequence);
    const reference = hashTable[hashValue];
    hashTable[hashValue] = inputOffset;
    const canMatch = reference >= 0
      && inputOffset - reference <= 65535
      && read32(reference) === sequence;
    if (!canMatch) { inputOffset += 1; continue; }

    const tokenOffset = outputOffset;
    outputOffset += 1;
    const literalLength = inputOffset - anchor;
    let token = Math.min(literalLength, 15) << 4;
    if (literalLength >= 15) writeLength(literalLength - 15);
    output.set(input.subarray(anchor, inputOffset), outputOffset);
    outputOffset += literalLength;

    const distance = inputOffset - reference;
    output[outputOffset] = distance & 0xff;
    output[outputOffset + 1] = distance >>> 8;
    outputOffset += 2;

    const matchStart = inputOffset;
    let matchReference = reference;
    inputOffset += 4;
    matchReference += 4;
    while (inputOffset < matchCopyLimit && input[inputOffset] === input[matchReference]) {
      inputOffset += 1;
      matchReference += 1;
    }
    const encodedMatchLength = inputOffset - matchStart - 4;
    token |= Math.min(encodedMatchLength, 15);
    output[tokenOffset] = token;
    if (encodedMatchLength >= 15) writeLength(encodedMatchLength - 15);
    anchor = inputOffset;
  }

  const literalLength = sourceLength - anchor;
  const tokenOffset = outputOffset;
  outputOffset += 1;
  output[tokenOffset] = Math.min(literalLength, 15) << 4;
  if (literalLength >= 15) writeLength(literalLength - 15);
  output.set(input.subarray(anchor), outputOffset);
  outputOffset += literalLength;
  return output.slice(0, outputOffset);
}

function extractGray4Region(frame, sourceWidth, x, y, width, height) {
  const sourceStride = sourceWidth / 2;
  const tileStride = width / 2;
  const tile = new Uint8Array(tileStride * height);
  const xBytes = x / 2;
  for (let row = 0; row < height; row += 1) {
    const start = (y + row) * sourceStride + xBytes;
    tile.set(frame.subarray(start, start + tileStride), row * tileStride);
  }
  return tile;
}

async function presentRawFallback(region, geometry) {
  const maxWidth = 200;
  const maxHeight = 175;
  for (let relativeY = 0; relativeY < geometry.height; relativeY += maxHeight) {
    for (let relativeX = 0; relativeX < geometry.width; relativeX += maxWidth) {
      const width = Math.min(maxWidth, geometry.width - relativeX);
      const height = Math.min(maxHeight, geometry.height - relativeY);
      const tile = extractGray4Region(region, geometry.width, relativeX, relativeY, width, height);
      await gm.display.updateImage({
        x: geometry.x + relativeX,
        y: geometry.y + relativeY,
        width,
        height,
        stride: width / 2,
        dataBase64: bytesToBase64(tile)
      });
    }
  }
}

async function presentTile(tile, geometry) {
  const stride = geometry.width / 2;
  if (lz4Available) {
    const compressed = lz4CompressBlock(tile);
    try {
      await gm.display.updateImageLz4({
        ...geometry,
        stride,
        decodedSize: tile.length,
        dataBase64: bytesToBase64(compressed)
      });
      return;
    } catch (error) {
      if (!['METHOD_NOT_FOUND', 'CAPABILITY_UNAVAILABLE', 'TIMEOUT'].includes(error.code)) throw error;
      lz4Available = false;
      console.warn(`LZ4 unavailable (${error.code}); falling back to tiled GRAY_4`);
    }
  }
  await presentRawFallback(tile, geometry);
}

function nextAtomicFrameId() {
  const frameId = nextFrameId;
  nextFrameId = nextFrameId >= 0xffffffff ? 1 : nextFrameId + 1;
  return frameId;
}

async function presentAtomicFrame(updates) {
  const frameId = nextAtomicFrameId();
  try {
    await gm.display.beginFrame({ frameId, tileCount: updates.length });
  } catch (error) {
    if (['METHOD_NOT_FOUND', 'CAPABILITY_UNAVAILABLE'].includes(error.code)) {
      atomicFrameAvailable = false;
      if (error.code === 'CAPABILITY_UNAVAILABLE') lz4Available = false;
      return false;
    }
    throw error;
  }

  for (let tileIndex = 0; tileIndex < updates.length; tileIndex += 1) {
    const update = updates[tileIndex];
    const stride = update.geometry.width / 2;
    await gm.display.updateFrameImageLz4({
      frameId,
      tileIndex,
      ...update.geometry,
      stride,
      decodedSize: update.tile.length,
      dataBase64: bytesToBase64(update.compressed)
    });
  }
  return true;
}

async function presentDevice() {
  if (!bridgeReady || !deviceConnected) return;
  if (presenting) { pendingPresent = true; return; }
  presenting = true;
  const revision = renderRevision;
  try {
    if (!pageCreated) {
      await gm.display.createPage();
      pageCreated = true;
      presentedTiles = new Map();
    }
    const frame = toGray4Bytes();
    const tileWidth = 200;
    const tileHeight = 175;
    const updates = [];
    for (let y = 0; y < H; y += tileHeight) {
      for (let x = 0; x < W; x += tileWidth) {
        const key = `${x}:${y}`;
        const tile = extractGray4Region(frame, W, x, y, tileWidth, tileHeight);
        const previous = presentedTiles.get(key);
        let unchanged = Boolean(previous && previous.length === tile.length);
        if (unchanged) {
          for (let index = 0; index < tile.length; index += 1) {
            if (tile[index] !== previous[index]) { unchanged = false; break; }
          }
        }
        if (unchanged) continue;
        updates.push({
          key,
          tile,
          compressed: lz4CompressBlock(tile),
          geometry: { x, y, width: tileWidth, height: tileHeight }
        });
      }
    }
    if (updates.length === 0 || revision !== renderRevision) return;

    if (atomicFrameAvailable && lz4Available) {
      const presented = await presentAtomicFrame(updates);
      if (presented) {
        for (const update of updates) presentedTiles.set(update.key, update.tile);
        return;
      }
    }
    for (const update of updates) {
      await presentTile(update.tile, update.geometry);
      presentedTiles.set(update.key, update.tile);
    }
  } catch (error) {
    if (error.code === 'DEVICE_DISCONNECTED') deviceConnected = false;
    presentedTiles = new Map();
    console.warn('Device presentation failed', error);
  } finally {
    presenting = false;
    if (pendingPresent) { pendingPresent = false; void presentDevice(); }
  }
}

function requestRender() {
  renderRevision += 1;
  clearTimeout(renderTimer);
  renderTimer = setTimeout(() => {
    renderDevice();
    void presentDevice();
  }, 60);
}

function setScreen(next) {
  screen = next;
  requestRender();
}

function moveSelection(dx, dy = 0) {
  if (screen === 'home') {
    if (dy) homeIndex = (homeIndex + dy * 3 + APPS.length) % APPS.length;
    else homeIndex = (homeIndex + dx + APPS.length) % APPS.length;
  } else if (screen === 'weather') weatherOffset = Math.max(0, Math.min(12, weatherOffset + dx));
  else if (screen === 'typhoon' && storms.length) stormIndex = (stormIndex + (dy || dx) + storms.length) % storms.length;
  else if (screen === 'calculator') {
    const row = Math.floor(calcIndex / 4);
    const col = calcIndex % 4;
    calcIndex = ((row + dy + 5) % 5) * 4 + ((col + dx + 4) % 4);
  } else if (screen === 'focus' && !focusRunning && dx) {
    const options = [25, 5, 50];
    const index = options.indexOf(focusMinutes);
    focusMinutes = options[(index + dx + options.length) % options.length];
    focusRemaining = focusMinutes * 60;
  } else if (screen === 'clock') {
    const row = Math.floor(clockIndex / 2);
    const col = clockIndex % 2;
    clockIndex = ((row + dy + 2) % 2) * 2 + ((col + dx + 2) % 2);
  }
  requestRender();
}

function activateSelection() {
  if (screen === 'home') setScreen(APPS[homeIndex]);
  else if (screen === 'calculator') pressCalculator(CALC_KEYS[calcIndex]);
  else if (screen === 'focus') toggleFocus();
  else if (screen === 'weather') { weatherOffset = weatherOffset ? 0 : 1; requestRender(); }
  else if (screen === 'typhoon' && storms.length) { stormIndex = (stormIndex + 1) % storms.length; requestRender(); }
}

function longAction() {
  if (screen === 'home') setScreen('weather');
  else if (screen === 'weather') { toast('正在刷新天气'); void fetchWeather(); }
  else if (screen === 'typhoon') { toast('正在刷新台风数据'); void fetchStorms(); }
  else if (screen === 'calculator') { calc = { display: '0', left: null, op: null, waiting: false, expression: '' }; requestRender(); }
  else if (screen === 'focus') resetFocus();
  else requestRender();
}

function pressCalculator(key) {
  const ops = { '÷': '/', '×': '*', '−': '-', '+': '+' };
  if (/^\d$/.test(key)) { calc.display = calc.waiting || calc.display === '0' ? key : calc.display + key; calc.waiting = false; }
  else if (key === '.' && !calc.display.includes('.')) calc.display += '.';
  else if (key === 'C') calc = { display: '0', left: null, op: null, waiting: false, expression: '' };
  else if (key === '⌫') calc.display = calc.display.length > 1 ? calc.display.slice(0, -1) : '0';
  else if (key === '±') calc.display = String(-Number(calc.display));
  else if (key === '%') calc.display = String(Number(calc.display) / 100);
  else if (ops[key]) {
    if (calc.op && !calc.waiting) calculate();
    calc.left = Number(calc.display); calc.op = ops[key]; calc.waiting = true; calc.expression = `${calc.display} ${key}`;
  } else if (key === '=') calculate();
  requestRender();
}

function calculate() {
  if (calc.left === null || !calc.op) return;
  const right = Number(calc.display);
  const result = calc.op === '+' ? calc.left + right : calc.op === '-' ? calc.left - right : calc.op === '*' ? calc.left * right : right === 0 ? NaN : calc.left / right;
  const symbol = calc.op === '*' ? '×' : calc.op === '/' ? '÷' : calc.op;
  calc.expression = `${calc.left} ${symbol} ${right} =`;
  calc.display = Number.isFinite(result) ? String(Number(result.toPrecision(12))) : '无法计算';
  calc.left = null; calc.op = null; calc.waiting = true;
}

function toggleFocus() {
  focusRunning = !focusRunning;
  clearInterval(focusTimer);
  if (focusRunning) {
    focusEndsAt = Date.now() + focusRemaining * 1000;
    focusTimer = setInterval(() => {
      focusRemaining = Math.max(0, Math.ceil((focusEndsAt - Date.now()) / 1000));
      if (screen === 'focus') requestRender();
      if (!focusRemaining) { focusRunning = false; clearInterval(focusTimer); toast('专注完成'); requestRender(); }
    }, 1000);
  }
  requestRender();
}

function resetFocus() {
  focusRunning = false;
  clearInterval(focusTimer);
  focusRemaining = focusMinutes * 60;
  requestRender();
}

async function storageGet(key, fallback = null) {
  try {
    if (bridgeReady) return (await gm.storage.get(key)).value ?? fallback;
    const raw = localStorage.getItem(`lifedesk:${key}`);
    return raw === null ? fallback : JSON.parse(raw);
  } catch { return fallback; }
}

async function storageSet(key, value) {
  try {
    if (bridgeReady) await gm.storage.set(key, value);
    else localStorage.setItem(`lifedesk:${key}`, JSON.stringify(value));
  } catch { /* Keep the device usable if storage is temporarily unavailable. */ }
}

async function fetchWeather() {
  try {
    const params = new URLSearchParams({
      latitude: String(place.latitude), longitude: String(place.longitude), timezone: 'auto',
      current: 'temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m',
      hourly: 'temperature_2m,weather_code,precipitation_probability', forecast_days: '2'
    });
    const response = await fetch(`https://api.open-meteo.com/v1/forecast?${params}`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    weather = await response.json();
    await storageSet('weather-cache', { weather, place, time: Date.now() });
  } catch {
    const cache = await storageGet('weather-cache');
    if (cache?.weather) { weather = cache.weather; place = cache.place || place; }
  }
  $('#cityInput').value = place.name;
  $('#dataState').textContent = weather ? `天气已更新 · ${place.name}` : '天气暂不可用';
  if (screen === 'home' || screen === 'weather') requestRender();
}

async function fetchStorms() {
  try {
    const to = new Date();
    const from = new Date(to.getTime() - 45 * 86400000);
    const day = (date) => date.toISOString().slice(0, 10);
    const response = await fetch(`https://www.gdacs.org/gdacsapi/api/events/geteventlist/SEARCH?eventlist=TC&fromdate=${day(from)}&todate=${day(to)}&alertlevel=red;orange;green`);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    const unique = new Map();
    for (const feature of data.features || []) {
      const item = feature.properties || {};
      if (String(item.iscurrent).toLowerCase() === 'true') unique.set(item.eventid, { ...item, coordinates: feature.geometry?.coordinates });
    }
    storms = [...unique.values()].sort((a, b) => new Date(b.datemodified) - new Date(a.datemodified));
    await storageSet('storms-cache', { storms, time: Date.now() });
  } catch {
    const cache = await storageGet('storms-cache');
    storms = cache?.storms || [];
  }
  stormIndex = Math.min(stormIndex, Math.max(0, storms.length - 1));
  $('#dataState').textContent = `${weather ? `天气已更新 · ${place.name}` : '天气暂不可用'} · ${storms.length} 个活跃气旋`;
  if (screen === 'home' || screen === 'typhoon') requestRender();
}

async function updateCity(query) {
  if (!query.trim()) return;
  try {
    $('#saveState').textContent = '正在查询…';
    const params = new URLSearchParams({ name: query.trim(), count: '5', language: 'zh', format: 'json' });
    const response = await fetch(`https://geocoding-api.open-meteo.com/v1/search?${params}`);
    const data = await response.json();
    const result = data.results?.[0];
    if (!result) throw new Error('NOT_FOUND');
    place = { name: result.name, latitude: result.latitude, longitude: result.longitude, country: result.country };
    await storageSet('weather-place', place);
    await fetchWeather();
    $('#saveState').textContent = '已保存';
    toast(`设备天气已切换到${place.name}`);
  } catch { $('#saveState').textContent = '查询失败'; toast('没有找到这个城市'); }
}

function toast(message) {
  const target = $('#toast');
  target.textContent = message;
  target.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => target.classList.remove('show'), 1600);
}

async function initializeBridge() {
  try {
    gm = createGMPlugin({ timeoutMs: 12000 });
    await gm.ready();
    const capabilities = await gm.runtime.getCapabilities();
    atomicFrameAvailable = capabilities.display?.includes('atomic-framed-lz4') === true;
    bridgeReady = true;
    const savedPlace = await storageGet('weather-place');
    const savedMemo = await storageGet('memo');
    if (savedPlace) place = savedPlace;
    if (savedMemo) memo = savedMemo;
    $('#cityInput').value = place.name;
    $('#memoInput').value = memo;
    $('#deviceState').classList.add('ready');
    $('#deviceState span').textContent = '设备已就绪';
    gm.device.onButton((event) => {
      if (event.action === 'single') activateSelection();
      else if (event.action === 'double') setScreen('home');
      else if (event.action === 'long') longAction();
    });
    gm.device.onGesture((event) => {
      if (!event.active) return;
      if (event.gesture === 'left') moveSelection(-1, 0);
      else if (event.gesture === 'right') moveSelection(1, 0);
      else if (event.gesture === 'headRaise') moveSelection(0, -1);
      else if (event.gesture === 'headLower') moveSelection(0, 1);
      else if (event.gesture === 'nod') activateSelection();
      else if (event.gesture === 'shake') setScreen('home');
    });
    gm.device.onConnection((event) => {
      deviceConnected = event.connected;
      pageCreated = false;
      presentedTiles = new Map();
      $('#deviceState').classList.toggle('ready', event.connected);
      $('#deviceState span').textContent = event.connected ? '设备已就绪' : '设备已断开';
      if (event.connected) requestRender();
    });
    await gm.device.subscribeEvents(['button', 'imuGesture', 'connection']);
    requestRender();
  } catch (error) {
    $('#deviceState span').textContent = '仅配置模式';
    console.warn('GM Bridge unavailable', error);
  }
}

$('#wakeDevice').addEventListener('click', () => { setScreen('home'); toast('设备桌面已点亮'); });
$('#cityForm').addEventListener('submit', (event) => { event.preventDefault(); void updateCity($('#cityInput').value); });
let memoTimer;
$('#memoInput').addEventListener('input', (event) => {
  memo = event.target.value;
  $('#saveState').textContent = '正在保存…';
  clearTimeout(memoTimer);
  memoTimer = setTimeout(async () => {
    await storageSet('memo', memo);
    $('#saveState').textContent = '已保存';
    if (screen === 'memo') requestRender();
  }, 400);
});
window.addEventListener('pagehide', () => { clearInterval(focusTimer); gm?.close(); });

renderDevice();
await initializeBridge();
await Promise.all([fetchWeather(), fetchStorms()]);
setInterval(() => { if (screen === 'home' || screen === 'clock') requestRender(); }, 60000);
