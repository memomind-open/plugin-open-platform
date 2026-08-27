const EXTENDED_UNC_PREFIX = '\\\\?\\UNC\\';
const EXTENDED_PATH_PREFIX = '\\\\?\\';

export function displayPath(path) {
  if (typeof path !== 'string') return '';
  if (path.startsWith(EXTENDED_UNC_PREFIX)) {
    return `\\\\${path.slice(EXTENDED_UNC_PREFIX.length)}`;
  }
  if (path.startsWith(EXTENDED_PATH_PREFIX)) {
    return path.slice(EXTENDED_PATH_PREFIX.length);
  }
  return path;
}
