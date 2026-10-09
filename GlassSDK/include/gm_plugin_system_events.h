#ifndef GM_PLUGIN_SYSTEM_EVENTS_H
#define GM_PLUGIN_SYSTEM_EVENTS_H

#include "gm_plugin_extensions.h"

/** Resolve the optional system-event subscription API. */
static inline gm_plugin_result_t gm_plugin_system_events_get(
    const gm_plugin_host_api_t *host,
    const gm_plugin_system_events_extension_api_t **api)
{
    const void *found = NULL;
    const gm_plugin_system_events_extension_api_t *table;
    gm_plugin_result_t result;
    if (api == NULL) return GM_PLUGIN_EINVAL;
    *api = NULL;
    if (host == NULL) return GM_PLUGIN_EINVAL;
    if (host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE || host->extension_get == NULL)
        return GM_PLUGIN_ENOTSUP;
    result = host->extension_get(GM_PLUGIN_EXTENSION_SYSTEM_EVENTS, &found);
    if (result != GM_PLUGIN_OK) return result;
    table = (const gm_plugin_system_events_extension_api_t *)found;
    if (table == NULL || table->subscribe == NULL || table->unsubscribe == NULL)
        return GM_PLUGIN_ENOTSUP;
    *api = table;
    return GM_PLUGIN_OK;
}

#endif
