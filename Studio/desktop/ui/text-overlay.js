function measureLine(context, text, letterSpace) {
  const characters = Array.from(text);
  if (characters.length === 0) return 0;
  const glyphWidth = characters.reduce(
    (width, character) => width + context.measureText(character).width,
    0,
  );
  return glyphWidth + Math.max(0, characters.length - 1) * letterSpace;
}

export function wrapOverlayText(context, text, maxWidth, wrap, letterSpace = 0) {
  if (maxWidth <= 0) return [];
  const lines = [];
  for (const paragraph of String(text).replace(/\r\n?/g, '\n').split('\n')) {
    if (!wrap || paragraph.length === 0) {
      lines.push(paragraph);
      continue;
    }
    let line = '';
    for (const character of Array.from(paragraph)) {
      const candidate = `${line}${character}`;
      if (line && measureLine(context, candidate, letterSpace) > maxWidth) {
        lines.push(line);
        line = character;
      } else {
        line = candidate;
      }
    }
    lines.push(line);
  }
  return lines;
}

function drawLine(context, text, x, y, letterSpace) {
  if (letterSpace === 0) {
    context.fillText(text, x, y);
    return;
  }
  let cursor = x;
  for (const character of Array.from(text)) {
    context.fillText(character, cursor, y);
    cursor += context.measureText(character).width + letterSpace;
  }
}

export function drawTextOverlays(context, overlays, canvasWidth, canvasHeight) {
  context.save();
  context.beginPath();
  context.rect(0, 0, canvasWidth, canvasHeight);
  context.clip();
  context.textBaseline = 'top';

  for (const overlay of overlays ?? []) {
    if (!overlay.text || overlay.width <= 0 || overlay.height <= 0) continue;
    const characterHeight = overlay.fontHeight >= 40 ? 30 : 24;
    const letterSpace = Number.isFinite(overlay.letterSpace) ? overlay.letterSpace : 0;
    const lineSpace = Number.isFinite(overlay.lineSpace) ? overlay.lineSpace : 0;
    const lineHeight = Math.max(1, overlay.fontHeight + lineSpace);
    const gray = Math.max(0, Math.min(255, overlay.gray));
    context.font = `${characterHeight}px "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif`;
    context.fillStyle = `rgb(${Math.round(gray / 12)}, ${gray}, ${Math.round(gray / 5)})`;
    context.globalAlpha = Math.max(0, Math.min(255, overlay.opacity)) / 255;

    const lines = wrapOverlayText(
      context,
      overlay.text,
      overlay.width,
      overlay.wrap,
      letterSpace,
    );
    for (let index = 0; index < lines.length; index += 1) {
      const y = overlay.y + index * lineHeight;
      if (y >= overlay.y + overlay.height) break;
      const line = lines[index];
      const lineWidth = measureLine(context, line, letterSpace);
      let x = overlay.x;
      if (overlay.alignment === 2) x += (overlay.width - lineWidth) / 2;
      else if (overlay.alignment === 3) x += overlay.width - lineWidth;
      drawLine(context, line, x, y, letterSpace);
    }
  }
  context.restore();
}
