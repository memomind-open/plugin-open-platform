#include "gm_plugin.h"
#include "gm_plugin_extensions.h"

static const gm_plugin_host_api_t *s_host;
static const gm_plugin_demo_extension_api_t *s_demo;

static gm_plugin_result_t extension_load(void *context)
{
    (void)context;
    s_host->log("extension demo: 20 + 22 = %d", s_demo->add(20, 22));
    return GM_PLUGIN_OK;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const void *api = 0;
    gm_plugin_result_t result;

    if (host == 0 || plugin == 0 || host->log == 0 ||
        host->extension_get == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;

    result = host->extension_get(GM_PLUGIN_EXTENSION_DEMO, &api);
    if (result != GM_PLUGIN_OK) return result;
    s_demo = api;
    if (s_demo == 0 || s_demo->add == 0) return GM_PLUGIN_EVERSION;

    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_load = extension_load;
    return GM_PLUGIN_OK;
}
