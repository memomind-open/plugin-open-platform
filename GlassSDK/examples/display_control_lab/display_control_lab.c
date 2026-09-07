#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"

#define CONTROL_COMMAND_CHANNEL UINT16_C(0x4443)
#define CONTROL_STATE_CHANNEL UINT16_C(0x4444)
#define PROTOCOL_VERSION 1U
#define COMMAND_PACKET_SIZE 8U
#define STATE_PACKET_SIZE 12U

enum {
    OP_QUERY = 1,
    OP_SET_SCREEN,
    OP_SET_BRIGHTNESS,
    OP_SET_DISTANCE,
    OP_SET_HEIGHT,
    OP_RESTORE,
};

enum {
    RESTORE_STEP_SCREEN_ON = 0,
    RESTORE_STEP_BRIGHTNESS,
    RESTORE_STEP_DISTANCE,
    RESTORE_STEP_HEIGHT,
    RESTORE_STEP_AUTO_BRIGHTNESS,
    RESTORE_STEP_SCREEN_OFF,
    RESTORE_STEP_DONE,
};

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *title_label;
    gm_plugin_lvgl_obj_t *status_label;
    gm_plugin_lvgl_obj_t *screen_label;
    gm_plugin_lvgl_obj_t *brightness_label;
    gm_plugin_lvgl_obj_t *height_label;
    gm_plugin_lvgl_obj_t *distance_label;
    gm_plugin_lvgl_obj_t *hint_label;
    bool initial_screen_on;
    bool screen_on;
    bool requested_screen_on;
    bool preview_only;
    bool auto_brightness_blocked;
    bool restore_active;
    uint8_t initial_brightness;
    uint8_t initial_height;
    uint8_t initial_distance;
    uint8_t brightness;
    uint8_t height;
    uint8_t distance;
    uint8_t last_operation;
    uint8_t restore_step;
    gm_plugin_result_t last_result;
    gm_plugin_result_t restore_result;
    uint32_t restore_request_id;
    uint8_t language;
} display_lab_t;

static display_lab_t s_lab;

static gm_plugin_lvgl_style_value_t font_value(
    const gm_plugin_lvgl_font_t *font)
{
    gm_plugin_lvgl_style_value_t value = {0};
    value.ptr = font;
    return value;
}

static void set_style(display_lab_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value,
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static gm_plugin_lvgl_obj_t *create_box(
    display_lab_t *self, int16_t x, int16_t y, int16_t width, int16_t height,
    uint8_t fill, uint8_t border, uint8_t radius)
{
    gm_plugin_lvgl_obj_t *object = self->ui->obj_create(self->root);
    if (object == 0) return 0;
    self->ui->obj_set_pos(object, x, y);
    self->ui->obj_set_size(object, width, height);
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
              gm_plugin_lvgl_style_color((uint8_t)((fill << 4) | fill)));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_OPA,
              gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
              gm_plugin_lvgl_style_color((uint8_t)((border << 4) | border)));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA,
              gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
              gm_plugin_lvgl_style_number(1));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_RADIUS,
              gm_plugin_lvgl_style_number(radius));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
              gm_plugin_lvgl_style_number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
              gm_plugin_lvgl_style_number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
              gm_plugin_lvgl_style_number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
              gm_plugin_lvgl_style_number(0));
    self->ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    return object;
}

static gm_plugin_lvgl_obj_t *create_label(
    display_lab_t *self, int16_t x, int16_t y, int16_t width, int16_t height,
    uint8_t gray, uint8_t alignment)
{
    gm_plugin_lvgl_obj_t *label = self->ui->label_create(self->root);
    if (label == 0) return 0;
    self->ui->obj_set_pos(label, x, y);
    self->ui->obj_set_size(label, width, height);
    self->ui->label_set_long_mode(label, GM_PLUGIN_LVGL_LABEL_CLIP);
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_BG_OPA,
              gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_TRANSPARENT));
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              gm_plugin_lvgl_style_color((uint8_t)((gray << 4) | gray)));
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_OPA,
              gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER));
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
              font_value(self->ui->font_default));
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              gm_plugin_lvgl_style_number(alignment));
    return label;
}

static uint32_t read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static uint8_t status_code(gm_plugin_result_t result)
{
    if (result == GM_PLUGIN_OK) return 0U;
    if (result <= GM_PLUGIN_EINVAL && result >= GM_PLUGIN_EVERSION)
        return (uint8_t)(-result);
    return UINT8_MAX;
}

static void read_current_state(display_lab_t *self)
{
    self->screen_on = self->host->display_control.screen_is_on();
    self->brightness = (uint8_t)self->host->display_control.brightness_get();
    self->distance = (uint8_t)self->host->display_control.distance_get();
    self->height = (uint8_t)self->host->display_control.height_get();
    if (!self->preview_only) self->requested_screen_on = self->screen_on;
}

static const char *result_text(display_lab_t *self)
{
    if (self->restore_active)
        return self->language != 0U ? "正在恢复" : "RESTORING";
    if (self->last_result != GM_PLUGIN_OK)
        return self->language != 0U ? "执行失败" : "CONTROL ERROR";
    if (self->preview_only)
        return self->language != 0U ? "STUDIO 预览" : "STUDIO PREVIEW";
    return self->language != 0U ? "控制就绪" : "CONTROL READY";
}

static void update_labels(display_lab_t *self)
{
    char text[80];
    self->ui->label_set_text(self->title_label,
        self->language != 0U ? "屏幕控制" : "DISPLAY CONTROL");
    self->ui->label_set_text(self->status_label, result_text(self));

    if (self->preview_only) {
        self->libc->snprintf(text, sizeof(text),
            self->language != 0U ? "屏幕    %s（预览）" : "SCREEN    %s (PREVIEW)",
            self->requested_screen_on
                ? (self->language != 0U ? "开启" : "ON")
                : (self->language != 0U ? "关闭" : "OFF"));
    } else {
        self->libc->snprintf(text, sizeof(text),
            self->language != 0U ? "屏幕    %s" : "SCREEN    %s",
            self->screen_on
                ? (self->language != 0U ? "开启" : "ON")
                : (self->language != 0U ? "关闭" : "OFF"));
    }
    self->ui->label_set_text(self->screen_label, text);

    self->libc->snprintf(text, sizeof(text),
        self->language != 0U ? "亮度    %u / 10" : "BRIGHTNESS    %u / 10",
        (unsigned int)self->brightness);
    self->ui->label_set_text(self->brightness_label, text);
    self->libc->snprintf(text, sizeof(text),
        self->language != 0U ? "高度    %u / 8" : "HEIGHT    %u / 8",
        (unsigned int)self->height);
    self->ui->label_set_text(self->height_label, text);
    self->libc->snprintf(text, sizeof(text),
        self->language != 0U ? "距离    %u / 8" : "DISTANCE    %u / 8",
        (unsigned int)self->distance);
    self->ui->label_set_text(self->distance_label, text);
    self->ui->label_set_text(self->hint_label,
        self->language != 0U
            ? "按键或抬头亮屏  ·  亮屏时长按退出"
            : "BUTTON / RAISE TO WAKE  ·  HOLD TO EXIT");
}

static gm_plugin_result_t create_ui(display_lab_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_style_value_t large_font;
    int16_t width;
    if (self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width < 540U || display.height < 320U)
        return GM_PLUGIN_ENOTSUP;
    width = (int16_t)display.width;
    self->root = self->ui->root_get();
    if (self->root == 0) return GM_PLUGIN_ENOTSUP;
    self->ui->obj_clean(self->root);

    if (create_box(self, 0, 0, width, (int16_t)display.height, 0, 0, 0) == 0 ||
        create_box(self, 24, 70, (int16_t)(width - 48), 46, 1, 5, 8) == 0 ||
        create_box(self, 24, 124, (int16_t)(width - 48), 46, 1, 5, 8) == 0 ||
        create_box(self, 24, 178, (int16_t)(width - 48), 46, 1, 5, 8) == 0 ||
        create_box(self, 24, 232, (int16_t)(width - 48), 46, 1, 5, 8) == 0)
        return GM_PLUGIN_ENOMEM;

    self->title_label = create_label(self, 24, 16, (int16_t)(width - 230), 38,
        15, GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->status_label = create_label(self, (int16_t)(width - 210), 18, 186, 30,
        9, GM_PLUGIN_LVGL_TEXT_ALIGN_RIGHT);
    self->screen_label = create_label(self, 42, 80, (int16_t)(width - 84), 28,
        14, GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->brightness_label = create_label(self, 42, 134, (int16_t)(width - 84), 28,
        13, GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->height_label = create_label(self, 42, 188, (int16_t)(width - 84), 28,
        12, GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->distance_label = create_label(self, 42, 242, (int16_t)(width - 84), 28,
        11, GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->hint_label = create_label(self, 24, 303, (int16_t)(width - 48), 28,
        8, GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER);
    if (self->title_label == 0 || self->status_label == 0 ||
        self->screen_label == 0 || self->brightness_label == 0 ||
        self->height_label == 0 || self->distance_label == 0 ||
        self->hint_label == 0)
        return GM_PLUGIN_ENOMEM;

    large_font = font_value(self->ui->font_large);
    set_style(self, self->title_label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
              large_font);
    update_labels(self);
    return GM_PLUGIN_OK;
}

static void clear_ui(display_lab_t *self)
{
    if (self->root != 0) self->ui->obj_clean(self->root);
    self->root = 0;
    self->title_label = 0;
    self->status_label = 0;
    self->screen_label = 0;
    self->brightness_label = 0;
    self->height_label = 0;
    self->distance_label = 0;
    self->hint_label = 0;
}

static void send_state(display_lab_t *self, uint32_t request_id)
{
    uint8_t data[STATE_PACKET_SIZE];
    uint8_t flags = 0U;
    read_current_state(self);
    if (self->screen_on) flags |= 0x01U;
    if (self->auto_brightness_blocked) flags |= 0x02U;
    if (self->preview_only) flags |= 0x04U;
    if (self->requested_screen_on) flags |= 0x08U;
    if (self->restore_active) flags |= 0x10U;
    data[0] = PROTOCOL_VERSION;
    data[1] = 1U;
    data[2] = status_code(self->last_result);
    data[3] = flags;
    data[4] = self->brightness;
    data[5] = self->distance;
    data[6] = self->height;
    data[7] = self->last_operation;
    write_u32(data + 8, request_id);
    (void)self->host->bt_send(CONTROL_STATE_CHANNEL, data, sizeof(data));
}

static gm_plugin_result_t first_error(gm_plugin_result_t current,
                                      gm_plugin_result_t next)
{
    return current == GM_PLUGIN_OK ? next : current;
}

static gm_plugin_result_t release_auto_brightness(display_lab_t *self)
{
    gm_plugin_result_t result;
    if (!self->auto_brightness_blocked) return GM_PLUGIN_OK;
    result = self->host->display_control.auto_brightness_block(false);
    if (result == GM_PLUGIN_OK) self->auto_brightness_blocked = false;
    return result;
}

static gm_plugin_result_t start_restore(display_lab_t *self,
                                        uint32_t request_id)
{
    if (self->restore_active) return GM_PLUGIN_EBUSY;
    read_current_state(self);
    self->preview_only = false;
    self->requested_screen_on = self->initial_screen_on;
    self->restore_active = true;
    self->restore_step = RESTORE_STEP_SCREEN_ON;
    self->restore_result = GM_PLUGIN_OK;
    self->restore_request_id = request_id;
    return GM_PLUGIN_OK;
}

static void restore_loop(display_lab_t *self)
{
    gm_plugin_result_t next = GM_PLUGIN_OK;
    bool called_host = false;
    bool settings_changed;

    if (!self->restore_active) return;
    while (self->restore_step < RESTORE_STEP_DONE && !called_host) {
        switch (self->restore_step) {
        case RESTORE_STEP_SCREEN_ON:
            settings_changed =
                self->brightness != self->initial_brightness ||
                self->distance != self->initial_distance ||
                self->height != self->initial_height;
            if (!self->screen_on &&
                (self->initial_screen_on || settings_changed)) {
                next = self->host->display_control.screen_turn_on(true);
                called_host = true;
            }
            break;
        case RESTORE_STEP_BRIGHTNESS:
            if (self->brightness != self->initial_brightness) {
                next = self->host->display_control.brightness_set(
                    (gm_plugin_display_brightness_t)self->initial_brightness);
                called_host = true;
            }
            break;
        case RESTORE_STEP_DISTANCE:
            if (self->distance != self->initial_distance) {
                next = self->host->display_control.distance_set(
                    (gm_plugin_display_distance_t)self->initial_distance);
                called_host = true;
            }
            break;
        case RESTORE_STEP_HEIGHT:
            if (self->height != self->initial_height) {
                next = self->host->display_control.height_set(
                    (gm_plugin_display_height_t)self->initial_height);
                called_host = true;
            }
            break;
        case RESTORE_STEP_AUTO_BRIGHTNESS:
            if (self->auto_brightness_blocked) {
                next = release_auto_brightness(self);
                called_host = true;
            }
            break;
        case RESTORE_STEP_SCREEN_OFF:
            if (!self->initial_screen_on && self->screen_on) {
                next = self->host->display_control.screen_turn_on(false);
                called_host = true;
            }
            break;
        default:
            self->restore_step = RESTORE_STEP_DONE;
            break;
        }
        if (self->restore_step < RESTORE_STEP_DONE) self->restore_step++;
    }

    if (called_host) {
        self->restore_result = first_error(self->restore_result, next);
        self->last_result = self->restore_result;
        send_state(self, self->restore_request_id);
        update_labels(self);
        return;
    }

    self->restore_active = false;
    self->last_result = self->restore_result;
    self->requested_screen_on = self->initial_screen_on;
    send_state(self, self->restore_request_id);
    update_labels(self);
}

static bool receive_command(display_lab_t *self,
                            const uint8_t *data, uint32_t length)
{
    uint8_t operation = 0U;
    uint8_t value = 0U;
    bool preview_only = false;
    uint32_t request_id = 0U;
    gm_plugin_result_t result = GM_PLUGIN_OK;

    if (data != 0 && length >= COMMAND_PACKET_SIZE)
        request_id = read_u32(data + 4);
    if (data == 0 || length != COMMAND_PACKET_SIZE ||
        data[0] != PROTOCOL_VERSION) {
        self->last_result = GM_PLUGIN_EINVAL;
        send_state(self, request_id);
        update_labels(self);
        return true;
    }

    operation = data[1];
    value = data[2];
    preview_only = (data[3] & 0x01U) != 0U;
    self->last_operation = operation;
    switch (operation) {
    case OP_QUERY:
        if (value != 0U || preview_only) result = GM_PLUGIN_EINVAL;
        break;
    case OP_SET_SCREEN:
        if (value > 1U) {
            result = GM_PLUGIN_EINVAL;
            break;
        }
        self->requested_screen_on = value != 0U;
        self->preview_only = preview_only;
        if (!preview_only)
            result = self->host->display_control.screen_turn_on(value != 0U);
        break;
    case OP_SET_BRIGHTNESS:
        if (value < 1U || value > 10U || preview_only) {
            result = GM_PLUGIN_EINVAL;
            break;
        }
        if (!self->auto_brightness_blocked) {
            result = self->host->display_control.auto_brightness_block(true);
            if (result == GM_PLUGIN_OK) self->auto_brightness_blocked = true;
        }
        if (result == GM_PLUGIN_OK)
            result = self->host->display_control.brightness_set(
                (gm_plugin_display_brightness_t)value);
        break;
    case OP_SET_DISTANCE:
        if (value > 8U || preview_only) result = GM_PLUGIN_EINVAL;
        else result = self->host->display_control.distance_set(
            (gm_plugin_display_distance_t)value);
        break;
    case OP_SET_HEIGHT:
        if (value > 8U || preview_only) result = GM_PLUGIN_EINVAL;
        else result = self->host->display_control.height_set(
            (gm_plugin_display_height_t)value);
        break;
    case OP_RESTORE:
        if (value != 0U || preview_only) result = GM_PLUGIN_EINVAL;
        else result = start_restore(self, request_id);
        break;
    default:
        result = GM_PLUGIN_EINVAL;
        break;
    }
    self->last_result = result;
    send_state(self, request_id);
    update_labels(self);
    return true;
}

static gm_plugin_result_t on_start(void *context)
{
    display_lab_t *self = context;
    char locale[GM_PLUGIN_LOCALE_TAG_MAX] = {0};
    gm_plugin_result_t result;
    self->preview_only = false;
    self->auto_brightness_blocked = false;
    self->restore_active = false;
    self->restore_step = RESTORE_STEP_DONE;
    self->restore_request_id = 0U;
    self->restore_result = GM_PLUGIN_OK;
    self->last_operation = OP_QUERY;
    self->last_result = GM_PLUGIN_OK;
    read_current_state(self);
    self->initial_screen_on = self->screen_on;
    self->initial_brightness = self->brightness;
    self->initial_height = self->height;
    self->initial_distance = self->distance;
    self->requested_screen_on = self->screen_on;
    self->language = 0U;
    if ((self->host->capabilities & GM_PLUGIN_CAP_LOCALE) != 0U &&
        self->host->locale_get != 0 &&
        self->host->locale_get(locale) == GM_PLUGIN_OK &&
        locale[0] == 'z' && locale[1] == 'h')
        self->language = 1U;
    result = self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_GESTURES);
    if (result != GM_PLUGIN_OK) return result;
    result = create_ui(self);
    if (result != GM_PLUGIN_OK) {
        (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
        clear_ui(self);
    }
    return result;
}

static void safe_exit(display_lab_t *self)
{
    (void)self->host->display_control.screen_turn_on(true);
    (void)release_auto_brightness(self);
    self->host->app_exit();
}

static bool wake_display_if_needed(display_lab_t *self)
{
    bool preview_screen_off;

    read_current_state(self);
    preview_screen_off = self->preview_only && !self->requested_screen_on;
    if (self->screen_on && !preview_screen_off) return false;

    self->last_operation = OP_SET_SCREEN;
    self->requested_screen_on = true;
    if (preview_screen_off) {
        self->last_result = GM_PLUGIN_OK;
    } else {
        self->preview_only = false;
        self->last_result = self->host->display_control.screen_turn_on(true);
    }
    send_state(self, 0U);
    update_labels(self);
    return true;
}

static bool on_event(void *context, const gm_plugin_event_t *event)
{
    display_lab_t *self = context;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != CONTROL_COMMAND_CHANNEL) return false;
        return receive_command(self, event->data.bt.data,
                               event->data.bt.length);
    }
    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        if (event->data.connection.connected) {
            self->last_operation = OP_QUERY;
            self->last_result = GM_PLUGIN_OK;
            send_state(self, 0U);
            update_labels(self);
        }
        return true;
    }
    if (event->type == GM_PLUGIN_EVENT_BUTTON) {
        if (wake_display_if_needed(self)) return true;
        if ((event->data.button.button == GM_PLUGIN_BUTTON_PRIMARY &&
             (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_LONG ||
              event->data.button.action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG)) ||
            (event->data.button.button == GM_PLUGIN_BUTTON_BACK &&
             event->data.button.action == GM_PLUGIN_BUTTON_ACTION_TRIGGER)) {
            safe_exit(self);
            return true;
        }
        return false;
    }
    if (event->type == GM_PLUGIN_EVENT_IMU_GESTURE &&
        event->data.imu_gesture.gesture == GM_PLUGIN_IMU_GESTURE_HEAD_RAISE &&
        event->data.imu_gesture.active) {
        if (wake_display_if_needed(self)) return true;
        return false;
    }
    return false;
}

static void on_resume(void *context)
{
    display_lab_t *self = context;
    read_current_state(self);
    update_labels(self);
}

static void on_loop(void *context, uint32_t elapsed_ms)
{
    (void)elapsed_ms;
    restore_loop((display_lab_t *)context);
}

static void on_stop(void *context)
{
    display_lab_t *self = context;
    self->restore_active = false;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    (void)release_auto_brightness(self);
    clear_ui(self);
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_BUTTON |
        GM_PLUGIN_CAP_IMU_EVENTS | GM_PLUGIN_CAP_BLUETOOTH |
        GM_PLUGIN_CAP_DISPLAY_CONTROL;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        (host->capabilities & required) != required ||
        host->display_get_info == 0 || host->graphics.lvgl == 0 ||
        host->display_control.screen_is_on == 0 ||
        host->display_control.screen_turn_on == 0 ||
        host->display_control.brightness_get == 0 ||
        host->display_control.brightness_set == 0 ||
        host->display_control.distance_get == 0 ||
        host->display_control.distance_set == 0 ||
        host->display_control.height_get == 0 ||
        host->display_control.height_set == 0 ||
        host->display_control.auto_brightness_block == 0 ||
        host->imu_enable == 0 || host->bt_send == 0 || host->app_exit == 0)
        return GM_PLUGIN_ENOTSUP;
    s_lab.host = host;
    s_lab.ui = host->graphics.lvgl;
    if (gm_plugin_libc_get(host, &s_lab.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    if (s_lab.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(s_lab.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &s_lab;
    plugin->on_start = on_start;
    plugin->on_resume = on_resume;
    plugin->on_loop = on_loop;
    plugin->on_event = on_event;
    plugin->on_stop = on_stop;
    return GM_PLUGIN_OK;
}
