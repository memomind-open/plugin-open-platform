#!/usr/bin/env node

import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import {
  cpSync,
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  rmSync,
  statSync,
  writeFileSync,
} from 'node:fs';
import { basename, dirname, extname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const studioRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const tauriRoot = join(studioRoot, 'desktop', 'src-tauri');
const targetRoot = join(studioRoot, '.build', 'cargo');
const exportRoot = resolve(optionValue('--output-root') ?? join(studioRoot, 'export'));
const supportedPlatforms = new Set([
  'windows-x64',
  'windows-arm64',
  'linux-x64',
  'linux-arm64',
  'macos-x64',
  'macos-arm64',
]);

function optionValue(name) {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : undefined;
}

function hasFlag(name) {
  return process.argv.includes(name);
}

function hostPlatform() {
  const os = { win32: 'windows', linux: 'linux', darwin: 'macos' }[process.platform];
  const arch = { x64: 'x64', arm64: 'arm64' }[process.arch];
  if (!os || !arch) throw new Error(`Unsupported build host: ${process.platform}-${process.arch}`);
  return `${os}-${arch}`;
}

function runBuild(target, runner, noBundle, bundles) {
  const args = ['tauri', 'build'];
  if (target) args.push('--target', target);
  if (runner) args.push('--runner', runner);
  if (noBundle) args.push('--no-bundle');
  if (bundles) args.push('--bundles', bundles);

  const env = { ...process.env };
  const toolDirectories = [
    optionValue('--tool-dir'),
    env.GM_STUDIO_TOOL_DIR,
    resolve(studioRoot, '../../..', '.windows-build-tools', 'bin'),
    env.HOME ? join(env.HOME, '.windows-build-tools', 'bin') : null,
  ].filter((path) => path && existsSync(path));
  if (toolDirectories.length > 0) {
    env.PATH = `${toolDirectories.join(':')}:${env.PATH ?? ''}`;
  }
  const llvmPath = '/usr/lib/llvm-19/bin';
  if (runner === 'cargo-xwin' && process.platform === 'linux' && existsSync(llvmPath)) {
    env.PATH = `${llvmPath}:${env.PATH ?? ''}`;
  }
  execFileSync('cargo', args, { cwd: tauriRoot, env, stdio: 'inherit' });
}

function walk(directory, predicate, output = []) {
  if (!existsSync(directory)) return output;
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) {
      if (entry.name.endsWith('.app') && predicate(path, true)) output.push(path);
      else walk(path, predicate, output);
    } else if (predicate(path, false)) {
      output.push(path);
    }
  }
  return output;
}

function discoverArtifacts(target, platform) {
  const release = target ? join(targetRoot, target, 'release') : join(targetRoot, 'release');
  const executable = join(release, `gm-plugin-studio-desktop${platform.startsWith('windows-') ? '.exe' : ''}`);
  const artifacts = existsSync(executable) ? [executable] : [];
  const bundleExtensions = new Set(['.msi', '.exe', '.deb', '.rpm', '.AppImage', '.dmg']);
  artifacts.push(...walk(join(release, 'bundle'), (path, directory) => (
    directory ? path.endsWith('.app') : bundleExtensions.has(extname(path))
  )));
  return [...new Set(artifacts.map((path) => resolve(path)))];
}

function sha256(path) {
  return createHash('sha256').update(readFileSync(path)).digest('hex');
}

function exportedName(source) {
  const name = basename(source);
  if (name === 'gm-plugin-studio-desktop.exe') return 'MemoMind-Plugin-Studio-portable.exe';
  if (name === 'gm-plugin-studio-desktop') return 'MemoMind-Plugin-Studio-portable';
  return name;
}

function preferredArtifact(paths, platform) {
  const preferences = platform.startsWith('windows-')
    ? [/setup\.exe$/iu, /\.msi$/iu, /\.exe$/iu]
    : platform.startsWith('linux-')
      ? [/\.AppImage$/u, /\.deb$/u, /\.rpm$/u, /gm-plugin-studio-desktop$/u]
      : [/\.dmg$/iu, /\.app$/iu];
  for (const preference of preferences) {
    const match = paths.find((path) => preference.test(path));
    if (match) return match;
  }
  return paths[0];
}

const platform = optionValue('--platform') ?? hostPlatform();
if (!supportedPlatforms.has(platform)) {
  throw new Error(`Unsupported export platform '${platform}'. Expected one of: ${[...supportedPlatforms].join(', ')}`);
}

const target = optionValue('--target');
const runner = optionValue('--runner');
if (hasFlag('--build')) {
  runBuild(target, runner, hasFlag('--no-bundle'), optionValue('--bundles'));
}
if (hasFlag('--build-only')) {
  process.exit(0);
}

const explicitInputs = [];
for (let index = 0; index < process.argv.length; index += 1) {
  if (process.argv[index] === '--input' && process.argv[index + 1]) explicitInputs.push(resolve(process.argv[index + 1]));
}
let artifacts = explicitInputs.length > 0 ? explicitInputs : discoverArtifacts(target, platform);
if (artifacts.length === 0) {
  throw new Error('No Studio artifacts found. Use --build or pass one or more --input paths.');
}
if (hasFlag('--single-file')) {
  artifacts = [preferredArtifact(artifacts, platform)];
}
for (const artifact of artifacts) {
  if (!existsSync(artifact)) throw new Error(`Artifact does not exist: ${artifact}`);
}

const outputDirectory = join(exportRoot, platform);
rmSync(outputDirectory, { recursive: true, force: true });
mkdirSync(outputDirectory, { recursive: true });

const exported = artifacts.map((source) => {
  const name = exportedName(source);
  const destination = join(outputDirectory, name);
  const directory = statSync(source).isDirectory();
  cpSync(source, destination, { recursive: directory });
  return {
    name,
    type: directory ? 'directory' : 'file',
    size: directory ? null : statSync(destination).size,
    sha256: directory ? null : sha256(destination),
  };
});

let gitCommit = null;
let sourceDirty = null;
try {
  gitCommit = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: studioRoot, encoding: 'utf8' }).trim();
  sourceDirty = execFileSync('git', ['status', '--porcelain', '--untracked-files=normal'], {
    cwd: studioRoot,
    encoding: 'utf8',
  }).trim().length > 0;
} catch {
  // Exporting from a source archive is supported even when Git is unavailable.
}
const tauriConfig = JSON.parse(readFileSync(join(tauriRoot, 'tauri.conf.json'), 'utf8'));
const manifest = {
  productName: tauriConfig.productName,
  version: tauriConfig.version,
  platform,
  generatedAt: new Date().toISOString(),
  gitCommit,
  sourceDirty,
  artifacts: exported,
};
if (!hasFlag('--single-file')) {
  writeFileSync(join(outputDirectory, 'manifest.json'), `${JSON.stringify(manifest, null, 2)}\n`);
  const checksums = exported
    .filter((artifact) => artifact.sha256)
    .map((artifact) => `${artifact.sha256}  ${artifact.name}`)
    .join('\n');
  writeFileSync(join(outputDirectory, 'SHA256SUMS.txt'), checksums ? `${checksums}\n` : '');
}

console.log(`Exported ${exported.length} artifact(s) to ${relative(process.cwd(), outputDirectory) || outputDirectory}`);
