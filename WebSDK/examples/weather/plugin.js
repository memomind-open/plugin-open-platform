(() => {
  const apiOrigin = 'https://api.open-meteo.com';
  const cities = {
    beijing: { name: 'Beijing', latitude: 39.9042, longitude: 116.4074 },
    shanghai: { name: 'Shanghai', latitude: 31.2304, longitude: 121.4737 },
    shenzhen: { name: 'Shenzhen', latitude: 22.5431, longitude: 114.0579 },
    hangzhou: { name: 'Hangzhou', latitude: 30.2741, longitude: 120.1551 },
    chengdu: { name: 'Chengdu', latitude: 30.5728, longitude: 104.0668 },
  };

  const weatherLabels = new Map([
    [0, 'Clear'], [1, 'Mostly clear'], [2, 'Partly cloudy'], [3, 'Overcast'],
    [45, 'Fog'], [48, 'Rime fog'],
    [51, 'Light drizzle'], [53, 'Drizzle'], [55, 'Heavy drizzle'],
    [56, 'Freezing drizzle'], [57, 'Heavy freezing drizzle'],
    [61, 'Light rain'], [63, 'Rain'], [65, 'Heavy rain'],
    [66, 'Freezing rain'], [67, 'Heavy freezing rain'],
    [71, 'Light snow'], [73, 'Snow'], [75, 'Heavy snow'], [77, 'Snow grains'],
    [80, 'Light showers'], [81, 'Showers'], [82, 'Heavy showers'],
    [85, 'Light snow showers'], [86, 'Heavy snow showers'],
    [95, 'Thunderstorm'], [96, 'Thunderstorm with light hail'], [99, 'Thunderstorm with heavy hail'],
  ]);

  const pendingBridgeCalls = new Map();
  let bridgeToken = '';
  let runtimeGeneration = 0;
  let bridgeSequence = 0;
  let requestController = null;

  const elements = {
    city: document.getElementById('city'),
    refresh: document.getElementById('refresh'),
    badge: document.getElementById('network-badge'),
    card: document.getElementById('weather-card'),
    forecast: document.getElementById('forecast'),
    updatedAt: document.getElementById('updated-at'),
  };

  function bridgeCall(method, params = {}) {
    return new Promise((resolve, reject) => {
      if (!bridgeToken || !window.MemoPluginBridge) {
        reject(new Error('App Bridge is not ready'));
        return;
      }
      const requestId = `weather-${Date.now()}-${++bridgeSequence}`;
      const timer = window.setTimeout(() => {
        pendingBridgeCalls.delete(requestId);
        reject(new Error(`${method} timed out`));
      }, 7000);
      pendingBridgeCalls.set(requestId, { resolve, reject, timer });
      window.MemoPluginBridge.postMessage(JSON.stringify({
        version: '1.0',
        sessionToken: bridgeToken,
        requestId,
        method,
        params,
        runtimeGeneration,
      }));
    });
  }

  window.__memoPluginBootstrap = async (sessionToken, generation) => {
    bridgeToken = sessionToken;
    runtimeGeneration = generation;
    try {
      await bridgeCall('runtime.ready');
      await loadWeather();
    } catch (error) {
      renderError(error);
    }
  };

  window.__memoPluginResolve = (response) => {
    if (response.runtimeGeneration !== runtimeGeneration) return;
    const pending = pendingBridgeCalls.get(response.requestId);
    if (!pending) return;
    window.clearTimeout(pending.timer);
    pendingBridgeCalls.delete(response.requestId);
    if (response.ok) {
      pending.resolve(response.result);
    } else {
      pending.reject(new Error(response.error?.message || 'Bridge call failed'));
    }
  };

  window.__memoPluginEmit = () => {};
  window.__memoPluginHeartbeat = () => true;

  elements.refresh.addEventListener('click', loadWeather);
  elements.city.addEventListener('change', loadWeather);

  async function loadWeather() {
    const city = cities[elements.city.value];
    if (!city) return;
    requestController?.abort();
    const controller = new AbortController();
    requestController = controller;
    const timeout = window.setTimeout(() => controller.abort(), 10000);
    setLoading(true);

    try {
      const url = new URL('/v1/forecast', apiOrigin);
      url.searchParams.set('latitude', String(city.latitude));
      url.searchParams.set('longitude', String(city.longitude));
      url.searchParams.set(
        'current',
        'temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m',
      );
      url.searchParams.set(
        'daily',
        'weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max',
      );
      url.searchParams.set('timezone', 'auto');
      url.searchParams.set('forecast_days', '3');

      const response = await fetch(url, {
        method: 'GET',
        headers: { Accept: 'application/json' },
        signal: controller.signal,
      });
      if (!response.ok) throw new Error(`Weather service returned HTTP ${response.status}`);
      const payload = await response.json();
      const weather = parseWeather(payload, city);
      renderWeather(weather);
      setBadge('Request succeeded', 'success');
      elements.updatedAt.textContent = `Updated ${new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;
      await syncToGlasses(weather);
    } catch (error) {
      if (controller.signal.aborted && requestController !== controller) return;
      renderError(
        controller.signal.aborted
          ? new Error('Weather request timed out; please try again')
          : error,
      );
    } finally {
      window.clearTimeout(timeout);
      if (requestController === controller) {
        requestController = null;
        setLoading(false);
      }
    }
  }

  function parseWeather(payload, city) {
    const current = payload?.current;
    const daily = payload?.daily;
    const requiredDaily = [
      daily?.time,
      daily?.weather_code,
      daily?.temperature_2m_max,
      daily?.temperature_2m_min,
      daily?.precipitation_probability_max,
    ];
    if (
      !current ||
      !Number.isFinite(current.temperature_2m) ||
      !Number.isFinite(current.apparent_temperature) ||
      !Number.isFinite(current.relative_humidity_2m) ||
      !Number.isFinite(current.wind_speed_10m) ||
      requiredDaily.some((value) => !Array.isArray(value) || value.length < 3)
    ) {
      throw new Error('Weather response is missing required fields');
    }
    return {
      city: city.name,
      code: Number(current.weather_code),
      condition: weatherLabel(current.weather_code),
      temperature: Math.round(current.temperature_2m),
      apparent: Math.round(current.apparent_temperature),
      humidity: Math.round(current.relative_humidity_2m),
      wind: Math.round(current.wind_speed_10m),
      days: daily.time.slice(0, 3).map((date, index) => ({
        date,
        code: Number(daily.weather_code[index]),
        condition: weatherLabel(daily.weather_code[index]),
        high: Math.round(daily.temperature_2m_max[index]),
        low: Math.round(daily.temperature_2m_min[index]),
        rain: Math.round(daily.precipitation_probability_max[index] ?? 0),
      })),
    };
  }

  function renderWeather(weather) {
    const theme = weatherTheme(weather.code);
    document.body.dataset.weather = theme;
    elements.card.innerHTML = `
      <div class="hero">
        <div class="hero-top">
          <p class="location">${escapeHtml(weather.city)}</p>
          <p class="summary">${escapeHtml(weather.condition)} · Today ${weather.days[0].low}° / ${weather.days[0].high}°</p>
        </div>
        <div class="temperature">${weather.temperature}<sup>°C</sup></div>
        <div class="weather-art ${theme}" aria-label="${escapeHtml(weather.condition)}">
          <div class="sun"></div>
          <div class="cloud"></div>
          <div class="drops"><i></i><i></i><i></i></div>
        </div>
        <div class="metrics">
          <div class="metric"><span class="metric-icon">◒</span><strong>${weather.apparent}°</strong><span>Feels like</span></div>
          <div class="metric"><span class="metric-icon">◉</span><strong>${weather.humidity}%</strong><span>Humidity</span></div>
          <div class="metric"><span class="metric-icon">≋</span><strong>${weather.wind}</strong><span>km/h wind</span></div>
        </div>
      </div>`;
    elements.forecast.innerHTML = weather.days.map((day, index) => `
      <div class="forecast-row">
        <span class="forecast-day">${index === 0 ? 'Today' : index === 1 ? 'Tomorrow' : weekday(day.date)}</span>
        <span class="forecast-icon" title="${escapeHtml(day.condition)}">${weatherIcon(day.code)}</span>
        <span class="forecast-temp">${day.high}° <span class="forecast-low">${day.low}°</span></span>
        <span class="forecast-rain">${day.rain > 0 ? `Rain ${day.rain}%` : '&nbsp;'}</span>
      </div>`).join('');
  }

  async function syncToGlasses(weather) {
    try {
      const frame = renderGlassesFrame(weather);
      const compressed = compressLz4(frame.pixels);
      const geometry = {
        x: 8,
        y: 8,
        width: frame.width,
        height: frame.height,
        stride: frame.stride,
      };
      if (compressed.length + 14 < frame.pixels.length + 10) {
        await bridgeCall('display.updateImageLz4', {
          ...geometry,
          decodedSize: frame.pixels.length,
          dataBase64: toBase64(compressed),
        });
      } else {
        await bridgeCall('display.updateImage', {
          ...geometry,
          dataBase64: toBase64(frame.pixels),
        });
      }
    } catch (error) {
      setBadge('Weather updated · Glasses sync failed', 'error');
    }
  }

  function renderGlassesFrame(weather) {
    const width = 560;
    const height = 272;
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext('2d', { alpha: false });
    if (!context) throw new Error('Could not create the glasses weather canvas');

    context.fillStyle = '#000';
    context.fillRect(0, 0, width, height);
    context.fillStyle = '#fff';
    context.strokeStyle = '#fff';
    context.lineCap = 'round';
    context.lineJoin = 'round';

    context.font = '700 24px sans-serif';
    context.fillText(weather.city, 24, 39);
    context.font = '18px sans-serif';
    context.fillStyle = '#aaa';
    context.fillText(weather.condition, 24, 65);

    context.fillStyle = '#fff';
    context.font = '300 98px sans-serif';
    context.fillText(`${weather.temperature}°`, 18, 164);
    drawWeatherSymbol(context, weather.code, 214, 58, 94);

    context.strokeStyle = '#555';
    context.lineWidth = 2;
    context.beginPath();
    context.moveTo(24, 188);
    context.lineTo(326, 188);
    context.stroke();

    drawMetric(context, 24, 208, 'FEELS', `${weather.apparent}°`);
    drawMetric(context, 130, 208, 'HUMIDITY', `${weather.humidity}%`);
    drawMetric(context, 236, 208, 'WIND', `${weather.wind} km/h`);

    context.fillStyle = '#141414';
    roundedRect(context, 346, 14, 200, 244, 20);
    context.fill();
    context.fillStyle = '#fff';
    context.font = '700 18px sans-serif';
    context.fillText('NEXT 3 DAYS', 364, 43);

    weather.days.forEach((day, index) => {
      const top = 59 + index * 62;
      const label = index === 0 ? 'TODAY' : index === 1 ? 'TOMORROW' : weekday(day.date);
      context.fillStyle = '#aaa';
      context.font = '15px sans-serif';
      context.fillText(label, 364, top + 21);
      drawWeatherSymbol(context, day.code, 414, top + 4, 32);
      context.fillStyle = '#fff';
      context.font = '700 17px sans-serif';
      context.textAlign = 'right';
      context.fillText(`${day.high}°`, 501, top + 21);
      context.fillStyle = '#999';
      context.font = '15px sans-serif';
      context.fillText(`${day.low}°`, 531, top + 21);
      context.textAlign = 'left';
      if (day.rain > 0) {
        context.fillStyle = '#999';
        context.font = '12px sans-serif';
        context.fillText(`RAIN ${day.rain}%`, 456, top + 42);
      }
      if (index < weather.days.length - 1) {
        context.strokeStyle = '#3d3d3d';
        context.lineWidth = 1;
        context.beginPath();
        context.moveTo(364, top + 53);
        context.lineTo(528, top + 53);
        context.stroke();
      }
    });

    const image = context.getImageData(0, 0, width, height).data;
    const stride = width / 2;
    const pixels = new Uint8Array(stride * height);
    for (let index = 0; index < image.length; index += 8) {
      const first = grayscaleNibble(image, index);
      const second = grayscaleNibble(image, index + 4);
      pixels[index / 8] = (first << 4) | second;
    }
    return { width, height, stride, pixels, canvas };
  }

  function drawMetric(context, x, y, label, value) {
    context.fillStyle = '#888';
    context.font = '13px sans-serif';
    context.fillText(label, x, y + 15);
    context.fillStyle = '#fff';
    context.font = '700 19px sans-serif';
    context.fillText(value, x, y + 41);
  }

  function drawWeatherSymbol(context, code, x, y, size) {
    const theme = weatherTheme(code);
    const line = Math.max(2, Math.round(size / 25));
    context.save();
    context.strokeStyle = '#fff';
    context.fillStyle = '#fff';
    context.lineWidth = line;

    if (theme === 'clear' || theme === 'cloudy') {
      const radius = size * 0.22;
      const centerX = x + size * 0.62;
      const centerY = y + size * 0.3;
      context.beginPath();
      context.arc(centerX, centerY, radius, 0, Math.PI * 2);
      theme === 'clear' ? context.stroke() : context.fill();
      if (theme === 'clear') {
        for (let ray = 0; ray < 8; ray += 1) {
          const angle = ray * Math.PI / 4;
          context.beginPath();
          context.moveTo(
            centerX + Math.cos(angle) * radius * 1.35,
            centerY + Math.sin(angle) * radius * 1.35,
          );
          context.lineTo(
            centerX + Math.cos(angle) * radius * 1.72,
            centerY + Math.sin(angle) * radius * 1.72,
          );
          context.stroke();
        }
      }
    }

    if (theme !== 'clear') {
      context.beginPath();
      context.arc(x + size * 0.34, y + size * 0.55, size * 0.2, Math.PI, 0);
      context.arc(x + size * 0.55, y + size * 0.44, size * 0.28, Math.PI, 0);
      context.arc(x + size * 0.76, y + size * 0.58, size * 0.18, Math.PI, 0);
      context.lineTo(x + size * 0.76, y + size * 0.72);
      context.lineTo(x + size * 0.34, y + size * 0.72);
      context.closePath();
      context.fill();
    }

    if (theme === 'rain' || theme === 'storm') {
      for (let drop = 0; drop < 3; drop += 1) {
        const dropX = x + size * (0.35 + drop * 0.2);
        context.beginPath();
        context.moveTo(dropX, y + size * 0.81);
        context.lineTo(dropX - size * 0.06, y + size * 0.95);
        context.stroke();
      }
    } else if (theme === 'snow') {
      context.font = `${Math.round(size * 0.25)}px sans-serif`;
      context.fillText('✦  ✦', x + size * 0.3, y + size * 0.96);
    } else if (theme === 'fog') {
      for (let fog = 0; fog < 2; fog += 1) {
        context.beginPath();
        context.moveTo(x + size * 0.27, y + size * (0.83 + fog * 0.12));
        context.lineTo(x + size * 0.82, y + size * (0.83 + fog * 0.12));
        context.stroke();
      }
    }
    if (theme === 'storm') {
      context.beginPath();
      context.moveTo(x + size * 0.58, y + size * 0.74);
      context.lineTo(x + size * 0.46, y + size * 0.9);
      context.lineTo(x + size * 0.57, y + size * 0.89);
      context.lineTo(x + size * 0.48, y + size * 1.06);
      context.stroke();
    }
    context.restore();
  }

  function roundedRect(context, x, y, width, height, radius) {
    context.beginPath();
    context.moveTo(x + radius, y);
    context.arcTo(x + width, y, x + width, y + height, radius);
    context.arcTo(x + width, y + height, x, y + height, radius);
    context.arcTo(x, y + height, x, y, radius);
    context.arcTo(x, y, x + width, y, radius);
    context.closePath();
  }

  function grayscaleNibble(image, offset) {
    const luminance = image[offset] * 0.299
      + image[offset + 1] * 0.587
      + image[offset + 2] * 0.114;
    return Math.max(0, Math.min(15, Math.round(luminance / 17)));
  }

  function toBase64(bytes) {
    let binary = '';
    for (let offset = 0; offset < bytes.length; offset += 16384) {
      binary += String.fromCharCode(...bytes.subarray(offset, offset + 16384));
    }
    return btoa(binary);
  }

  function compressLz4(input) {
    const output = [];
    const dictionary = new Int32Array(65536);
    dictionary.fill(-1);
    let anchor = 0;
    let position = 0;
    const hash = (offset) => {
      const value = (
        input[offset]
        | input[offset + 1] << 8
        | input[offset + 2] << 16
        | input[offset + 3] << 24
      ) >>> 0;
      return (Math.imul(value, 2654435761) >>> 16) & 65535;
    };
    const writeLength = (length) => {
      while (length >= 255) {
        output.push(255);
        length -= 255;
      }
      output.push(length);
    };

    while (position + 12 <= input.length) {
      const slot = hash(position);
      const match = dictionary[slot];
      dictionary[slot] = position;
      if (
        match < 0
        || position - match > 65535
        || input[match] !== input[position]
        || input[match + 1] !== input[position + 1]
        || input[match + 2] !== input[position + 2]
        || input[match + 3] !== input[position + 3]
      ) {
        position += 1;
        continue;
      }
      let matchLength = 4;
      while (
        position + matchLength < input.length - 5
        && input[match + matchLength] === input[position + matchLength]
      ) {
        matchLength += 1;
      }
      const literalLength = position - anchor;
      const encodedMatchLength = matchLength - 4;
      output.push(
        Math.min(literalLength, 15) << 4
        | Math.min(encodedMatchLength, 15),
      );
      if (literalLength >= 15) writeLength(literalLength - 15);
      output.push(...input.subarray(anchor, position));
      const distance = position - match;
      output.push(distance & 255, distance >>> 8);
      if (encodedMatchLength >= 15) writeLength(encodedMatchLength - 15);
      position += matchLength;
      anchor = position;
    }
    const remaining = input.length - anchor;
    output.push(Math.min(remaining, 15) << 4);
    if (remaining >= 15) writeLength(remaining - 15);
    output.push(...input.subarray(anchor));
    return Uint8Array.from(output);
  }

  function renderError(error) {
    setBadge('Request failed', 'error');
    const message = error instanceof Error ? error.message : String(error);
    if (!elements.card.querySelector('.hero')) {
      elements.card.innerHTML = `
        <div class="placeholder">
          <div class="error-mark" aria-hidden="true">×</div>
          <p>${escapeHtml(message)}</p>
        </div>`;
      elements.forecast.innerHTML = '<div class="forecast-skeleton"></div><div class="forecast-skeleton"></div><div class="forecast-skeleton"></div>';
    }
  }

  function setLoading(loading) {
    elements.refresh.disabled = loading;
    elements.city.disabled = loading;
    elements.refresh.classList.toggle('is-loading', loading);
    if (loading) setBadge('Requesting', 'loading');
  }

  function setBadge(label, state) {
    elements.badge.textContent = label;
    elements.badge.className = `badge badge-${state}`;
  }

  function weatherLabel(code) {
    return weatherLabels.get(Number(code)) || `Weather code ${code}`;
  }

  function weatherTheme(code) {
    const value = Number(code);
    if (value <= 1) return 'clear';
    if (value <= 3) return 'cloudy';
    if (value === 45 || value === 48) return 'fog';
    if (value >= 71 && value <= 86) return 'snow';
    if (value >= 95) return 'storm';
    return 'rain';
  }

  function weatherIcon(code) {
    return {
      clear: '☀︎',
      cloudy: '☁︎',
      fog: '≋',
      snow: '✦',
      storm: 'ϟ',
      rain: '☂︎',
    }[weatherTheme(code)];
  }

  function weekday(date) {
    return new Intl.DateTimeFormat('zh-CN', { weekday: 'short' }).format(
      new Date(`${date}T12:00:00`),
    );
  }

  function escapeHtml(value) {
    return String(value)
      .replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;')
      .replaceAll('>', '&gt;')
      .replaceAll('"', '&quot;')
      .replaceAll("'", '&#039;');
  }

})();
