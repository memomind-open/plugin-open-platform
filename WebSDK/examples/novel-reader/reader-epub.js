import { trimWindowEnd } from './reader-core.js';

export const EPUB_INDEX_VERSION = 1;
export const EPUB_IMAGE_MARKER_PREFIX = '\x1eGMIMG:';
const EPUB_IMAGE_MARKER_SUFFIX = '\x1e\n';
const MAX_DIRECTORY_BYTES = 4 * 1024 * 1024;
const MAX_DOCUMENT_BYTES = 16 * 1024 * 1024;
const MAX_IMAGE_BYTES = 32 * 1024 * 1024;
const SECTION_CACHE_LIMIT = 4;
const archiveCache = new Map();
const sectionCache = new Map();
const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder('utf-8', { fatal: true });

export async function indexPersistentEpub(storage, file, onProgress = () => {}) {
  requirePersistentFile(storage, file);
  const archive = await getArchive(storage, file);
  if (archive.has('META-INF/encryption.xml')) {
    throw new Error('Encrypted or DRM-protected EPUB files are not supported');
  }
  const mimetype = textDecoder.decode(await archive.read('mimetype', 128)).trim();
  if (mimetype !== 'application/epub+zip') throw new Error('The selected file is not a valid EPUB');

  const container = parseXml(textDecoder.decode(
    await archive.read('META-INF/container.xml', MAX_DIRECTORY_BYTES),
  ));
  const rootfile = descendants(container, 'rootfile')[0];
  const packagePath = archivePath('', attribute(rootfile, 'full-path'));
  if (!packagePath) throw new Error('EPUB container does not declare a package document');
  const packageRoot = parseXml(textDecoder.decode(
    await archive.read(packagePath, MAX_DIRECTORY_BYTES),
  ));
  const packageDirectory = directoryOf(packagePath);
  const layout = descendants(packageRoot, 'meta').find((node) =>
    attribute(node, 'property') === 'rendition:layout');
  if (layout && textContent(layout).trim() === 'pre-paginated') {
    throw new Error('Fixed-layout EPUB is not supported by this reader');
  }

  const manifest = new Map();
  for (const item of descendants(packageRoot, 'item')) {
    const id = attribute(item, 'id');
    const href = attribute(item, 'href');
    if (!id || !href) continue;
    manifest.set(id, {
      id,
      path: archivePath(packageDirectory, href),
      mediaType: attribute(item, 'media-type').toLowerCase(),
      properties: attribute(item, 'properties').split(/\s+/u).filter(Boolean),
    });
  }
  const spineElement = descendants(packageRoot, 'spine')[0];
  const spine = childElements(spineElement, 'itemref')
    .filter((item) => attribute(item, 'linear') !== 'no')
    .map((item) => manifest.get(attribute(item, 'idref')))
    .filter((item) => item && isXhtml(item.mediaType));
  if (spine.length === 0) throw new Error('EPUB does not contain readable spine documents');

  const navigation = await readNavigation(archive, manifest, spineElement, packageDirectory);
  const imageByPath = new Map();
  const epubImages = [];
  const epubSections = [];
  const chapters = [];
  let byteOffset = 0;
  for (let index = 0; index < spine.length; index += 1) {
    const item = spine[index];
    const source = textDecoder.decode(await archive.read(item.path, MAX_DOCUMENT_BYTES));
    const document = parseXml(source);
    const extracted = extractDocument(document, item.path, manifest, imageByPath, epubImages);
    if (!extracted.text) continue;
    const bytes = textEncoder.encode(`${extracted.text.trim()}\n\n`);
    const title = navigation.get(item.path) || extracted.title || `Section ${index + 1}`;
    epubSections.push({
      path: item.path,
      byteOffset,
      byteLength: bytes.length,
      title: title.slice(0, 80),
    });
    chapters.push({ title: title.slice(0, 80), sourceOffset: byteOffset, byteOffset });
    byteOffset += bytes.length;
    onProgress(index + 1, spine.length);
  }
  if (byteOffset === 0 || epubSections.length === 0) throw new Error('EPUB has no readable content');
  return {
    indexVersion: EPUB_INDEX_VERSION,
    format: 'epub',
    encoding: 'utf-8',
    textBytes: byteOffset,
    chapters,
    epubSections,
    epubImages,
    epubTitle: firstText(packageRoot, 'title'),
  };
}

export async function readEpubWindow(storage, file, index, byteOffset, maxUtf8Bytes) {
  requirePersistentFile(storage, file);
  if (!Number.isSafeInteger(byteOffset) || byteOffset < 0 || byteOffset >= index.textBytes) {
    throw new TypeError('byteOffset is outside the EPUB');
  }
  if (!Number.isSafeInteger(maxUtf8Bytes) || maxUtf8Bytes < 1) {
    throw new TypeError('maxUtf8Bytes must be a positive integer');
  }
  const sections = requireSections(index);
  let sectionIndex = sectionIndexAt(sections, byteOffset);
  let cursor = byteOffset;
  let output = new Uint8Array();
  while (sectionIndex < sections.length && output.length < maxUtf8Bytes) {
    const section = sections[sectionIndex];
    const bytes = await loadSectionBytes(storage, file, index, section);
    const localOffset = Math.max(0, cursor - section.byteOffset);
    const available = bytes.subarray(localOffset);
    const wanted = Math.min(available.length, maxUtf8Bytes - output.length);
    let end = wanted;
    if (wanted < available.length) end = trimWindowEnd(available, 0, wanted);
    if (end === 0) end = utf8PrefixLength(available, wanted);
    output = concatenate(output, available.subarray(0, end));
    cursor += end;
    if (end < available.length) break;
    sectionIndex += 1;
  }
  return {
    sourceOffset: byteOffset,
    sourceLength: output.length,
    text: textDecoder.decode(output),
    bytes: output,
    final: byteOffset + output.length >= index.textBytes,
  };
}

export async function readEpubImage(storage, file, index, imageId) {
  requirePersistentFile(storage, file);
  if (!Number.isSafeInteger(imageId) || imageId < 1) throw new TypeError('Invalid EPUB image ID');
  const image = index?.epubImages?.find((entry) => entry.id === imageId);
  if (!image || !isSupportedImage(image.mediaType, image.path)) {
    throw new Error('EPUB image is unavailable or unsupported');
  }
  const archive = await getArchive(storage, file);
  return {
    ...image,
    bytes: await archive.read(image.path, MAX_IMAGE_BYTES),
  };
}

export function imageMarker(imageId) {
  if (!Number.isSafeInteger(imageId) || imageId < 1 || imageId > 0xffffffff) {
    throw new TypeError('Invalid EPUB image ID');
  }
  return `${EPUB_IMAGE_MARKER_PREFIX}${imageId.toString(16).padStart(8, '0')}${EPUB_IMAGE_MARKER_SUFFIX}`;
}

export function imageMarkerAt(bytes, offset = 0) {
  if (!(bytes instanceof Uint8Array) || offset < 0 || offset >= bytes.length) return null;
  const markerLength = textEncoder.encode(imageMarker(1)).length;
  if (offset + markerLength > bytes.length || bytes[offset] !== 0x1e) return null;
  const marker = new TextDecoder().decode(bytes.subarray(offset, offset + markerLength));
  const match = /^\x1eGMIMG:([0-9a-f]{8})\x1e\n$/u.exec(marker);
  return match ? { imageId: Number.parseInt(match[1], 16), length: markerLength } : null;
}

async function loadSectionBytes(storage, file, index, section) {
  const key = `${file.fileId}:${section.path}`;
  if (sectionCache.has(key)) {
    const cached = sectionCache.get(key);
    sectionCache.delete(key);
    sectionCache.set(key, cached);
    return cached;
  }
  const archive = await getArchive(storage, file);
  const source = textDecoder.decode(await archive.read(section.path, MAX_DOCUMENT_BYTES));
  const manifest = new Map((index.epubImages ?? []).map((image) => [image.path, {
    path: image.path,
    mediaType: image.mediaType,
  }]));
  const imageByPath = new Map((index.epubImages ?? []).map((image) => [image.path, image.id]));
  const extracted = extractDocument(parseXml(source), section.path, manifest, imageByPath, []);
  const bytes = textEncoder.encode(`${extracted.text.trim()}\n\n`);
  if (bytes.length !== section.byteLength) throw new Error('EPUB content changed after it was indexed');
  sectionCache.set(key, bytes);
  while (sectionCache.size > SECTION_CACHE_LIMIT) sectionCache.delete(sectionCache.keys().next().value);
  return bytes;
}

async function getArchive(storage, file) {
  const key = `${file.fileId}:${file.size}`;
  if (archiveCache.has(key)) return archiveCache.get(key);
  const archive = await ZipArchive.open(storage, file);
  archiveCache.set(key, archive);
  if (archiveCache.size > 4) archiveCache.delete(archiveCache.keys().next().value);
  return archive;
}

class ZipArchive {
  constructor(storage, file, entries) {
    this.storage = storage;
    this.file = file;
    this.entries = entries;
  }

  static async open(storage, file) {
    const tailLength = Math.min(file.size, 65_557);
    const tailOffset = file.size - tailLength;
    const tail = await readRange(storage, file, tailOffset, tailLength);
    const eocd = findSignatureBackward(tail, 0x06054b50);
    if (eocd < 0 || eocd + 22 > tail.length) throw new Error('EPUB ZIP directory is missing');
    const view = new DataView(tail.buffer, tail.byteOffset, tail.byteLength);
    const entryCount = view.getUint16(eocd + 10, true);
    const directorySize = view.getUint32(eocd + 12, true);
    const directoryOffset = view.getUint32(eocd + 16, true);
    if (entryCount === 0xffff || directorySize === 0xffffffff || directoryOffset === 0xffffffff) {
      throw new Error('ZIP64 EPUB files are not supported');
    }
    if (directorySize > MAX_DIRECTORY_BYTES || directoryOffset + directorySize > file.size) {
      throw new Error('EPUB ZIP directory exceeds the supported limit');
    }
    const directory = await readRange(storage, file, directoryOffset, directorySize);
    const entries = new Map();
    let cursor = 0;
    for (let index = 0; index < entryCount; index += 1) {
      if (readU32(directory, cursor) !== 0x02014b50 || cursor + 46 > directory.length) {
        throw new Error('EPUB ZIP directory is malformed');
      }
      const flags = readU16(directory, cursor + 8);
      const method = readU16(directory, cursor + 10);
      const compressedSize = readU32(directory, cursor + 20);
      const size = readU32(directory, cursor + 24);
      const nameLength = readU16(directory, cursor + 28);
      const extraLength = readU16(directory, cursor + 30);
      const commentLength = readU16(directory, cursor + 32);
      const localOffset = readU32(directory, cursor + 42);
      const end = cursor + 46 + nameLength + extraLength + commentLength;
      if (end > directory.length) throw new Error('EPUB ZIP entry is truncated');
      const name = new TextDecoder('utf-8').decode(directory.subarray(cursor + 46, cursor + 46 + nameLength));
      if (!name.endsWith('/') && !entries.has(name)) {
        entries.set(name, { name, flags, method, compressedSize, size, localOffset });
      }
      cursor = end;
    }
    return new ZipArchive(storage, file, entries);
  }

  has(path) {
    return this.entries.has(path);
  }

  async read(path, limit) {
    const entry = this.entries.get(path);
    if (!entry) throw new Error(`EPUB resource is missing: ${path}`);
    if ((entry.flags & 1) !== 0) throw new Error('Encrypted EPUB resources are not supported');
    if (entry.size > limit) throw new Error(`EPUB resource is too large: ${path}`);
    const header = await readRange(this.storage, this.file, entry.localOffset, 30);
    if (readU32(header, 0) !== 0x04034b50) throw new Error('EPUB ZIP local header is malformed');
    const nameLength = readU16(header, 26);
    const extraLength = readU16(header, 28);
    const dataOffset = entry.localOffset + 30 + nameLength + extraLength;
    if (dataOffset + entry.compressedSize > this.file.size) throw new Error('EPUB ZIP entry exceeds file bounds');
    const compressed = await readRange(this.storage, this.file, dataOffset, entry.compressedSize);
    let output;
    if (entry.method === 0) output = compressed;
    else if (entry.method === 8) output = await inflateRaw(compressed);
    else throw new Error(`Unsupported EPUB ZIP compression method: ${entry.method}`);
    if (output.length !== entry.size) throw new Error(`EPUB resource has an invalid size: ${path}`);
    return output;
  }
}

async function readNavigation(archive, manifest, spine, packageDirectory) {
  const titles = new Map();
  const navItem = [...manifest.values()].find((item) => item.properties.includes('nav'));
  if (navItem) {
    const root = parseXml(textDecoder.decode(await archive.read(navItem.path, MAX_DOCUMENT_BYTES)));
    const nav = descendants(root, 'nav').find((node) =>
      attribute(node, 'type').split(/\s+/u).includes('toc')) || descendants(root, 'nav')[0];
    for (const anchor of descendants(nav, 'a')) {
      const path = archivePath(directoryOf(navItem.path), attribute(anchor, 'href'));
      const title = normalizedText(anchor);
      if (path && title && !titles.has(path)) titles.set(path, title);
    }
    return titles;
  }
  const ncxId = attribute(spine, 'toc');
  const ncx = manifest.get(ncxId) || [...manifest.values()].find((item) =>
    item.mediaType === 'application/x-dtbncx+xml');
  if (!ncx) return titles;
  const root = parseXml(textDecoder.decode(await archive.read(ncx.path, MAX_DOCUMENT_BYTES)));
  for (const point of descendants(root, 'navpoint')) {
    const content = descendants(point, 'content')[0];
    const label = descendants(point, 'navlabel')[0];
    const path = archivePath(directoryOf(ncx.path), attribute(content, 'src'));
    const title = normalizedText(label);
    if (path && title && !titles.has(path)) titles.set(path, title);
  }
  return titles;
}

function extractDocument(root, documentPath, manifest, imageByPath, addedImages) {
  const body = descendants(root, 'body')[0] || root;
  const output = [];
  const blockNames = new Set(['address', 'article', 'aside', 'blockquote', 'div', 'dl', 'dt', 'dd',
    'figcaption', 'figure', 'footer', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'header', 'li', 'main',
    'nav', 'ol', 'p', 'pre', 'section', 'table', 'tr', 'ul']);
  const ignoredNames = new Set(['audio', 'canvas', 'head', 'script', 'style', 'video']);
  const walk = (node) => {
    if (node.type === 'text') {
      appendText(output, node.text);
      return;
    }
    if (ignoredNames.has(node.name)) return;
    if (node.name === 'br') {
      ensureBreak(output, 1);
      return;
    }
    if (node.name === 'img') {
      const path = archivePath(directoryOf(documentPath), attribute(node, 'src'));
      const item = findManifestByPath(manifest, path);
      if (path && isSupportedImage(item?.mediaType, path)) {
        let imageId = imageByPath.get(path);
        if (!imageId) {
          imageId = imageByPath.size + 1;
          imageByPath.set(path, imageId);
          addedImages.push({ id: imageId, path, mediaType: item?.mediaType || mediaTypeFromPath(path),
            alt: attribute(node, 'alt').slice(0, 120) });
        }
        ensureBreak(output, 1);
        output.push(imageMarker(imageId));
      } else {
        const alt = attribute(node, 'alt').trim();
        if (alt) appendText(output, `[Illustration: ${alt}]`);
      }
      return;
    }
    const block = blockNames.has(node.name);
    if (block) ensureBreak(output, 1);
    for (const child of node.children) walk(child);
    if (block) ensureBreak(output, node.name === 'p' ? 2 : 1);
  };
  walk(body);
  return {
    text: output.join('').replace(/[ \t]+\n/gu, '\n').replace(/\n{3,}/gu, '\n\n').trim(),
    title: firstText(body, 'h1') || firstText(body, 'h2') || '',
  };
}

function parseXml(source) {
  const root = { type: 'element', name: '#document', attributes: {}, children: [] };
  const stack = [root];
  const tokens = String(source).match(/<!--[\s\S]*?-->|<!\[CDATA\[[\s\S]*?\]\]>|<[^>]+>|[^<]+/gu) ?? [];
  for (const token of tokens) {
    if (token.startsWith('<!--') || token.startsWith('<?') || /^<!DOCTYPE/iu.test(token)) continue;
    if (token.startsWith('<![CDATA[')) {
      stack.at(-1).children.push({ type: 'text', text: token.slice(9, -3) });
      continue;
    }
    if (token.startsWith('</')) {
      if (stack.length > 1) stack.pop();
      continue;
    }
    if (token.startsWith('<')) {
      const match = /^<\s*([^\s/>]+)([\s\S]*?)\/?\s*>$/u.exec(token);
      if (!match) continue;
      const node = {
        type: 'element',
        name: localName(match[1]),
        attributes: parseAttributes(match[2]),
        children: [],
      };
      stack.at(-1).children.push(node);
      if (!/\/\s*>$/u.test(token) && !['br', 'img', 'meta', 'link', 'hr', 'input'].includes(node.name)) {
        stack.push(node);
      }
      continue;
    }
    stack.at(-1).children.push({ type: 'text', text: decodeEntities(token) });
  }
  return root;
}

function parseAttributes(source) {
  const attributes = {};
  const pattern = /([^\s=]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/gu;
  for (const match of source.matchAll(pattern)) attributes[localName(match[1])] = decodeEntities(match[2] ?? match[3] ?? '');
  return attributes;
}

function decodeEntities(value) {
  const named = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ', copy: '©',
    ndash: '–', mdash: '—', hellip: '…', ldquo: '“', rdquo: '”', lsquo: '‘', rsquo: '’' };
  return String(value).replace(/&(#x[0-9a-f]+|#\d+|[a-z]+);/giu, (entity, body) => {
    if (body[0] !== '#') return named[body.toLowerCase()] ?? entity;
    const codePoint = body[1].toLowerCase() === 'x'
      ? Number.parseInt(body.slice(2), 16) : Number.parseInt(body.slice(1), 10);
    try { return String.fromCodePoint(codePoint); } catch { return '�'; }
  });
}

function descendants(node, name) {
  if (!node?.children) return [];
  const found = [];
  for (const child of node.children) {
    if (child.type !== 'element') continue;
    if (!name || child.name === name) found.push(child);
    found.push(...descendants(child, name));
  }
  return found;
}

function childElements(node, name) {
  return (node?.children ?? []).filter((child) => child.type === 'element' && (!name || child.name === name));
}

function textContent(node) {
  if (!node) return '';
  if (node.type === 'text') return node.text;
  return (node.children ?? []).map(textContent).join('');
}

function normalizedText(node) {
  return textContent(node).replace(/\s+/gu, ' ').trim();
}

function firstText(node, name) {
  return normalizedText(descendants(node, name)[0]);
}

function attribute(node, name) {
  return String(node?.attributes?.[localName(name)] ?? '');
}

function localName(name) {
  return String(name).split(':').at(-1).toLowerCase();
}

function appendText(output, text) {
  const normalized = String(text).replace(/\s+/gu, ' ');
  if (!normalized.trim()) {
    if (output.length > 0 && !/[\s\n]$/u.test(output.at(-1))) output.push(' ');
    return;
  }
  if (output.length > 0 && /\s$/u.test(output.at(-1)) && /^\s/u.test(normalized)) output.push(normalized.trimStart());
  else output.push(normalized);
}

function ensureBreak(output, count) {
  if (output.length === 0) return;
  const joinedTail = output.slice(-3).join('');
  const existing = /\n*$/u.exec(joinedTail)?.[0].length ?? 0;
  if (existing < count) output.push('\n'.repeat(count - existing));
}

function archivePath(base, href) {
  if (!href) return '';
  let raw = String(href).split('#', 1)[0].split('?', 1)[0].replace(/\\/gu, '/');
  try { raw = decodeURIComponent(raw); } catch { /* Keep the archive spelling. */ }
  const parts = raw.startsWith('/') ? [] : base.split('/').filter(Boolean);
  for (const part of raw.split('/')) {
    if (!part || part === '.') continue;
    if (part === '..') {
      if (parts.length === 0) return '';
      parts.pop();
    } else parts.push(part);
  }
  return parts.join('/');
}

function directoryOf(path) {
  const index = String(path).lastIndexOf('/');
  return index < 0 ? '' : path.slice(0, index);
}

function findManifestByPath(manifest, path) {
  for (const item of manifest.values()) if (item.path === path) return item;
  return undefined;
}

function isXhtml(mediaType) {
  return mediaType === 'application/xhtml+xml' || mediaType === 'text/html';
}

function isSupportedImage(mediaType, path) {
  return ['image/jpeg', 'image/png'].includes(mediaType) || /\.(?:jpe?g|png)$/iu.test(path ?? '');
}

function mediaTypeFromPath(path) {
  return /\.png$/iu.test(path) ? 'image/png' : 'image/jpeg';
}

function requireSections(index) {
  if (!Array.isArray(index?.epubSections) || index.epubSections.length === 0) {
    throw new Error('EPUB section index is unavailable');
  }
  return index.epubSections;
}

function sectionIndexAt(sections, offset) {
  let low = 0;
  let high = sections.length - 1;
  while (low < high) {
    const middle = Math.floor((low + high + 1) / 2);
    if (sections[middle].byteOffset <= offset) low = middle;
    else high = middle - 1;
  }
  return low;
}

function utf8PrefixLength(bytes, maximum) {
  let length = Math.min(bytes.length, maximum);
  while (length > 0 && length < bytes.length && (bytes[length] & 0xc0) === 0x80) length -= 1;
  return length;
}

function concatenate(left, right) {
  if (left.length === 0) return Uint8Array.from(right);
  const output = new Uint8Array(left.length + right.length);
  output.set(left);
  output.set(right, left.length);
  return output;
}

async function inflateRaw(compressed) {
  if (typeof DecompressionStream !== 'function') {
    throw new Error('This WebView does not support EPUB ZIP decompression');
  }
  const stream = new Blob([compressed]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

async function readRange(storage, file, offset, length) {
  const opened = await storage.openRead(file.fileId, { offset, length });
  if (opened?.fileId !== file.fileId || opened?.size !== file.size || opened?.offset !== offset ||
      opened?.length !== length || !opened?.stream?.getReader) {
    throw new Error('Host returned an invalid EPUB file stream ticket');
  }
  const output = new Uint8Array(length);
  const reader = opened.stream.getReader();
  let written = 0;
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (!(value instanceof Uint8Array) || written + value.length > output.length) {
        throw new Error('Host returned an invalid EPUB binary stream');
      }
      output.set(value, written);
      written += value.length;
    }
  } finally {
    reader.releaseLock();
  }
  if (written !== length) throw new Error('Host EPUB stream ended before its declared range');
  return output;
}

function readU16(bytes, offset) {
  if (offset + 2 > bytes.length) throw new Error('EPUB ZIP integer is truncated');
  return bytes[offset] | (bytes[offset + 1] << 8);
}

function readU32(bytes, offset) {
  if (offset + 4 > bytes.length) throw new Error('EPUB ZIP integer is truncated');
  return (bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) |
    (bytes[offset + 3] << 24)) >>> 0;
}

function findSignatureBackward(bytes, signature) {
  for (let offset = bytes.length - 4; offset >= 0; offset -= 1) {
    if (readU32(bytes, offset) === signature) return offset;
  }
  return -1;
}

function requirePersistentFile(storage, file) {
  if (!storage?.openRead) throw new TypeError('storage.openRead is required');
  if (!file?.fileId || !Number.isSafeInteger(file.size) || file.size <= 0) {
    throw new TypeError('A non-empty persistent EPUB file is required');
  }
}
