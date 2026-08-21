#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_preview_handle gm_preview_handle;

gm_preview_handle *gm_preview_create(void);
void gm_preview_destroy(gm_preview_handle *handle);

int gm_preview_load(gm_preview_handle *handle, const char *path);
int gm_preview_start(gm_preview_handle *handle);
int gm_preview_stop(gm_preview_handle *handle);
int gm_preview_tick(gm_preview_handle *handle, uint32_t elapsed_ms);
int gm_preview_send_bluetooth(gm_preview_handle *handle, uint16_t channel,
                              const uint8_t *payload, size_t payload_size,
                              int *handled);
int gm_preview_send_button(gm_preview_handle *handle, uint16_t action,
                           uint16_t button, int *handled);
int gm_preview_send_gesture(gm_preview_handle *handle, uint16_t gesture,
                            int active, int *handled);
int gm_preview_simulate_direction_gesture(gm_preview_handle *handle,
                                          uint16_t gesture, int *handled);

int gm_preview_is_loaded(const gm_preview_handle *handle);
int gm_preview_is_running(const gm_preview_handle *handle);
size_t gm_preview_frame_size(void);
int gm_preview_copy_frame(gm_preview_handle *handle, uint8_t *output,
                          size_t output_size);
size_t gm_preview_outbox_count(const gm_preview_handle *handle);
int gm_preview_outbox_channel(const gm_preview_handle *handle, size_t index,
                              uint16_t *channel);
size_t gm_preview_outbox_payload_size(const gm_preview_handle *handle,
                                      size_t index);
int gm_preview_copy_outbox_payload(gm_preview_handle *handle, size_t index,
                                   uint8_t *output, size_t output_size);
int gm_preview_clear_outbox(gm_preview_handle *handle);
const char *gm_preview_last_error(const gm_preview_handle *handle);

#ifdef __cplusplus
}
#endif
