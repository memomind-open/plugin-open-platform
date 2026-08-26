#include "previewer_c_api.h"

#include "previewer.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <vector>

struct gm_preview_handle {
    gmpreview::Previewer previewer;
    std::string last_error;
};

namespace {

template <typename Operation>
int protect(gm_preview_handle *handle, Operation operation)
{
    if (!handle) return 0;
    try {
        operation(handle->previewer);
        handle->last_error.clear();
        return 1;
    } catch (const std::exception &error) {
        handle->last_error = error.what();
    } catch (...) {
        handle->last_error = "unknown native previewer error";
    }
    return 0;
}

} // namespace

gm_preview_handle *gm_preview_create(void)
{
    try {
        return new gm_preview_handle();
    } catch (...) {
        return nullptr;
    }
}

void gm_preview_destroy(gm_preview_handle *handle)
{
    delete handle;
}

int gm_preview_load(gm_preview_handle *handle, const char *path)
{
    if (!path) return 0;
    return protect(handle, [path](gmpreview::Previewer &previewer) {
        previewer.loadFile(path);
    });
}

int gm_preview_start(gm_preview_handle *handle)
{
    return protect(handle, [](gmpreview::Previewer &previewer) {
        previewer.start();
    });
}

int gm_preview_stop(gm_preview_handle *handle)
{
    return protect(handle, [](gmpreview::Previewer &previewer) {
        previewer.stop();
    });
}

int gm_preview_tick(gm_preview_handle *handle, uint32_t elapsed_ms)
{
    return protect(handle, [elapsed_ms](gmpreview::Previewer &previewer) {
        previewer.tick(elapsed_ms);
    });
}

int gm_preview_send_bluetooth(gm_preview_handle *handle, uint16_t channel,
                              const uint8_t *payload, size_t payload_size,
                              int *handled)
{
    if ((!payload && payload_size != 0) || !handled) return 0;
    return protect(handle, [&](gmpreview::Previewer &previewer) {
        std::vector<uint8_t> bytes;
        if (payload_size != 0) bytes.assign(payload, payload + payload_size);
        *handled = previewer.sendBluetooth(channel, bytes) ? 1 : 0;
    });
}

int gm_preview_send_button(gm_preview_handle *handle, uint16_t action,
                           uint16_t button, int *handled)
{
    if (!handled) return 0;
    return protect(handle, [&](gmpreview::Previewer &previewer) {
        *handled = previewer.sendButton(action, button) ? 1 : 0;
    });
}

int gm_preview_send_gesture(gm_preview_handle *handle, uint16_t gesture,
                            int active, int *handled)
{
    if (!handled) return 0;
    return protect(handle, [&](gmpreview::Previewer &previewer) {
        *handled = previewer.sendGesture(gesture, active != 0) ? 1 : 0;
    });
}

int gm_preview_simulate_direction_gesture(gm_preview_handle *handle,
                                          uint16_t gesture, int *handled)
{
    if (!handled) return 0;
    return protect(handle, [&](gmpreview::Previewer &previewer) {
        *handled = previewer.simulateDirectionGesture(gesture) ? 1 : 0;
    });
}

int gm_preview_is_loaded(const gm_preview_handle *handle)
{
    return handle && handle->previewer.loaded();
}

int gm_preview_is_running(const gm_preview_handle *handle)
{
    return handle && handle->previewer.running();
}

size_t gm_preview_frame_size(void)
{
    return static_cast<size_t>(gmpreview::Previewer::kDisplayWidth) *
           static_cast<size_t>(gmpreview::Previewer::kDisplayHeight);
}

int gm_preview_copy_frame(gm_preview_handle *handle, uint8_t *output,
                          size_t output_size)
{
    if (!output || output_size < gm_preview_frame_size()) return 0;
    return protect(handle, [&](gmpreview::Previewer &previewer) {
        const std::vector<uint8_t> &frame = previewer.renderFrame();
        std::copy(frame.begin(), frame.end(), output);
    });
}

size_t gm_preview_text_overlay_count(const gm_preview_handle *handle)
{
    return handle ? handle->previewer.textOverlays().size() : 0;
}

int gm_preview_get_text_overlay(const gm_preview_handle *handle, size_t index,
                                gm_preview_text_overlay *output)
{
    if (!handle || !output || index >= handle->previewer.textOverlays().size()) return 0;
    const gmpreview::TextOverlay &overlay = handle->previewer.textOverlays()[index];
    output->x = overlay.x;
    output->y = overlay.y;
    output->width = overlay.width;
    output->height = overlay.height;
    output->font_height = overlay.font_height;
    output->alignment = overlay.alignment;
    output->letter_space = overlay.letter_space;
    output->line_space = overlay.line_space;
    output->gray = overlay.gray;
    output->opacity = overlay.opacity;
    output->wrap = overlay.wrap ? 1 : 0;
    output->reserved = 0;
    output->utf8_size = overlay.utf8.size();
    return 1;
}

int gm_preview_copy_text_overlay_utf8(const gm_preview_handle *handle,
                                      size_t index, char *output,
                                      size_t output_size)
{
    if (!handle || index >= handle->previewer.textOverlays().size()) return 0;
    const std::string &text = handle->previewer.textOverlays()[index].utf8;
    if ((!output && !text.empty()) || output_size < text.size()) return 0;
    if (!text.empty()) std::copy(text.begin(), text.end(), output);
    return 1;
}

size_t gm_preview_outbox_count(const gm_preview_handle *handle)
{
    return handle ? handle->previewer.bluetoothOutbox().size() : 0;
}

int gm_preview_outbox_service(const gm_preview_handle *handle, size_t index,
                              uint8_t *service)
{
    if (!handle || !service || index >= handle->previewer.bluetoothOutbox().size()) return 0;
    *service = handle->previewer.bluetoothOutbox()[index].service;
    return 1;
}

int gm_preview_outbox_command(const gm_preview_handle *handle, size_t index,
                              uint8_t *command)
{
    if (!handle || !command || index >= handle->previewer.bluetoothOutbox().size()) return 0;
    *command = handle->previewer.bluetoothOutbox()[index].command;
    return 1;
}

int gm_preview_outbox_channel(const gm_preview_handle *handle, size_t index,
                              uint16_t *channel)
{
    if (!handle || !channel || index >= handle->previewer.bluetoothOutbox().size()) return 0;
    *channel = handle->previewer.bluetoothOutbox()[index].channel;
    return 1;
}

size_t gm_preview_outbox_payload_size(const gm_preview_handle *handle,
                                      size_t index)
{
    if (!handle || index >= handle->previewer.bluetoothOutbox().size()) return 0;
    return handle->previewer.bluetoothOutbox()[index].payload.size();
}

int gm_preview_copy_outbox_payload(gm_preview_handle *handle, size_t index,
                                   uint8_t *output, size_t output_size)
{
    if (!handle || index >= handle->previewer.bluetoothOutbox().size()) return 0;
    const std::vector<uint8_t> &payload = handle->previewer.bluetoothOutbox()[index].payload;
    if ((!output && !payload.empty()) || output_size < payload.size()) return 0;
    return protect(handle, [&](gmpreview::Previewer &) {
        if (!payload.empty()) std::copy(payload.begin(), payload.end(), output);
    });
}

int gm_preview_clear_outbox(gm_preview_handle *handle)
{
    return protect(handle, [](gmpreview::Previewer &previewer) {
        previewer.clearBluetoothOutbox();
    });
}

const char *gm_preview_last_error(const gm_preview_handle *handle)
{
    return handle ? handle->last_error.c_str() : "previewer handle is null";
}
