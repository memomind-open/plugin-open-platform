#include "gm_plugin_lvgl_api.h"

#define TEXT_CHANNEL 1U
#define MAX_TEXT_BYTES 1024U
#define LABEL_MARGIN 20U

static const gm_plugin_host_api_t *s_host;
static const gm_plugin_lvgl_api_t *s_lvgl;
static gm_plugin_lvgl_obj_t *s_label;

static gm_plugin_result_t bluetooth_start(void *context)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *root;
    (void)context;
    if (s_host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width <= LABEL_MARGIN * 2U ||
        display.height <= LABEL_MARGIN * 2U)
        return GM_PLUGIN_ENOTSUP;
    root = s_lvgl->root_get();
    if (root == 0) return GM_PLUGIN_ESTATE;
    s_label = s_lvgl->label_create(root);
    if (s_label == 0) return GM_PLUGIN_ENOMEM;
    s_lvgl->obj_set_size(s_label,
                         (int16_t)(display.width - LABEL_MARGIN * 2U),
                         (int16_t)(display.height - LABEL_MARGIN * 2U));
    s_lvgl->obj_align(s_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    s_lvgl->label_set_long_mode(s_label, GM_PLUGIN_LVGL_LABEL_WRAP);
    s_lvgl->label_set_text(s_label, "Waiting for phone text...");
    return GM_PLUGIN_OK;
}

static bool bluetooth_event(void *context, const gm_plugin_event_t *event)
{
    static const char prefix[] = "[GLASSES] ";
    char *reply;
    uint32_t prefix_length = (uint32_t)sizeof(prefix) - 1U;
    uint32_t reply_length;
    uint32_t index;
    gm_plugin_result_t result;
    (void)context;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        s_host->log("phone connected=%u", event->data.connection.connected);
        return true;
    }
    if (event->type != GM_PLUGIN_EVENT_BT_MESSAGE ||
        event->data.bt.channel != TEXT_CHANNEL) return false;
    if (event->data.bt.data == 0 || event->data.bt.length == 0U ||
        event->data.bt.length > MAX_TEXT_BYTES) return false;

    reply_length = prefix_length + event->data.bt.length;
    reply = s_host->alloc(reply_length + 1U);
    if (reply == 0) return false;
    for (index = 0; index < prefix_length; ++index) reply[index] = prefix[index];
    for (index = 0; index < event->data.bt.length; ++index) {
        uint8_t value = event->data.bt.data[index];
        /* Modify visible ASCII while preserving every UTF-8 byte unchanged. */
        if (value >= (uint8_t)'a' && value <= (uint8_t)'z')
            value = (uint8_t)(value - (uint8_t)'a' + (uint8_t)'A');
        reply[prefix_length + index] = (char)value;
    }
    reply[reply_length] = '\0';
    s_lvgl->label_set_text(s_label, reply);
    result = s_host->bt_send(TEXT_CHANNEL, reply, reply_length);
    s_host->free(reply);
    return result == GM_PLUGIN_OK;
}

static void bluetooth_stop(void *context)
{
    (void)context;
    s_label = 0;
    s_lvgl->obj_clean(s_lvgl->root_get());
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_BLUETOOTH;
    if (host == 0 || plugin == 0 || host->bt_send == 0 || host->log == 0 ||
        host->alloc == 0 || host->free == 0 || host->display_get_info == 0 ||
        host->graphics.lvgl == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required)
        return GM_PLUGIN_ENOTSUP;
    s_host = host;
    s_lvgl = host->graphics.lvgl;
    if (s_lvgl == 0 || s_lvgl->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(s_lvgl->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = bluetooth_start;
    plugin->on_event = bluetooth_event;
    plugin->on_stop = bluetooth_stop;
    return GM_PLUGIN_OK;
}
