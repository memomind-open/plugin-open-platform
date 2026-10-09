#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../examples/game/fighter_arena/fighter_arena.c"

static unsigned locks, unlocks, presents, subscriptions, unsubscriptions;
static unsigned sends;
static gm_plugin_result_t send_result = GM_PLUGIN_OK;
static gm_plugin_result_t lock_result = GM_PLUGIN_OK;
static uint32_t clock_ms;
static bool call_active;
static uint8_t pixels[320 * 175];
static gm_plugin_system_event_callback_t callback;
static void *callback_context;
static uint32_t now(void) { return clock_ms; }
static gm_plugin_result_t lock_surface(uint16_t y, gm_plugin_framebuffer_surface_t *s)
{
    if (lock_result != GM_PLUGIN_OK) return lock_result;
    ++locks;
    s->pixels = pixels; s->y = y; s->height = 175;
    s->width = 600; s->stride = 320;
    return GM_PLUGIN_OK;
}
static gm_plugin_result_t unlock_surface(const gm_plugin_rect_t *dirty, bool present)
{
    assert(dirty != NULL); ++unlocks; if (present) ++presents;
    return GM_PLUGIN_OK;
}
static gm_plugin_result_t info(gm_plugin_display_info_t *d)
{
    memset(d, 0, sizeof(*d)); d->width = 600; d->height = 350;
    d->pixel_format = GM_PLUGIN_PIXEL_GRAY_4; return GM_PLUGIN_OK;
}
static void notify(bool active)
{
    gm_plugin_system_event_t e = {0};
    e.struct_size = sizeof(e); e.type = GM_PLUGIN_SYSTEM_EVENT_CALL_UI;
    e.active = active; call_active = active;
    assert(callback); callback(callback_context, &e);
}
static gm_plugin_result_t subscribe(gm_plugin_system_event_callback_t cb, void *ctx)
{
    ++subscriptions; callback = cb; callback_context = ctx;
    notify(call_active); return GM_PLUGIN_OK;
}
static void unsubscribe(void) { ++unsubscriptions; callback = NULL; }
static const gm_plugin_system_events_extension_api_t system_api = {subscribe, unsubscribe};
static const gm_plugin_libc_extension_api_t libc_api = {
    .memset = memset, .strlen = strlen, .snprintf = snprintf,
};
static gm_plugin_result_t discover(gm_plugin_extension_id_t id, const void **api)
{
    *api = NULL;
    if (id == GM_PLUGIN_EXTENSION_SYSTEM_EVENTS) *api = &system_api;
    if (id == GM_PLUGIN_EXTENSION_LIBC) *api = &libc_api;
    return *api ? GM_PLUGIN_OK : GM_PLUGIN_ENOTSUP;
}
static gm_plugin_result_t send_mock(gm_plugin_bt_channel_t channel,
                                      const void *data, uint32_t size)
{
    assert(channel == EVENT_CHANNEL && data != NULL && size == 4);
    ++sends;
    return send_result;
}
static void exit_mock(void) { assert(!"call UI must not trigger game exit"); }

int main(void)
{
    gm_plugin_host_api_t host = {0};
    gm_plugin_descriptor_t plugin = {0};
    gm_plugin_event_t input = {0};
    uint8_t snapshot[4] = {INPUT_VERSION, 1, 0, KEY_LIGHT};
    const gm_plugin_system_events_extension_api_t *found = &system_api;
    host.struct_size = sizeof(host); host.abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    host.capabilities = GM_PLUGIN_CAP_DISPLAY_BITMAP | GM_PLUGIN_CAP_BLUETOOTH | GM_PLUGIN_CAP_BUTTON;
    host.monotonic_ms = now; host.display_get_info = info; host.app_exit = exit_mock;
    host.graphics.framebuffer.lock = lock_surface; host.graphics.framebuffer.unlock = unlock_surface;
    host.bt_send = send_mock;
    host.extension_get = discover; plugin.struct_size = sizeof(plugin);
    assert(gm_plugin_system_events_get(NULL, &found) == GM_PLUGIN_EINVAL && found == NULL);
    assert(gm_plugin_entry(&host, &plugin) == GM_PLUGIN_OK);
    call_active = true; /* Loading while an existing call UI is visible. */
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_OK);
    assert(subscriptions == 1 && game.call_ui_active);
    assert(locks == 0 && unlocks == 0 && presents == 0);
    memset(pixels, 0xa5, sizeof(pixels));
    game.round_left_ms = 10000; game.screen = SCREEN_FIGHT;
    input.struct_size = sizeof(input); input.type = GM_PLUGIN_EVENT_BT_MESSAGE;
    input.data.bt.channel = INPUT_CHANNEL; input.data.bt.data = snapshot; input.data.bt.length = 4;
    snapshot[3] = 0;
    send_result = GM_PLUGIN_EBUSY;
    send_fight_event(&game, EVENT_MENU, 1);
    send_result = GM_PLUGIN_OK;
    unsigned initial_sends = sends;
    for (unsigned i = 0; i < 30; ++i) {
        clock_ms += FRAME_MS;
        assert(plugin.on_event(plugin.context, &input));
        assert(game.input_active && game.input_last_ms == clock_ms);
        plugin.on_loop(plugin.context, FRAME_MS);
    }
    assert(game.round_left_ms < 10000 && locks == 0);
    assert(sends > initial_sends); /* The loop still drains outbound BT. */
    for (unsigned i = 0; i < sizeof(pixels); ++i) assert(pixels[i] == 0xa5);

    /* A busy transport keeps its queued event and retries during the call. */
    while (game.event_count) plugin.on_loop(plugin.context, 0);
    send_result = GM_PLUGIN_EBUSY;
    send_fight_event(&game, EVENT_MENU, 1);
    unsigned pending = game.event_count, sent_before = sends;
    plugin.on_loop(plugin.context, 0);
    assert(game.event_count == pending && sends == sent_before + 1);
    send_result = GM_PLUGIN_OK;
    plugin.on_loop(plugin.context, 0);
    assert(game.event_count == 0 && sends == sent_before + 2);

    /* Immediate button redraws must also leave the telephone pixels intact. */
    input.type = GM_PLUGIN_EVENT_BUTTON; input.data.button.action = GM_PLUGIN_BUTTON_ACTION_LONG;
    assert(plugin.on_event(plugin.context, &input));
    assert(game.holding_exit && locks == 0);
    input.data.button.action = GM_PLUGIN_BUTTON_ACTION_RELEASE;
    assert(plugin.on_event(plugin.context, &input));
    assert(!game.holding_exit && locks == 0);
    assert(render(&game) == GM_PLUGIN_OK && locks == 0);

    uint32_t round_before = game.round_left_ms;
    uint16_t input_before = game.input;
    send_result = GM_PLUGIN_EBUSY;
    send_fight_event(&game, EVENT_MENU, 1);
    pending = game.event_count;
    assert(pending > 0);
    notify(false); /* No drawing or queue/input reset in the callback. */
    assert(locks == 0 && game.input == input_before && game.event_count == pending);
    send_result = GM_PLUGIN_OK;
    plugin.on_loop(plugin.context, 0);
    assert(locks == 2 && unlocks == 2 && presents == 1);
    assert(game.round_left_ms == round_before && game.event_count == 0);
    /* A busy first redraw must remain pending even with no simulation tick. */
    notify(true); notify(false);
    lock_result = GM_PLUGIN_EBUSY;
    unsigned retry_locks = locks;
    plugin.on_loop(plugin.context, 0);
    assert(game.redraw_pending && locks == retry_locks);
    lock_result = GM_PLUGIN_OK;
    plugin.on_loop(plugin.context, 0);
    assert(!game.redraw_pending && locks > retry_locks);
    notify(true); notify(true);
    unsigned before = locks;
    input.type = GM_PLUGIN_EVENT_BT_MESSAGE; snapshot[3] = KEY_RIGHT;
    assert(plugin.on_event(plugin.context, &input));
    plugin.on_loop(plugin.context, FRAME_MS);
    assert(game.input == KEY_RIGHT && locks == before);
    input.type = GM_PLUGIN_EVENT_CONNECTION; input.data.connection.connected = false;
    assert(plugin.on_event(plugin.context, &input));
    assert(game.pause_reason == PAUSE_DISCONNECTED);
    plugin.on_loop(plugin.context, FRAME_MS); assert(locks == before);
    plugin.on_stop(plugin.context); assert(unsubscriptions == 1 && callback == NULL);
    assert(plugin.on_start(plugin.context) == GM_PLUGIN_OK);
    assert(game.call_ui_active && locks == before && subscriptions == 2);
    plugin.on_stop(plugin.context);
    puts("fighter call UI tests passed");
    return 0;
}
