import { localizedError } from './i18n.js';
import { openImageRead, imageMime, throwIfAborted } from './files.js';
export const WIDTH = 600;
export const HEIGHT = 350;
export const FRAME_BYTES = WIDTH * HEIGHT / 2;

export const ALIGNMENTS = ['top-left', 'top-center', 'top-right', 'middle-left', 'center', 'middle-right', 'bottom-left', 'bottom-center', 'bottom-right'];

// Source and destination extents are identical: never resample a source pixel.
export function placement(width, height, alignment = 'center') {
  if (!Number.isSafeInteger(width) || width < 1 || !Number.isSafeInteger(height) || height < 1) {
    throw localizedError('error.dimensions');
  }
  const index = ALIGNMENTS.indexOf(alignment);
  if (index < 0) throw new RangeError('Invalid image alignment');
  const w = Math.min(width, WIDTH);
  const h = Math.min(height, HEIGHT);
  return { sx: 0, sy: 0, width: w, height: h,
    dx: Math.floor((WIDTH - w) * (index % 3) / 2), dy: Math.floor((HEIGHT - h) * Math.floor(index / 3) / 2) };
}

export function composeImage(image, width, height, canvas, alignment = 'center') {
  canvas.width = WIDTH;
  canvas.height = HEIGHT;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  if (!ctx) throw localizedError('error.canvas');
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, WIDTH, HEIGHT);
  const p = placement(width, height, alignment);
  ctx.drawImage(image, p.sx, p.sy, p.width, p.height, p.dx, p.dy, p.width, p.height);
  const rgba = ctx.getImageData(0, 0, WIDTH, HEIGHT);
  const bytes = rgbaToGray4(rgba.data);
  // Preview the transmitted intensity in green, with the same black border.
  for (let i = 0; i < WIDTH * HEIGHT; i++) {
    const value = ((bytes[i >> 1] >> ((i & 1) ? 0 : 4)) & 15) * 17;
    rgba.data[i * 4] = 0;
    rgba.data[i * 4 + 1] = value;
    rgba.data[i * 4 + 2] = 0;
    rgba.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(rgba, 0, 0);
  return bytes;
}

export function rgbaToGray4(rgba) {
  if (rgba.length !== WIDTH * HEIGHT * 4) throw localizedError('error.rgba');
  const bytes = new Uint8Array(FRAME_BYTES);
  for (let i = 0; i < WIDTH * HEIGHT; i++) {
    const p = i * 4;
    const luma = (rgba[p] * 54 + rgba[p + 1] * 183 + rgba[p + 2] * 19) / 256;
    const gray = Math.round(luma * (rgba[p + 3] / 255) * 15 / 255);
    bytes[i >> 1] |= gray << ((i & 1) ? 0 : 4);
  }
  return bytes;
}

export async function readImageBlob(gm, file, signal) {
  if (!file?.fileId || !Number.isSafeInteger(file.size) || file.size < 1) {
    throw localizedError('error.emptyFile');
  }
  const opened = await openImageRead(gm, file.fileId, signal);
  if (opened.fileId !== file.fileId || opened.length !== file.size || opened.offset !== 0) {
    await opened.stream?.cancel();
    throw localizedError('error.stream');
  }
  const reader = opened.stream.getReader();
  const chunks = [];
  let size = 0;
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (!(value instanceof Uint8Array) || size + value.length > file.size) throw localizedError('error.length');
      chunks.push(value);
      size += value.length;
    }
    if (size !== file.size) throw localizedError('error.incompleteFile');
    throwIfAborted(signal);
    const blob = new Blob(chunks);
    const type = imageMime(new Uint8Array(await blob.slice(0, 12).arrayBuffer()));
    return blob.slice(0, blob.size, type);
  } catch (error) {
    await reader.cancel().catch(() => {});
    throw error;
  } finally { reader.releaseLock(); }
}

export async function decodeImage(blob) {
  // Native decoding preserves full resolution and applies the file's orientation.
  // No resizeWidth/resizeHeight or devicePixelRatio is used.
  if (typeof globalThis.createImageBitmap === 'function') {
    try {
      const image = await createImageBitmap(blob);
      return { image, width: image.width, height: image.height, close: () => image.close() };
    } catch {
      // Some Android WebViews expose ImageBitmap but cannot decode a supported
      // format through it. Try the ordinary image loader before reporting failure.
    }
  }
  const url = URL.createObjectURL(blob);
  try {
    const image = new Image();
    await new Promise((resolve, reject) => {
      image.onload = resolve;
      image.onerror = () => reject(localizedError('error.decode'));
      image.src = url;
    });
    return { image, width: image.naturalWidth, height: image.naturalHeight,
      close: () => URL.revokeObjectURL(url) };
  } catch (error) { URL.revokeObjectURL(url); throw error; }
}
