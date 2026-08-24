#include "gm_plugin.h"
#include "gm_plugin_extensions.h"

static const gm_plugin_host_api_t *s_host;
static const gm_plugin_lz4_extension_api_t *s_extension;

static gm_plugin_result_t extension_load(void *context)
{
    int32_t bound;

    (void)context;
    bound = s_extension->compress_bound(64);
    if (bound <= 0) return GM_PLUGIN_EIO;
    s_host->log("extension discovery: LZ4 bound for 64 bytes is %d", bound);
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

    result = host->extension_get(GM_PLUGIN_EXTENSION_LZ4, &api);
    if (result != GM_PLUGIN_OK) return result;
    s_extension = (const gm_plugin_lz4_extension_api_t *)api;
    if (s_extension == 0 || s_extension->compress_bound == 0)
        return GM_PLUGIN_EVERSION;

    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_load = extension_load;
    return GM_PLUGIN_OK;
}
