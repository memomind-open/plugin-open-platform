#include "gm_plugin.h"

static const gm_plugin_host_api_t *s_host;

static gm_plugin_result_t state_start(void *context)
{
    (void)context;
    s_host->log("battery=%u charging=%u wearing=%u",
                s_host->battery_percent(), s_host->battery_charging(),
                s_host->wearing());
    return GM_PLUGIN_OK;
}

static bool state_event(void *context, const gm_plugin_event_t *event)
{
    (void)context;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_CONNECTION) return false;
    s_host->log("phone connected=%u", event->data.connection.connected);
    return true;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 || host->battery_percent == 0 ||
        host->battery_charging == 0 || host->wearing == 0 || host->log == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & GM_PLUGIN_CAP_DEVICE_STATE) == 0U)
        return GM_PLUGIN_ENOTSUP;
    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = state_start;
    plugin->on_event = state_event;
    return GM_PLUGIN_OK;
}
