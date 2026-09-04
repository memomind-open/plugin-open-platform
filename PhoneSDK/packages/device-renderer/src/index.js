import { DEVICE_PROFILE } from '@memomind/gm-plugin-bridge-contract';

export class Gray4Framebuffer {
  constructor(profile = DEVICE_PROFILE) {
    this.width = profile.width;
    this.height = profile.height;
    this.pixels = new Uint8Array(this.width * this.height);
  }

  clear(level = 0) {
    requireGray(level);
    this.pixels.fill(level);
  }

  drawPacked({ x, y, width, height, stride, pixels }) {
    validateRect(this, { x, y, width, height });
    const expectedStride = Math.ceil(width / 2);
    if (stride < expectedStride || pixels.length !== stride * height) {
      throw new RangeError('GRAY_4 stride or pixel length is invalid');
    }
    for (let row = 0; row < height; row += 1) {
      for (let column = 0; column < width; column += 1) {
        const packed = pixels[row * stride + (column >> 1)];
        const level = (column & 1) === 0 ? packed >> 4 : packed & 0x0f;
        this.pixels[(y + row) * this.width + x + column] = level;
      }
    }
  }

  get(x, y) {
    return this.pixels[y * this.width + x];
  }
}

export class CanvasDeviceRenderer {
  constructor(canvas, profile = DEVICE_PROFILE) {
    this.canvas = canvas;
    this.profile = profile;
    this.framebuffer = new Gray4Framebuffer(profile);
    this.pendingFramebuffer = null;
    this.textElements = new Map();
    this.canvas.width = profile.width;
    this.canvas.height = profile.height;
    this.context = canvas.getContext('2d', { alpha: false });
    this.render();
  }

  clear() {
    this.pendingFramebuffer = null;
    this.framebuffer.clear();
    this.textElements.clear();
    this.render();
  }

  updateText(params) {
    validateRect(this.framebuffer, params);
    if (!Number.isInteger(params.id) || params.id < 0 || params.id > 255 || typeof params.text !== 'string' || params.text.length === 0) {
      throw new RangeError('Text element is invalid');
    }
    this.textElements.set(params.id, { ...params });
    this.render();
  }

  updateImage(params) {
    this.framebuffer.drawPacked(params);
    this.render();
  }

  beginFrame() {
    const pending = new Gray4Framebuffer(this.profile);
    pending.pixels.set(this.framebuffer.pixels);
    this.pendingFramebuffer = pending;
  }

  updateFrameImage(params, complete = false) {
    if (!this.pendingFramebuffer) throw new RangeError('Atomic frame is not active');
    this.pendingFramebuffer.drawPacked(params);
    if (!complete) return;
    this.framebuffer.pixels.set(this.pendingFramebuffer.pixels);
    this.pendingFramebuffer = null;
    this.render();
  }

  abortFrame() {
    this.pendingFramebuffer = null;
  }

  render() {
    const image = this.context.createImageData(this.profile.width, this.profile.height);
    for (let index = 0; index < this.framebuffer.pixels.length; index += 1) {
      const value = this.framebuffer.pixels[index] * 17;
      const target = index * 4;
      image.data[target] = 0;
      image.data[target + 1] = value;
      image.data[target + 2] = 0;
      image.data[target + 3] = 255;
    }
    this.context.putImageData(image, 0, 0);
    this.context.font = `${this.profile.defaultFontPx}px system-ui, sans-serif`;
    this.context.textBaseline = 'top';
    for (const element of this.textElements.values()) {
      this.context.strokeStyle = '#00ff00';
      this.context.fillStyle = '#00ff00';
      if (element.border > 0) {
        this.context.lineWidth = element.border;
        roundedRect(this.context, element.x, element.y, element.width, element.height, element.radius);
        this.context.stroke();
      }
      this.context.save();
      this.context.beginPath();
      this.context.rect(element.x + 3, element.y + 3, element.width - 6, element.height - 6);
      this.context.clip();
      drawWrappedText(
        this.context,
        element.text,
        element.x + 4,
        element.y + 4,
        element.width - 8,
        this.profile.defaultFontPx + 4,
        element.height - 8,
      );
      this.context.restore();
    }
  }
}

export function decodeBase64(value) {
  if (typeof value !== 'string' || value.length === 0) throw new TypeError('dataBase64 must be a non-empty string');
  if (typeof Buffer !== 'undefined') return Uint8Array.from(Buffer.from(value, 'base64'));
  const binary = atob(value);
  return Uint8Array.from(binary, (character) => character.charCodeAt(0));
}

export function decodeRawLz4(input, decodedSize) {
  if (!(input instanceof Uint8Array) || !Number.isInteger(decodedSize) || decodedSize < 0) throw new TypeError('Invalid raw LZ4 input');
  const output = new Uint8Array(decodedSize);
  let source = 0;
  let target = 0;
  while (source < input.length) {
    const token = input[source++];
    let literalLength = token >> 4;
    if (literalLength === 15) {
      let extension;
      do {
        if (source >= input.length) throw new RangeError('Malformed LZ4 literal length');
        extension = input[source++];
        literalLength += extension;
      } while (extension === 255);
    }
    if (source + literalLength > input.length || target + literalLength > output.length) throw new RangeError('LZ4 literal exceeds bounds');
    output.set(input.subarray(source, source + literalLength), target);
    source += literalLength;
    target += literalLength;
    if (source === input.length) break;
    if (source + 2 > input.length) throw new RangeError('Malformed LZ4 offset');
    const offset = input[source] | (input[source + 1] << 8);
    source += 2;
    if (offset === 0 || offset > target) throw new RangeError('Invalid LZ4 offset');
    let matchLength = token & 0x0f;
    if (matchLength === 15) {
      let extension;
      do {
        if (source >= input.length) throw new RangeError('Malformed LZ4 match length');
        extension = input[source++];
        matchLength += extension;
      } while (extension === 255);
    }
    matchLength += 4;
    if (target + matchLength > output.length) throw new RangeError('LZ4 match exceeds output');
    for (let index = 0; index < matchLength; index += 1) output[target + index] = output[target - offset + index];
    target += matchLength;
  }
  if (target !== decodedSize) throw new RangeError(`LZ4 decoded ${target} bytes, expected ${decodedSize}`);
  return output;
}

function validateRect(framebuffer, { x, y, width, height }) {
  const values = [x, y, width, height];
  if (values.some((value) => !Number.isInteger(value)) || x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > framebuffer.width || y + height > framebuffer.height) {
    throw new RangeError('Rectangle is outside the device display');
  }
}

function requireGray(level) {
  if (!Number.isInteger(level) || level < 0 || level > 15) throw new RangeError('GRAY_4 level must be 0..15');
}

function roundedRect(context, x, y, width, height, radius) {
  const safeRadius = Math.max(0, Math.min(radius ?? 0, width / 2, height / 2));
  context.beginPath();
  context.roundRect(x, y, width, height, safeRadius);
}

function drawWrappedText(context, text, x, y, maxWidth, lineHeight, maxHeight) {
  let line = '';
  let cursorY = y;
  const bottom = y + maxHeight;
  const flush = () => {
    if (cursorY >= bottom) return false;
    if (line) context.fillText(line, x, cursorY);
    line = '';
    cursorY += lineHeight;
    return true;
  };
  for (const character of text) {
    if (character === '\r') continue;
    if (character === '\n') {
      if (!flush()) return;
      continue;
    }
    const candidate = line + character;
    if (line && context.measureText(candidate).width > maxWidth) {
      if (!flush()) return;
      line = character;
    } else {
      line = candidate;
    }
  }
  if (line && cursorY < bottom) context.fillText(line, x, cursorY);
}
