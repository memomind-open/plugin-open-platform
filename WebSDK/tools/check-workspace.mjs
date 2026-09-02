import { readdirSync, readFileSync } from 'node:fs';
import { relative, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = resolve(fileURLToPath(new URL('..', import.meta.url)));
const sourceFiles = walk(root).filter((file) => /\.(?:js|mjs)$/.test(file));
const failures = [];

for (const file of sourceFiles) {
  const result = spawnSync(process.execPath, ['--check', file], { encoding: 'utf8' });
  if (result.status !== 0) failures.push(`${relative(root, file)}\n${result.stderr}`);
}

const requiredDocs = [
  'docs/web-plugin/developer-guide.md',
  'docs/web-plugin/README.md',
  'docs/web-plugin/quick-start.md',
  'docs/web-plugin/devkit-zip.md',
  'docs/web-plugin/api-reference.md',
  'docs/web-plugin/studio.md',
  'docs/web-plugin/package-format.md',
  'examples/app-counter/manifest.json',
];
for (const path of requiredDocs) {
  try {
    readFileSync(resolve(root, path));
  } catch {
    failures.push(`Missing required artifact: ${path}`);
  }
}

if (failures.length) {
  process.stderr.write(`${failures.join('\n\n')}\n`);
  process.exit(1);
}
process.stdout.write(`Checked ${sourceFiles.length} JavaScript files and ${requiredDocs.length} artifacts.\n`);

function walk(directory) {
  const files = [];
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    if (entry.name === '.git' || entry.name === 'node_modules' || entry.name === 'target') continue;
    const path = resolve(directory, entry.name);
    if (entry.isDirectory()) files.push(...walk(path));
    else files.push(path);
  }
  return files;
}
