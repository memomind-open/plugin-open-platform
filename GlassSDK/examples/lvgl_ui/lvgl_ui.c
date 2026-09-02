#include "gm_plugin_lvgl_api.h"

#define PROGRESS_STEP_MS 50U

static const gm_plugin_lvgl_api_t *s_lvgl;
static gm_plugin_lvgl_obj_t *s_progress;
static int32_t s_value;
static uint32_t s_progress_elapsed_ms;

static gm_plugin_result_t plugin_start(void *context)
{
    gm_plugin_lvgl_obj_t *screen;
    gm_plugin_lvgl_obj_t *title;
    (void)context;
    screen = s_lvgl->root_get();
    if (screen == 0) return GM_PLUGIN_ESTATE;
    title = s_lvgl->label_create(screen);
    s_progress = s_lvgl->arc_create(screen);
    if (title == 0 || s_progress == 0) {
        /* on_stop() is not called after a failed start, so release every
         * object that may already have been created here. */
        s_progress = 0;
        s_lvgl->obj_clean(screen);
        return GM_PLUGIN_ENOMEM;
    }
    s_lvgl->label_set_text(title, "LVGL UI example");
    s_lvgl->obj_align(title, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 28);
    s_lvgl->obj_set_size(s_progress, 96, 96);
    s_lvgl->obj_align(s_progress, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 12);
    s_lvgl->arc_set_range(s_progress, 0, 100);
    s_value = 0;
    s_progress_elapsed_ms = 0U;
    return GM_PLUGIN_OK;
}

static void plugin_tick(void *context, uint32_t elapsed_ms)
{
    uint32_t steps;
    (void)context;
    if (elapsed_ms > 1000U) elapsed_ms = 1000U;
    s_progress_elapsed_ms += elapsed_ms;
    steps = s_progress_elapsed_ms / PROGRESS_STEP_MS;
    if (steps == 0U) return;
    s_progress_elapsed_ms %= PROGRESS_STEP_MS;
    s_value = (s_value + (int32_t)steps) % 101;
    s_lvgl->arc_set_value(s_progress, (int16_t)s_value);
}

static void plugin_stop(void *context)
{
    (void)context;
    s_progress = 0;
    s_lvgl->obj_clean(s_lvgl->root_get());
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                    gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        host->graphics.lvgl == 0 ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;
    s_lvgl = host->graphics.lvgl;
    if (s_lvgl == 0 ||
        s_lvgl->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(s_lvgl->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = plugin_start;
    plugin->on_loop = plugin_tick;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
