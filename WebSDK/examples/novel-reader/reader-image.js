export function rgbaToGray4(rgba, width, height) {
  if (!(rgba instanceof Uint8ClampedArray) || !Number.isInteger(width) || width < 1 ||
      !Number.isInteger(height) || height < 1 || rgba.length !== width * height * 4) {
    throw new TypeError('RGBA pixels do not match the requested image geometry');
  }
  const stride = Math.ceil(width / 2);
  const output = new Uint8Array(stride * height);
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const source = (y * width + x) * 4;
      const luminance = Math.round((rgba[source] * 54 + rgba[source + 1] * 183 +
        rgba[source + 2] * 19) / 256);
      const gray4 = Math.max(0, Math.min(15, Math.round(luminance * 15 / 255)));
      const target = y * stride + (x >> 1);
      if ((x & 1) === 0) output[target] = gray4 << 4;
      else output[target] |= gray4;
    }
  }
  return output;
}
