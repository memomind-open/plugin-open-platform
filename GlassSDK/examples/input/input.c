#include "gm_plugin_lvgl_api.h"

#define SCREEN_MARGIN 20
#define PANEL_MAX_WIDTH 360
#define PANEL_HEIGHT 132

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    gm_plugin_lvgl_obj_t *screen;
    gm_plugin_lvgl_obj_t *event_label;
} input_t;

static input_t input;

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void set_style(input_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value,
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_box(input_t *self, gm_plugin_lvgl_obj_t *object,
                      uint8_t fill, uint8_t border, uint8_t radius)
{
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xA0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_RADIUS, number(radius));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_TOP, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_LEFT, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT, number(0));
    self->ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
}

static gm_plugin_lvgl_obj_t *create_label(input_t *self,
                                           gm_plugin_lvgl_obj_t *parent,
                                           const char *text, uint8_t shade)
{
    gm_plugin_lvgl_obj_t *label = self->ui->label_create(parent);
    if (label == 0) return 0;
    self->ui->label_set_text(label, text);
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(shade));
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    return label;
}

static gm_plugin_result_t input_start(void *context)
{
    input_t *self = context;
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *host_root = self->ui->root_get();
    gm_plugin_lvgl_obj_t *title;
    gm_plugin_lvgl_obj_t *panel;
    gm_plugin_lvgl_obj_t *prompt;
    gm_plugin_lvgl_obj_t *help;
    int16_t panel_width;

    if (host_root == 0 ||
        self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width <= SCREEN_MARGIN * 2U || display.height < PANEL_HEIGHT)
        return GM_PLUGIN_ESTATE;

    self->ui->obj_clean(host_root);
    self->screen = self->ui->obj_create(host_root);
    if (self->screen == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->screen, display.width, display.height);
    self->ui->obj_align(self->screen, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->screen, 0x00, 0, 0);

    title = create_label(self, self->screen, "BUTTON INPUT", 0xC0);
    panel = self->ui->obj_create(self->screen);
    help = create_label(self, self->screen,
                        "Click once  |  twice  |  hold 1s", 0x80);
    if (title == 0 || panel == 0 || help == 0) goto no_memory;

    self->ui->obj_set_size(title, display.width - SCREEN_MARGIN * 2U, 36);
    self->ui->obj_align(title, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 24);

    panel_width = (int16_t)display.width - SCREEN_MARGIN * 2;
    if (panel_width > PANEL_MAX_WIDTH) panel_width = PANEL_MAX_WIDTH;
    self->ui->obj_set_size(panel, panel_width, PANEL_HEIGHT);
    self->ui->obj_align(panel, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 4);
    style_box(self, panel, 0x08, 2, 7);

    prompt = create_label(self, panel, "LAST EVENT", 0x70);
    self->event_label = create_label(self, panel, "WAITING...", 0xFF);
    if (prompt == 0 || self->event_label == 0) goto no_memory;
    self->ui->obj_set_size(prompt, panel_width - 24, 36);
    self->ui->obj_align(prompt, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 18);
    self->ui->obj_set_size(self->event_label, panel_width - 24, 42);
    self->ui->obj_align(self->event_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 16);

    self->ui->obj_set_size(help, display.width - SCREEN_MARGIN * 2U, 36);
    self->ui->obj_align(help, GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID, 0, -24);
    return GM_PLUGIN_OK;

no_memory:
    self->ui->obj_clean(host_root);
    self->screen = 0;
    self->event_label = 0;
    return GM_PLUGIN_ENOMEM;
}

static bool input_event(void *context, const gm_plugin_event_t *event)
{
    input_t *self = context;
    const char *message;

    if (event == 0 || event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    switch (event->data.button.action) {
    case GM_PLUGIN_BUTTON_ACTION_SINGLE:
        message = "SINGLE CLICK";
        break;
    case GM_PLUGIN_BUTTON_ACTION_DOUBLE:
        message = "DOUBLE CLICK";
        break;
    case GM_PLUGIN_BUTTON_ACTION_LONG:
        message = "LONG PRESS";
        break;
    case GM_PLUGIN_BUTTON_ACTION_TRIGGER:
        switch (event->data.button.button) {
        case GM_PLUGIN_BUTTON_UP: message = "UP"; break;
        case GM_PLUGIN_BUTTON_DOWN: message = "DOWN"; break;
        case GM_PLUGIN_BUTTON_LEFT: message = "LEFT"; break;
        case GM_PLUGIN_BUTTON_RIGHT: message = "RIGHT"; break;
        case GM_PLUGIN_BUTTON_PAGE_UP: message = "PAGE UP"; break;
        case GM_PLUGIN_BUTTON_PAGE_DOWN: message = "PAGE DOWN"; break;
        case GM_PLUGIN_BUTTON_SCROLL_UP: message = "SCROLL UP"; break;
        case GM_PLUGIN_BUTTON_SCROLL_DOWN: message = "SCROLL DOWN"; break;
        case GM_PLUGIN_BUTTON_BACK: message = "BACK"; break;
        case GM_PLUGIN_BUTTON_HOME: message = "HOME"; break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    self->host->log("button=%u action=%u",
                    (unsigned int)event->data.button.button,
                    (unsigned int)event->data.button.action);
    if (self->event_label != 0)
        self->ui->label_set_text(self->event_label, message);
    return true;
}

static void input_stop(void *context)
{
    input_t *self = context;
    gm_plugin_lvgl_obj_t *host_root = self->ui->root_get();
    if (host_root != 0) self->ui->obj_clean(host_root);
    self->screen = 0;
    self->event_label = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 || host->log == 0 ||
        host->display_get_info == 0 || host->graphics.lvgl == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & GM_PLUGIN_CAP_BUTTON) == 0U)
        return GM_PLUGIN_ENOTSUP;

    input.host = host;
    input.ui = host->graphics.lvgl;
    if (input.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(input.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;

    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &input;
    plugin->on_start = input_start;
    plugin->on_event = input_event;
    plugin->on_stop = input_stop;
    return GM_PLUGIN_OK;
}
