#include "gm_plugin.h"
#include "gm_plugin_extensions.h"

static const gm_plugin_host_api_t *s_host;
static const gm_plugin_lz4_extension_api_t *s_lz4;

static gm_plugin_result_t lz4_load(void *context)
{
    static const uint8_t source[] =
        "GM Plugin LZ4 extension round-trip data data data data";
    uint8_t compressed[96];
    uint8_t restored[sizeof(source)];
    int32_t compression_bound;
    int32_t compressed_size;
    int32_t restored_size;
    uint32_t index;

    (void)context;
    compression_bound = s_lz4->compress_bound((int32_t)sizeof(source));
    if (compression_bound <= 0 ||
        compression_bound > (int32_t)sizeof(compressed)) {
        s_host->log("lz4 example: invalid compression bound (%d)",
                    compression_bound);
        return GM_PLUGIN_EIO;
    }
    compressed_size = s_lz4->compress_default(
        source, compressed, (int32_t)sizeof(source),
        compression_bound);
    if (compressed_size <= 0) {
        s_host->log("lz4 example: compression failed");
        return GM_PLUGIN_EIO;
    }

    restored_size = s_lz4->decompress_safe(
        compressed, restored, compressed_size, (int32_t)sizeof(restored));
    if (restored_size != (int32_t)sizeof(source)) {
        s_host->log("lz4 example: decompression failed (%d)", restored_size);
        return GM_PLUGIN_EIO;
    }
    for (index = 0; index < sizeof(source); ++index) {
        if (restored[index] != source[index]) {
            s_host->log("lz4 example: mismatch at %u", index);
            return GM_PLUGIN_EIO;
        }
    }

    s_host->log("lz4 example: %u bytes compressed to %d bytes",
                (uint32_t)sizeof(source), compressed_size);
    return GM_PLUGIN_OK;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const void *api = 0;
    gm_plugin_result_t result;

    if (host == 0 || plugin == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        host->extension_get == 0 || host->log == 0) {
        return GM_PLUGIN_EVERSION;
    }

    result = host->extension_get(GM_PLUGIN_EXTENSION_LZ4, &api);
    if (result != GM_PLUGIN_OK) return result;

    s_host = host;
    s_lz4 = (const gm_plugin_lz4_extension_api_t *)api;
    if (s_lz4 == 0 || s_lz4->compress_bound == 0 ||
        s_lz4->compress_default == 0 || s_lz4->decompress_safe == 0)
        return GM_PLUGIN_EVERSION;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = 0;
    plugin->on_load = lz4_load;
    plugin->on_unload = 0;
    plugin->on_event = 0;
    plugin->on_loop = 0;
    return GM_PLUGIN_OK;
}
