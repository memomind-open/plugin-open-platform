#include "gm_plugin.h"

static const gm_plugin_host_api_t *s_host;

static gm_plugin_result_t minimal_load(void *context)
{
    (void)context;
    s_host->log("minimal plugin loaded");
    return GM_PLUGIN_OK;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                    gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 || host->log == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;

    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_load = minimal_load;
    return GM_PLUGIN_OK;
}
