/* Compile once per example so these checks exercise the shipped entry/start,
 * loop/event and stop callbacks, including their actual framebuffer writers. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gm_plugin_libc.h"
#include TEST_PLUGIN_SOURCE

static unsigned t_locks, t_sends, t_subscriptions, t_unsubscriptions;
static gm_plugin_result_t t_send_result = GM_PLUGIN_OK;
#ifdef TEST_WEB
static uint8_t t_last_status[20];
/* The fixture tests frame handling; compression itself is a Host service. */
static int32_t t_decompress(const void *src, void *dst, int32_t n, int32_t capacity)
{
    if (n < 0 || n > capacity) return -1;
    memcpy(dst, src, (size_t)n);
    return n;
}
static const gm_plugin_lz4_extension_api_t t_lz4 = {.decompress_safe=t_decompress};
#endif
static bool t_active, t_hidden, t_old_firmware;
static unsigned char t_root, t_child, t_pixels[300 * 350];
static gm_plugin_system_event_callback_t t_callback;
static void *t_context;
static gm_plugin_lvgl_obj_t *t_root_get(void) { return (void *)&t_root; }
static gm_plugin_lvgl_obj_t *t_create(gm_plugin_lvgl_obj_t *parent)
{ assert(parent); return (void *)&t_child; }
static void t_object(gm_plugin_lvgl_obj_t *o) { (void)o; }
static void t_invalidate(const gm_plugin_lvgl_obj_t *o) { (void)o; }
static void t_pos(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_coord_t x, gm_plugin_lvgl_coord_t y)
{ (void)o; (void)x; (void)y; }
static void t_align(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_align_t a, gm_plugin_lvgl_coord_t x, gm_plugin_lvgl_coord_t y)
{ (void)o; (void)a; (void)x; (void)y; }
static void t_add(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_flag_t f)
{ if (o == t_root_get() && (f & GM_PLUGIN_LVGL_FLAG_HIDDEN)) t_hidden = true; }
static void t_clear(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_flag_t f)
{ if (o == t_root_get() && (f & GM_PLUGIN_LVGL_FLAG_HIDDEN)) t_hidden = false; }
static void t_text(gm_plugin_lvgl_obj_t *o, const char *v) { (void)o; (void)v; }
static void t_mode(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_label_mode_t v) { (void)o; (void)v; }
static void t_range(gm_plugin_lvgl_obj_t *o, int16_t a, int16_t b)
{ (void)o; (void)a; (void)b; }
static void t_value(gm_plugin_lvgl_obj_t *o, int16_t v) { (void)o; (void)v; }
static void t_style(gm_plugin_lvgl_obj_t *o, gm_plugin_lvgl_style_prop_t p,
                    gm_plugin_lvgl_style_value_t v, gm_plugin_lvgl_selector_t s)
{ (void)o; (void)p; (void)v; (void)s; }
static void t_points(gm_plugin_lvgl_obj_t *o, const gm_plugin_lvgl_point_t *p, uint16_t n)
{ (void)o; (void)p; (void)n; }
static gm_plugin_lvgl_coord_t t_font(const gm_plugin_lvgl_font_t *f) { (void)f; return 20; }
static void t_measure(gm_plugin_lvgl_point_t *size, const char *s,
    const gm_plugin_lvgl_font_t *f, gm_plugin_lvgl_coord_t ls, gm_plugin_lvgl_coord_t line, gm_plugin_lvgl_coord_t w,
    gm_plugin_lvgl_text_flag_t flags)
{ (void)s; (void)f; (void)ls; (void)line; (void)w; (void)flags; size->x=80; size->y=20; }
static uint32_t t_next_line(const char *s, const gm_plugin_lvgl_font_t *f,
    gm_plugin_lvgl_coord_t ls, gm_plugin_lvgl_coord_t w, gm_plugin_lvgl_coord_t *used, gm_plugin_lvgl_text_flag_t flags)
{ (void)f; (void)ls; (void)w; (void)flags; if (used) *used=80; return strlen(s); }
static gm_plugin_lvgl_obj_t *t_image(gm_plugin_lvgl_obj_t *p, const gm_plugin_lvgl_image_dsc_t *s)
{ (void)s; return t_create(p); }
static gm_plugin_result_t t_source(gm_plugin_lvgl_obj_t *o, const gm_plugin_lvgl_image_dsc_t *s)
{ (void)o; (void)s; return GM_PLUGIN_OK; }
static gm_plugin_lvgl_obj_t *t_animation(gm_plugin_lvgl_obj_t *p,
    const gm_plugin_lvgl_image_dsc_t *const f[], uint16_t n, uint32_t d, uint16_t r)
{ (void)f; (void)n; (void)d; (void)r; return t_create(p); }
static gm_plugin_result_t t_frames(gm_plugin_lvgl_obj_t *o,
    const gm_plugin_lvgl_image_dsc_t *const f[], uint16_t n)
{ (void)o; (void)f; (void)n; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_duration(gm_plugin_lvgl_obj_t *o, uint32_t d)
{ (void)o; (void)d; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_repeat(gm_plugin_lvgl_obj_t *o, uint16_t r)
{ (void)o; (void)r; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_play(gm_plugin_lvgl_obj_t *o) { (void)o; return GM_PLUGIN_OK; }
static gm_plugin_lvgl_api_t t_ui = {
    .struct_size=sizeof(t_ui), .api_version=GM_PLUGIN_LVGL_API_VERSION,
    .root_get=t_root_get, .obj_create=t_create, .obj_clean=t_object, .obj_delete=t_object,
    .obj_set_pos=t_pos, .obj_set_size=t_pos, .obj_align=t_align,
    .obj_add_flag=t_add, .obj_clear_flag=t_clear, .obj_invalidate=t_invalidate,
    .style_set=t_style, .label_create=t_create, .label_set_text=t_text,
    .label_set_long_mode=t_mode, .arc_create=t_create, .arc_set_range=t_range,
    .arc_set_value=t_value, .line_create=t_create, .line_set_points=t_points,
    .font_get_line_height=t_font, .text_get_size=t_measure, .text_get_next_line=t_next_line,
    .image_create=t_image, .image_set_source=t_source, .anim_image_create=t_animation,
    .anim_image_set_sources=t_frames, .anim_image_set_frame_duration=t_duration,
    .anim_image_set_repeat_count=t_repeat, .anim_image_start=t_play, .anim_image_stop=t_play,
};
static void t_notify(bool active)
{
    gm_plugin_system_event_t e={0};
    e.struct_size=sizeof(e); e.type=GM_PLUGIN_SYSTEM_EVENT_CALL_UI; e.active=active;
    t_active=active; assert(t_callback); t_callback(t_context, &e);
}
static gm_plugin_result_t t_sub(gm_plugin_system_event_callback_t cb, void *ctx)
{ ++t_subscriptions; t_callback=cb; t_context=ctx; t_notify(t_active); return GM_PLUGIN_OK; }
static void t_unsub(void) { ++t_unsubscriptions; t_callback=NULL; }
static const gm_plugin_system_events_extension_api_t t_events={t_sub,t_unsub};
static const gm_plugin_libc_extension_api_t t_libc={
    .memcpy=memcpy, .memset=memset, .memmove=memmove, .memcmp=memcmp,
    .strlen=strlen, .strcmp=strcmp, .strncmp=strncmp, .snprintf=snprintf,
};
static gm_plugin_result_t t_discover(gm_plugin_extension_id_t id, const void **api)
{
    *api=NULL;
    if (id==GM_PLUGIN_EXTENSION_SYSTEM_EVENTS && !t_old_firmware) *api=&t_events;
    if (id==GM_PLUGIN_EXTENSION_LIBC) *api=&t_libc;
#ifdef TEST_WEB
    if (id==GM_PLUGIN_EXTENSION_LZ4) *api=&t_lz4;
#endif
    return *api ? GM_PLUGIN_OK : GM_PLUGIN_ENOTSUP;
}
static gm_plugin_result_t t_info(gm_plugin_display_info_t *d)
{ memset(d,0,sizeof(*d)); d->width=600; d->height=350; d->pixel_format=GM_PLUGIN_PIXEL_GRAY_4; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_lock(uint16_t y, gm_plugin_framebuffer_surface_t *s)
{
    assert(!t_active || t_old_firmware); ++t_locks;
    s->pixels=t_pixels+y*300; s->width=600; s->height=(350-y < 175 ? 350-y : 175); s->y=y; s->stride=300;
    return GM_PLUGIN_OK;
}
static gm_plugin_result_t t_unlock(const gm_plugin_rect_t *r, bool p)
{ (void)r; (void)p; assert(!t_active || t_old_firmware); return GM_PLUGIN_OK; }
static gm_plugin_result_t t_send(gm_plugin_bt_channel_t c, const void *d, uint32_t n)
{
    (void)c; (void)d; (void)n;
#ifdef TEST_WEB
    if (c == WEB_BRIDGE_CHANNEL_FRAME_STATUS && n == sizeof(t_last_status))
        memcpy(t_last_status, d, n);
#endif
    ++t_sends; return t_send_result;
}
static uint32_t t_now(void) { return 100; }
static void t_log(const char *s, ...) { (void)s; }
static void t_exit(void) { assert(!"unexpected exit"); }
static gm_plugin_result_t t_imu_enable(gm_plugin_imu_modes_t m) { (void)m; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_imu_read(gm_plugin_imu_sample_t *s) { memset(s,0,sizeof(*s)); return GM_PLUGIN_OK; }
static gm_plugin_result_t t_locale(char *s) { strcpy(s,"en"); return GM_PLUGIN_OK; }
static bool t_screen(void) { return true; }
static gm_plugin_result_t t_bool(bool v) { (void)v; return GM_PLUGIN_OK; }
static gm_plugin_display_brightness_t t_brightness(void) { return 2; }
static gm_plugin_display_distance_t t_distance(void) { return 2; }
static gm_plugin_display_height_t t_height(void) { return 2; }
static gm_plugin_result_t t_set_brightness(gm_plugin_display_brightness_t v) { (void)v; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_set_distance(gm_plugin_display_distance_t v) { (void)v; return GM_PLUGIN_OK; }
static gm_plugin_result_t t_set_height(gm_plugin_display_height_t v) { (void)v; return GM_PLUGIN_OK; }

int main(void)
{
    gm_plugin_host_api_t host={0}; gm_plugin_descriptor_t plugin={0};
    host.struct_size=sizeof(host); host.abi_version=GM_PLUGIN_ABI_MIN_VERSION;
    host.capabilities=~(gm_plugin_capabilities_t)0;
    host.graphics.lvgl=&t_ui; host.graphics.framebuffer.lock=t_lock;
    host.graphics.framebuffer.unlock=t_unlock; host.display_get_info=t_info;
    host.extension_get=t_discover; host.monotonic_ms=t_now; host.bt_send=t_send;
    host.log=t_log; host.app_exit=t_exit; host.alloc=malloc; host.free=free;
    host.imu_enable=t_imu_enable; host.imu_read=t_imu_read; host.locale_get=t_locale;
    host.display_control.screen_is_on=t_screen; host.display_control.screen_turn_on=t_bool;
    host.display_control.auto_brightness_block=t_bool;
    host.display_control.brightness_get=t_brightness; host.display_control.brightness_set=t_set_brightness;
    host.display_control.distance_get=t_distance; host.display_control.distance_set=t_set_distance;
    host.display_control.height_get=t_height; host.display_control.height_set=t_set_height;
    plugin.struct_size=sizeof(plugin);
    assert(gm_plugin_entry(&host,&plugin)==GM_PLUGIN_OK);
    t_active=true;
    memset(t_pixels,0xa5,sizeof(t_pixels));
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
    assert(t_subscriptions==1 && t_hidden && s_call_ui.active && t_locks==0);
    for (unsigned i=0;i<20;++i) if (plugin.on_loop) plugin.on_loop(plugin.context,40);
#ifdef TEST_BLUETOOTH
    unsigned before=t_sends;
    gm_plugin_event_t event={0}; event.type=GM_PLUGIN_EVENT_BT_MESSAGE;
    event.data.bt.channel=TEXT_CHANNEL; event.data.bt.data=(const uint8_t *)"hello"; event.data.bt.length=5;
    assert(plugin.on_event(plugin.context,&event)); assert(t_sends>before);
#endif
#ifdef TEST_PET
    unsigned before=t_sends;
    gm_plugin_event_t event={0}; event.type=GM_PLUGIN_EVENT_BUTTON;
    event.data.button.action=GM_PLUGIN_BUTTON_ACTION_SINGLE;
    assert(plugin.on_event(plugin.context,&event)); assert(t_sends>before);
    uint32_t animation_before=s_pet.animation_ms;
    plugin.on_loop(plugin.context,FRAME_MS); assert(s_pet.animation_ms>animation_before);
    assert(render(&s_pet)==GM_PLUGIN_OK);
#endif
#ifdef TEST_WEB
    unsigned before=t_sends; uint8_t bitmap[11]={0,0,0,0,0,2,0,1,0,1,0xff};
    assert(draw_bitmap(&context,bitmap,sizeof(bitmap),true)==GM_PLUGIN_OK);
    uint8_t begin[6]={0,0,0,7,0,1};
    uint8_t tile[21]={0};
    handle_frame_begin(&context,begin,sizeof(begin));
    assert(context.frame_active && t_last_status[18]==FRAME_STATUS_OK);
    write_u32(tile,7); memcpy(tile+6,bitmap,10); write_u32(tile+16,1); tile[20]=0xff;
    handle_frame_tile_lz4(&context,tile,sizeof(tile));
    assert(!context.frame_active && context.frame_next_tile==1);
    assert(t_last_status[18]==FRAME_STATUS_OK && t_last_status[19]==1);
    handle_frame_tile_lz4(&context,tile,sizeof(tile)); /* Retransmit is still ACKed. */
    assert(t_last_status[18]==FRAME_STATUS_OK && t_last_status[19]==1);
    gm_plugin_event_t event={0}; event.type=GM_PLUGIN_EVENT_BT_MESSAGE;
    event.data.bt.channel=WEB_BRIDGE_CHANNEL_PING;
    event.data.bt.data=bitmap; event.data.bt.length=sizeof(bitmap);
    assert(plugin.on_event(plugin.context,&event)); assert(t_sends>before);
#endif
#ifdef TEST_READER
    unsigned before=t_sends;
    uint8_t tile[19+300]={0};
    reader.session=1; reader.active=1; reader.current_page=0;
    reader.page_type[0]=PAGE_IMAGE; reader.page_image_id[0]=7;
    reader.image_id=7; reader.image_receiving=1; reader.image_tile_count=350;
    write_u32(tile+2,1); write_u32(tile+6,7); write_u16(tile+14,1); write_u16(tile+16,300);
    assert(handle_image_tile(&reader,tile,sizeof(tile)));
    assert(reader.image_next_tile==1 && t_sends>before);

#endif
    assert(t_locks==0);
    for (unsigned i=0;i<sizeof(t_pixels);++i) assert(t_pixels[i]==0xa5);
    t_notify(false); assert(!t_hidden && s_call_ui.redraw_pending);
#ifdef TEST_READER
    /* Resume must not cancel the transfer midway or lose a busy-queue retry. */
    plugin.on_loop(plugin.context,0);
    assert(reader.image_receiving && s_call_ui.redraw_pending && !reader.waiting_image);
    /* A stalled phone transfer must time out instead of blocking restoration. */
    t_send_result=GM_PLUGIN_EBUSY;
    plugin.on_loop(plugin.context,IMAGE_RESTORE_IDLE_MS - 1);
    assert(reader.image_receiving && s_call_ui.redraw_pending);
    plugin.on_loop(plugin.context,1);
    assert(!reader.image_receiving);
    assert(s_call_ui.redraw_pending && !reader.waiting_image);
    t_send_result=GM_PLUGIN_OK;
#endif
    if (plugin.on_loop) plugin.on_loop(plugin.context,0);
#if defined(TEST_PET) || defined(TEST_IMU) || defined(TEST_FRAMEBUFFER)
    assert(t_locks>0 && !s_call_ui.redraw_pending);
#endif
#ifdef TEST_READER
    assert(reader.waiting_image && t_sends>before+1);
#endif
#ifdef TEST_WEB
    /* A complete call can occur between tiles: never present only the tail. */
    write_u32(begin,8); write_u16(begin+4,2);
    handle_frame_begin(&context,begin,sizeof(begin));
    write_u32(tile,8); write_u16(tile+4,0);
    handle_frame_tile_lz4(&context,tile,sizeof(tile));
    assert(context.frame_next_tile==1 && context.frame_active);
    t_notify(true); t_notify(false);
    unsigned partial_locks=t_locks;
    write_u16(tile+4,1);
    handle_frame_tile_lz4(&context,tile,sizeof(tile));
    assert(t_locks==partial_locks && !context.frame_active && t_last_status[19]==1);
    write_u32(begin,9); write_u16(begin+4,1);
    handle_frame_begin(&context,begin,sizeof(begin));
    write_u32(tile,9); write_u16(tile+4,0);
    handle_frame_tile_lz4(&context,tile,sizeof(tile));
    assert(t_locks>partial_locks && !context.frame_active && t_last_status[19]==1);
#endif
    t_notify(true);
    unsigned locks_before=t_locks;
    if (plugin.on_loop) plugin.on_loop(plugin.context,40);
    assert(t_locks==locks_before && t_hidden);
    plugin.on_stop(plugin.context);
    assert(!t_callback && !t_hidden && t_unsubscriptions==1);
    /* Firmware without extension ID 5 still loads and runs the example. */
    t_active=false; t_old_firmware=true;
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
    assert(!s_call_ui.active && !t_hidden && t_subscriptions==1);
    plugin.on_stop(plugin.context);
    puts("example call UI passed: " TEST_PLUGIN_SOURCE);
    return 0;
}
