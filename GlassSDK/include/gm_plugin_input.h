#ifndef GM_PLUGIN_INPUT_H
#define GM_PLUGIN_INPUT_H

#include "gm_plugin_extensions.h"

/* Optional lookup: absence/failure must not prevent legacy controls. */
static inline gm_plugin_result_t gm_plugin_input_get(
    const gm_plugin_host_api_t *host,
    const gm_plugin_input_extension_api_t **input)
{
    const void *api = 0;
    gm_plugin_result_t result;
    const gm_plugin_input_extension_api_t *table;
    if (input == 0) return GM_PLUGIN_EINVAL;
    *input = 0;
    if (host == 0) return GM_PLUGIN_EINVAL;
    if (host->struct_size < GM_PLUGIN_MEMBER_END(gm_plugin_host_api_t, extension_get)
        || host->extension_get == 0) return GM_PLUGIN_ENOTSUP;
    result = host->extension_get(GM_PLUGIN_EXTENSION_INPUT, &api);
    if (result != GM_PLUGIN_OK) return result;
    table = (const gm_plugin_input_extension_api_t *)api;
    if (table == 0 || table->subscribe == 0 || table->unsubscribe == 0)
        return GM_PLUGIN_ENOTSUP;
    *input = table;
    return GM_PLUGIN_OK;
}

/* Exactly mirrors host_input_event_class(). Class is NOT a wire field. */
static inline gm_plugin_input_classes_t gm_plugin_input_event_class(
    const gm_plugin_input_event_t *event)
{
    if (event == 0 || event->struct_size < sizeof(*event)) return 0;
    if (event->source == GM_PLUGIN_INPUT_SOURCE_DIGITIZER)
        return GM_PLUGIN_INPUT_CLASS_TOUCH;
    if (event->key >= GM_PLUGIN_INPUT_KEY_REL_X &&
        event->key <= GM_PLUGIN_INPUT_KEY_REL_HWHEEL)
        return GM_PLUGIN_INPUT_CLASS_REL;
    if (event->key >= GM_PLUGIN_INPUT_KEY_ABS_X &&
        event->key <= GM_PLUGIN_INPUT_KEY_ABS_PRESSURE)
        return GM_PLUGIN_INPUT_CLASS_ABS;
    if (event->key >= GM_PLUGIN_INPUT_KEY_TOUCH_DOWN &&
        event->key <= GM_PLUGIN_INPUT_KEY_TOUCH_SWIPE_RIGHT)
        return GM_PLUGIN_INPUT_CLASS_TOUCH;
    return GM_PLUGIN_INPUT_CLASS_KEY;
}

#endif
