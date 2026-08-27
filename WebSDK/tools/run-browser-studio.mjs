#!/usr/bin/env node
import { createReadStream, existsSync, readFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { extname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { resolveServedFile } from './browser-studio-paths.mjs';

const repositoryRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const options = parseArguments(process.argv.slice(2));
const pluginRoot = resolve(repositoryRoot, options.plugin);

if (!existsSync(resolve(pluginRoot, 'index.html'))) {
  process.stderr.write(`Plugin entry not found: ${resolve(pluginRoot, 'index.html')}\n`);
  process.exit(1);
}

const mounts = [
  ['/plugin/', pluginRoot],
  ['/sdk/', resolve(repositoryRoot, 'packages/web-sdk/src')],
  ['/contract/', resolve(repositoryRoot, 'packages/bridge-contract/src')],
  ['/renderer/', resolve(repositoryRoot, 'packages/device-renderer/src')],
  ['/runtime/', resolve(repositoryRoot, 'packages/studio-runtime/src')],
  ['/', resolve(repositoryRoot, 'tools/browser-studio')],
];

const server = createServer((request, response) => {
  try {
    const url = new URL(request.url, `http://${request.headers.host ?? 'localhost'}`);
    const decodedPath = decodeURIComponent(url.pathname);
    const mount = mounts.find(([prefix]) => decodedPath.startsWith(prefix));
    if (!mount) return notFound(response);
    const [prefix, root] = mount;
    let relative = decodedPath.slice(prefix.length);
    if (decodedPath === '/') relative = 'index.html';
    if (decodedPath === '/plugin/') relative = 'index.html';
    const file = resolveServedFile(root, relative);
    if (!file) return notFound(response);
    response.writeHead(200, {
      'Content-Type': mimeType(file),
      'Cache-Control': 'no-store',
      'Cross-Origin-Resource-Policy': 'same-origin',
    });
    if (decodedPath === '/plugin/index.html') {
      response.end(injectStudioHost(readFileSync(file, 'utf8')));
      return;
    }
    createReadStream(file).pipe(response);
  } catch (error) {
    response.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
    response.end(`Studio server error: ${error.message}`);
  }
});

server.listen(options.port, options.host, () => {
  process.stdout.write(`GM Plugin Studio\nhttp://${options.host}:${options.port}\nPlugin: ${pluginRoot}\n`);
});

function parseArguments(arguments_) {
  const result = { plugin: 'examples/basic-counter', host: '127.0.0.1', port: 4173 };
  for (let index = 0; index < arguments_.length; index += 1) {
    const name = arguments_[index];
    const value = arguments_[index + 1];
    if (name === '--plugin' && value) result.plugin = value;
    else if (name === '--host' && value) result.host = value;
    else if (name === '--port' && value) result.port = Number(value);
    else continue;
    index += 1;
  }
  if (!Number.isInteger(result.port) || result.port < 1 || result.port > 65535) throw new Error('port must be 1..65535');
  return result;
}

function notFound(response) {
  response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
  response.end('Not found');
}

function mimeType(file) {
  return {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.mjs': 'text/javascript; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.svg': 'image/svg+xml',
    '.png': 'image/png',
    '.wav': 'audio/wav',
    '.mp3': 'audio/mpeg',
  }[extname(file)] ?? 'application/octet-stream';
}

function injectStudioHost(html) {
  const script = '<script src="/plugin-host-shim.js"></script>';
  const head = html.search(/<\/head\s*>/i);
  if (head >= 0) return `${html.slice(0, head)}${script}\n${html.slice(head)}`;
  return `${script}\n${html}`;
}
