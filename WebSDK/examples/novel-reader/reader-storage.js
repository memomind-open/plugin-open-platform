const DATABASE_NAME = 'memomind-novel-reader';
const DATABASE_VERSION = 1;

export class ReaderStorage {
  async open() {
    this.database = await new Promise((resolve, reject) => {
      const request = indexedDB.open(DATABASE_NAME, DATABASE_VERSION);
      request.addEventListener('upgradeneeded', () => {
        const database = request.result;
        if (!database.objectStoreNames.contains('books')) {
          database.createObjectStore('books', { keyPath: 'id' });
        }
        if (!database.objectStoreNames.contains('contents')) {
          database.createObjectStore('contents', { keyPath: 'id' });
        }
      });
      request.addEventListener('success', () => resolve(request.result), { once: true });
      request.addEventListener('error', () => reject(request.error), { once: true });
    });
    return this;
  }

  async listBooks() {
    return this.#request('books', 'readonly', (store) => store.getAll());
  }

  async getBook(id) {
    const [metadata, content] = await Promise.all([
      this.#request('books', 'readonly', (store) => store.get(id)),
      this.#request('contents', 'readonly', (store) => store.get(id)),
    ]);
    if (!metadata || !content) return null;
    return { meta: metadata, text: content.text };
  }

  async getMetadata(id) {
    return this.#request('books', 'readonly', (store) => store.get(id));
  }

  async putBook(metadata, text) {
    const transaction = this.database.transaction(['books', 'contents'], 'readwrite');
    transaction.objectStore('books').put(metadata);
    transaction.objectStore('contents').put({ id: metadata.id, text });
    await transactionDone(transaction);
  }

  async putMetadata(metadata) {
    await this.#request('books', 'readwrite', (store) => store.put(metadata));
  }

  async deleteBook(id) {
    const transaction = this.database.transaction(['books', 'contents'], 'readwrite');
    transaction.objectStore('books').delete(id);
    transaction.objectStore('contents').delete(id);
    await transactionDone(transaction);
  }

  #request(storeName, mode, operation) {
    return new Promise((resolve, reject) => {
      const transaction = this.database.transaction(storeName, mode);
      const request = operation(transaction.objectStore(storeName));
      request.addEventListener('success', () => resolve(request.result), { once: true });
      request.addEventListener('error', () => reject(request.error), { once: true });
    });
  }
}

function transactionDone(transaction) {
  return new Promise((resolve, reject) => {
    transaction.addEventListener('complete', resolve, { once: true });
    transaction.addEventListener('abort', () => reject(transaction.error), { once: true });
    transaction.addEventListener('error', () => reject(transaction.error), { once: true });
  });
}
