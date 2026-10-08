#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include "input_abi_asserts.h"
#include "../examples/game/breakout/breakout.c"

static gm_plugin_input_callback_t callback;
static void *callback_context;
static int subscriptions, unsubscriptions, exits, positions;
static gm_plugin_result_t subscribe_result = GM_PLUGIN_OK;
static gm_plugin_result_t discovery_result = GM_PLUGIN_OK;
static gm_plugin_result_t sub(gm_plugin_input_classes_t classes,
                              gm_plugin_input_callback_t cb, void *ctx)
{
    assert(classes == 15);
    ++subscriptions; callback = cb; callback_context = ctx;
    return subscribe_result;
}
static void unsub(void) { ++unsubscriptions; callback = 0; }
static const gm_plugin_input_extension_api_t api = {sub, unsub};
static const void *discovered = &api;
static gm_plugin_result_t discover(gm_plugin_extension_id_t id, const void **out)
{ assert(id == 4); *out = discovered; return discovery_result; }
static void pos(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_coord_t x, gm_plugin_lvgl_coord_t y)
{ (void)o; (void)x; (void)y; ++positions; }
static void flag(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_flag_t f) { (void)o; (void)f; }
static void text(gm_plugin_lvgl_obj_t *o, const char *s) { (void)o; (void)s; }
static gm_plugin_result_t imu(gm_plugin_imu_modes_t modes) { (void)modes; return GM_PLUGIN_OK; }
static void exit_mock(void) { ++exits; }
static uint32_t now(void) { return 100; }
static gm_plugin_lvgl_obj_t *root(void) { return 0; }
static void clean(gm_plugin_lvgl_obj_t *o) { (void)o; }
static void measure(gm_plugin_lvgl_point_t *p, const char *t,
    const gm_plugin_lvgl_font_t *f, gm_plugin_lvgl_coord_t ls,
    gm_plugin_lvgl_coord_t line, gm_plugin_lvgl_coord_t w,
    gm_plugin_lvgl_text_flag_t flags)
{ (void)t; (void)f; (void)ls; (void)line; (void)w; (void)flags; p->x=80; p->y=20; }
static void align_mock(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_align_t a,
    gm_plugin_lvgl_coord_t x, gm_plugin_lvgl_coord_t y)
{ (void)o; (void)a; (void)x; (void)y; }

static void sample(breakout_t *s, gm_plugin_input_key_t key,
    gm_plugin_input_axis_t axis, int32_t value)
{
    gm_plugin_input_event_t e = {0};
    e.struct_size=sizeof(e); e.key=key; e.axis=axis; e.value=value;
    e.phase=GM_PLUGIN_INPUT_PHASE_MOVE;
    on_input(s, &e);
}

int main(void)
{
    gm_plugin_host_api_t host = {0};
    gm_plugin_lvgl_api_t ui = {0};
    breakout_t s = {0};
    breakout_strings_t strings = {0};
    const gm_plugin_input_extension_api_t *found = &api;
    gm_plugin_input_event_t e = {0};
    gm_plugin_input_extension_api_t incomplete = {sub, 0};
    gm_plugin_event_t button = {0};
    assert(gm_plugin_input_get(0, &found)==GM_PLUGIN_EINVAL && found==0);
    assert(gm_plugin_input_get(&host, 0)==GM_PLUGIN_EINVAL);
    assert(gm_plugin_input_get(&host, &found)==GM_PLUGIN_ENOTSUP);
    host.struct_size=sizeof(host); host.extension_get=discover;
    discovery_result=GM_PLUGIN_ENOTSUP;
    assert(gm_plugin_input_get(&host, &found)==GM_PLUGIN_ENOTSUP && found==0);
    discovery_result=GM_PLUGIN_OK; discovered=&incomplete;
    assert(gm_plugin_input_get(&host, &found)==GM_PLUGIN_ENOTSUP);
    discovered=0;
    assert(gm_plugin_input_get(&host, &found)==GM_PLUGIN_ENOTSUP);
    discovered=&api;
    assert(gm_plugin_input_get(&host, &found)==GM_PLUGIN_OK && found==&api);
    ui.obj_set_pos=pos; ui.obj_set_size=pos; ui.obj_add_flag=flag; ui.obj_clear_flag=flag;
    ui.label_set_text=text; ui.root_get=root; ui.obj_clean=clean;
    ui.text_get_size=measure; ui.obj_align=align_mock;
    host.imu_enable=imu; host.app_exit=exit_mock; host.monotonic_ms=now;
    s.host=&host; s.ui=&ui; s.strings=&strings;
    s.board_width=580; s.board_height=294; s.screen_width=600; s.screen_height=350;
    s.paddle_x=100*Q; s.paddle_y=270; s.paddle=(gm_plugin_lvgl_obj_t *)&s;
    button.type=GM_PLUGIN_EVENT_BUTTON;
    button.data.button.action=GM_PLUGIN_BUTTON_ACTION_TRIGGER;
    button.data.button.button=GM_PLUGIN_BUTTON_RIGHT;
    subscribe_input(&s); assert(!s.input_active);
    assert(on_event(&s,&button)); assert(s.paddle_x==132*Q);
    s.input=&api; subscribe_result=GM_PLUGIN_ENOTSUP;
    subscribe_input(&s); assert(!s.input_active);
    assert(on_event(&s,&button)); assert(s.paddle_x==164*Q);
    subscribe_result=GM_PLUGIN_OK; subscribe_input(&s);
    assert(s.input_active && callback==on_input && callback_context==&s);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_X, GM_PLUGIN_INPUT_AXIS_X, 12);
    assert(s.paddle_x==176*Q);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_X, GM_PLUGIN_INPUT_AXIS_X, INT32_MAX);
    assert(s.paddle_x==500*Q);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_X, GM_PLUGIN_INPUT_AXIS_X, INT32_MIN);
    assert(s.paddle_x==0);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_Y, GM_PLUGIN_INPUT_AXIS_Y, -10);
    assert(s.paddle_y==260);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_WHEEL, GM_PLUGIN_INPUT_AXIS_WHEEL, 1);
    assert(s.paddle_y==248);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_WHEEL, GM_PLUGIN_INPUT_AXIS_WHEEL, INT32_MIN);
    assert(s.paddle_y==284);
    sample(&s, GM_PLUGIN_INPUT_KEY_REL_WHEEL, GM_PLUGIN_INPUT_AXIS_WHEEL, INT32_MAX);
    assert(s.paddle_y==2);
    e.struct_size=sizeof(e); e.key=GM_PLUGIN_INPUT_KEY_ABS_RX;
    e.axis=GM_PLUGIN_INPUT_AXIS_RX; e.source=GM_PLUGIN_INPUT_SOURCE_GAMEPAD;
    e.phase=GM_PLUGIN_INPUT_PHASE_MOVE;
    on_input(&s,&e); assert(s.paddle_x==250*Q);
    e.norm_value=INT32_MAX; on_input(&s,&e); assert(s.paddle_x==500*Q);
    e.norm_value=INT32_MIN; on_input(&s,&e); assert(s.paddle_x==0);
    e.source=GM_PLUGIN_INPUT_SOURCE_GENERIC; e.axis=GM_PLUGIN_INPUT_AXIS_X;
    e.key=GM_PLUGIN_INPUT_KEY_ABS_X; e.norm_value=120;
    on_input(&s,&e); assert(s.paddle_x==120*Q);
    e.key=GM_PLUGIN_INPUT_KEY_RIGHT; e.phase=GM_PLUGIN_INPUT_PHASE_DOWN;
    on_input(&s,&e); assert(s.paddle_x==152*Q);
    e.phase=GM_PLUGIN_INPUT_PHASE_REPEAT; on_input(&s,&e); assert(s.paddle_x==184*Q);
    e.phase=GM_PLUGIN_INPUT_PHASE_UP; on_input(&s,&e); assert(s.paddle_x==184*Q);
    const gm_plugin_input_key_t confirms[]={GM_PLUGIN_INPUT_KEY_MOUSE_LEFT,
        GM_PLUGIN_INPUT_KEY_SELECT, GM_PLUGIN_INPUT_KEY_GAMEPAD_A, GM_PLUGIN_INPUT_KEY_TOUCH_TAP};
    for (unsigned i=0;i<sizeof(confirms)/sizeof(*confirms);++i) {
        e.key=confirms[i]; e.phase=GM_PLUGIN_INPUT_PHASE_DOWN;
        on_input(&s,&e); assert(s.paused);
        e.phase=GM_PLUGIN_INPUT_PHASE_UP; on_input(&s,&e); assert(s.paused);
        e.phase=GM_PLUGIN_INPUT_PHASE_DOWN; on_input(&s,&e); assert(!s.paused);
    }
    e.source=GM_PLUGIN_INPUT_SOURCE_DIGITIZER; e.key=GM_PLUGIN_INPUT_KEY_TOUCH_DOWN;
    e.phase=GM_PLUGIN_INPUT_PHASE_DOWN; e.x=100; e.y=50; on_input(&s,&e);
    e.key=GM_PLUGIN_INPUT_KEY_TOUCH_MOVE; e.phase=GM_PLUGIN_INPUT_PHASE_MOVE; e.x=125;
    on_input(&s,&e); assert(s.paddle_x==209*Q);
    e.phase=GM_PLUGIN_INPUT_PHASE_CANCEL; on_input(&s,&e); assert(!s.touch_axes);
    e.phase=GM_PLUGIN_INPUT_PHASE_MOVE; e.key=GM_PLUGIN_INPUT_KEY_ABS_Y;
    e.axis=GM_PLUGIN_INPUT_AXIS_Y; e.value=100; on_input(&s,&e);
    e.value=120; on_input(&s,&e); assert(s.paddle_x==229*Q); /* 1-D vertical slider */
    e.value=INT32_MIN; on_input(&s,&e); assert(s.paddle_x==0);
    e.value=INT32_MAX; on_input(&s,&e); assert(s.paddle_x==500*Q);
    int before=positions;
    e.struct_size=4; on_input(&s,&e); assert(positions==before);
    e.struct_size=sizeof(e)+8; e.source=GM_PLUGIN_INPUT_SOURCE_MOUSE;
    e.key=GM_PLUGIN_INPUT_KEY_REL_X; e.axis=GM_PLUGIN_INPUT_AXIS_X; e.value=-5;
    s.paused=1; on_input(&s,&e); s.paused=0;
    s.ended=1; on_input(&s,&e); s.ended=0; assert(positions==before);
    on_suspend(&s); on_input(&s,&e); assert(positions==before);
    on_resume(&s); on_input(&s,&e); assert(s.paddle_x==495*Q);
    on_stop(&s); on_input(&s,&e); assert(unsubscriptions==1 && !callback);
    subscribe_input(&s); on_suspend(&s); on_stop(&s); assert(unsubscriptions==2);
    s.input=0; on_stop(&s); assert(unsubscriptions==2);
    button.data.button.button=GM_PLUGIN_BUTTON_SCROLL_DOWN;
    assert(on_event(&s,&button)); assert(s.paddle_y==14);
    subscribe_input(&s); assert(!s.input_active);
    s.input=&api; subscribe_input(&s);
    e.key=GM_PLUGIN_INPUT_KEY_MOUSE_RIGHT; e.phase=GM_PLUGIN_INPUT_PHASE_DOWN;
    on_input(&s,&e); assert(exits==1 && s.exiting==2);
    on_input(&s,&e); assert(exits==1);
    puts("Input ABI/discovery, Breakout REL/ABS/KEY/TOUCH, fallback and lifecycle passed");
    return 0;
}
