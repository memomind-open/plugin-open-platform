#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"

#define AUDIO_LAB_STATE_CHANNEL UINT16_C(0x414C)
#define BUTTON_CHANNEL UINT16_C(0x0100)
#define CONNECTION_CHANNEL UINT16_C(0x0103)
#define PROTOCOL_VERSION 1U
#define STATE_PACKET_SIZE 23U

enum {
    LAB_STATE_READY = 0,
    LAB_STATE_STARTING,
    LAB_STATE_RECORDING,
    LAB_STATE_STREAMING,
    LAB_STATE_STOPPING,
    LAB_STATE_PLAYBACK,
    LAB_STATE_ERROR,
    LAB_STATE_UNAVAILABLE,
};

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *title_label;
    gm_plugin_lvgl_obj_t *state_label;
    gm_plugin_lvgl_obj_t *summary_label;
    gm_plugin_lvgl_obj_t *elapsed_label;
    gm_plugin_lvgl_obj_t *stats_label;
    gm_plugin_lvgl_obj_t *hint_label;
    uint32_t sequence;
    uint32_t elapsed_ms;
    uint32_t chunk_count;
    uint32_t dropped_frames;
    uint16_t queue_latency_ms;
    uint8_t state;
    uint8_t mode;
    uint8_t pickup_mode;
    uint8_t profile;
    uint8_t noise_reduction;
    uint8_t voice;
    uint8_t error_code;
    uint8_t language;
} audio_lab_t;

static audio_lab_t s_lab;

static gm_plugin_lvgl_style_value_t font_value(
    const gm_plugin_lvgl_font_t *font)
{
    gm_plugin_lvgl_style_value_t value = {0};
    value.ptr = font;
    return value;
}

static void set_style(audio_lab_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value,
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static gm_plugin_lvgl_obj_t *create_box(
    audio_lab_t *self, int16_t x, int16_t y, int16_t width, int16_t height,
    uint8_t fill, uint8_t border, uint8_t border_width, uint8_t radius)
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
              gm_plugin_lvgl_style_number(border_width));
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
    audio_lab_t *self, int16_t x, int16_t y, int16_t width, int16_t height,
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

static const char *state_text(uint8_t state, uint8_t language)
{
    static const char *const names_en[] = {
        "READY", "STARTING", "RECORDING", "STREAMING",
        "STOPPING", "PLAYBACK", "ERROR", "UNAVAILABLE"
    };
    static const char *const names_zh[] = {
        "就绪", "启动中", "录音中", "传输中",
        "停止中", "播放中", "错误", "不可用"
    };
    if (state > LAB_STATE_UNAVAILABLE) return language != 0U ? "错误" : "ERROR";
    return language != 0U ? names_zh[state] : names_en[state];
}

static const char *mode_text(uint8_t mode, uint8_t language)
{
    static const char *const names_en[] = {"NONE", "RECORDING", "STREAM"};
    static const char *const names_zh[] = {"无", "录音", "实时流"};
    if (mode > 2U) return language != 0U ? "未知" : "UNKNOWN";
    return language != 0U ? names_zh[mode] : names_en[mode];
}

static const char *pickup_text(uint8_t pickup, uint8_t language)
{
    static const char *const names_en[] = {
        "UNCHANGED", "FRONT FIXED", "MEETING AUTO",
        "NON-WEARER", "FRONT BALANCED", "FRONT FOCUS"
    };
    static const char *const names_zh[] = {
        "保持不变", "正前方固定", "会议自动",
        "非佩戴者", "正前方均衡", "正前方聚焦"
    };
    if (pickup > 5U) return language != 0U ? "未知" : "UNKNOWN";
    return language != 0U ? names_zh[pickup] : names_en[pickup];
}

static const char *profile_text(uint8_t profile, uint8_t language)
{
    static const char *const names_en[] = {
        "N/A", "INTERACTIVE", "BALANCED", "RELIABLE", "CUSTOM"
    };
    static const char *const names_zh[] = {
        "无", "交互优先", "均衡", "可靠优先", "自定义"
    };
    if (profile > 4U) return language != 0U ? "未知" : "UNKNOWN";
    return language != 0U ? names_zh[profile] : names_en[profile];
}

static const char *voice_text(uint8_t voice, uint8_t language)
{
    static const char *const names_en[] = {
        "ORIGINAL", "CUTE", "DEEP", "OVERLORD"
    };
    static const char *const names_zh[] = {
        "原声", "可爱", "低沉", "领主"
    };
    if (voice > 3U) return language != 0U ? "未知" : "UNKNOWN";
    return language != 0U ? names_zh[voice] : names_en[voice];
}

static const char *error_text(uint8_t error, uint8_t language)
{
    static const char *const names_en[] = {
        "NONE", "INVALID REQUEST", "PERMISSION", "AUDIO BUSY",
        "NO AUDIO", "BUFFER FULL", "DISCONNECTED", "UNAVAILABLE",
        "TIMEOUT", "RUNTIME CLOSED", "RUNTIME REPLACED", "INTERNAL"
    };
    static const char *const names_zh[] = {
        "无", "参数错误", "没有权限", "音频忙",
        "没有音频", "缓冲区已满", "连接已断开", "能力不可用",
        "超时", "运行环境已关闭", "运行环境已替换", "内部错误"
    };
    if (error > 11U) return language != 0U ? "未知错误" : "UNKNOWN ERROR";
    return language != 0U ? names_zh[error] : names_en[error];
}

static void update_labels(audio_lab_t *self)
{
    char text[96];
    uint32_t seconds = self->elapsed_ms / 1000U;

    self->ui->label_set_text(self->state_label,
        self->state == LAB_STATE_ERROR ? error_text(self->error_code, self->language)
                                      : state_text(self->state, self->language));

    self->ui->label_set_text(self->title_label,
                             self->language != 0U ? "音频采集" : "AUDIO CAPTURE");
    self->ui->label_set_text(self->hint_label,
        self->language != 0U ? "单击：停止     长按：退出"
                             : "CLICK: STOP     HOLD: EXIT");

    if (self->state == LAB_STATE_PLAYBACK) {
        self->libc->snprintf(text, sizeof(text),
                             self->language != 0U ? "播放 / %s" : "PLAYBACK / %s",
                             voice_text(self->voice, self->language));
    } else if (self->mode == 2U) {
        self->libc->snprintf(text, sizeof(text), "%s / %s / %s",
                             profile_text(self->profile, self->language),
                             pickup_text(self->pickup_mode, self->language),
                             self->noise_reduction != 0U
                                 ? (self->language != 0U ? "降噪开" : "NR ON")
                                 : (self->language != 0U ? "降噪关" : "NR OFF"));
    } else {
        self->libc->snprintf(text, sizeof(text), "%s / %s / %s",
                             mode_text(self->mode, self->language),
                             pickup_text(self->pickup_mode, self->language),
                             self->noise_reduction != 0U
                                 ? (self->language != 0U ? "降噪开" : "NR ON")
                                 : (self->language != 0U ? "降噪关" : "NR OFF"));
    }
    self->ui->label_set_text(self->summary_label, text);

    self->libc->snprintf(text, sizeof(text),
                         "%02u:%02u",
                         (unsigned int)(seconds / 60U),
                         (unsigned int)(seconds % 60U));
    self->ui->label_set_text(self->elapsed_label, text);

    self->libc->snprintf(text, sizeof(text),
        self->language != 0U
            ? "分片 %u  /  丢帧 %u  /  队列 %u ms"
            : "CHUNKS %u  /  DROPPED %u  /  QUEUE %u ms",
        (unsigned int)self->chunk_count,
        (unsigned int)self->dropped_frames,
        (unsigned int)self->queue_latency_ms);
    self->ui->label_set_text(self->stats_label, text);
}

static gm_plugin_result_t create_ui(audio_lab_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_style_value_t large_font;
    int16_t width;

    if (self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width < 600U || display.height < 350U)
        return GM_PLUGIN_ENOTSUP;
    width = (int16_t)display.width;
    self->root = self->ui->root_get();
    if (self->root == 0) return GM_PLUGIN_ENOTSUP;
    self->ui->obj_clean(self->root);

    if (create_box(self, 0, 0, width, (int16_t)display.height,
                   0, 0, 0, 0) == 0 ||
        create_box(self, 28, 50, (int16_t)(width - 56), 1,
                   5, 5, 0, 0) == 0 ||
        create_box(self, 28, 286, (int16_t)(width - 56), 1,
                   5, 5, 0, 0) == 0)
        return GM_PLUGIN_ENOMEM;

    self->title_label = create_label(
        self, 24, 13, (int16_t)(width - 48), 28, 9,
        GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->state_label = create_label(
        self, 24, 72, (int16_t)(width - 48), 42, 15,
        GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER);
    self->summary_label = create_label(
        self, 24, 126, (int16_t)(width - 48), 28, 10,
        GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER);
    self->elapsed_label = create_label(
        self, 24, 171, (int16_t)(width - 48), 48, 15,
        GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER);
    self->stats_label = create_label(
        self, 24, 239, (int16_t)(width - 48), 28, 9,
        GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER);
    self->hint_label = create_label(
        self, 24, 307, (int16_t)(width - 48), 24, 7,
        GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER);
    if (self->title_label == 0 || self->state_label == 0 ||
        self->summary_label == 0 || self->elapsed_label == 0 ||
        self->stats_label == 0 || self->hint_label == 0)
        return GM_PLUGIN_ENOMEM;

    large_font = font_value(self->ui->font_large);
    set_style(self, self->state_label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
              large_font);
    set_style(self, self->elapsed_label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
              large_font);
    update_labels(self);
    return GM_PLUGIN_OK;
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static bool receive_state(audio_lab_t *self,
                          const uint8_t *data, uint32_t length)
{
    if (data == 0 || length != STATE_PACKET_SIZE ||
        data[0] != PROTOCOL_VERSION || data[1] > LAB_STATE_UNAVAILABLE ||
        data[2] > 2U || data[3] > 5U || data[4] > 4U ||
        data[5] > 1U || data[6] > 3U || data[22] > 1U)
        return false;
    self->state = data[1];
    self->mode = data[2];
    self->pickup_mode = data[3];
    self->profile = data[4];
    self->noise_reduction = data[5];
    self->voice = data[6];
    self->error_code = data[7];
    self->language = data[22];
    self->elapsed_ms = read_u32(data + 8);
    self->chunk_count = read_u32(data + 12);
    self->dropped_frames = read_u32(data + 16);
    self->queue_latency_ms = read_u16(data + 20);
    update_labels(self);
    return true;
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

static void event_header(audio_lab_t *self, uint8_t *data, uint8_t type,
                         uint32_t timestamp_ms)
{
    data[0] = PROTOCOL_VERSION;
    data[1] = type;
    write_u32(data + 2, self->sequence++);
    write_u32(data + 6, timestamp_ms);
}

static bool send_button(audio_lab_t *self, const gm_plugin_event_t *event)
{
    uint8_t data[14];
    event_header(self, data, 1, event->timestamp_ms);
    write_u16(data + 10, event->data.button.button);
    write_u16(data + 12, event->data.button.action);
    (void)self->host->bt_send(BUTTON_CHANNEL, data, sizeof(data));
    return true;
}

static gm_plugin_result_t on_start(void *context)
{
    audio_lab_t *self = context;
    self->sequence = 0;
    self->elapsed_ms = 0;
    self->chunk_count = 0;
    self->dropped_frames = 0;
    self->queue_latency_ms = 0;
    self->state = LAB_STATE_READY;
    self->mode = 1;
    self->pickup_mode = 4;
    self->profile = 0;
    self->noise_reduction = 1;
    self->voice = 0;
    self->error_code = 0;
    self->language = 0;
    return create_ui(self);
}

static bool on_event(void *context, const gm_plugin_event_t *event)
{
    audio_lab_t *self = context;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != AUDIO_LAB_STATE_CHANNEL) return false;
        return receive_state(self, event->data.bt.data,
                             event->data.bt.length);
    }
    if (event->type == GM_PLUGIN_EVENT_BUTTON) {
        bool handled = send_button(self, event);
        if ((event->data.button.button == GM_PLUGIN_BUTTON_PRIMARY &&
             event->data.button.action == GM_PLUGIN_BUTTON_ACTION_LONG) ||
            (event->data.button.button == GM_PLUGIN_BUTTON_BACK &&
             event->data.button.action == GM_PLUGIN_BUTTON_ACTION_TRIGGER)) {
            self->host->app_exit();
        }
        return handled;
    }
    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        uint8_t data[11];
        if (!event->data.connection.connected) return true;
        event_header(self, data, 4, event->timestamp_ms);
        data[10] = 1;
        (void)self->host->bt_send(CONNECTION_CHANNEL, data, sizeof(data));
        return true;
    }
    return false;
}

static void on_resume(void *context)
{
    update_labels((audio_lab_t *)context);
}

static void on_stop(void *context)
{
    audio_lab_t *self = context;
    if (self->root != 0) self->ui->obj_clean(self->root);
    self->root = 0;
    self->title_label = 0;
    self->state_label = 0;
    self->summary_label = 0;
    self->elapsed_label = 0;
    self->stats_label = 0;
    self->hint_label = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required =
        GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_BLUETOOTH;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        (host->capabilities & required) != required ||
        host->display_get_info == 0 || host->graphics.lvgl == 0 ||
        host->bt_send == 0 || host->app_exit == 0)
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
    plugin->on_event = on_event;
    plugin->on_stop = on_stop;
    return GM_PLUGIN_OK;
}
