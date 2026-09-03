const STORAGE_KEY_PREFIX = 'novel-reader.book.v3.';
const CHAPTER_KEY_PREFIX = 'novel-reader.chapters.v1.';
const CHAPTER_PAGE_BYTES = 48 * 1024;
const encoder = new TextEncoder();

export class ReaderStorage {
  constructor(gm) {
    if (!gm?.files || !gm?.storage) {
      throw new TypeError('ReaderStorage requires files and storage APIs');
    }
    this.gm = gm;
    this.filesById = new Map();
  }

  async open() {
    await this.reloadFiles();
    return this;
  }

  async reloadFiles() {
    const result = await this.gm.files.list();
    const files = Array.isArray(result?.files) ? result.files : [];
    this.filesById = new Map(
      files.filter(isTextFile).map((file) => [file.fileId, normalizeFile(file)]),
    );
    return [...this.filesById.values()];
  }

  async listBooks() {
    const files = await this.reloadFiles();
    return Promise.all(files.map(async (file) => {
      const saved = await this.getMetadata(file.fileId, false);
      if (saved) return mergeFileMetadata(file, saved);
      const metadata = createDefaultMetadata(file);
      await this.putMetadata(metadata);
      return metadata;
    }));
  }

  async pickBook() {
    const result = await this.gm.files.pick({
      extensions: ['txt'],
      allowMultiple: false,
    });
    const file = result?.files?.[0];
    if (!file) return null;
    const normalized = normalizeFile(file);
    this.filesById.set(normalized.fileId, normalized);
    return normalized;
  }

  async getBook(fileId) {
    let file = this.filesById.get(fileId);
    if (!file) {
      const result = await this.gm.files.stat(fileId);
      file = normalizeFile(result?.file);
      this.filesById.set(file.fileId, file);
    }
    const saved = await this.getMetadata(fileId);
    return {
      file,
      meta: saved ? mergeFileMetadata(file, saved) : createDefaultMetadata(file),
    };
  }

  async getMetadata(fileId, includeChapters = true) {
    const result = await this.gm.storage.get(storageKey(fileId));
    if (!isBookMetadata(result?.value, fileId)) return null;
    const metadata = result.value;
    if (!includeChapters || !Number.isSafeInteger(metadata.chapterIndexPageCount) ||
        metadata.chapterIndexPageCount <= 0) return metadata;
    const pages = await Promise.all(Array.from(
      { length: metadata.chapterIndexPageCount },
      (_, index) => this.gm.storage.get(chapterKey(fileId, index)),
    ));
    const chapters = pages.flatMap((page) => Array.isArray(page?.value) ? page.value : []);
    return chapters.length > 0 ? { ...metadata, chapters } : metadata;
  }

  async putMetadata(metadata, { writeIndex = false } = {}) {
    const fileId = requireFileId(metadata?.fileId);
    const { chapters, ...stored } = metadata;
    if (writeIndex) {
      const pages = paginateChapters(chapters);
      const previousCount = Number.isSafeInteger(metadata.chapterIndexPageCount)
        ? metadata.chapterIndexPageCount
        : 0;
      await Promise.all(pages.map((page, index) =>
        this.gm.storage.set(chapterKey(fileId, index), page)));
      await Promise.all(Array.from(
        { length: Math.max(0, previousCount - pages.length) },
        (_, index) => this.gm.storage.remove(chapterKey(fileId, pages.length + index)),
      ));
      stored.chapterIndexPageCount = pages.length;
      metadata.chapterIndexPageCount = pages.length;
    }
    await this.gm.storage.set(storageKey(fileId), {
      ...stored,
      fileId,
    });
  }

  async deleteBook(fileId) {
    requireFileId(fileId);
    const metadata = await this.getMetadata(fileId, false);
    const result = await this.gm.files.delete(fileId);
    await this.gm.storage.remove(storageKey(fileId));
    await Promise.all(Array.from(
      { length: metadata?.chapterIndexPageCount ?? 0 },
      (_, index) => this.gm.storage.remove(chapterKey(fileId, index)),
    ));
    this.filesById.delete(fileId);
    return result?.deleted === true;
  }

  openRead(fileId, options) {
    requireFileId(fileId);
    return this.gm.files.openRead(fileId, options);
  }

  getUsage() {
    return this.gm.files.getUsage();
  }
}

export function createDefaultMetadata(file) {
  const normalized = normalizeFile(file);
  const importedAt = Date.parse(normalized.importedAt) || Date.now();
  return {
    fileId: normalized.fileId,
    title: safeTitle(normalized.name),
    encoding: 'auto',
    sourceBytes: normalized.size,
    textBytes: normalized.size,
    addedAt: importedAt,
    updatedAt: importedAt,
    progressOffset: 0,
    bookmarks: [],
    fontMode: 0,
    speed: 16,
    speedProfileVersion: 2,
    playing: true,
  };
}

function mergeFileMetadata(file, metadata) {
  return {
    ...createDefaultMetadata(file),
    ...metadata,
    fileId: file.fileId,
    sourceBytes: file.size,
  };
}

function normalizeFile(file) {
  const fileId = requireFileId(file?.fileId);
  if (typeof file?.name !== 'string' || !file.name.trim()) {
    throw new Error('Host returned a file without a valid name');
  }
  if (!Number.isSafeInteger(file.size) || file.size < 0) {
    throw new Error('Host returned a file without a valid size');
  }
  return {
    fileId,
    name: file.name,
    size: file.size,
    importedAt: typeof file.importedAt === 'string'
      ? file.importedAt
      : new Date().toISOString(),
    ...(typeof file.extension === 'string' ? { extension: file.extension } : {}),
  };
}

function isTextFile(file) {
  const extension = String(file?.extension ?? '').replace(/^\./u, '').toLowerCase();
  return extension === 'txt' || (!extension && /\.txt$/iu.test(file?.name ?? ''));
}

function isBookMetadata(value, fileId) {
  return Boolean(value && typeof value === 'object' && !Array.isArray(value) &&
    value.fileId === fileId);
}

function storageKey(fileId) {
  return `${STORAGE_KEY_PREFIX}${requireFileId(fileId)}`;
}

function chapterKey(fileId, index) {
  return `${CHAPTER_KEY_PREFIX}${requireFileId(fileId)}.${index}`;
}

function paginateChapters(chapters) {
  if (!Array.isArray(chapters) || chapters.length === 0) {
    throw new Error('A non-empty chapter index is required');
  }
  const pages = [];
  let page = [];
  let pageBytes = 2;
  for (const chapter of chapters) {
    const itemBytes = encoder.encode(JSON.stringify(chapter)).length + (page.length > 0 ? 1 : 0);
    if (itemBytes + 2 > CHAPTER_PAGE_BYTES) throw new Error('Chapter index entry is too large');
    if (page.length > 0 && pageBytes + itemBytes > CHAPTER_PAGE_BYTES) {
      pages.push(page);
      page = [];
      pageBytes = 2;
    }
    page.push(chapter);
    pageBytes += itemBytes;
  }
  if (page.length > 0) pages.push(page);
  return pages;
}

function requireFileId(value) {
  if (typeof value !== 'string' || !/^[0-9a-f]{32}$/u.test(value)) {
    throw new Error('Invalid persistent file ID');
  }
  return value;
}

function safeTitle(filename) {
  const title = String(filename ?? '').replace(/\.txt$/iu, '').trim();
  return title || 'Untitled Novel';
}
