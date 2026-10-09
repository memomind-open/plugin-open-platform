#ifndef EXAMPLE_CALL_UI_H
#define EXAMPLE_CALL_UI_H

#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_system_events.h"

/* Cooperative example policy, not a Host drawing restriction. Raw framebuffer
 * writers must also check active before touching pixels. Loops and BT stay live. */
typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_system_events_extension_api_t *events;
    gm_plugin_lvgl_obj_t *root;
    bool active;
    bool redraw_pending;
} example_call_ui_t;

static void example_call_ui_event(void *context,
                                   const gm_plugin_system_event_t *event)
{
    example_call_ui_t *state = context;
    if (event == NULL || event->struct_size < sizeof(*event) ||
        event->type != GM_PLUGIN_SYSTEM_EVENT_CALL_UI ||
        state->active == event->active) return;
    state->active = event->active;
    if (state->root != NULL) {
        const gm_plugin_lvgl_api_t *ui = state->host->graphics.lvgl;
        if (event->active)
            ui->obj_add_flag(state->root, GM_PLUGIN_LVGL_FLAG_HIDDEN);
        else
            ui->obj_clear_flag(state->root, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
    if (!event->active) state->redraw_pending = true;
}

static inline gm_plugin_result_t example_call_ui_start(example_call_ui_t *state)
{
    gm_plugin_result_t result;
    state->active = false;
    state->redraw_pending = false;
    state->root = state->host->graphics.lvgl != NULL
        ? state->host->graphics.lvgl->root_get() : NULL;
    result = gm_plugin_system_events_get(state->host, &state->events);
    if (result == GM_PLUGIN_ENOTSUP) return GM_PLUGIN_OK;
    if (result != GM_PLUGIN_OK) return result;
    /* Subscription immediately reports an already visible call UI, before
     * the plugin builds its UI or writes its first framebuffer slice. */
    return state->events->subscribe(example_call_ui_event, state);
}

static inline void example_call_ui_stop(example_call_ui_t *state)
{
    if (state->events != NULL) state->events->unsubscribe();
    if (state->active && state->root != NULL)
        state->host->graphics.lvgl->obj_clear_flag(
            state->root, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    state->events = NULL;
    state->root = NULL;
    state->active = false;
    state->redraw_pending = false;
}

#endif
