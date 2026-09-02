#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_extensions.h"
#include "gm_plugin_libc.h"

#define WEB_BRIDGE_MAX_ELEMENTS 16U
#define WEB_BRIDGE_PROTOCOL_VERSION 1U
#define VERTICAL_TRIGGER_THRESHOLD 55
#define VERTICAL_RETURN_THRESHOLD 25
#define HORIZONTAL_TRIGGER_THRESHOLD 40
#define HORIZONTAL_RETURN_THRESHOLD 20
#define GYRO_RELEASE_THRESHOLD 20
#define DIRECTION_REARM_QUIET_MS 100U
#define WEB_BRIDGE_MAX_FRAME_TILES 256U
#define WEB_BRIDGE_FRAME_TIMEOUT_MS 5000U
#ifndef WEB_BRIDGE_RAW_IMU_INTERVAL_MS
#define WEB_BRIDGE_RAW_IMU_INTERVAL_MS 0U
#endif

enum {
    WEB_BRIDGE_CHANNEL_CLEAR = 1,
    WEB_BRIDGE_CHANNEL_TEXT,
    WEB_BRIDGE_CHANNEL_RECT,
    WEB_BRIDGE_CHANNEL_DELETE,
    WEB_BRIDGE_CHANNEL_LINE,
    WEB_BRIDGE_CHANNEL_BITMAP,
    WEB_BRIDGE_CHANNEL_BITMAP_LZ4,
    WEB_BRIDGE_CHANNEL_FRAME_BEGIN,
    WEB_BRIDGE_CHANNEL_FRAME_TILE_LZ4,
    WEB_BRIDGE_CHANNEL_BUTTON = 0x0100U,
    WEB_BRIDGE_CHANNEL_IMU_GESTURE = 0x0101U,
    WEB_BRIDGE_CHANNEL_RAW_IMU = 0x0102U,
    WEB_BRIDGE_CHANNEL_CONNECTION = 0x0103U,
    WEB_BRIDGE_CHANNEL_FRAME_STATUS = 0x0104U,
    WEB_BRIDGE_CHANNEL_PING = 0x7FFEU,
};

enum {
    WEB_BRIDGE_EVENT_BUTTON = 1,
    WEB_BRIDGE_EVENT_IMU_GESTURE,
    WEB_BRIDGE_EVENT_RAW_IMU,
    WEB_BRIDGE_EVENT_CONNECTION,
    WEB_BRIDGE_EVENT_FRAME_STATUS,
};

enum {
    FRAME_STATUS_OK = 0,
    FRAME_STATUS_INVALID_PAYLOAD,
    FRAME_STATUS_LZ4_UNAVAILABLE,
    FRAME_STATUS_DECODE_FAILED,
    FRAME_STATUS_OUT_OF_MEMORY,
    FRAME_STATUS_FRAMEBUFFER_FAILED,
    FRAME_STATUS_WRONG_FRAME,
    FRAME_STATUS_WRONG_TILE_INDEX,
    FRAME_STATUS_TIMEOUT,
};

enum {
    ELEMENT_NONE = 0,
    ELEMENT_TEXT,
    ELEMENT_RECT,
    ELEMENT_LINE,
};

typedef enum {
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
} web_bridge_direction_t;

typedef struct {
    uint8_t id;
    uint8_t type;
    gm_plugin_lvgl_obj_t *object;
    gm_plugin_lvgl_point_t points[2];
} web_bridge_element_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *lvgl;
    const gm_plugin_lz4_extension_api_t *lz4;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *root;
    uint8_t *decode_buffer;
    uint32_t decode_capacity;
    web_bridge_element_t elements[WEB_BRIDGE_MAX_ELEMENTS];
    uint32_t sequence;
    uint32_t frame_id;
    uint32_t frame_elapsed_ms;
    uint32_t raw_imu_interval_ms;
    uint32_t raw_imu_elapsed_ms;
    uint16_t frame_tile_count;
    uint16_t frame_next_tile;
    uint16_t direction_quiet_ms;
    web_bridge_direction_t direction;
    bool raw_imu_enabled;
    bool raw_direction_enabled;
    bool direction_armed;
    bool direction_return_seen;
    bool direction_active;
    bool frame_active;
} web_bridge_context_t;

static web_bridge_context_t context;

#define style_number gm_plugin_lvgl_style_number
#define style_color gm_plugin_lvgl_style_color

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void write_event_header(web_bridge_context_t *self, uint8_t *data,
                               uint8_t event_type, uint32_t timestamp_ms)
{
    data[0] = WEB_BRIDGE_PROTOCOL_VERSION;
    data[1] = event_type;
    write_u32(data + 2, self->sequence++);
    write_u32(data + 6, timestamp_ms);
}

static bool send_event(web_bridge_context_t *self,
                       gm_plugin_bt_channel_t channel, const uint8_t *data,
                       uint32_t length)
{
    gm_plugin_result_t result = self->host->bt_send(channel, data, length);
    if (result != GM_PLUGIN_OK)
        self->host->log("web bridge send channel=%u failed=%d",
                        (unsigned int)channel, (int)result);
    return result == GM_PLUGIN_OK;
}

static bool send_button_event(web_bridge_context_t *self,
                              const gm_plugin_event_t *event)
{
    uint8_t payload[14];
    write_event_header(self, payload, WEB_BRIDGE_EVENT_BUTTON,
                       event->timestamp_ms);
    write_u16(payload + 10, event->data.button.button);
    write_u16(payload + 12, event->data.button.action);
    (void)send_event(self, WEB_BRIDGE_CHANNEL_BUTTON, payload,
                     (uint32_t)sizeof(payload));
    return true;
}

static bool send_imu_gesture(web_bridge_context_t *self,
                             gm_plugin_imu_gesture_t gesture, bool active,
                             uint32_t timestamp_ms)
{
    uint8_t payload[13];
    write_event_header(self, payload, WEB_BRIDGE_EVENT_IMU_GESTURE,
                       timestamp_ms);
    write_u16(payload + 10, gesture);
    payload[12] = active ? 1U : 0U;
    (void)send_event(self, WEB_BRIDGE_CHANNEL_IMU_GESTURE, payload,
                     (uint32_t)sizeof(payload));
    return true;
}

static bool send_imu_gesture_event(web_bridge_context_t *self,
                                   const gm_plugin_event_t *event)
{
    return send_imu_gesture(self, event->data.imu_gesture.gesture,
                            event->data.imu_gesture.active,
                            event->timestamp_ms);
}

static bool send_connection_event(web_bridge_context_t *self,
                                  const gm_plugin_event_t *event)
{
    uint8_t payload[11];
    if (!event->data.connection.connected) return true;
    write_event_header(self, payload, WEB_BRIDGE_EVENT_CONNECTION,
                       event->timestamp_ms);
    payload[10] = 1U;
    (void)send_event(self, WEB_BRIDGE_CHANNEL_CONNECTION, payload,
                     (uint32_t)sizeof(payload));
    return true;
}

static bool send_frame_status(web_bridge_context_t *self, uint32_t frame_id,
                              uint16_t tile_index, uint16_t next_index,
                              uint8_t status, bool complete)
{
    uint8_t payload[20];
    write_event_header(self, payload, WEB_BRIDGE_EVENT_FRAME_STATUS,
                       self->host->monotonic_ms());
    write_u32(payload + 10, frame_id);
    write_u16(payload + 14, tile_index);
    write_u16(payload + 16, next_index);
    payload[18] = status;
    payload[19] = complete ? 1U : 0U;
    return send_event(self, WEB_BRIDGE_CHANNEL_FRAME_STATUS, payload,
                      (uint32_t)sizeof(payload));
}

static void reset_element(web_bridge_context_t *self,
                          web_bridge_element_t *element)
{
    if (element->object != 0) self->lvgl->obj_delete(element->object);
    self->libc->memset(element, 0, sizeof(*element));
}

static void clear_scene(web_bridge_context_t *self)
{
    if (self->root != 0) self->lvgl->obj_clean(self->root);
    self->libc->memset(self->elements, 0, sizeof(self->elements));
}

static web_bridge_element_t *prepare_element(web_bridge_context_t *self,
                                             uint8_t id, uint8_t type)
{
    web_bridge_element_t *empty = 0;
    uint8_t index;
    for (index = 0; index < WEB_BRIDGE_MAX_ELEMENTS; ++index) {
        web_bridge_element_t *element = &self->elements[index];
        if (element->type == ELEMENT_NONE) {
            if (empty == 0) empty = element;
        } else if (element->id == id) {
            if (element->type == type) return element;
            reset_element(self, element);
            empty = element;
            break;
        }
    }
    if (empty != 0) {
        empty->id = id;
        empty->type = type;
    }
    return empty;
}

static void set_box(web_bridge_context_t *self,
                    gm_plugin_lvgl_obj_t *object, const uint8_t *data)
{
    self->lvgl->obj_set_pos(object, read_u16(data), read_u16(data + 2));
    self->lvgl->obj_set_size(object, read_u16(data + 4), read_u16(data + 6));
}

static void set_border(web_bridge_context_t *self,
                       gm_plugin_lvgl_obj_t *object, uint8_t width,
                       uint8_t radius)
{
    self->lvgl->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                          style_number(width), 0);
    self->lvgl->style_set(object, GM_PLUGIN_LVGL_STYLE_RADIUS,
                          style_number(radius), 0);
}

static void draw_text(web_bridge_context_t *self, const uint8_t *data,
                      uint32_t length)
{
    web_bridge_element_t *element;
    char *text;
    uint32_t text_length;
    if (length < 11U) return;
    element = prepare_element(self, data[0], ELEMENT_TEXT);
    if (element == 0) return;
    if (element->object == 0) {
        element->object = self->lvgl->label_create(self->root);
        if (element->object == 0) {
            reset_element(self, element);
            return;
        }
        self->lvgl->label_set_long_mode(element->object,
                                        GM_PLUGIN_LVGL_LABEL_WRAP);
        self->lvgl->style_set(element->object,
                              GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
                              style_color(0xffU), 0);
        self->lvgl->style_set(element->object,
                              GM_PLUGIN_LVGL_STYLE_BORDER_OPA,
                              style_number(255), 0);
    }
    set_box(self, element->object, data + 1);
    set_border(self, element->object, data[9], data[10]);
    text_length = length - 11U;
    text = self->host->alloc(text_length + 1U);
    if (text == 0) return;
    self->libc->memcpy(text, data + 11U, text_length);
    text[text_length] = '\0';
    self->lvgl->label_set_text(element->object, text);
    self->host->free(text);
}

static void draw_rect(web_bridge_context_t *self, const uint8_t *data,
                      uint32_t length)
{
    web_bridge_element_t *element;
    if (length != 11U) return;
    element = prepare_element(self, data[0], ELEMENT_RECT);
    if (element == 0) return;
    if (element->object == 0) {
        element->object = self->lvgl->obj_create(self->root);
        if (element->object == 0) {
            reset_element(self, element);
            return;
        }
        self->lvgl->style_set(element->object, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                              style_number(0), 0);
        self->lvgl->style_set(element->object,
                              GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
                              style_color(0xffU), 0);
        self->lvgl->style_set(element->object,
                              GM_PLUGIN_LVGL_STYLE_BORDER_OPA,
                              style_number(255), 0);
    }
    set_box(self, element->object, data + 1);
    set_border(self, element->object, data[9] == 0U ? 1U : data[9], data[10]);
}

static void delete_element(web_bridge_context_t *self, const uint8_t *data,
                           uint32_t length)
{
    uint8_t index;
    if (length != 1U) return;
    for (index = 0; index < WEB_BRIDGE_MAX_ELEMENTS; ++index) {
        if (self->elements[index].type != ELEMENT_NONE &&
            self->elements[index].id == data[0]) {
            reset_element(self, &self->elements[index]);
            return;
        }
    }
}

static void draw_line(web_bridge_context_t *self, const uint8_t *data,
                      uint32_t length)
{
    web_bridge_element_t *element;
    if (length != 10U) return;
    element = prepare_element(self, data[0], ELEMENT_LINE);
    if (element == 0) return;
    if (element->object == 0) {
        element->object = self->lvgl->line_create(self->root);
        if (element->object == 0) {
            reset_element(self, element);
            return;
        }
        self->lvgl->style_set(element->object, GM_PLUGIN_LVGL_STYLE_LINE_COLOR,
                              style_color(0xffU), 0);
        self->lvgl->style_set(element->object,
                              GM_PLUGIN_LVGL_STYLE_LINE_ROUNDED,
                              style_number(1), 0);
    }
    element->points[0].x = read_u16(data + 1);
    element->points[0].y = read_u16(data + 3);
    element->points[1].x = read_u16(data + 5);
    element->points[1].y = read_u16(data + 7);
    self->lvgl->line_set_points(element->object, element->points, 2);
    self->lvgl->style_set(element->object, GM_PLUGIN_LVGL_STYLE_LINE_WIDTH,
                          style_number(data[9] == 0U ? 1U : data[9]), 0);
}

static gm_plugin_result_t draw_bitmap(web_bridge_context_t *self,
                                      const uint8_t *data, uint32_t length,
                                      bool present)
{
    gm_plugin_framebuffer_surface_t surface;
    gm_plugin_rect_t destination;
    gm_plugin_display_info_t display;
    uint16_t stride;
    uint32_t byte_size;
    uint16_t next_y;
    uint16_t end_y;
    if (length < 10U) return GM_PLUGIN_EINVAL;
    destination.x = (int16_t)read_u16(data);
    destination.y = (int16_t)read_u16(data + 2);
    destination.width = read_u16(data + 4);
    destination.height = read_u16(data + 6);
    stride = read_u16(data + 8);
    byte_size = length - 10U;
    if (destination.width == 0U || destination.height == 0U ||
        stride < (destination.width + 1U) / 2U ||
        byte_size != (uint32_t)stride * destination.height)
        return GM_PLUGIN_EINVAL;
    if (destination.x < 0 || destination.y < 0 ||
        self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.pixel_format != GM_PLUGIN_PIXEL_GRAY_4 ||
        (uint32_t)destination.x + destination.width > display.width ||
        (uint32_t)destination.y + destination.height > display.height)
        return GM_PLUGIN_EINVAL;

    next_y = (uint16_t)destination.y;
    end_y = (uint16_t)(destination.y + destination.height);
    while (next_y < end_y) {
        uint32_t surface_end;
        uint16_t part_end;
        uint32_t width;
        uint32_t row;
        gm_plugin_result_t result =
            self->host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) return result;
        surface_end = (uint32_t)surface.y + surface.height;
        part_end = surface_end > end_y ? end_y : (uint16_t)surface_end;
        if (surface.pixels == 0 || surface.height == 0U ||
            surface.stride < (surface.width + 1U) / 2U ||
            surface.y > next_y || surface_end <= next_y ||
            surface_end > display.height || part_end <= next_y ||
            (uint32_t)destination.x + destination.width > surface.width) {
            (void)self->host->graphics.framebuffer.unlock(0, false);
            return GM_PLUGIN_EINVAL;
        }

        width = destination.width;
        if ((destination.x & 1) == 0 && (width & 1U) == 0U &&
            destination.x == 0 && width == surface.width &&
            stride == surface.stride) {
            uint32_t rows = (uint32_t)(part_end - next_y);
            const uint8_t *source = data + 10U +
                ((uint32_t)next_y - (uint16_t)destination.y) * stride;
            uint8_t *target = surface.pixels +
                ((uint32_t)next_y - surface.y) * surface.stride;
            self->libc->memcpy(target, source, rows * stride);
        } else {
            for (row = next_y; row < part_end; ++row) {
                const uint8_t *source = data + 10U +
                    (row - (uint16_t)destination.y) * stride;
                uint8_t *target = surface.pixels +
                    (row - surface.y) * surface.stride +
                    ((uint16_t)destination.x >> 1);
                if ((destination.x & 1) == 0) {
                    uint32_t bytes = width >> 1;
                    self->libc->memcpy(target, source, bytes);
                    if ((width & 1U) != 0U)
                        target[bytes] =
                            (uint8_t)((target[bytes] & 0x0FU) |
                                      (source[bytes] & 0xF0U));
                } else {
                    uint32_t pairs = (width - 1U) >> 1;
                    uint32_t index;
                    target[0] =
                        (uint8_t)((target[0] & 0xF0U) | (source[0] >> 4));
                    for (index = 0; index < pairs; ++index)
                        target[index + 1U] =
                            (uint8_t)((source[index] << 4) |
                                      (source[index + 1U] >> 4));
                    if ((width & 1U) == 0U)
                        target[pairs + 1U] =
                            (uint8_t)((target[pairs + 1U] & 0x0FU) |
                                      ((source[pairs] & 0x0FU) << 4));
                }
            }
        }

        gm_plugin_rect_t dirty = destination;
        dirty.y = (int16_t)next_y;
        dirty.height = (uint16_t)(part_end - next_y);
        result = self->host->graphics.framebuffer.unlock(
            &dirty, present && part_end == end_y);
        if (result != GM_PLUGIN_OK) return result;
        next_y = part_end;
    }
    return GM_PLUGIN_OK;
}

static uint8_t draw_bitmap_lz4(web_bridge_context_t *self,
                               const uint8_t *data, uint32_t length,
                               bool present)
{
    gm_plugin_display_info_t display;
    uint8_t *decoded;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    uint32_t decoded_size;
    uint32_t compressed_size;
    int32_t restored_size;

    gm_plugin_result_t draw_result;

    if (self->lz4 == 0) return FRAME_STATUS_LZ4_UNAVAILABLE;
    if (length <= 14U) return FRAME_STATUS_INVALID_PAYLOAD;
    x = read_u16(data);
    y = read_u16(data + 2);
    width = read_u16(data + 4);
    height = read_u16(data + 6);
    stride = read_u16(data + 8);
    decoded_size = read_u32(data + 10);
    compressed_size = length - 14U;
    if (width == 0U || height == 0U ||
        stride != (uint16_t)((width + 1U) / 2U) ||
        decoded_size != (uint32_t)stride * height ||
        decoded_size > UINT32_C(0x7fffffff) ||
        compressed_size > UINT32_C(0x7fffffff) ||
        self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        (uint32_t)x + width > display.width ||
        (uint32_t)y + height > display.height)
        return FRAME_STATUS_INVALID_PAYLOAD;

    if (self->decode_capacity < decoded_size + 10U) {
        uint8_t *larger = self->host->alloc(decoded_size + 10U);
        if (larger == 0) return FRAME_STATUS_OUT_OF_MEMORY;
        if (self->decode_buffer != 0) self->host->free(self->decode_buffer);
        self->decode_buffer = larger;
        self->decode_capacity = decoded_size + 10U;
    }
    decoded = self->decode_buffer;
    if (decoded == 0) return FRAME_STATUS_OUT_OF_MEMORY;
    self->libc->memcpy(decoded, data, 10U);
    restored_size = self->lz4->decompress_safe(
        data + 14U, decoded + 10U, (int32_t)compressed_size,
        (int32_t)decoded_size);
    if (restored_size != (int32_t)decoded_size) {
        self->host->log("web bridge lz4 decode failed=%d",
                        (int)restored_size);
        return FRAME_STATUS_DECODE_FAILED;
    }
    draw_result = draw_bitmap(self, decoded, decoded_size + 10U, present);
    return draw_result == GM_PLUGIN_OK ? FRAME_STATUS_OK :
                                        FRAME_STATUS_FRAMEBUFFER_FAILED;
}

static void handle_frame_begin(web_bridge_context_t *self,
                               const uint8_t *data, uint32_t length)
{
    uint32_t frame_id = length >= 4U ? read_u32(data) : 0U;
    uint16_t tile_count = 0U;
    uint8_t status = FRAME_STATUS_OK;
    if (length != 6U) {
        status = FRAME_STATUS_INVALID_PAYLOAD;
    } else if (self->lz4 == 0) {
        status = FRAME_STATUS_LZ4_UNAVAILABLE;
    } else {
        tile_count = read_u16(data + 4);
        if (tile_count == 0U || tile_count > WEB_BRIDGE_MAX_FRAME_TILES) {
            status = FRAME_STATUS_INVALID_PAYLOAD;
        } else {
            /* Commit the new frame only after the complete begin message has
             * passed validation. A malformed retry must not cancel the frame
             * that is currently receiving tiles. */
            self->frame_id = frame_id;
            self->frame_tile_count = tile_count;
            self->frame_next_tile = 0;
            self->frame_elapsed_ms = 0;
            self->frame_active = true;
        }
    }
    (void)send_frame_status(self, frame_id, UINT16_MAX,
                            status == FRAME_STATUS_OK ? 0U : self->frame_next_tile,
                            status, false);
}

static void handle_frame_tile_lz4(web_bridge_context_t *self,
                                  const uint8_t *data, uint32_t length)
{
    uint32_t frame_id = length >= 4U ? read_u32(data) : 0U;
    uint16_t tile_index = length >= 6U ? read_u16(data + 4) : UINT16_MAX;
    uint8_t status;
    bool complete;
    bool last;

    if (length <= 20U) {
        (void)send_frame_status(self, frame_id, tile_index,
                                self->frame_next_tile,
                                FRAME_STATUS_INVALID_PAYLOAD, false);
        return;
    }
    complete = !self->frame_active && frame_id == self->frame_id &&
               self->frame_next_tile == self->frame_tile_count &&
               self->frame_tile_count != 0U;
    if (complete && tile_index < self->frame_next_tile) {
        (void)send_frame_status(self, frame_id, tile_index,
                                self->frame_next_tile, FRAME_STATUS_OK, true);
        return;
    }
    if (!self->frame_active || frame_id != self->frame_id) {
        (void)send_frame_status(self, frame_id, tile_index,
                                self->frame_next_tile,
                                FRAME_STATUS_WRONG_FRAME, false);
        return;
    }
    if (tile_index < self->frame_next_tile) {
        (void)send_frame_status(self, frame_id, tile_index,
                                self->frame_next_tile, FRAME_STATUS_OK, false);
        return;
    }
    if (tile_index != self->frame_next_tile) {
        (void)send_frame_status(self, frame_id, tile_index,
                                self->frame_next_tile,
                                FRAME_STATUS_WRONG_TILE_INDEX, false);
        return;
    }

    last = (uint32_t)tile_index + 1U == self->frame_tile_count;
    status = draw_bitmap_lz4(self, data + 6U, length - 6U, last);
    if (status == FRAME_STATUS_OK) {
        self->frame_elapsed_ms = 0;
        ++self->frame_next_tile;
        if (last) self->frame_active = false;
    }
    (void)send_frame_status(self, frame_id, tile_index,
                            self->frame_next_tile, status,
                            status == FRAME_STATUS_OK && last);
}

static bool classify_direction(int32_t gyro_x, int32_t gyro_y,
                               int32_t gyro_z,
                               web_bridge_direction_t *direction)
{
    int32_t horizontal = gyro_x - gyro_y;
    int32_t abs_horizontal = horizontal < 0 ? -horizontal : horizontal;
    int32_t abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    if (abs_z >= VERTICAL_TRIGGER_THRESHOLD && abs_z > abs_horizontal) {
        *direction = gyro_z > 0 ? DIRECTION_UP : DIRECTION_DOWN;
        return true;
    }
    if (abs_horizontal < HORIZONTAL_TRIGGER_THRESHOLD ||
        abs_horizontal <= abs_z)
        return false;
    *direction = horizontal < 0 ? DIRECTION_LEFT : DIRECTION_RIGHT;
    return true;
}

static gm_plugin_imu_gesture_t direction_gesture(
    web_bridge_direction_t direction)
{
    switch (direction) {
    case DIRECTION_UP:
        return GM_PLUGIN_IMU_GESTURE_HEAD_RAISE;
    case DIRECTION_DOWN:
        return GM_PLUGIN_IMU_GESTURE_HEAD_LOWER;
    case DIRECTION_LEFT:
        return GM_PLUGIN_IMU_GESTURE_LEFT;
    default:
        return GM_PLUGIN_IMU_GESTURE_RIGHT;
    }
}

static void start_direction(web_bridge_context_t *self,
                            web_bridge_direction_t direction,
                            uint32_t timestamp_ms)
{
    self->direction = direction;
    self->direction_armed = false;
    self->direction_return_seen = false;
    self->direction_active = true;
    self->direction_quiet_ms = 0;
    (void)send_imu_gesture(self, direction_gesture(direction), true,
                           timestamp_ms);
}

static void release_direction(web_bridge_context_t *self,
                              uint32_t timestamp_ms)
{
    if (self->direction_active)
        (void)send_imu_gesture(self, direction_gesture(self->direction), false,
                               timestamp_ms);
    self->direction_active = false;
}

static void process_raw_direction(web_bridge_context_t *self,
                                  const gm_plugin_imu_sample_t *sample,
                                  uint32_t elapsed_ms, uint32_t timestamp_ms)
{
    int32_t gyro_x = sample->gyro_raw[0];
    int32_t gyro_y = sample->gyro_raw[1];
    int32_t gyro_z = sample->gyro_raw[2];
    int32_t abs_x = gyro_x < 0 ? -gyro_x : gyro_x;
    int32_t abs_y = gyro_y < 0 ? -gyro_y : gyro_y;
    int32_t abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    int32_t horizontal = gyro_x - gyro_y;
    web_bridge_direction_t detected = DIRECTION_UP;
    bool valid = classify_direction(gyro_x, gyro_y, gyro_z, &detected);

    if (!self->direction_armed) {
        if (self->direction_active && valid && detected != self->direction &&
            (((uint8_t)detected + 2U) & 3U) != (uint8_t)self->direction) {
            release_direction(self, timestamp_ms);
            start_direction(self, detected, timestamp_ms);
            return;
        }
        if (!self->direction_return_seen) {
            if ((self->direction == DIRECTION_UP &&
                 gyro_z < -VERTICAL_RETURN_THRESHOLD) ||
                (self->direction == DIRECTION_DOWN &&
                 gyro_z > VERTICAL_RETURN_THRESHOLD) ||
                (self->direction == DIRECTION_LEFT &&
                 horizontal > HORIZONTAL_RETURN_THRESHOLD) ||
                (self->direction == DIRECTION_RIGHT &&
                 horizontal < -HORIZONTAL_RETURN_THRESHOLD)) {
                self->direction_return_seen = true;
                self->direction_quiet_ms = 0;
            }
            return;
        }
        if (abs_x < GYRO_RELEASE_THRESHOLD &&
            abs_y < GYRO_RELEASE_THRESHOLD &&
            abs_z < GYRO_RELEASE_THRESHOLD) {
            uint32_t quiet = (uint32_t)self->direction_quiet_ms + elapsed_ms;
            self->direction_quiet_ms = quiet > DIRECTION_REARM_QUIET_MS ?
                DIRECTION_REARM_QUIET_MS : (uint16_t)quiet;
            if (self->direction_quiet_ms >= DIRECTION_REARM_QUIET_MS) {
                release_direction(self, timestamp_ms);
                self->direction_armed = true;
                self->direction_return_seen = false;
                self->direction_quiet_ms = 0;
            }
        } else {
            self->direction_quiet_ms = 0;
        }
        return;
    }

    if (valid) start_direction(self, detected, timestamp_ms);
}

static bool is_firmware_direction_gesture(gm_plugin_imu_gesture_t gesture)
{
    return gesture == GM_PLUGIN_IMU_GESTURE_HEAD_RAISE ||
           gesture == GM_PLUGIN_IMU_GESTURE_HEAD_LOWER ||
           gesture == GM_PLUGIN_IMU_GESTURE_HEAD_RAISE_TIMEOUT ||
           gesture == GM_PLUGIN_IMU_GESTURE_HEAD_LOWER_TIMEOUT ||
           gesture == GM_PLUGIN_IMU_GESTURE_LEFT ||
           gesture == GM_PLUGIN_IMU_GESTURE_RIGHT;
}

static gm_plugin_result_t web_bridge_start(void *opaque)
{
    web_bridge_context_t *self = opaque;
    gm_plugin_imu_modes_t imu_modes = GM_PLUGIN_IMU_ENABLE_GESTURES;
    gm_plugin_result_t result;
    bool raw_available;
    self->root = self->lvgl->root_get();
    if (self->root == 0) return GM_PLUGIN_ESTATE;
    clear_scene(self);
    self->sequence = 0;
    self->frame_id = 0;
    self->frame_elapsed_ms = 0;
    self->frame_tile_count = 0;
    self->frame_next_tile = 0;
    self->frame_active = false;
    self->raw_imu_interval_ms = WEB_BRIDGE_RAW_IMU_INTERVAL_MS;
    self->raw_imu_elapsed_ms = 0;
    self->raw_imu_enabled = false;
    self->raw_direction_enabled = false;
    self->direction_armed = false;
    self->direction_return_seen = true;
    self->direction_active = false;
    self->direction_quiet_ms = 0;
    raw_available =
        (self->host->capabilities & GM_PLUGIN_CAP_IMU_RAW) != 0U &&
        self->host->imu_read != 0 && self->host->monotonic_ms != 0;
    if (raw_available) {
        imu_modes |= GM_PLUGIN_IMU_ENABLE_RAW;
    }
    result = self->host->imu_enable(imu_modes);
    if (result != GM_PLUGIN_OK && raw_available) {
        self->host->log("web bridge raw IMU unavailable=%d", (int)result);
        result = self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_GESTURES);
        raw_available = false;
    }
    if (result != GM_PLUGIN_OK) {
        clear_scene(self);
        self->root = 0;
    } else if (raw_available) {
        self->raw_imu_enabled = true;
        self->raw_direction_enabled = true;
    }
    return result;
}

static void web_bridge_loop(void *opaque, uint32_t elapsed_ms)
{
    web_bridge_context_t *self = opaque;
    gm_plugin_imu_sample_t sample;
    uint8_t payload[26];
    uint32_t direction_elapsed_ms = elapsed_ms;
    uint32_t timestamp_ms;
    if (self->frame_active) {
        if (elapsed_ms >=
            WEB_BRIDGE_FRAME_TIMEOUT_MS - self->frame_elapsed_ms) {
            self->frame_active = false;
            self->frame_elapsed_ms = WEB_BRIDGE_FRAME_TIMEOUT_MS;
            (void)send_frame_status(self, self->frame_id,
                                    self->frame_next_tile,
                                    self->frame_next_tile,
                                    FRAME_STATUS_TIMEOUT, false);
        } else {
            self->frame_elapsed_ms += elapsed_ms;
        }
    }
    if (!self->raw_imu_enabled) return;
    if (self->host->imu_read(&sample) != GM_PLUGIN_OK) return;
    timestamp_ms = self->host->monotonic_ms();
    if (direction_elapsed_ms > 150U) direction_elapsed_ms = 150U;
    if (self->raw_direction_enabled)
        process_raw_direction(self, &sample, direction_elapsed_ms,
                              timestamp_ms);
    if (self->raw_imu_interval_ms == 0U) return;
    if (elapsed_ms < self->raw_imu_interval_ms - self->raw_imu_elapsed_ms) {
        self->raw_imu_elapsed_ms += elapsed_ms;
        return;
    }
    self->raw_imu_elapsed_ms = 0;
    write_event_header(self, payload, WEB_BRIDGE_EVENT_RAW_IMU,
                       timestamp_ms);
    write_u16(payload + 10, (uint16_t)sample.accel_raw[0]);
    write_u16(payload + 12, (uint16_t)sample.accel_raw[1]);
    write_u16(payload + 14, (uint16_t)sample.accel_raw[2]);
    write_u16(payload + 16, (uint16_t)sample.gyro_raw[0]);
    write_u16(payload + 18, (uint16_t)sample.gyro_raw[1]);
    write_u16(payload + 20, (uint16_t)sample.gyro_raw[2]);
    write_u16(payload + 22, (uint16_t)sample.temperature_raw);
    write_u16(payload + 24, (uint16_t)sample.pitch_degrees);
    (void)send_event(self, WEB_BRIDGE_CHANNEL_RAW_IMU, payload,
                     (uint32_t)sizeof(payload));
}

static bool web_bridge_event(void *opaque, const gm_plugin_event_t *event)
{
    web_bridge_context_t *self = opaque;
    if (event == 0) return false;
    switch (event->type) {
    case GM_PLUGIN_EVENT_BT_MESSAGE:
        if (event->data.bt.length != 0U && event->data.bt.data == 0)
            return false;
        if (self->frame_active && event->data.bt.channel >= 1U &&
            event->data.bt.channel <= WEB_BRIDGE_CHANNEL_BITMAP_LZ4)
            return true;
        switch (event->data.bt.channel) {
        case WEB_BRIDGE_CHANNEL_CLEAR:
            clear_scene(self);
            return true;
        case WEB_BRIDGE_CHANNEL_TEXT:
            draw_text(self, event->data.bt.data, event->data.bt.length);
            return true;
        case WEB_BRIDGE_CHANNEL_RECT:
            draw_rect(self, event->data.bt.data, event->data.bt.length);
            return true;
        case WEB_BRIDGE_CHANNEL_DELETE:
            delete_element(self, event->data.bt.data, event->data.bt.length);
            return true;
        case WEB_BRIDGE_CHANNEL_LINE:
            draw_line(self, event->data.bt.data, event->data.bt.length);
            return true;
        case WEB_BRIDGE_CHANNEL_BITMAP:
            (void)draw_bitmap(self, event->data.bt.data,
                              event->data.bt.length, true);
            return true;
        case WEB_BRIDGE_CHANNEL_BITMAP_LZ4:
            (void)draw_bitmap_lz4(self, event->data.bt.data,
                                  event->data.bt.length, true);
            return true;
        case WEB_BRIDGE_CHANNEL_FRAME_BEGIN:
            handle_frame_begin(self, event->data.bt.data,
                               event->data.bt.length);
            return true;
        case WEB_BRIDGE_CHANNEL_FRAME_TILE_LZ4:
            handle_frame_tile_lz4(self, event->data.bt.data,
                                  event->data.bt.length);
            return true;
        case WEB_BRIDGE_CHANNEL_PING:
            return send_event(self, WEB_BRIDGE_CHANNEL_PING,
                              event->data.bt.data, event->data.bt.length);
        default:
            return false;
        }
    case GM_PLUGIN_EVENT_BUTTON:
        return send_button_event(self, event);
    case GM_PLUGIN_EVENT_IMU_GESTURE:
        if (self->raw_direction_enabled &&
            is_firmware_direction_gesture(
                event->data.imu_gesture.gesture))
            return true;
        return send_imu_gesture_event(self, event);
    case GM_PLUGIN_EVENT_CONNECTION:
        if (!event->data.connection.connected) self->frame_active = false;
        return send_connection_event(self, event);
    default:
        return false;
    }
}

static void web_bridge_stop(void *opaque)
{
    web_bridge_context_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    clear_scene(self);
    if (self->decode_buffer != 0) self->host->free(self->decode_buffer);
    self->root = 0;
    self->decode_buffer = 0;
    self->decode_capacity = 0;
    self->raw_imu_enabled = false;
    self->raw_direction_enabled = false;
    self->direction_active = false;
    self->frame_active = false;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const void *lz4_api = 0;
    const gm_plugin_capabilities_t required =
        GM_PLUGIN_CAP_DISPLAY_BITMAP | GM_PLUGIN_CAP_BLUETOOTH |
        GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_IMU_EVENTS;
    if (host == 0 || plugin == 0 || host->log == 0 ||
        host->monotonic_ms == 0 || host->alloc == 0 ||
        host->free == 0 || host->display_get_info == 0 ||
        host->extension_get == 0 ||
        host->graphics.lvgl == 0 ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 || host->bt_send == 0 ||
        host->imu_enable == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required)
        return GM_PLUGIN_ENOTSUP;

    context.host = host;
    context.lvgl = host->graphics.lvgl;
    context.lz4 = 0;
    context.libc = 0;
    if (context.lvgl->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(context.lvgl->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;

    if (host->extension_get != 0 &&
        host->extension_get(GM_PLUGIN_EXTENSION_LZ4, &lz4_api) ==
            GM_PLUGIN_OK &&
        lz4_api != 0) {
        const gm_plugin_lz4_extension_api_t *lz4 =
            (const gm_plugin_lz4_extension_api_t *)lz4_api;
        if (lz4->decompress_safe != 0) context.lz4 = lz4;
    }
    if (gm_plugin_libc_get(host, &context.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;

    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &context;
    plugin->on_start = web_bridge_start;
    plugin->on_loop = web_bridge_loop;
    plugin->on_event = web_bridge_event;
    plugin->on_stop = web_bridge_stop;
    return GM_PLUGIN_OK;
}
