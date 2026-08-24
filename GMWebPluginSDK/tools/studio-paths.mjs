import { lstatSync, realpathSync, statSync } from 'node:fs';
import { relative, resolve, sep } from 'node:path';

export function resolveServedFile(root, requestedPath) {
  const normalizedRoot = resolve(root);
  const candidate = resolve(normalizedRoot, requestedPath);
  if (!inside(normalizedRoot, candidate)) return null;

  try {
    const pathFromRoot = relative(normalizedRoot, candidate);
    let componentPath = normalizedRoot;
    for (const component of pathFromRoot.split(sep).filter(Boolean)) {
      componentPath = resolve(componentPath, component);
      if (lstatSync(componentPath).isSymbolicLink()) return null;
    }
    const realRoot = realpathSync(normalizedRoot);
    const realFile = realpathSync(candidate);
    if (!inside(realRoot, realFile) || !statSync(realFile).isFile()) return null;
    return realFile;
  } catch {
    return null;
  }
}

function inside(root, file) {
  return file === root || file.startsWith(`${root}${sep}`);
}
