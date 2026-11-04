// Plugin UI intentionally stays English, independent of host/system locale.
export const messages = {
  "en": {
    "ui.alignment": "Image alignment",
    "alignment.top-left": "Top left",
    "alignment.top-center": "Top center",
    "alignment.top-right": "Top right",
    "alignment.middle-left": "Middle left",
    "alignment.center": "Center",
    "alignment.middle-right": "Middle right",
    "alignment.bottom-left": "Bottom left",
    "alignment.bottom-center": "Bottom center",
    "alignment.bottom-right": "Bottom right",
    "app.title": "Pixel Image Viewer",
    "ui.cancel": "Cancel",
    "ui.retry": "Check connection",
    "ui.connectionStatus": "Connection and transfer status",
    "ui.progress": "Image transfer progress",
    "ui.slots": "Image slots",
    "ui.slotHint": "Dot: loaded · +: add",
    "ui.chooseSlot": "Choose an image slot",
    "ui.addSlot": "Add an image slot",
    "ui.preview": "Image preview",
    "ui.pick": "Choose image",
    "ui.replace": "Replace image",
    "ui.send": "Send to glasses",
    "ui.empty": "No image selected",
    "ui.canvas": "600 × 350 pixel-perfect green preview",
    "ui.info": "Image details",
    "ui.rules": "Display rules and supported formats",
    "ui.rulesBody": "600 × 350 pixel-perfect green preview, without scaling or interpolation. Small images use the selected alignment on black (center by default); excess width or height is cropped from the right or bottom. Switching slots only changes the preview. Tap Send to transfer it to the glasses.",
    "ui.formats": "Supports PNG, JPEG, BMP and WebP, subject to the phone decoder. Transparency is composited on black. Imported files stay in the plugin’s private storage; the glasses keep only the current frame.",
    "connection.loading": "Connecting to host…",
    "connection.on": "Glasses connected",
    "connection.off": "Glasses offline · Images available",
    "slot.preview": "Slot {slot} · Preview",
    "slot.loadedLabel": "Slot {slot}, image loaded",
    "slot.emptyLabel": "Slot {slot}, no image",
    "slot.ready": "Slot {slot} is ready. Tap “Send to glasses”.",
    "slot.empty": "Slot {slot} is empty. Choose an image.",
    "slot.added": "Slot {slot} added. Choose an image.",
    "slot.saved": "Image ready in slot {slot}. Tap “Send to glasses”.",
    "preview.empty": "600 × 350 physical pixels · Green · No scaling",
    "preview.details": "Source {width} × {height} → 600 × 350; green preview at 1:1 physical pixels. 4 bit: {raw} B; LZ4: {compressed} B.",
    "notice.initializing": "Initializing…",
    "notice.decoding": "File loaded. Decoding a pixel-perfect frame…",
    "notice.pick": "Choose an image. Cancel to keep the current image.",
    "notice.pickCancelled": "Selection cancelled. Current image kept.",
    "notice.reading": "Reading image file…",
    "notice.sending": "Sending image…",
    "notice.progress": "{percent}% · {encoding}{waiting}",
    "notice.waiting": " · Confirming…",
    "notice.call": "Image received. It will appear after the call screen closes.",
    "notice.displayed": "Glasses confirmed display (600 × 350, pixel-perfect).",
    "notice.stats": " {encoding}: {raw} → {wire} B in {seconds} s. {reason}",
    "notice.checking": "Checking connection…",
    "notice.connected": "Connection refreshed. Images kept; ready to send.",
    "notice.offline": "Glasses offline. Images kept. Connect the glasses in the App, then check again.",
    "notice.ready": "Choose a slot, load an image, then tap “Send to glasses”. Loaded slots are ready to preview and send.",
    "notice.deviceError": "Local images are available. Device unavailable: {error}",
    "notice.restoreErrors": "{errors}. Please choose the affected images again.",
    "notice.slotError": "Slot {slot}: {error}",
    "error.permission": "Permission denied or revoked. Grant access in the host, then tap “Check connection”.",
    "error.init": "Initialization failed: {error}. Tap “Check connection” to retry, or reopen the plugin if the host is unavailable.",
    "error.cleanup": "New image saved, but the old private copy could not be removed.",
    "error.disconnected": "Glasses disconnected. Reconnect and send again.",
    "error.cancelled": "Operation cancelled. Choose or send an image again.",
    "error.hostTimeout": "Host connection timed out. Open this plugin in the App or Studio.",
    "error.bluetooth": "Bluetooth disconnected. Reconnect and resend the complete image.",
    "error.suspended": "Plugin paused. Return and send again.",
    "error.missingFile": "Saved file no longer exists.",
    "error.closed": "Page closed.",
    "error.abort": "Operation cancelled.",
    "error.stopped": "Plugin stopped. Reopen it to choose an image.",
    "error.notResumed": "File selected, but the App has not resumed. Return to the plugin and retry.",
    "error.format": "Unsupported image. Choose PNG, JPEG, BMP or WebP; export HEIC/HEIF to PNG first.",
    "error.decode": "Image decoding failed. The file may be damaged, too large or unsupported by this phone.",
    "error.transferCancelled": "Transfer cancelled. Send again.",
    "error.ackTimeout": "Glasses response timed out. Check the connection and companion plugin, then resend.",
    "error.rejected": "Glasses rejected the image (status {status}: {reason}). Send again.",
    "reject.2": "invalid data",
    "reject.3": "out of memory",
    "reject.4": "display failed",
    "reject.5": "invalid transfer state",
    "reject.6": "firmware does not support LZ4",
    "reject.unknown": "unsupported",
    "error.busy": "An image transfer is already in progress.",
    "error.companion": "Requires the companion Pixel Image Viewer glasses plugin supporting 600×350.",
    "reason.noLz4": "The glasses plugin or firmware does not provide LZ4.",
    "reason.noSaving": "Compression did not reduce the payload.",
    "error.chunkAck": "Image chunk acknowledgement mismatch. Send again.",
    "error.dimensions": "Invalid image dimensions",
    "error.canvas": "Canvas is unavailable",
    "error.rgba": "Expected 600 × 350 RGBA",
    "error.emptyFile": "Empty or invalid image file",
    "error.stream": "Invalid file stream",
    "error.length": "Invalid file length",
    "error.incompleteFile": "Incomplete image file",
    "error.frame": "Invalid frame",
    "error.beginAck": "Invalid begin acknowledgement",
    "error.incompleteAck": "Incomplete frame acknowledgement"
  }
};

export function createI18n(environment = globalThis) {
  const language = 'en';
  const t = (key, values = {}) => (messages.en[key] ?? key)
    .replace(/\{(\w+)\}/g, (token, name) => {
      if (!(name in values)) return token;
      const value = values[name];
      return String(typeof value === 'function' ? value() : value);
    });
  return {
    get language() { return language; },
    t,
    number(value) { return value.toLocaleString('en-US'); },
    apply(root = environment.document) {
      if (!root) return;
      root.documentElement?.setAttribute('lang', 'en');
      for (const [attribute, target] of [['data-i18n', null], ['data-i18n-aria', 'aria-label'], ['data-i18n-title', 'title']]) {
        for (const node of root.querySelectorAll(`[${attribute}]`)) {
          const text = t(node.getAttribute(attribute));
          if (target) node.setAttribute(target, text); else node.textContent = text;
        }
      }
    },
  };
}
export const i18n = createI18n();
export const t = i18n.t;
// Keep structured error keys and protocol codes independent of presentation.
export class LocalizedError extends Error {
  constructor(key, values = {}) { super(); this.messageKey = key; this.values = values; }
  get message() { return t(this.messageKey, this.values); }
}
export const localizedError = (key, values) => new LocalizedError(key, values);
