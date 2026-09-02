#ifndef GM_PLUGIN_LIBC_H
#define GM_PLUGIN_LIBC_H

#include "gm_plugin.h"
#include "gm_plugin_extensions.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve the immutable Host C runtime extension table.
 *
 * Call this during gm_plugin_entry(), before publishing lifecycle callbacks.
 * The returned table is borrowed from the Host, requires no release, and
 * remains valid until plugin unload. A successful Host must provide every
 * function declared by gm_plugin_libc_extension_api_t.
 *
 * @param host Valid Host table whose core ABI and struct_size were checked by
 *        the caller.
 * @param libc Non-NULL output. It is cleared before extension discovery and
 *        receives the borrowed table only on success.
 * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL for invalid arguments,
 *         GM_PLUGIN_ENOTSUP when the Host has no extension discovery entry or
 *         no libc extension, GM_PLUGIN_EVERSION for a malformed NULL table,
 *         or another error returned by extension_get().
 */
static inline gm_plugin_result_t gm_plugin_libc_get(
    const gm_plugin_host_api_t *host,
    const gm_plugin_libc_extension_api_t **libc)
{
    const void *api = 0;
    gm_plugin_result_t result;
    if (libc == 0) return GM_PLUGIN_EINVAL;
    *libc = 0;
    if (host == 0) return GM_PLUGIN_EINVAL;
    if (host->extension_get == 0) return GM_PLUGIN_ENOTSUP;
    result = host->extension_get(GM_PLUGIN_EXTENSION_LIBC, &api);
    if (result != GM_PLUGIN_OK) return result;
    if (api == 0) return GM_PLUGIN_EVERSION;
    *libc = (const gm_plugin_libc_extension_api_t *)api;
    return GM_PLUGIN_OK;
}

#ifdef __cplusplus
}
#endif

#endif
