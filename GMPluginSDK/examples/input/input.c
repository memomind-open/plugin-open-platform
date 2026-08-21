#include "gm_plugin.h"

static const gm_plugin_host_api_t *s_host;

static bool input_event(void *context, const gm_plugin_event_t *event)
{
    (void)context;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    s_host->log("button=%u action=%u", event->data.button.button,
                event->data.button.action);
    if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_LONG &&
        s_host->app_exit != 0)
        s_host->app_exit();
    return true;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 || host->log == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & GM_PLUGIN_CAP_BUTTON) == 0U)
        return GM_PLUGIN_ENOTSUP;
    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_event = input_event;
    return GM_PLUGIN_OK;
}
