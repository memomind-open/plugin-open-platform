#!/usr/bin/env node
import { createHash, randomBytes } from 'node:crypto';
import { realpathSync } from 'node:fs';
import { mkdir, readFile, readdir, rename, rm, stat, writeFile } from 'node:fs/promises';
import { dirname, extname, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { deflateRawSync } from 'node:zlib';

export const MMPKG_LIMITS = Object.freeze({
  maxPackageBytes: 10 * 1024 * 1024,
  maxExtractedBytes: 30 * 1024 * 1024,
  maxFileBytes: 10 * 1024 * 1024,
  maxFiles: 500,
});

const SUPPORTED_PERMISSIONS = new Set([
  'display',
  'device.events',
  'storage',
  'network',
  'audio.capture',
]);
const PLUGIN_ID = /^[a-zA-Z][a-zA-Z0-9_-]*(\.[a-zA-Z0-9_-]+)+$/;
const SEMVER = /^[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?$/;
const UTF8_FLAG = 0x0800;
const DEFLATE = 8;
const DOS_DATE_1980_01_01 = 0x0021;

export async function buildMmpkg(sourceDirectory, outputPath) {
  const sourceRoot = resolve(sourceDirectory);
  const outputFile = resolve(outputPath);
  if (extname(outputFile).toLowerCase() !== '.mmpkg') throw new Error('输出文件必须使用 .mmpkg 扩展名');
  const outputRelative = relative(sourceRoot, outputFile);
  if (outputRelative === '' || (!outputRelative.startsWith(`..${sep}`) && outputRelative !== '..')) {
    throw new Error('mmpkg 输出文件不能位于插件输入目录内');
  }
  const sourceStat = await stat(sourceRoot).catch(() => null);
  if (!sourceStat?.isDirectory()) throw new Error(`插件输入目录不存在: ${sourceRoot}`);

  const manifestPath = resolve(sourceRoot, 'manifest.json');
  const sourceManifest = parseJson(await readFile(manifestPath, 'utf8').catch(() => {
    throw new Error('插件输入目录缺少 manifest.json');
  }));
  validateManifest(sourceManifest);

  const files = await collectFiles(sourceRoot);
  const payloads = new Map();
  let signature;
  for (const file of files) {
    const bytes = await readFile(file.absolutePath);
    if (bytes.length > MMPKG_LIMITS.maxFileBytes) throw new Error(`${file.path} 超过单文件 10 MB 限制`);
    if (file.path === 'manifest.json') continue;
    if (file.path === 'signature.sig') {
      signature = bytes;
      continue;
    }
    payloads.set(file.path, bytes);
  }
  if (!payloads.has(sourceManifest.entry)) throw new Error(`entry 文件不存在: ${sourceManifest.entry}`);

  const hashes = {};
  for (const path of [...payloads.keys()].sort()) {
    hashes[path] = `sha256:${createHash('sha256').update(payloads.get(path)).digest('hex')}`;
  }
  const finalManifest = {
    ...sourceManifest,
    schemaVersion: 1,
    files: hashes,
  };
  const manifestBytes = Buffer.from(`${JSON.stringify(finalManifest, null, 2)}\n`);
  const entries = [
    { path: 'manifest.json', bytes: manifestBytes },
    ...[...payloads].sort(([left], [right]) => left.localeCompare(right)).map(([path, bytes]) => ({ path, bytes })),
    ...(signature ? [{ path: 'signature.sig', bytes: signature }] : []),
  ];
  validatePackageLimits(entries);

  const archive = createZip(entries);
  if (archive.length > MMPKG_LIMITS.maxPackageBytes) throw new Error('生成的 mmpkg 超过 10 MB 限制');
  await mkdir(dirname(outputFile), { recursive: true });
  const temporary = `${outputFile}.tmp-${randomBytes(8).toString('hex')}`;
  try {
    await writeFile(temporary, archive, { flag: 'wx' });
    await rename(temporary, outputFile);
  } finally {
    await rm(temporary, { force: true });
  }
  return {
    id: finalManifest.id,
    version: finalManifest.version,
    fileCount: entries.length,
    outputBytes: archive.length,
    outputFile,
  };
}

async function collectFiles(root) {
  const result = [];
  await walk(root, '');
  return result;

  async function walk(directory, prefix) {
    const entries = await readdir(directory, { withFileTypes: true });
    entries.sort((left, right) => left.name.localeCompare(right.name));
    for (const entry of entries) {
      const path = prefix ? `${prefix}/${entry.name}` : entry.name;
      requireSafePath(path, '文件路径');
      const absolutePath = resolve(directory, entry.name);
      if (entry.isSymbolicLink()) throw new Error(`插件输入目录不允许符号链接: ${path}`);
      if (entry.isDirectory()) await walk(absolutePath, path);
      else if (entry.isFile()) result.push({ path, absolutePath });
      else throw new Error(`插件输入目录包含不支持的文件类型: ${path}`);
    }
  }
}

function validateManifest(manifest) {
  if (!manifest || typeof manifest !== 'object' || Array.isArray(manifest)) throw new Error('manifest.json 必须是 JSON 对象');
  requireString(manifest, 'id', 128);
  requireString(manifest, 'name', 80);
  requireString(manifest, 'version', 64);
  requireString(manifest, 'entry', 256);
  requireString(manifest, 'bridgeVersion', 32);
  if (!PLUGIN_ID.test(manifest.id)) throw new Error('插件 id 格式无效');
  if (!SEMVER.test(manifest.version)) throw new Error('插件 version 必须是语义化版本');
  requireSafePath(manifest.entry, 'entry');
  if (!manifest.entry.toLowerCase().endsWith('.html')) throw new Error('entry 必须指向 HTML 文件');
  if (manifest.bridgeVersion !== '1.0') throw new Error(`不支持 bridgeVersion=${manifest.bridgeVersion}`);
  if (!Array.isArray(manifest.permissions) || manifest.permissions.length > 16) throw new Error('permissions 必须是有界数组');
  const seen = new Set();
  for (const item of manifest.permissions) {
    const permission = typeof item === 'string' ? item : item?.name;
    if (typeof permission !== 'string' || !SUPPORTED_PERMISSIONS.has(permission)) throw new Error(`不支持插件权限 ${String(permission)}`);
    if (seen.has(permission)) throw new Error(`插件权限 ${permission} 重复`);
    seen.add(permission);
  }
  validateDeviceRequirements(manifest.deviceRequirements);
}

function validateDeviceRequirements(requirements) {
  if (requirements === undefined) return;
  if (!requirements || typeof requirements !== 'object' || Array.isArray(requirements)) {
    throw new Error('deviceRequirements 必须是 JSON 对象');
  }
  for (const field of ['preferredPluginId', 'requiredPluginId']) {
    if (requirements[field] === undefined) continue;
    requireString(requirements, field, 128);
    if (!PLUGIN_ID.test(requirements[field])) throw new Error(`${field} 插件 id 格式无效`);
  }
  if (requirements.minPluginVersion !== undefined &&
      (typeof requirements.minPluginVersion !== 'string' ||
       !/^\d+(?:\.\d+){0,3}$/u.test(requirements.minPluginVersion))) {
    throw new Error('minPluginVersion 必须是数字版本字符串');
  }
  if (!Array.isArray(requirements.protocols) || requirements.protocols.length > 16) {
    throw new Error('deviceRequirements.protocols 必须是最多 16 项的数组');
  }
  const protocols = new Set();
  for (const protocol of requirements.protocols) {
    if (!protocol || typeof protocol !== 'object' || Array.isArray(protocol)) {
      throw new Error('设备协议声明必须是 JSON 对象');
    }
    requireString(protocol, 'id', 80);
    requireString(protocol, 'minVersion', 32);
    if (!/^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$/u.test(protocol.id)) {
      throw new Error(`设备协议 id 格式无效: ${protocol.id}`);
    }
    if (!/^\d+(?:\.\d+){0,3}$/u.test(protocol.minVersion)) {
      throw new Error(`设备协议版本格式无效: ${protocol.minVersion}`);
    }
    if (protocols.has(protocol.id)) throw new Error(`设备协议 ${protocol.id} 重复`);
    protocols.add(protocol.id);
  }
}

function validatePackageLimits(entries) {
  if (entries.length > MMPKG_LIMITS.maxFiles) throw new Error('插件包普通文件超过 500 个');
  let total = 0;
  for (const entry of entries) {
    if (entry.bytes.length > MMPKG_LIMITS.maxFileBytes) throw new Error(`${entry.path} 超过单文件 10 MB 限制`);
    total += entry.bytes.length;
  }
  if (total > MMPKG_LIMITS.maxExtractedBytes) throw new Error('插件包解压后超过 30 MB 限制');
}

export function createZip(entries) {
  const localParts = [];
  const centralParts = [];
  let offset = 0;
  for (const entry of entries) {
    const name = Buffer.from(entry.path);
    const compressed = deflateRawSync(entry.bytes, { level: 9 });
    const checksum = crc32(entry.bytes);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(UTF8_FLAG, 6);
    local.writeUInt16LE(DEFLATE, 8);
    local.writeUInt16LE(0, 10);
    local.writeUInt16LE(DOS_DATE_1980_01_01, 12);
    local.writeUInt32LE(checksum, 14);
    local.writeUInt32LE(compressed.length, 18);
    local.writeUInt32LE(entry.bytes.length, 22);
    local.writeUInt16LE(name.length, 26);
    local.writeUInt16LE(0, 28);
    localParts.push(local, name, compressed);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(0x0314, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(UTF8_FLAG, 8);
    central.writeUInt16LE(DEFLATE, 10);
    central.writeUInt16LE(0, 12);
    central.writeUInt16LE(DOS_DATE_1980_01_01, 14);
    central.writeUInt32LE(checksum, 16);
    central.writeUInt32LE(compressed.length, 20);
    central.writeUInt32LE(entry.bytes.length, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt16LE(0, 30);
    central.writeUInt16LE(0, 32);
    central.writeUInt16LE(0, 34);
    central.writeUInt16LE(0, 36);
    central.writeUInt32LE((0o100644 << 16) >>> 0, 38);
    central.writeUInt32LE(offset, 42);
    centralParts.push(central, name);
    offset += local.length + name.length + compressed.length;
  }
  const centralDirectory = Buffer.concat(centralParts);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralDirectory.length, 12);
  end.writeUInt32LE(offset, 16);
  end.writeUInt16LE(0, 20);
  return Buffer.concat([...localParts, centralDirectory, end]);
}

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) crc = CRC32_TABLE[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

const CRC32_TABLE = Uint32Array.from({ length: 256 }, (_, index) => {
  let value = index;
  for (let bit = 0; bit < 8; bit += 1) value = (value & 1) ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
  return value >>> 0;
});

function parseJson(value) {
  try {
    return JSON.parse(value);
  } catch {
    throw new Error('manifest.json 不是有效 JSON');
  }
}

function requireString(values, key, maxLength) {
  const value = values[key];
  if (typeof value !== 'string' || value.trim() === '' || value.length > maxLength) throw new Error(`${key} 必须是有效字符串`);
}

function requireSafePath(value, field) {
  const segments = value.split('/');
  if (!value || value.startsWith('/') || value.includes('\\') || segments.some((segment) => !segment || segment === '.' || segment === '..')) {
    throw new Error(`${field} 包含不安全路径`);
  }
}

async function main(arguments_) {
  if (arguments_.length !== 2) {
    process.stderr.write('用法: build-mmpkg <插件构建目录> <输出.mmpkg>\n');
    process.exitCode = 64;
    return;
  }
  try {
    const result = await buildMmpkg(arguments_[0], arguments_[1]);
    process.stdout.write(`${result.id} ${result.version} -> ${result.outputFile} (${result.fileCount} files, ${result.outputBytes} bytes)\n`);
  } catch (error) {
    process.stderr.write(`mmpkg 构建失败: ${error.message}\n`);
    process.exitCode = 1;
  }
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  await main(process.argv.slice(2));
}
