#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_system_events.h"

/* Record actual plugin calls, keeping each object's identity and parent. */
struct _lv_obj_t {
    struct _lv_obj_t *parent;
    int32_t x, y, w, h;
    gm_plugin_lvgl_align_t alignment;
    uint32_t flags;
    gm_plugin_lvgl_style_value_t style[128];
    char text[768];
    const gm_plugin_lvgl_point_t *points;
    uint16_t point_count;
};
static struct _lv_obj_t objects[1024];
static unsigned count = 1, fail_at, exits, unsubscribed;
static unsigned images, ui_writes, imu_calls;
static uint32_t clock_ms;
static int16_t pitch, gyro_x, gyro_y;
static bool deny_imu, old_host, call_active;
static const char *locale = "zh-CN";
static uint16_t display_w = 600, display_h = 350;
static gm_plugin_system_event_callback_t notify_call;
static void *notify_context;

#ifdef TEST_JET_RUNNER
#include "../examples/game/jet_runner/jet_runner.c"
#elif defined(TEST_BREAKOUT)
#include "../examples/game/breakout/breakout.c"
#else
#include "../examples/game/tetris/tetris.c"
#endif

static gm_plugin_lvgl_obj_t *root(void) { return &objects[0]; }
static gm_plugin_lvgl_obj_t *create(gm_plugin_lvgl_obj_t *parent)
{
    ++ui_writes;
    assert(parent);
    if (fail_at && count >= fail_at) return NULL;
    assert(count < 1024);
    gm_plugin_lvgl_obj_t *obj = &objects[count++];
    memset(obj, 0, sizeof(*obj));
    obj->parent = parent;
    return obj;
}
static void clean(gm_plugin_lvgl_obj_t *obj)
{ ++ui_writes; assert(obj == root()); count = 1; }
static void pos(gm_plugin_lvgl_obj_t *obj, int32_t x, int32_t y)
{ ++ui_writes; assert(obj); obj->x=x; obj->y=y; }
static void size(gm_plugin_lvgl_obj_t *obj, int32_t w, int32_t h)
{ ++ui_writes; assert(obj); assert(w>=0 && h>=0); obj->w=w; obj->h=h; }
static void align_obj(gm_plugin_lvgl_obj_t *obj, gm_plugin_lvgl_align_t a, int32_t x, int32_t y)
{ ++ui_writes; assert(obj); obj->alignment=a; obj->x=x; obj->y=y; }
static void add(gm_plugin_lvgl_obj_t *obj, uint32_t flag)
{ ++ui_writes; assert(obj); obj->flags |= flag; }
static void clear(gm_plugin_lvgl_obj_t *obj, uint32_t flag)
{ ++ui_writes; assert(obj); obj->flags &= ~flag; }
static void style(gm_plugin_lvgl_obj_t *obj, gm_plugin_lvgl_style_prop_t p,
                  gm_plugin_lvgl_style_value_t v, gm_plugin_lvgl_selector_t selector)
{ ++ui_writes; (void)selector; assert(obj && p<128); obj->style[p]=v; if(p==1) obj->w=v.num; }
static void text(gm_plugin_lvgl_obj_t *obj, const char *s)
{ ++ui_writes; assert(obj && s); assert(strlen(s)<sizeof(obj->text)); strcpy(obj->text,s); }
static void mode(gm_plugin_lvgl_obj_t *obj, gm_plugin_lvgl_label_mode_t v)
{ ++ui_writes; assert(obj); (void)v; }
static void points(gm_plugin_lvgl_obj_t *obj, const gm_plugin_lvgl_point_t *p, uint16_t n)
{ ++ui_writes; assert(obj); obj->points=p; obj->point_count=n; }
static gm_plugin_lvgl_obj_t *create_image(gm_plugin_lvgl_obj_t *parent,
                                         const gm_plugin_lvgl_image_dsc_t *image)
{
    assert((image->width==18 || image->width==48) && image->height==image->width);
    assert(image->data_size==64U+(uint32_t)image->width*image->height/2U && ((uintptr_t)image->data&3)==0);
    ++images;
    return create(parent);
}
/* Firmware lv_font_xgimi_17 has a 34px line height. Glyph advances here are
 * conservative synthetic widths, not the flash font's real glyph metrics. */
static int32_t font_height(const gm_plugin_lvgl_font_t *f) { (void)f; return 34; }
static void measure(gm_plugin_lvgl_point_t *out, const char *s,
    const gm_plugin_lvgl_font_t *f, int32_t ls, int32_t line, int32_t w,
    gm_plugin_lvgl_text_flag_t flags)
{
    (void)f; (void)ls; (void)line; (void)flags;
    int x=0,max=0,lines=1;
    for (;*s;++s) {
        if(*s=='\n') { if(x>max) max=x; x=0; ++lines; }
        else if (((unsigned char)*s&0xC0)!=0x80) {
            int advance=(unsigned char)*s>=0xE0 ? 34:17;
            if(x+advance>w) { if(x>max) max=x; x=0; ++lines; }
            x+=advance;
        }
    }
    out->x = x>max ? x:max; out->y=font_height(f)*lines;
}
static uint32_t now(void) { return clock_ms; }
static void exit_app(void) { ++exits; }
static void log_message(const char *format, ...) { (void)format; }
static gm_plugin_result_t get_info(gm_plugin_display_info_t *d)
{ memset(d,0,sizeof(*d)); d->width=display_w; d->height=display_h; return GM_PLUGIN_OK; }
static gm_plugin_result_t read_imu(gm_plugin_imu_sample_t *s)
{ ++imu_calls; memset(s,0,sizeof(*s)); s->pitch_degrees=pitch;
  s->gyro_raw[0]=gyro_x; s->gyro_raw[1]=gyro_y; return GM_PLUGIN_OK; }
static gm_plugin_result_t enable_imu(gm_plugin_imu_modes_t m)
{ ++imu_calls; return deny_imu && m ? GM_PLUGIN_ENOTSUP:GM_PLUGIN_OK; }
static gm_plugin_result_t language(char *s) { strcpy(s,locale); return GM_PLUGIN_OK; }
static void send_call(bool active)
{
    gm_plugin_system_event_t e={0}; e.struct_size=sizeof(e);
    e.type=GM_PLUGIN_SYSTEM_EVENT_CALL_UI; e.active=active;
    assert(notify_call); notify_call(notify_context,&e);
}
static gm_plugin_result_t subscribe(gm_plugin_system_event_callback_t cb, void *ctx)
{ notify_call=cb; notify_context=ctx; send_call(call_active); return GM_PLUGIN_OK; }
static void unsubscribe(void) { ++unsubscribed; notify_call=NULL; }
static const gm_plugin_system_events_extension_api_t events={subscribe,unsubscribe};
static const gm_plugin_libc_extension_api_t libc_api={
    .memcpy=memcpy,.memset=memset,.memmove=memmove,.memcmp=memcmp,.memchr=memchr,
    .strlen=strlen,.strcmp=strcmp,.strncmp=strncmp,.strcspn=strcspn,.snprintf=snprintf,
};
static gm_plugin_result_t discover(gm_plugin_extension_id_t id,const void **api)
{
    *api=NULL;
    if(id==GM_PLUGIN_EXTENSION_LIBC) *api=&libc_api;
    if(id==GM_PLUGIN_EXTENSION_SYSTEM_EVENTS && !old_host) *api=&events;
    return *api ? GM_PLUGIN_OK:GM_PLUGIN_ENOTSUP;
}
static gm_plugin_lvgl_api_t ui={
    .struct_size=sizeof(ui),.api_version=GM_PLUGIN_LVGL_API_MIN_VERSION,
    .root_get=root,.obj_create=create,.obj_clean=clean,
    .obj_set_pos=pos,.obj_set_size=size,.obj_align=align_obj,
    .obj_add_flag=add,.obj_clear_flag=clear,.style_set=style,
    .label_create=create,.label_set_text=text,.label_set_long_mode=mode,
    .line_create=create,.line_set_points=points,
    .font_get_line_height=font_height,.text_get_size=measure,
    .image_create=create_image,
};
static gm_plugin_descriptor_t plugin;
static void button(gm_plugin_button_t b,gm_plugin_button_action_t a)
{
    gm_plugin_event_t e={0}; e.type=GM_PLUGIN_EVENT_BUTTON;
    e.data.button.button=b; e.data.button.action=a;
    plugin.on_event(plugin.context,&e);
}
static void tick(uint32_t elapsed)
{ clock_ms+=elapsed; plugin.on_loop(plugin.context,elapsed); }
static bool hidden(gm_plugin_lvgl_obj_t *o)
{ return (o->flags&GM_PLUGIN_LVGL_FLAG_HIDDEN)!=0; }
static gm_plugin_lvgl_obj_t *find_text(const char *s)
{ for(unsigned i=1;i<count;++i) if(strcmp(objects[i].text,s)==0) return &objects[i]; return NULL; }

#ifdef TEST_BREAKOUT
static void assert_text_fits(gm_plugin_lvgl_obj_t *panel)
{
    int border=2*panel->style[GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH].num;
    int content_w=panel->w-panel->style[GM_PLUGIN_LVGL_STYLE_PAD_LEFT].num-
                  panel->style[GM_PLUGIN_LVGL_STYLE_PAD_RIGHT].num-border;
    int content_h=panel->h-panel->style[GM_PLUGIN_LVGL_STYLE_PAD_TOP].num-
                  panel->style[GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM].num-border;
    gm_plugin_lvgl_point_t rendered;
    measure(&rendered,panel->text,ui.font_default,0,0,content_w,
            GM_PLUGIN_LVGL_TEXT_FLAG_NONE);
    assert(!hidden(panel) && rendered.x<=content_w && rendered.y<=content_h);
}
#endif

static void audit_edge_cases(void)
{
#ifdef TEST_JET_RUNNER
    /* Native collision uses fractional coordinates, not displayed pixels. */
    obstacle_t obstacle={0}; obstacle.gap_top=100;
    game.player_y_q=(100-HIT_Y)*Q+Q/2;
    obstacle.x_q=(PLAYER_X+HIT_X-OBSTACLE_W)*Q+Q/2;
    obstacle.gap_top=126;
    assert(collision(&game,&obstacle));
    game.player_y_q=(100+GAP_H-HIT_Y-HIT_H)*Q+Q/2;
    obstacle.x_q=PLAYER_X*Q; obstacle.gap_top=100;
    assert(collision(&game,&obstacle));
    pitch=12; reset_game(&game); pitch=22; tick(50);
    assert(game.neutral_pitch==12 && game.player_y_q<125*Q);
    reset_game(&game); tick(40); tick(100);
    assert(game.obstacles[0].x_q==(650*Q-125*Q/10));
    pitch=0; reset_game(&game);
#elif defined(TEST_BREAKOUT)
    assert(game.ball->x==285 && game.ball->y==256);
    assert(game.paddle->x==250 && game.paddle->y==268);
    tick(50);
    assert(game.trails[0]->x==285 && game.trails[0]->y==256);
    /* Half a pixel of overlap is already a hit in firmware. */
    memset(game.alive,0,sizeof(game.alive)); game.alive[0][0]=1;
    game.ball_x=(game.brick_left-BALL_SIZE)*Q+Q/2;
    game.ball_y=BRICK_TOP*Q; game.ball_vx=100; game.ball_vy=-100;
    brick_collision(&game); assert(!game.alive[0][0]);
    assert(restart(&game)==GM_PLUGIN_OK);
    gyro_x=100; gyro_y=-100; tick(100);
    /* Native step=2: smoothing reaches target=20, then moves 40 pixels. */
    assert(game.paddle_x==290*Q);
    gyro_x=gyro_y=0; assert(restart(&game)==GM_PLUGIN_OK);
    unsigned old_exits=exits;
    game.paused=1; pitch=31; tick(50);
    for(unsigned i=0;i<80;++i) tick(50);
    assert(exits==old_exits+1);
    exits=old_exits; pitch=0;
    assert(restart(&game)==GM_PLUGIN_OK);
#else
    /* Masks transcribed from the reference's 7 x 4 x 4 x 4 matrices. */
    const uint16_t native_shapes[7][4]={
        {0x0F00,0x2222,0x0F00,0x4444},
        {0x8E00,0x6440,0x0E20,0x44C0},
        {0x2E00,0x4460,0x0E80,0xC440},
        {0x6600,0x6600,0x6600,0x6600},
        {0x6C00,0x4620,0x6C00,0x8C40},
        {0x4E00,0x4640,0x0E40,0x4C40},
        {0xC600,0x2640,0xC600,0x4C80},
    };
    assert(memcmp(shapes,native_shapes,sizeof(shapes))==0);
    clock_ms=1000; reset_game(&game); gyro_x=20; gyro_y=-20;
    tick(100); assert(game.piece.x==4);
    tick(100); assert(game.piece.x==4); /* Strictly >100ms, sampled at 100ms. */
    tick(100); assert(game.piece.x==5);
    gyro_x=gyro_y=0; reset_game(&game);
    memset(game.board[19],1,COLS); game.board[18][0]=1;
    game.score=65500; assert(begin_line_clear(&game));
    tick(200); assert(game.clear_animation_ms==100); /* One native timer tick. */
    for(unsigned i=0;i<5;++i) tick(100);
    assert(game.clearing_lines && game.shown[19][0]==0 && game.shown[19][4]==0xF0);
    tick(100); assert(!game.clearing_lines && game.score==65600);
    assert(game.board[19][0]==1 && game.board[19][1]==0);
    reset_game(&game);
    /* I rotation at the floor succeeds without an unintended downward step. */
    game.piece=(piece_t){0,1,3,16}; rotate(&game);
    assert(game.piece.rotation==2 && game.piece.y==16);
    memset(game.board,1,sizeof(game.board)); spawn(&game);
    assert(game.game_over && strcmp(game.info_label->text,game.strings->game_over)==0);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
    assert(!game.game_over && game.score==0);
    reset_game(&game);
#endif
}

static void assert_waiting_for_stop(unsigned expected_exits)
{
    unsigned char frozen_game[sizeof(game)];
    unsigned writes_before=ui_writes, imu_before=imu_calls;
    memcpy(frozen_game,&game,sizeof(game));
    /* Keep dispatching callbacks after app_exit, without delivering on_stop. */
    pitch=31; gyro_x=20; gyro_y=-20;
    for(unsigned i=0;i<25;++i) tick(100);
    assert(exits==expected_exits);
    assert(memcmp(frozen_game,&game,sizeof(game))==0);
    assert(ui_writes==writes_before && imu_calls==imu_before);

    button(GM_PLUGIN_BUTTON_BACK,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    button(GM_PLUGIN_BUTTON_UP,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    button(GM_PLUGIN_BUTTON_DOWN,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    button(GM_PLUGIN_BUTTON_LEFT,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    button(GM_PLUGIN_BUTTON_RIGHT,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_DOUBLE);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_VERY_LONG);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_RELEASE);
    pitch=0; gyro_x=gyro_y=0; tick(100);
    plugin.on_suspend(plugin.context);
    plugin.on_resume(plugin.context); tick(100);
    assert(exits==expected_exits);
    assert(memcmp(frozen_game,&game,sizeof(game))==0);
    assert(ui_writes==writes_before && imu_calls==imu_before);
}

static void audit_pending_exit(void)
{
#ifdef TEST_BREAKOUT
    const unsigned scenarios=8;
#else
    const unsigned scenarios=3;
#endif
    for(unsigned scenario=0;scenario<scenarios;++scenario) {
        unsigned old_exits=exits;
        pitch=0; gyro_x=gyro_y=0;
        assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
#ifdef TEST_JET_RUNNER
        if(scenario==1) {
            button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
            tick(801); /* BACK while the cancellable hold overlay is visible. */
        } else if(scenario==2) {
            finish(&game); /* Queued clicks must not restart after BACK. */
        }
        button(GM_PLUGIN_BUTTON_BACK,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
#elif defined(TEST_BREAKOUT)
        /* BACK/head-exit while running, paused, or over; BACK during a hold;
         * and an allocation failure when restarting after game over. */
        if(scenario<6 && scenario%3==1)
            button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
        if((scenario<6 && scenario%3==2) || scenario==7)
            finish_game(&game,0);
        if(scenario==6) {
            button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
            tick(801);
        }
        if(scenario==7) {
            fail_at=4;
            button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
            fail_at=0;
        } else if(scenario>=3 && scenario<6) {
            pitch=31; tick(50);
            for(unsigned i=0;i<80;++i) tick(50);
        } else {
            button(GM_PLUGIN_BUTTON_BACK,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
        }
#else
        pitch=31; tick(100);
        for(unsigned second=0;second<3;++second) {
            for(unsigned i=0;i<10;++i) tick(100);
            assert(exits==old_exits); /* Preserve the native 3, 2, 1 cadence. */
        }
        assert(game.exit_seconds==0 && !hidden(game.exit_label));
        for(unsigned i=0;i<9;++i) tick(100);
        if(scenario==1) {
            memset(game.board[ROWS-1],1,COLS);
            assert(begin_line_clear(&game));
        } else if(scenario==2) {
            memset(game.board,1,sizeof(game.board)); spawn(&game);
            assert(game.game_over);
        }
        tick(100);
#endif
        assert(exits==old_exits+1);
        assert_waiting_for_stop(old_exits+1);
        plugin.on_stop(plugin.context);
        assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
#ifdef TEST_JET_RUNNER
        int32_t before=game.obstacles[0].x_q;
        tick(100); assert(game.obstacles[0].x_q<before);
        button(GM_PLUGIN_BUTTON_UP,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
        assert(game.accessory_offset_y==-20);
#elif defined(TEST_BREAKOUT)
        int32_t before=game.ball_x;
        tick(50); assert(game.ball_x!=before);
        button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
        assert(game.paused);
#else
        int8_t before=game.piece.y;
        for(unsigned i=0;i<3;++i) tick(100);
        assert(game.piece.y==before+1);
        button(GM_PLUGIN_BUTTON_DOWN,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
        assert(game.piece.y==before+2);
#endif
        assert(exits==old_exits+1);
        plugin.on_stop(plugin.context);
    }
}

#ifdef TEST_TETRIS
static void assert_tetris_cadence(const uint32_t *intervals, unsigned count)
{
    uint32_t elapsed=0;
    reset_game(&game);
    int8_t initial_y=game.piece.y;
    for(unsigned i=0;i<50;++i) {
        elapsed+=intervals[i%count];
        tick(intervals[i%count]);
        /* Gravity follows elapsed time even when callbacks do not divide 100ms. */
        assert(game.piece.y==initial_y+(int8_t)(elapsed/NORMAL_DROP_MS));
        assert(game.drop_accumulator==(elapsed/FRAME_MS*FRAME_MS)%NORMAL_DROP_MS);
    }
}

static void audit_tetris_cadence(void)
{
    const uint32_t fixed[]={16,33,60};
    const uint32_t jitter[]={16,33,60,17,74};
    pitch=0; gyro_x=gyro_y=0;
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
    for(unsigned i=0;i<sizeof(fixed)/sizeof(*fixed);++i)
        assert_tetris_cadence(&fixed[i],1);
    assert_tetris_cadence(jitter,sizeof(jitter)/sizeof(*jitter));

    reset_game(&game);
    int8_t initial_y=game.piece.y;
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    for(unsigned i=0;i<10;++i) tick(60);
    assert(game.piece.y==initial_y+6); /* Six fast-drop steps in 600ms. */

    reset_game(&game);
    memset(game.board[ROWS-1],1,COLS);
    assert(begin_line_clear(&game));
    for(unsigned i=0;i<21;++i) tick(33);
    assert(game.clearing_lines && game.clear_animation_ms==600);
    tick(33); assert(!game.clearing_lines && game.score==100);

    reset_game(&game);
    tick(60); tick(2000); /* A stalled callback still advances only one step. */
    assert(game.drop_accumulator==100);
    tick(40); assert(game.drop_accumulator==200); /* Preserve the 60ms remainder. */
    tick(1); assert(game.drop_accumulator==200); /* No queued catch-up steps. */
    plugin.on_stop(plugin.context);
}
#endif

int main(void)
{
    gm_plugin_host_api_t host={0};
    host.struct_size=sizeof(host); host.abi_version=GM_PLUGIN_ABI_MIN_VERSION;
    host.capabilities=~(gm_plugin_capabilities_t)0; host.graphics.lvgl=&ui;
    host.display_get_info=get_info; host.imu_enable=enable_imu; host.imu_read=read_imu;
    host.monotonic_ms=now; host.app_exit=exit_app; host.locale_get=language;
    host.extension_get=discover; host.log=log_message;
    plugin.struct_size=sizeof(plugin);
    assert(gm_plugin_entry(&host,&plugin)==GM_PLUGIN_OK);
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
    audit_edge_cases();
#ifdef TEST_JET_RUNNER
    assert(game.board->x==10 && game.board->y==38 && game.board->w==580 && game.board->h==280);
    assert(game.end_label->w==260);
    assert(find_text("抬头上升 · 低头下降 · 长按退出"));
    assert(game.score_label->alignment==GM_PLUGIN_LVGL_ALIGN_RIGHT_MID);
    tick(50); /* Calibrate before accessory movement. */
    int32_t before=game.player_y_q;
    button(GM_PLUGIN_BUTTON_UP,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    assert(game.accessory_offset_y==-20 && game.player_y_q==before);
    tick(50); assert(game.player_y_q<before);
#elif defined(TEST_BREAKOUT)
    assert(game.board->x==10 && game.board->y==40 && game.board->w==580 && game.board->h==294);
    assert(game.brick_width==28 && game.brick_left==66);
    assert(game.control_label->x==140 && game.control_label->w==386);
    assert(game.score_label->w==0 && game.score_label->h==0); /* Native content sizing. */
    assert(game.timer_label->x==526 && game.timer_label->w==64);
    assert(find_text("长按/抬头3秒退出"));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
    assert(game.paused && !hidden(game.message_label) && game.message_label->w<380);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
    assert(!game.paused && hidden(game.message_label));
#else
    assert(game.board_obj->x==414 && game.board_obj->y==5);
    assert(game.board_obj->w==170 && game.board_obj->h==340);
    assert(game.preview_obj->x==340 && game.preview_obj->y==8);
    assert(game.preview_obj->w==60 && game.preview_cells[0][0]->w==9);
    assert(game.info_label->w==388 && game.info_label->y==72);
    assert(find_text("操作说明:\n头向左：左移\n头向右：右移\n单击：旋转\n长按：快速下落\n抬头3秒退出"));
    pitch=31; tick(100); assert(hidden(game.preview_obj));
    for(unsigned i=0;i<10;++i) tick(100);
    assert(!hidden(game.exit_label) && strcmp(game.exit_label->text,"3s后退出")==0);
    pitch=0; tick(100); assert(!hidden(game.preview_obj) && hidden(game.exit_label));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG); assert(game.fast_drop);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_RELEASE); assert(!game.fast_drop);
#endif

#if defined(TEST_JET_RUNNER) || defined(TEST_BREAKOUT)
    assert(hidden(game.exit_ui.overlay) && find_text("按住2s结束"));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    assert(game.exit_ui.active && !hidden(game.exit_ui.overlay) && hidden(game.exit_ui.ring) && hidden(game.exit_ui.ring_full));
    tick(801); assert(game.exit_ui.step==1 && !hidden(game.exit_ui.ring) && hidden(game.exit_ui.ring_full));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_VERY_LONG);
    tick(799); assert(game.exit_ui.step==2 && hidden(game.exit_ui.ring) && !hidden(game.exit_ui.ring_full));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_RELEASE);
    assert(!game.exit_ui.active && hidden(game.exit_ui.overlay));
    tick(500); assert(exits==0);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    tick(2001); assert(exits==1); /* Real time, even after a slow frame. */
    /* app_exit only queues a Host stop. The game must stay covered until it
     * arrives, including queued release/click events and extra loop calls. */
    assert(!hidden(game.exit_ui.overlay));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_RELEASE);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    button(GM_PLUGIN_BUTTON_BACK,GM_PLUGIN_BUTTON_ACTION_TRIGGER);
    tick(100);
    assert(exits==1 && !hidden(game.exit_ui.overlay));
    plugin.on_suspend(plugin.context);
    assert(!hidden(game.exit_ui.overlay));
    plugin.on_resume(plugin.context); tick(2100);
    assert(exits==1 && !hidden(game.exit_ui.overlay));
    plugin.on_stop(plugin.context);
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
    assert(hidden(game.exit_ui.overlay));
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    plugin.on_suspend(plugin.context); assert(!game.exit_ui.active);
    plugin.on_resume(plugin.context); tick(2100); assert(exits==1);
#else
    button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_LONG);
    plugin.on_suspend(plugin.context); assert(!game.fast_drop);
    plugin.on_resume(plugin.context);
#endif
    send_call(true); assert(hidden(root()));
    tick(50); assert(hidden(root()));
    send_call(false); assert(!hidden(root()));
    plugin.on_stop(plugin.context); assert(notify_call==NULL);
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
#if defined(TEST_JET_RUNNER) || defined(TEST_BREAKOUT)
    assert(!game.exit_ui.active && hidden(game.exit_ui.overlay));
    button(GM_PLUGIN_BUTTON_BACK,GM_PLUGIN_BUTTON_ACTION_TRIGGER); assert(exits==2);
#endif
    plugin.on_stop(plugin.context);
    unsigned previous=unsubscribed;
    deny_imu=true; assert(plugin.on_start(plugin.context)!=GM_PLUGIN_OK);
    assert(unsubscribed>previous && notify_call==NULL); deny_imu=false;
    fail_at=4; assert(plugin.on_start(plugin.context)==GM_PLUGIN_ENOMEM);
    assert(count==1 && notify_call==NULL); fail_at=0;
    display_w=100; assert(plugin.on_start(plugin.context)==GM_PLUGIN_ENOTSUP);
    assert(notify_call==NULL); display_w=600;
    /* Existing firmware without system events must still work. */
    old_host=true; locale="unknown";
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
    plugin.on_stop(plugin.context);
    call_active=true; old_host=false; locale="en";
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK); assert(hidden(root()));
    plugin.on_stop(plugin.context); assert(!hidden(root()));
    call_active=false;
    ui.api_version=GM_PLUGIN_LVGL_API_VERSION;
    images=0;
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
#if defined(TEST_JET_RUNNER) || defined(TEST_BREAKOUT)
    assert(images==3); /* Two antialiased ring states and the original icon. */
#endif
    unsigned object_count=count;
    plugin.on_stop(plugin.context);
    /* Every partial creation path must reject cleanly, including nested UI
     * children and the new exit overlay, without leaking a subscription. */
    for(unsigned i=1;i<object_count;++i) {
        fail_at=i;
        assert(plugin.on_start(plugin.context)==GM_PLUGIN_ENOMEM);
        assert(count==1 && notify_call==NULL);
    }
    fail_at=0;
#if defined(TEST_JET_RUNNER) || defined(TEST_BREAKOUT)
    ui.api_version=GM_PLUGIN_LVGL_API_MIN_VERSION;
    for(unsigned i=40;i<=400;i+=40) {
        fail_at=i;
        assert(plugin.on_start(plugin.context)==GM_PLUGIN_ENOMEM);
        assert(count==1 && notify_call==NULL);
    }
    fail_at=0;
    ui.api_version=GM_PLUGIN_LVGL_API_VERSION;
#endif
#ifdef TEST_TETRIS
    ui.font_get_line_height=NULL;
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_ENOTSUP && notify_call==NULL);
    ui.font_get_line_height=font_height;
#else
    ui.line_create=NULL;
    ui.line_set_points=NULL;
    assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK); /* Exit no longer needs polylines. */
    plugin.on_stop(plugin.context);
    ui.line_create=create;
    ui.line_set_points=points;
#endif
    const char *languages[]={"zh-CN","en","ja","ko","de","fr","es","it","ru","pt","zh-TW"};
    for(unsigned i=0;i<sizeof(languages)/sizeof(*languages);++i) {
        locale=languages[i];
        assert(plugin.on_start(plugin.context)==GM_PLUGIN_OK);
#ifdef TEST_BREAKOUT
        /* LVGL subtracts both padding AND borders from label content bounds.
         * Re-measure inside those bounds to catch accidental wrapping/clipping. */
        finish_game(&game, 0);
        assert_text_fits(game.message_label);
        finish_game(&game,1); assert_text_fits(game.message_label);
        button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
        assert(!game.ended && game.score==0 && hidden(game.message_label));
        button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_SINGLE);
        assert_text_fits(game.message_label);
#elif defined(TEST_TETRIS)
        show_exit_countdown(&game,3);
        gm_plugin_lvgl_point_t badge_text;
        measure(&badge_text,game.exit_label->text,ui.font_default,0,0,
                game.exit_label->w-18,GM_PLUGIN_LVGL_TEXT_FLAG_NONE);
        assert(badge_text.y==34 && game.exit_label->h==34); /* Native fixed-height badge; no wrap. */
        assert(game.preview_obj>game.info_label); /* Same parent, later paint order. */
#else
        game.score=65536; finish(&game);
        assert(strstr(game.end_label->text,"65536"));
        assert(strstr(game.end_label->text,game.strings->restart));
        assert(!hidden(game.end_label) && game.end_label->w==260 && game.end_label->h==0);
        button(GM_PLUGIN_BUTTON_PRIMARY,GM_PLUGIN_BUTTON_ACTION_DOUBLE);
        assert(!game.game_over && game.score==0 && hidden(game.end_label));
#endif
        plugin.on_stop(plugin.context);
    }
    audit_pending_exit();
#ifdef TEST_TETRIS
    audit_tetris_cadence();
#endif
    puts("Firmware game layout, exit, input, failure cleanup and lifecycle passed");
    return 0;
}
