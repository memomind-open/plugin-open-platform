#include "gm_plugin_lvgl_api.h"

#define SCENE_MAX_ELEMENTS 16U

enum {
    SCENE_CHANNEL_CLEAR = 1,
    SCENE_CHANNEL_TEXT,
    SCENE_CHANNEL_RECT,
    SCENE_CHANNEL_DELETE,
    SCENE_CHANNEL_LINE,
    SCENE_CHANNEL_BITMAP,
    /* Optional phone/bridge liveness check; drawing commands never reply. */
    SCENE_CHANNEL_PING = 0x7FFEU,
};

enum {
    ELEMENT_NONE = 0,
    ELEMENT_TEXT,
    ELEMENT_RECT,
    ELEMENT_LINE,
};

typedef struct {
    uint8_t id;
    uint8_t type;
    gm_plugin_lvgl_obj_t *object;
    gm_plugin_lvgl_point_t points[2];
} scene_element_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *lvgl;
    gm_plugin_lvgl_obj_t *root;
    scene_element_t elements[SCENE_MAX_ELEMENTS];
} scene_context_t;

static scene_context_t context;

#define style_number gm_plugin_lvgl_style_number
#define style_color gm_plugin_lvgl_style_color

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static void reset_element(scene_context_t *self, scene_element_t *element)
{
    if (element->object != 0) self->lvgl->obj_delete(element->object);
    element->id = 0;
    element->type = ELEMENT_NONE;
    element->object = 0;
}

static void clear_scene(scene_context_t *self)
{
    uint8_t i;
    if (self->root != 0) self->lvgl->obj_clean(self->root);
    for (i = 0; i < SCENE_MAX_ELEMENTS; ++i) {
        self->elements[i].id = 0;
        self->elements[i].type = ELEMENT_NONE;
        self->elements[i].object = 0;
    }
}

static scene_element_t *prepare_element(scene_context_t *self, uint8_t id,
                                        uint8_t type)
{
    scene_element_t *empty = 0;
    uint8_t i;
    for (i = 0; i < SCENE_MAX_ELEMENTS; ++i) {
        scene_element_t *element = &self->elements[i];
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

static void set_box(scene_context_t *self, gm_plugin_lvgl_obj_t *object,
                    const uint8_t *data)
{
    self->lvgl->obj_set_pos(object, read_u16(data), read_u16(data + 2));
    self->lvgl->obj_set_size(object, read_u16(data + 4), read_u16(data + 6));
}

static void set_border(scene_context_t *self, gm_plugin_lvgl_obj_t *object,
                       uint8_t width, uint8_t radius)
{
    self->lvgl->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                          style_number(width), 0);
    self->lvgl->style_set(object, GM_PLUGIN_LVGL_STYLE_RADIUS,
                          style_number(radius), 0);
}

static void draw_text(scene_context_t *self, const uint8_t *data,
                      uint32_t length)
{
    scene_element_t *element;
    char *text;
    uint32_t text_length;
    uint32_t i;
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
    for (i = 0; i < text_length; ++i) text[i] = (char)data[11U + i];
    text[text_length] = '\0';
    self->lvgl->label_set_text(element->object, text);
    self->host->free(text);
}

static void draw_rect(scene_context_t *self, const uint8_t *data,
                      uint32_t length)
{
    scene_element_t *element;
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

static void delete_element(scene_context_t *self, const uint8_t *data,
                           uint32_t length)
{
    uint8_t i;
    if (length != 1U) return;
    for (i = 0; i < SCENE_MAX_ELEMENTS; ++i) {
        if (self->elements[i].type != ELEMENT_NONE &&
            self->elements[i].id == data[0]) {
            reset_element(self, &self->elements[i]);
            return;
        }
    }
}

static void draw_line(scene_context_t *self, const uint8_t *data,
                      uint32_t length)
{
    scene_element_t *element;
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

static void draw_bitmap(scene_context_t *self, const uint8_t *data,
                        uint32_t length)
{
    gm_plugin_framebuffer_surface_t surface;
    gm_plugin_rect_t destination;
    gm_plugin_display_info_t display;
    uint16_t stride;
    uint32_t byte_size;
    if (length < 10U) return;
    destination.x = (int16_t)read_u16(data);
    destination.y = (int16_t)read_u16(data + 2);
    destination.width = read_u16(data + 4);
    destination.height = read_u16(data + 6);
    stride = read_u16(data + 8);
    byte_size = length - 10U;
    if (destination.width == 0U || destination.height == 0U ||
        stride < (destination.width + 1U) / 2U ||
        byte_size != (uint32_t)stride * destination.height)
        return;
    if (destination.x < 0 || destination.y < 0 ||
        self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        (uint32_t)destination.x + destination.width > display.width ||
        (uint32_t)destination.y + destination.height > display.height)
        return;

    uint16_t next_y = (uint16_t)destination.y;
    uint16_t end_y = (uint16_t)(destination.y + destination.height);
    while (next_y < end_y) {
        gm_plugin_result_t result =
            self->host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) return;
        uint16_t part_end = (uint16_t)(surface.y + surface.height);
        if (part_end > end_y) part_end = end_y;
        if (next_y < surface.y || part_end <= next_y ||
            (uint32_t)destination.x + destination.width > surface.width) {
            (void)self->host->graphics.framebuffer.unlock(0, false);
            return;
        }

        uint32_t width = destination.width;
        uint32_t row;
        for (row = next_y; row < part_end; ++row) {
            const uint8_t *source = data + 10U +
                (row - (uint16_t)destination.y) * stride;
            uint8_t *target = surface.pixels +
                (row - surface.y) * surface.stride +
                ((uint16_t)destination.x >> 1);
            if ((destination.x & 1) == 0) {
                uint32_t bytes = width >> 1;
                uint32_t i;
                for (i = 0; i < bytes; ++i) target[i] = source[i];
                if ((width & 1U) != 0U)
                    target[bytes] = (uint8_t)((target[bytes] & 0x0FU) |
                                               (source[bytes] & 0xF0U));
            } else {
                uint32_t pairs = (width - 1U) >> 1;
                uint32_t i;
                target[0] = (uint8_t)((target[0] & 0xF0U) | (source[0] >> 4));
                for (i = 0; i < pairs; ++i)
                    target[i + 1U] = (uint8_t)((source[i] << 4) |
                                               (source[i + 1U] >> 4));
                if ((width & 1U) == 0U)
                    target[pairs + 1U] =
                        (uint8_t)((target[pairs + 1U] & 0x0FU) |
                                  ((source[pairs] & 0x0FU) << 4));
            }
        }

        gm_plugin_rect_t dirty = destination;
        dirty.y = (int16_t)next_y;
        dirty.height = (uint16_t)(part_end - next_y);
        if (self->host->graphics.framebuffer.unlock(&dirty,
                                                     part_end == end_y) !=
            GM_PLUGIN_OK)
            return;
        next_y = part_end;
    }
}

static gm_plugin_result_t scene_start(void *opaque)
{
    scene_context_t *self = opaque;
    self->root = self->lvgl->root_get();
    if (self->root == 0) return GM_PLUGIN_ESTATE;
    clear_scene(self);
    return GM_PLUGIN_OK;
}

static bool scene_event(void *opaque, const gm_plugin_event_t *event)
{
    scene_context_t *self = opaque;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_BT_MESSAGE) return false;
    switch (event->data.bt.channel) {
    case SCENE_CHANNEL_CLEAR:
        clear_scene(self);
        break;
    case SCENE_CHANNEL_TEXT:
        draw_text(self, event->data.bt.data, event->data.bt.length);
        break;
    case SCENE_CHANNEL_RECT:
        draw_rect(self, event->data.bt.data, event->data.bt.length);
        break;
    case SCENE_CHANNEL_DELETE:
        delete_element(self, event->data.bt.data, event->data.bt.length);
        break;
    case SCENE_CHANNEL_LINE:
        draw_line(self, event->data.bt.data, event->data.bt.length);
        break;
    case SCENE_CHANNEL_BITMAP:
        draw_bitmap(self, event->data.bt.data, event->data.bt.length);
        break;
    case SCENE_CHANNEL_PING:
        if (self->host->bt_send(SCENE_CHANNEL_PING, event->data.bt.data,
                                event->data.bt.length) != GM_PLUGIN_OK) {
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static void scene_stop(void *opaque)
{
    scene_context_t *self = opaque;
    clear_scene(self);
    self->root = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 || host->alloc == 0 || host->free == 0 ||
        host->graphics.lvgl == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & (GM_PLUGIN_CAP_DISPLAY_BITMAP |
                               GM_PLUGIN_CAP_BLUETOOTH)) !=
            (GM_PLUGIN_CAP_DISPLAY_BITMAP | GM_PLUGIN_CAP_BLUETOOTH) ||
        host->display_get_info == 0 ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 || host->bt_send == 0) {
        return GM_PLUGIN_ENOTSUP;
    }
    context.host = host;
    context.lvgl = host->graphics.lvgl;
    if (context.lvgl == 0 ||
        context.lvgl->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(context.lvgl->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION)) {
        return GM_PLUGIN_EVERSION;
    }
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &context;
    plugin->on_start = scene_start;
    plugin->on_event = scene_event;
    plugin->on_stop = scene_stop;
    return GM_PLUGIN_OK;
}
