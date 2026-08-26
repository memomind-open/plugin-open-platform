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

function isHorizontallyCentered(alignment) {
  return alignment === 2 || alignment === 5 || alignment === 9;
}

function isRightAligned(alignment) {
  return alignment === 3 || alignment === 6 || alignment === 8;
}

export function layoutOverlayText(context, overlay) {
  const characterHeight = overlay.fontHeight >= 40 ? 30 : 24;
  const letterSpace = Number.isFinite(overlay.letterSpace) ? overlay.letterSpace : 0;
  const lineSpace = Number.isFinite(overlay.lineSpace) ? overlay.lineSpace : 0;
  const lineHeight = Math.max(1, overlay.fontHeight + lineSpace);
  const lines = wrapOverlayText(
    context,
    overlay.text,
    overlay.width,
    overlay.autoSize ? false : overlay.wrap,
    letterSpace,
  );
  const measured = lines.map((line) => ({
    text: line,
    width: measureLine(context, line, letterSpace),
  }));
  const contentWidth = measured.reduce((width, line) => Math.max(width, line.width), 0);
  let x = overlay.x;
  let width = overlay.width;
  if (overlay.autoSize) {
    if (isHorizontallyCentered(overlay.objectAlignment)) {
      x += (width - contentWidth) / 2;
    } else if (isRightAligned(overlay.objectAlignment)) {
      x += width - contentWidth;
    }
    width = contentWidth;
  }
  const visibleLines = [];
  for (let index = 0; index < measured.length; index += 1) {
    const y = overlay.y + index * lineHeight;
    if (y + characterHeight > overlay.y + overlay.height) break;
    visibleLines.push({ ...measured[index], y });
  }
  return {
    x,
    width,
    characterHeight,
    letterSpace,
    visibleLines,
  };
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
    const gray = Math.max(0, Math.min(255, overlay.gray));
    context.font = `${characterHeight}px "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif`;
    context.fillStyle = `rgb(${Math.round(gray / 12)}, ${gray}, ${Math.round(gray / 5)})`;
    context.globalAlpha = Math.max(0, Math.min(255, overlay.opacity)) / 255;
    const layout = layoutOverlayText(context, overlay);
    if (layout.width <= 0) continue;

    context.save();
    context.beginPath();
    context.rect(layout.x, overlay.y, layout.width, overlay.height);
    context.clip();
    for (const line of layout.visibleLines) {
      let x = layout.x;
      if (overlay.alignment === 2) x += (layout.width - line.width) / 2;
      else if (overlay.alignment === 3) x += layout.width - line.width;
      drawLine(context, line.text, x, line.y, layout.letterSpace);
    }
    context.restore();
  }
  context.restore();
}
