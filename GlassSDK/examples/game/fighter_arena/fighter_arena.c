#include "gm_plugin.h"
#include "gm_plugin_libc.h"
#include "zen_combat_sprites.h"
#undef PIXEL_FIGHTER_SPRITES_H
#define pf_sprite_offsets rival_sprite_offsets
#define pf_sprite_data rival_sprite_data
#include "rival_combat_sprites.h"
#undef pf_sprite_offsets
#undef pf_sprite_data
#undef PIXEL_FIGHTER_SPRITES_H
#undef PF_SPRITE_WIDTH
#undef PF_SPRITE_HEIGHT
#undef PF_SPRITE_FRAME_COUNT
#define pf_sprite_offsets zen_hurt_sprite_offsets
#define pf_sprite_data zen_hurt_sprite_data
#include "zen_hurt_sprites.h"
#undef pf_sprite_offsets
#undef pf_sprite_data
#undef PIXEL_FIGHTER_SPRITES_H
#undef PF_SPRITE_WIDTH
#undef PF_SPRITE_HEIGHT
#undef PF_SPRITE_FRAME_COUNT
#define pf_sprite_offsets rival_hurt_sprite_offsets
#define pf_sprite_data rival_hurt_sprite_data
#include "rival_hurt_sprites.h"
#undef pf_sprite_offsets
#undef pf_sprite_data

#define INPUT_CHANNEL UINT16_C(0x4647)
#define INPUT_VERSION 2U
#define INPUT_SIZE 4U
#define INPUT_TIMEOUT_MS 300U
#define EVENT_CHANNEL UINT16_C(0x4648)
#define EVENT_VERSION 1U

#define KEY_LEFT  UINT16_C(1)
#define KEY_RIGHT UINT16_C(2)
#define KEY_UP    UINT16_C(4)
#define KEY_DOWN  UINT16_C(8)
#define KEY_LIGHT UINT16_C(16)
#define KEY_HEAVY UINT16_C(32)
#define KEY_KICK  UINT16_C(64)
#define KEY_BLOCK UINT16_C(128)
#define KEY_START UINT16_C(256)
#define KEY_PAUSE UINT16_C(512)
#define KEY_UPPERCUT UINT16_C(1024)
#define KEY_SWEEP UINT16_C(2048)

#define FRAME_MS 50U
#define MAX_CATCHUP_MS 150U
#define ROUND_MS 60000U
#define INTRO_MS 1400U
#define ROUND_OVER_MS 1800U
#define COMBO_WINDOW_MS 750U
#define FIGHTER_W 36
#define FIGHTER_H 72
#define ARENA_MARGIN 10
#define MIN_WIDTH 280
#define MIN_HEIGHT 170
#define CHARACTER_COUNT 2U
#define PROJECTILE_COUNT 3U
#define GUARD_RECOVERY_DELAY_MS 1600U
#define LANDING_RECOVERY_MS 150U
#define TURN_DELAY_MS 100U
#define WALK_FRAME_MS 100U
#define EXIT_HOLD_MS 2000U

#define SCREEN_TITLE 0U
#define SCREEN_DIFFICULTY 1U
#define SCREEN_INTRO 3U
#define SCREEN_FIGHT 4U
#define SCREEN_ROUND_OVER 5U
#define SCREEN_ENDING 6U
#define SCREEN_GAME_OVER 7U

#define PAUSE_NONE 0U
#define PAUSE_REMOTE 1U
#define PAUSE_INPUT_LOST 2U
#define PAUSE_DISCONNECTED 3U

#define ATTACK_NONE 0U
#define ATTACK_LIGHT 1U
#define ATTACK_HEAVY 2U
#define ATTACK_KICK 3U
#define ATTACK_SPECIAL 4U
#define ATTACK_COMBO 5U
#define ATTACK_SWEEP 6U

#define SPRITE_GROUND_ROWS 95U

#define ATTACK_LEVEL_HIGH 0U
#define ATTACK_LEVEL_LOW 1U
#define ATTACK_LEVEL_OVERHEAD 2U
#define ATTACK_LEVEL_MID 3U

#define EVENT_HIT 1U
#define EVENT_BLOCK 2U
#define EVENT_GUARD_BREAK 3U
#define EVENT_SPECIAL_LAUNCH 4U
#define EVENT_ROUND_END 5U
#define EVENT_ATTACK 6U
#define EVENT_JUMP 7U
#define EVENT_ROUND_START 8U
#define EVENT_MENU 9U
#define EVENT_KO 10U
#define EVENT_MUSIC 11U

#define MUSIC_TITLE 0U
#define MUSIC_SELECT 1U
#define MUSIC_FIGHT 2U
#define MUSIC_VICTORY 3U
#define MUSIC_DEFEAT 4U

#define HURT_NONE 0U
#define HURT_LIGHT 1U
#define HURT_HEAVY 2U
#define HURT_KNOCKDOWN 3U
#define HURT_GUARD_BREAK 4U
#define HURT_KO 5U
#define HURT_KICK 6U

#define IMPACT_HIT 0U
#define IMPACT_BLOCK 1U
#define IMPACT_GUARD_BREAK 2U

#define FIGHT_EVENT_QUEUE_SIZE 12U
#define FIGHT_EVENT_MAX_ATTEMPTS 6U

typedef struct {
    const char *name;
    uint16_t health;
    uint8_t speed;
    uint8_t damage_percent;
    uint8_t reach_percent;
    uint8_t shade;
} character_t;

typedef struct {
    const char *name;
    uint16_t reaction_ms;
    uint16_t decision_ms;
    uint16_t retreat_ms;
    uint8_t block_percent;
    uint8_t special_percent;
    uint8_t preferred_distance;
} difficulty_t;

typedef struct {
    uint16_t damage;
    uint16_t chip_damage;
    uint16_t startup_ms;
    uint16_t active_end_ms;
    uint16_t duration_ms;
    uint16_t hit_stun_ms;
    uint16_t block_stun_ms;
    uint16_t reach;
    uint8_t knockback;
    uint8_t guard_damage;
    uint8_t energy_cost;
    uint8_t attack_level;
    bool strong;
} move_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t velocity_y;
    uint16_t health;
    uint16_t energy;
    uint16_t guard;
    uint16_t guard_recovery_ms;
    uint16_t attack_ms;
    uint16_t hurt_ms;
    uint16_t hurt_total_ms;
    uint16_t block_stun_ms;
    uint16_t landing_ms;
    uint16_t turn_ms;
    uint16_t walk_anim_ms;
    uint8_t character;
    uint8_t attack;
    uint8_t queued_attack;
    uint8_t hurt_kind;
    bool facing_right;
    bool crouching;
    bool blocking;
    bool hurt_right;
    bool hit_landed;
    bool special_spawned;
    bool walk_phase;
    bool moving;
    bool turning;
} fighter_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t velocity_x;
    uint16_t age_ms;
    uint8_t owner;
    uint8_t shade;
    bool active;
} projectile_t;

typedef struct {
    uint8_t event;
    uint8_t value;
    uint8_t attempts;
} fight_event_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_libc_extension_api_t *libc;
    fighter_t player;
    fighter_t cpu;
    projectile_t projectiles[PROJECTILE_COUNT];
    fight_event_t pending_events[FIGHT_EVENT_QUEUE_SIZE];
    uint16_t width;
    uint16_t height;
    int16_t ground_y;
    uint8_t scale;
    uint16_t input;
    uint16_t previous_input;
    uint32_t input_last_ms;
    uint32_t frame_accumulator;
    uint32_t exit_hold_ms;
    uint32_t round_left_ms;
    uint32_t random_state;
    uint16_t screen_ms;
    uint16_t hit_stop_ms;
    uint16_t impact_ms;
    uint16_t combo_ms;
    uint16_t cpu_think_ms;
    uint16_t cpu_reaction_ms;
    uint16_t cpu_retreat_ms;
    int16_t impact_x;
    int16_t impact_y;
    uint8_t screen;
    uint8_t selected_character;
    uint8_t cpu_character;
    uint8_t player_rounds;
    uint8_t cpu_rounds;
    uint8_t combo;
    uint8_t max_combo;
    uint8_t round_result;
    uint8_t difficulty;
    uint8_t event_sequence;
    uint8_t event_head;
    uint8_t event_count;
    uint8_t cpu_observed_attack;
    uint8_t pause_reason;
    uint8_t impact_kind;
    uint8_t rush_owner;
    bool input_active;
    bool holding_exit;
    bool impact_strong;
    bool cpu_defense_decided;
} game_t;

static const character_t characters[CHARACTER_COUNT] = {
    {"ZEN",   100U, 5U, 100U, 100U, 15U},
    {"RIVAL", 105U, 4U, 100U, 102U, 12U}
};

static const difficulty_t difficulties[] = {
    {"EASY",   250U, 900U, 400U, 15U, 10U, 58U},
    {"NORMAL", 100U, 650U, 300U, 30U, 25U, 52U},
    {"HARD",    50U, 450U, 200U, 45U, 35U, 48U}
};

static const move_t moves[] = {
    {0U,  0U,   0U,   0U,   0U,   0U,   0U,  0U,  0U,  0U,  0U,
     ATTACK_LEVEL_MID, false},
    {4U,  0U, 100U, 200U, 350U, 220U, 100U, 46U,  6U, 16U,  0U,
     ATTACK_LEVEL_HIGH, false},
    {7U,  1U, 150U, 300U, 600U, 300U, 180U, 54U, 10U, 34U,  0U,
     ATTACK_LEVEL_OVERHEAD, true},
    {3U,  0U, 100U, 100U, 400U, 180U, 100U, 38U,  4U, 14U,  0U,
     ATTACK_LEVEL_LOW, false},
    {9U,  2U, 150U, 400U, 700U, 350U, 220U, 62U, 12U, 42U, 50U,
     ATTACK_LEVEL_MID, true},
    {24U, 3U, 200U, 850U, 1000U, 380U, 180U, 54U, 12U, 30U, 35U,
     ATTACK_LEVEL_MID, true},
    {5U,  1U, 200U, 200U, 650U, 300U, 180U, 44U,  9U, 28U,  0U,
     ATTACK_LEVEL_HIGH, true}
};

static const move_t combo_hits[] = {
    {1U, 0U, 200U, 250U, 1000U, 180U, 80U, 42U, 0U, 6U, 0U,
     ATTACK_LEVEL_MID, false},
    {2U, 0U, 350U, 400U, 1000U, 350U, 100U, 46U, 0U, 8U, 0U,
     ATTACK_LEVEL_MID, false},
    {1U, 0U, 450U, 500U, 1000U, 250U, 80U, 44U, 0U, 6U, 0U,
     ATTACK_LEVEL_MID, false},
    {2U, 0U, 550U, 600U, 1000U, 250U, 80U, 44U, 0U, 6U, 0U,
     ATTACK_LEVEL_MID, false},
    {2U, 0U, 650U, 700U, 1000U, 250U, 80U, 44U, 0U, 6U, 0U,
     ATTACK_LEVEL_MID, false},
    {4U, 1U, 800U, 850U, 1000U, 500U, 180U, 54U, 12U, 18U, 0U,
     ATTACK_LEVEL_MID, true}
};

static game_t game;

static int16_t absolute(int16_t value)
{
    return value < 0 ? (int16_t)-value : value;
}

static uint32_t random_next(game_t *self)
{
    self->random_state = self->random_state * UINT32_C(1664525) +
                         UINT32_C(1013904223);
    return self->random_state;
}

static uint16_t clamp_add(uint16_t value, uint16_t amount, uint16_t maximum)
{
    return value > maximum - amount ? maximum : (uint16_t)(value + amount);
}

static void flush_fight_event(game_t *self)
{
    fight_event_t *pending;
    uint8_t payload[4];
    gm_plugin_result_t result;
    if (self->host->bt_send == 0 || self->event_count == 0U) return;
    pending = &self->pending_events[self->event_head];
    payload[0] = EVENT_VERSION;
    payload[1] = self->event_sequence;
    payload[2] = pending->event;
    payload[3] = pending->value;
    result = self->host->bt_send(EVENT_CHANNEL, payload, sizeof(payload));
    if (result == GM_PLUGIN_OK || ++pending->attempts >= FIGHT_EVENT_MAX_ATTEMPTS) {
        if (result == GM_PLUGIN_OK) ++self->event_sequence;
        self->event_head = (uint8_t)((self->event_head + 1U) %
                                     FIGHT_EVENT_QUEUE_SIZE);
        --self->event_count;
    }
}

static void send_fight_event(game_t *self, uint8_t event, uint8_t value)
{
    uint8_t tail;
    if (self->event_count >= FIGHT_EVENT_QUEUE_SIZE) {
        self->event_head = (uint8_t)((self->event_head + 1U) %
                                     FIGHT_EVENT_QUEUE_SIZE);
        --self->event_count;
    }
    tail = (uint8_t)((self->event_head + self->event_count) %
                     FIGHT_EVENT_QUEUE_SIZE);
    self->pending_events[tail].event = event;
    self->pending_events[tail].value = value;
    self->pending_events[tail].attempts = 0U;
    ++self->event_count;
    flush_fight_event(self);
}

static void pixel(gm_plugin_framebuffer_surface_t *surface,
                  int16_t x, int16_t y, uint8_t gray)
{
    uint8_t *target;
    if (x < 0 || y < (int16_t)surface->y ||
        x >= (int16_t)surface->width ||
        y >= (int16_t)(surface->y + surface->height)) return;
    target = surface->pixels +
             (uint32_t)(y - (int16_t)surface->y) * surface->stride +
             ((uint16_t)x >> 1);
    if (((uint16_t)x & 1U) == 0U)
        *target = (uint8_t)((*target & 0x0FU) | ((gray & 0x0FU) << 4));
    else
        *target = (uint8_t)((*target & 0xF0U) | (gray & 0x0FU));
}

static void rectangle(gm_plugin_framebuffer_surface_t *surface,
                      int16_t x, int16_t y, int16_t width, int16_t height,
                      uint8_t gray)
{
    int16_t end_x = (int16_t)(x + width);
    int16_t end_y = (int16_t)(y + height);
    int16_t row;
    uint8_t packed = (uint8_t)(((gray & 0x0FU) << 4) | (gray & 0x0FU));
    if (x < 0) x = 0;
    if (y < (int16_t)surface->y) y = (int16_t)surface->y;
    if (end_x > (int16_t)surface->width) end_x = (int16_t)surface->width;
    if (end_y > (int16_t)(surface->y + surface->height))
        end_y = (int16_t)(surface->y + surface->height);
    if (x >= end_x || y >= end_y) return;
    for (row = y; row < end_y; ++row) {
        int16_t column = x;
        uint8_t *target;
        if ((column & 1) != 0) pixel(surface, column++, row, gray);
        target = surface->pixels +
                 (uint32_t)(row - (int16_t)surface->y) * surface->stride +
                 ((uint16_t)column >> 1);
        game.libc->memset(target, packed,
                              (size_t)(end_x - column) >> 1);
        column = (int16_t)(column + ((end_x - column) & ~1));
        if (column < end_x) pixel(surface, column, row, gray);
    }
}

static void outline_rectangle(gm_plugin_framebuffer_surface_t *surface,
                              int16_t x, int16_t y, int16_t width,
                              int16_t height, int16_t thickness, uint8_t gray)
{
    rectangle(surface, x, y, width, thickness, gray);
    rectangle(surface, x, (int16_t)(y + height - thickness),
              width, thickness, gray);
    rectangle(surface, x, y, thickness, height, gray);
    rectangle(surface, (int16_t)(x + width - thickness), y,
              thickness, height, gray);
}

static void circle(gm_plugin_framebuffer_surface_t *surface,
                   int16_t center_x, int16_t center_y, int16_t radius,
                   uint8_t gray)
{
    int16_t start_y = (int16_t)-radius;
    int16_t end_y = radius;
    int16_t y;
    int16_t x;
    int32_t limit = (int32_t)radius * radius;
    if (center_y + start_y < (int16_t)surface->y)
        start_y = (int16_t)((int16_t)surface->y - center_y);
    if (center_y + end_y >= (int16_t)(surface->y + surface->height))
        end_y = (int16_t)(surface->y + surface->height - 1U - center_y);
    for (y = start_y; y <= end_y; ++y)
        for (x = (int16_t)-radius; x <= radius; ++x)
            if ((int32_t)x * x + (int32_t)y * y <= limit)
                pixel(surface, (int16_t)(center_x + x),
                      (int16_t)(center_y + y), gray);
}

static void line(gm_plugin_framebuffer_surface_t *surface,
                 int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 uint8_t width, uint8_t gray)
{
    int16_t dx = absolute((int16_t)(x1 - x0));
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = (int16_t)-absolute((int16_t)(y1 - y0));
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t error = (int16_t)(dx + dy);
    int16_t half = (int16_t)(width / 2U);
    for (;;) {
        int16_t twice_error;
        rectangle(surface, (int16_t)(x0 - half), (int16_t)(y0 - half),
                  width, width, gray);
        if (x0 == x1 && y0 == y1) break;
        twice_error = (int16_t)(2 * error);
        if (twice_error >= dy) {
            error = (int16_t)(error + dy);
            x0 = (int16_t)(x0 + sx);
        }
        if (twice_error <= dx) {
            error = (int16_t)(error + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}

#define GLYPH(a,b,c,d,e) ((uint16_t)((a) << 12 | (b) << 9 | (c) << 6 | \
                                      (d) << 3 | (e)))

static uint16_t glyph(char character)
{
    switch (character) {
    case '0': return GLYPH(7,5,5,5,7); case '1': return GLYPH(2,6,2,2,7);
    case '2': return GLYPH(7,1,7,4,7); case '3': return GLYPH(7,1,7,1,7);
    case '4': return GLYPH(5,5,7,1,1); case '5': return GLYPH(7,4,7,1,7);
    case '6': return GLYPH(7,4,7,5,7); case '7': return GLYPH(7,1,2,2,2);
    case '8': return GLYPH(7,5,7,5,7); case '9': return GLYPH(7,5,7,1,7);
    case 'A': return GLYPH(2,5,7,5,5); case 'B': return GLYPH(6,5,6,5,6);
    case 'C': return GLYPH(3,4,4,4,3); case 'D': return GLYPH(6,5,5,5,6);
    case 'E': return GLYPH(7,4,6,4,7); case 'F': return GLYPH(7,4,6,4,4);
    case 'G': return GLYPH(3,4,5,5,3); case 'H': return GLYPH(5,5,7,5,5);
    case 'I': return GLYPH(7,2,2,2,7); case 'J': return GLYPH(1,1,1,5,2);
    case 'K': return GLYPH(5,5,6,5,5); case 'L': return GLYPH(4,4,4,4,7);
    case 'M': return GLYPH(5,7,7,5,5); case 'N': return GLYPH(5,7,7,7,5);
    case 'O': return GLYPH(2,5,5,5,2); case 'P': return GLYPH(6,5,6,4,4);
    case 'Q': return GLYPH(2,5,5,3,1); case 'R': return GLYPH(6,5,6,5,5);
    case 'S': return GLYPH(3,4,2,1,6); case 'T': return GLYPH(7,2,2,2,2);
    case 'U': return GLYPH(5,5,5,5,7); case 'V': return GLYPH(5,5,5,5,2);
    case 'W': return GLYPH(5,7,7,7,5); case 'X': return GLYPH(5,5,2,5,5);
    case 'Y': return GLYPH(5,5,2,2,2); case 'Z': return GLYPH(7,1,2,4,7);
    case '!': return GLYPH(2,2,2,0,2); case ':': return GLYPH(0,2,0,2,0);
    case '>': return GLYPH(4,2,1,2,4); case '<': return GLYPH(1,2,4,2,1);
    case '-': return GLYPH(0,0,7,0,0); default: return 0;
    }
}

static void text(gm_plugin_framebuffer_surface_t *surface, int16_t x,
                 int16_t y, const char *value, uint8_t scale, uint8_t gray)
{
    while (*value != '\0') {
        uint16_t bits = glyph(*value++);
        uint8_t row;
        uint8_t column;
        for (row = 0; row < 5U; ++row)
            for (column = 0; column < 3U; ++column)
                if ((bits & (UINT16_C(1) << (14U - row * 3U - column))) != 0U)
                    rectangle(surface, (int16_t)(x + column * scale),
                              (int16_t)(y + row * scale), scale, scale, gray);
        x = (int16_t)(x + 4 * scale);
    }
}

static void centered_text(gm_plugin_framebuffer_surface_t *surface,
                          uint16_t width, int16_t y, const char *value,
                          uint8_t scale, uint8_t gray)
{
    int16_t x = (int16_t)((width - game.libc->strlen(value) * 4U * scale) /
                          2U);
    text(surface, x, y, value, scale, gray);
}

static void centered_text_panel(gm_plugin_framebuffer_surface_t *surface,
                                uint16_t width, int16_t y,
                                const char *value, uint8_t scale,
                                uint8_t gray)
{
    int16_t text_width = (int16_t)(game.libc->strlen(value) * 4U * scale);
    int16_t x = (int16_t)((width - text_width) / 2U);
    rectangle(surface, (int16_t)(x - 3 * scale),
              (int16_t)(y - 2 * scale),
              (int16_t)(text_width + 6 * scale),
              (int16_t)(9 * scale), 0U);
    text(surface, x, y, value, scale, gray);
}

static void draw_exit_countdown(game_t *self,
                                gm_plugin_framebuffer_surface_t *surface)
{
    static const int8_t ring_x[] = {
        0, 5, 9, 10, 9, 5, 0, -5, -9, -10, -9, -5, 0
    };
    static const int8_t ring_y[] = {
        -10, -9, -5, 0, 5, 9, 10, 9, 5, 0, -5, -9, -10
    };
    int16_t center_x = (int16_t)(self->width / 2U);
    int16_t center_y = (int16_t)(self->height / 2U);
    int16_t radius = (int16_t)(20U + self->scale * 4U);
    uint8_t thickness = (uint8_t)(self->scale * 2U);
    uint8_t completed = (uint8_t)(self->exit_hold_ms * 12U /
                                  EXIT_HOLD_MS);
    uint8_t index;
    uint32_t remaining = (EXIT_HOLD_MS - self->exit_hold_ms + 999U) / 1000U;
    char seconds[2] = {(char)('0' + remaining), '\0'};
    circle(surface, center_x, center_y, radius, 4U);
    circle(surface, center_x, center_y,
           (int16_t)(radius - thickness), 0U);
    if (completed > 12U) completed = 12U;
    for (index = 0; index < completed; ++index) {
        line(surface,
             (int16_t)(center_x + ring_x[index] * radius / 10),
             (int16_t)(center_y + ring_y[index] * radius / 10),
             (int16_t)(center_x + ring_x[index + 1U] * radius / 10),
             (int16_t)(center_y + ring_y[index + 1U] * radius / 10),
             thickness, 15U);
    }
    centered_text(surface, self->width,
                  (int16_t)(center_y - 8 * self->scale),
                  seconds, self->scale, 15U);
    centered_text(surface, self->width,
                  (int16_t)(center_y + 2 * self->scale),
                  "EXIT", self->scale, 10U);
}

static uint8_t sprite_frame(const fighter_t *fighter)
{
    if (fighter->blocking) return fighter->crouching ? 2U : 4U;
    if (fighter->attack != ATTACK_NONE) {
        if (fighter->attack == ATTACK_LIGHT)
            return fighter->attack_ms < 100U || fighter->attack_ms > 200U ?
                   4U : 5U;
        if (fighter->attack == ATTACK_HEAVY)
            return fighter->attack_ms < 150U || fighter->attack_ms > 300U ?
                   6U : 7U;
        if (fighter->attack == ATTACK_KICK)
            return fighter->attack_ms < 100U ? 2U :
                   (fighter->attack_ms <= 200U ? 8U : 0U);
        if (fighter->attack == ATTACK_COMBO) {
            if (fighter->attack_ms < 200U) return 1U;
            if (fighter->attack_ms <= 250U) return 5U;
            if (fighter->attack_ms < 350U) return 6U;
            if (fighter->attack_ms <= 400U) return 7U;
            if (fighter->attack_ms <= 500U) return 8U;
            if (fighter->attack_ms <= 600U) return 5U;
            if (fighter->attack_ms <= 700U) return 9U;
            if (fighter->attack_ms < 800U) return 6U;
            if (fighter->attack_ms <= 850U) return 10U;
            return 6U;
        }
        if (fighter->attack == ATTACK_SWEEP)
            return fighter->attack_ms < 200U ? 8U :
                   (fighter->attack_ms <= 350U ? 9U : 0U);
        return fighter->attack_ms < 150U || fighter->attack_ms > 400U ?
               6U : 7U;
    }
    if (fighter->landing_ms != 0U) return 2U;
    if (fighter->velocity_y != 0) return 3U;
    if (fighter->crouching) return 2U;
    if (fighter->turning) return 1U;
    if (fighter->moving && fighter->walk_phase) return 1U;
    return 0U;
}

static uint8_t hurt_sprite_frame(const fighter_t *fighter)
{
    uint16_t elapsed = (uint16_t)(fighter->hurt_total_ms - fighter->hurt_ms);
    if (fighter->hurt_kind == HURT_LIGHT)
        return elapsed < 100U ? 1U : 0U;
    if (fighter->hurt_kind == HURT_HEAVY)
        return elapsed < 180U ? 1U : (elapsed < 300U ? 3U : 0U);
    if (fighter->hurt_kind == HURT_KICK)
        return elapsed < 160U ? 0U : (elapsed < 320U ? 1U : 3U);
    if (fighter->hurt_kind == HURT_GUARD_BREAK)
        return elapsed < 250U ? 1U : 3U;
    if (fighter->hurt_kind == HURT_KO)
        return elapsed < 180U ? 1U : 2U;
    if (elapsed < 180U) return 1U;
    if (elapsed < 650U) return 2U;
    return 3U;
}

static uint8_t fighter_render_frame(const fighter_t *fighter)
{
    bool hurt = fighter->hurt_ms != 0U && fighter->hurt_kind != HURT_NONE;
    if (!hurt) return sprite_frame(fighter);
    return (uint8_t)(0x80U | hurt_sprite_frame(fighter));
}

static bool hurt_uses_source_direction(uint8_t character, uint8_t frame,
                                       bool hurt_right)
{
    /* The generated poses do not share one horizontal orientation. Frame 1
     * falls left for both fighters, frame 2 falls left for Zen and right for
     * Rival, and the standing/recovery poses lean right. */
    bool source_falls_right = frame == 2U ? character != 0U : frame != 1U;
    return hurt_right == source_falls_right;
}

static uint8_t tint_gray(uint8_t gray, uint8_t maximum)
{
    if (gray == 0U) return 0U;
    return (uint8_t)(1U + (uint16_t)(gray - 1U) * (maximum - 1U) / 14U);
}

static void draw_fighter(gm_plugin_framebuffer_surface_t *surface,
                         const fighter_t *fighter, int16_t camera_x,
                         uint8_t scale)
{
    bool hurt = fighter->hurt_ms != 0U && fighter->hurt_kind != HURT_NONE;
    uint8_t render_frame = fighter_render_frame(fighter);
    bool hurt_sheet = (render_frame & 0x80U) != 0U;
    bool draw_source_direction = fighter->facing_right;
    uint8_t frame = (uint8_t)(render_frame & 0x7FU);
    uint8_t shade = characters[fighter->character].shade;
    const uint32_t *offsets = hurt_sheet ?
        (fighter->character == 0U ? zen_hurt_sprite_offsets :
                                    rival_hurt_sprite_offsets) :
        (fighter->character == 0U ? pf_sprite_offsets : rival_sprite_offsets);
    const uint8_t *sprite_data = hurt_sheet ?
        (fighter->character == 0U ? zen_hurt_sprite_data :
                                    rival_hurt_sprite_data) :
        (fighter->character == 0U ? pf_sprite_data : rival_sprite_data);
    uint32_t offset = offsets[frame];
    uint16_t source_y;
    int16_t target_height = (int16_t)(FIGHTER_H * scale);
    int16_t target_width = (int16_t)(PF_SPRITE_WIDTH * target_height /
                                     PF_SPRITE_HEIGHT);
    int16_t render_top = (int16_t)(fighter->y + FIGHTER_H * scale -
                                   target_height);
    int16_t center = (int16_t)(fighter->x + FIGHTER_W * scale / 2 + camera_x);
    int16_t left = (int16_t)(center - target_width / 2);
    if (hurt)
        draw_source_direction = hurt_uses_source_direction(
            fighter->character, frame, fighter->hurt_right);
    for (source_y = 0; source_y < PF_SPRITE_HEIGHT; ++source_y) {
        uint8_t start = sprite_data[offset++];
        uint8_t length = sprite_data[offset++];
        uint16_t bytes = (uint16_t)((length + 1U) / 2U);
        int16_t target_y0 = (int16_t)(render_top +
            (uint32_t)source_y * target_height / SPRITE_GROUND_ROWS);
        int16_t target_y1 = (int16_t)(render_top +
            (uint32_t)(source_y + 1U) * target_height / SPRITE_GROUND_ROWS);
        uint16_t pixel_index;
        if (start == 0xFFU || target_y1 <= (int16_t)surface->y ||
            target_y0 >= (int16_t)(surface->y + surface->height)) {
            offset += bytes;
            continue;
        }
        for (pixel_index = 0; pixel_index < length; ++pixel_index) {
            uint8_t packed = sprite_data[offset + pixel_index / 2U];
            uint8_t gray = (pixel_index & 1U) == 0U ?
                           (uint8_t)(packed >> 4) :
                           (uint8_t)(packed & 0x0FU);
            uint16_t source_x = (uint16_t)(start + pixel_index);
            uint16_t mapped_x = draw_source_direction ? source_x :
                                (uint16_t)(PF_SPRITE_WIDTH - 1U - source_x);
            int16_t target_x0 = (int16_t)(left +
                (uint32_t)mapped_x * target_width / PF_SPRITE_WIDTH);
            int16_t target_x1 = (int16_t)(left +
                (uint32_t)(mapped_x + 1U) * target_width / PF_SPRITE_WIDTH);
            if (gray != 0U && target_x1 > target_x0 && target_y1 > target_y0) {
                uint8_t rendered_gray = tint_gray(gray, shade);
                if (source_y >= 90U && rendered_gray < 8U)
                    rendered_gray = 8U;
                rectangle(surface, target_x0, target_y0,
                          (int16_t)(target_x1 - target_x0),
                          (int16_t)(target_y1 - target_y0),
                          rendered_gray);
            }
        }
        offset += bytes;
    }
}

static void draw_title_fighter(gm_plugin_framebuffer_surface_t *surface,
                               int16_t center_x, int16_t bottom,
                               const fighter_t *source, uint8_t scale,
                               int16_t bounce)
{
    fighter_t preview;
    preview.x = (int16_t)(center_x - FIGHTER_W * scale / 2);
    preview.y = (int16_t)(bottom - FIGHTER_H * scale - bounce);
    preview.velocity_y = 0;
    preview.health = 0U;
    preview.energy = 0U;
    preview.guard = 0U;
    preview.guard_recovery_ms = 0U;
    preview.attack_ms = 0U;
    preview.hurt_ms = 0U;
    preview.hurt_total_ms = 0U;
    preview.block_stun_ms = 0U;
    preview.landing_ms = 0U;
    preview.turn_ms = 0U;
    preview.walk_anim_ms = 0U;
    preview.character = source->character;
    preview.attack = ATTACK_NONE;
    preview.queued_attack = ATTACK_NONE;
    preview.hurt_kind = HURT_NONE;
    preview.facing_right = source->facing_right;
    preview.crouching = false;
    preview.blocking = false;
    preview.hurt_right = false;
    preview.hit_landed = false;
    preview.special_spawned = false;
    preview.moving = false;
    preview.walk_phase = false;
    preview.turning = false;
    draw_fighter(surface, &preview, 0, scale);
}

static void draw_stage(game_t *self,
                       gm_plugin_framebuffer_surface_t *surface,
                       int16_t camera_x)
{
    int16_t width = (int16_t)self->width;
    int16_t ground = self->ground_y;
    int16_t scale = self->scale;
    int16_t moon_x = (int16_t)(width * 4 / 5 + camera_x);
    int16_t moon_y = (int16_t)(ground - 96 * scale);
    int16_t moon_radius = (int16_t)(18 * scale);
    int16_t x;

    circle(surface, moon_x, moon_y, moon_radius, 2U);
    circle(surface, (int16_t)(moon_x + 8 * scale),
           (int16_t)(moon_y - 5 * scale), moon_radius, 0U);

    for (x = 20; x < width; x = (int16_t)(x + 80 * scale)) {
        line(surface, (int16_t)(x + camera_x), ground,
             (int16_t)(x + camera_x), (int16_t)(ground - 34 * scale),
             self->scale, 3U);
        line(surface, (int16_t)(x - 8 * scale + camera_x),
             (int16_t)(ground - 34 * scale),
             (int16_t)(x + 8 * scale + camera_x),
             (int16_t)(ground - 34 * scale), self->scale, 3U);
    }

    line(surface, 0, ground, width, ground,
         self->scale > 2U ? 2U : 1U, 5U);
    line(surface, 0, (int16_t)(ground + 4 * scale), width,
         (int16_t)(ground + 4 * scale), self->scale, 2U);
}

static void draw_hud(game_t *self, gm_plugin_framebuffer_surface_t *surface)
{
    uint8_t ui = self->scale;
    uint8_t label = ui > 2U ? (uint8_t)(ui / 2U) : 1U;
    int16_t margin = (int16_t)(ui * 3U);
    int16_t gap = (int16_t)(ui * 14U);
    int16_t bar_w = (int16_t)((self->width - gap - margin * 2) / 2);
    int16_t bar_h = (int16_t)(ui * 12U);
    int16_t meter_h = (int16_t)(ui * 4U);
    int16_t inner_w = (int16_t)(bar_w - ui * 2U);
    int16_t meter_label_w = (int16_t)(label * 10U);
    int16_t meter_w = (int16_t)(bar_w - meter_label_w);
    int16_t meter_inner_w = (int16_t)(meter_w - ui * 2U);
    int16_t player_meter_x = (int16_t)(margin + meter_label_w);
    int16_t cpu_meter_x = (int16_t)(self->width - margin - bar_w);
    int16_t player_health = (int16_t)(inner_w * self->player.health /
                            characters[self->player.character].health);
    int16_t cpu_health = (int16_t)(inner_w * self->cpu.health /
                         characters[self->cpu.character].health);
    int16_t player_energy = (int16_t)(meter_inner_w *
                            self->player.energy / 100U);
    int16_t cpu_energy = (int16_t)(meter_inner_w * self->cpu.energy / 100U);
    char timer[4];
    uint8_t round;
    outline_rectangle(surface, margin, (int16_t)(ui * 2U), bar_w, bar_h,
                      ui, 10U);
    rectangle(surface, (int16_t)(margin + ui), (int16_t)(ui * 3U),
              player_health, (int16_t)(bar_h - ui * 2U), 15U);
    outline_rectangle(surface, (int16_t)(self->width - margin - bar_w),
                      (int16_t)(ui * 2U), bar_w, bar_h, ui, 9U);
    rectangle(surface, (int16_t)(self->width - margin - ui - cpu_health),
              (int16_t)(ui * 3U), cpu_health,
              (int16_t)(bar_h - ui * 2U), 11U);
    self->libc->snprintf(timer, sizeof(timer), "%u",
                        (unsigned int)((self->round_left_ms + 999U) / 1000U));
    centered_text(surface, self->width, (int16_t)(ui * 2U), timer, ui, 15U);
    text(surface, margin, (int16_t)(ui * 15U),
         characters[self->player.character].name, label, 13U);
    text(surface, (int16_t)(margin +
         (game.libc->strlen(characters[self->player.character].name) + 1U) *
         4U * label), (int16_t)(ui * 15U), "HP", label, 15U);
    text(surface, (int16_t)(self->width - margin -
         (game.libc->strlen(characters[self->cpu.character].name) + 3U) *
         4U * label), (int16_t)(ui * 15U), "HP", label, 12U);
    text(surface, (int16_t)(self->width - margin -
         game.libc->strlen(characters[self->cpu.character].name) * 4U * label),
         (int16_t)(ui * 15U), characters[self->cpu.character].name, label, 10U);
    text(surface, margin, (int16_t)(ui * 20U), "EN", label, 12U);
    outline_rectangle(surface, player_meter_x, (int16_t)(ui * 20U),
                      meter_w, meter_h,
                      ui, 8U);
    rectangle(surface, (int16_t)(player_meter_x + ui),
              (int16_t)(ui * 21U),
              player_energy, (int16_t)(meter_h - ui * 2U), 12U);
    outline_rectangle(surface, cpu_meter_x, (int16_t)(ui * 20U),
                      meter_w, meter_h, ui, 7U);
    rectangle(surface, (int16_t)(cpu_meter_x + meter_w - ui - cpu_energy),
              (int16_t)(ui * 21U), cpu_energy,
              (int16_t)(meter_h - ui * 2U), 10U);
    text(surface, (int16_t)(self->width - margin - 8U * label),
         (int16_t)(ui * 20U), "EN", label, 10U);
    for (round = 0; round < self->player_rounds; ++round)
        circle(surface, (int16_t)(margin + round * 5U * ui),
               (int16_t)(ui * 27U), (int16_t)(ui * 2U), 15U);
    for (round = 0; round < self->cpu_rounds; ++round)
        circle(surface, (int16_t)(self->width - margin - round * 5U * ui),
               (int16_t)(ui * 27U), (int16_t)(ui * 2U), 11U);
}

static void draw_projectiles(game_t *self,
                             gm_plugin_framebuffer_surface_t *surface,
                             int16_t camera_x)
{
    uint8_t index;
    for (index = 0; index < PROJECTILE_COUNT; ++index) {
        projectile_t *projectile = &self->projectiles[index];
        int16_t direction;
        int16_t radius;
        int16_t center_x;
        uint8_t wave_gray;
        if (!projectile->active) continue;
        direction = projectile->velocity_x > 0 ? 1 : -1;
        radius = (int16_t)((7U + ((projectile->age_ms / FRAME_MS) & 1U)) *
                           self->scale);
        center_x = (int16_t)(projectile->x + camera_x);
        wave_gray = projectile->shade > 11U ? 11U : projectile->shade;
        circle(surface, (int16_t)(center_x - direction * 5 * self->scale),
               projectile->y, (int16_t)(radius - 2 * self->scale),
               (uint8_t)(wave_gray > 4U ? wave_gray - 4U : wave_gray));
        rectangle(surface,
                  (int16_t)(center_x - 6 * self->scale),
                  (int16_t)(projectile->y - 5 * self->scale),
                  (int16_t)(12 * self->scale),
                  (int16_t)(10 * self->scale), (uint8_t)(wave_gray - 1U));
        circle(surface, (int16_t)(center_x + direction * 5 * self->scale),
               projectile->y, radius, wave_gray);
        circle(surface, (int16_t)(center_x + direction * 7 * self->scale),
               projectile->y, (int16_t)(2 * self->scale), 15U);
    }
}

static void draw_impact(game_t *self,
                        gm_plugin_framebuffer_surface_t *surface,
                        int16_t camera_x)
{
    int16_t radius;
    int16_t center_x;
    int16_t center_y;
    if (self->impact_ms == 0U) return;
    radius = (int16_t)((self->impact_strong ? 6 : 4) * self->scale);
    center_x = (int16_t)(self->impact_x + camera_x);
    center_y = self->impact_y;
    if (self->impact_kind == IMPACT_BLOCK) {
        line(surface, center_x, (int16_t)(center_y - radius),
             (int16_t)(center_x + radius), center_y, self->scale, 13U);
        line(surface, (int16_t)(center_x + radius), center_y,
             center_x, (int16_t)(center_y + radius), self->scale, 13U);
        line(surface, center_x, (int16_t)(center_y + radius),
             (int16_t)(center_x - radius / 2), center_y,
             self->scale, 9U);
    } else if (self->impact_kind == IMPACT_GUARD_BREAK) {
        line(surface, (int16_t)(center_x - radius),
             (int16_t)(center_y - radius),
             (int16_t)(center_x + radius),
             (int16_t)(center_y + radius), self->scale, 15U);
        line(surface, (int16_t)(center_x + radius),
             (int16_t)(center_y - radius),
             (int16_t)(center_x - radius),
             (int16_t)(center_y + radius), self->scale, 15U);
        circle(surface, center_x, center_y,
               (int16_t)(2 * self->scale), 8U);
    } else {
        int16_t spark = (int16_t)(radius / 2);
        rectangle(surface, (int16_t)(center_x - self->scale),
                  (int16_t)(center_y - self->scale),
                  (int16_t)(self->scale * 2),
                  (int16_t)(self->scale * 2), 15U);
        line(surface, (int16_t)(center_x - spark), center_y,
             (int16_t)(center_x + spark), center_y, self->scale, 13U);
        line(surface, center_x, (int16_t)(center_y - spark),
             center_x, (int16_t)(center_y + spark), self->scale, 13U);
        if (self->impact_strong) {
            line(surface, (int16_t)(center_x - spark),
                 (int16_t)(center_y - spark),
                 (int16_t)(center_x + spark),
                 (int16_t)(center_y + spark), self->scale, 10U);
        }
    }
}

static void draw_title(game_t *self, gm_plugin_framebuffer_surface_t *surface)
{
    uint8_t big = (uint8_t)(self->scale * 2U);
    int16_t center = (int16_t)(self->width / 2U);
    uint32_t phase = (self->host->monotonic_ms() / 250U) % 4U;
    int16_t bounce = (int16_t)((phase == 1U || phase == 3U ? 1U :
                               (phase == 2U ? 2U : 0U)) * self->scale);
    centered_text(surface, self->width, (int16_t)(self->height / 5U),
                  "FIGHTER", big, 15U);
    centered_text(surface, self->width,
                  (int16_t)(self->height / 5U + 7U * big),
                  "ARENA", big, 12U);
    draw_title_fighter(surface, (int16_t)(center - 55 * self->scale),
                       (int16_t)(self->height - 16 * self->scale),
                       &self->player, self->scale, bounce);
    draw_title_fighter(surface, (int16_t)(center + 55 * self->scale),
                       (int16_t)(self->height - 16 * self->scale),
                       &self->cpu, self->scale,
                       (int16_t)(2 * self->scale - bounce));
    centered_text(surface, self->width,
                  (int16_t)(self->height - 12U * self->scale),
                  "PRESS J", self->scale, 10U);
}

static void draw_select(game_t *self, gm_plugin_framebuffer_surface_t *surface)
{
    static const char *descriptions[] = {
        "SLOW REACTION", "BALANCED", "FAST AGGRESSIVE"
    };
    centered_text(surface, self->width, (int16_t)(8 * self->scale),
                  "DIFFICULTY", self->scale, 15U);
    centered_text(surface, self->width,
                  (int16_t)(self->height / 2U - 12 * self->scale),
                  difficulties[self->difficulty].name,
                  (uint8_t)(self->scale * 2U), 15U);
    text(surface, (int16_t)(self->width / 2U - 42 * self->scale),
         (int16_t)(self->height / 2U - 7 * self->scale),
         "<", self->scale, 10U);
    text(surface, (int16_t)(self->width / 2U + 38 * self->scale),
         (int16_t)(self->height / 2U - 7 * self->scale),
         ">", self->scale, 10U);
    centered_text(surface, self->width,
                  (int16_t)(self->height / 2U + 6 * self->scale),
                  descriptions[self->difficulty], self->scale, 10U);
    centered_text(surface, self->width,
                  (int16_t)(self->height - 10 * self->scale),
                  "A D SELECT  J START", self->scale, 9U);
}

static void draw_interstitial(game_t *self,
                              gm_plugin_framebuffer_surface_t *surface)
{
    const char *message;
    const char *prompt = 0;
    char score[4];
    char max_hit[12];
    if (self->screen == SCREEN_INTRO)
        message = self->screen_ms > 700U ? "ROUND READY" : "FIGHT!";
    else if (self->screen == SCREEN_ROUND_OVER)
    {
        message = self->round_result == 1U ? "YOU WIN" :
                  (self->round_result == 2U ? "CPU WINS" : "DRAW");
        prompt = "J CONTINUE";
    }
    else if (self->screen == SCREEN_ENDING) {
        message = "CHAMPION!";
        prompt = "J REPLAY";
    } else {
        message = "GAME OVER";
        prompt = "J REPLAY";
    }
    centered_text(surface, self->width,
                  (int16_t)(self->height / 2U - 6 * self->scale),
                  message, (uint8_t)(self->scale * 2U), 15U);
    if (self->screen == SCREEN_ENDING)
        centered_text(surface, self->width,
                      (int16_t)(self->height / 2U + 12 * self->scale),
                      characters[self->selected_character].name,
                      self->scale, 11U);
    if (self->screen == SCREEN_ENDING || self->screen == SCREEN_GAME_OVER) {
        self->libc->snprintf(score, sizeof(score), "%u-%u",
                            (unsigned int)self->player_rounds,
                            (unsigned int)self->cpu_rounds);
        centered_text(surface, self->width,
                      (int16_t)(self->height / 2U + 20 * self->scale),
                      score, self->scale, 15U);
        self->libc->snprintf(max_hit, sizeof(max_hit), "MAX HIT %u",
                            (unsigned int)self->max_combo);
        centered_text(surface, self->width,
                      (int16_t)(self->height / 2U + 28 * self->scale),
                      max_hit, self->scale, 10U);
    }
    if (prompt != 0) {
        int16_t prompt_y = self->screen == SCREEN_ROUND_OVER ?
                           (int16_t)(self->height / 2U + 14 * self->scale) :
                           (int16_t)(self->height / 2U + 38 * self->scale);
        centered_text_panel(surface, self->width, prompt_y,
                            prompt, self->scale, 12U);
    }
}

static void draw_fight(game_t *self, gm_plugin_framebuffer_surface_t *surface)
{
    int16_t camera_x = 0;
    fighter_t *rush = self->player.attack == ATTACK_COMBO ? &self->player :
                      (self->cpu.attack == ATTACK_COMBO ? &self->cpu : 0);
    draw_stage(self, surface, camera_x);
    if (rush != 0 && rush->attack_ms < 200U) {
        int16_t direction = rush->facing_right ? 1 : -1;
        int16_t trail_x = (int16_t)(rush->x + FIGHTER_W * self->scale / 2 +
                                    camera_x - direction * 10 * self->scale);
        int16_t trail_end = (int16_t)(trail_x - direction * 18 * self->scale);
        line(surface, trail_x, (int16_t)(rush->y + 24 * self->scale),
             trail_end, (int16_t)(rush->y + 24 * self->scale),
             self->scale, 10U);
        line(surface, trail_x, (int16_t)(rush->y + 38 * self->scale),
             (int16_t)(trail_end + direction * 5 * self->scale),
             (int16_t)(rush->y + 38 * self->scale), self->scale, 7U);
        line(surface, trail_x, (int16_t)(rush->y + 52 * self->scale),
             (int16_t)(trail_end + direction * 9 * self->scale),
             (int16_t)(rush->y + 52 * self->scale), self->scale, 5U);
    }
    draw_fighter(surface, &self->player, camera_x, self->scale);
    draw_fighter(surface, &self->cpu, camera_x, self->scale);
    draw_projectiles(self, surface, camera_x);
    draw_impact(self, surface, camera_x);
    draw_hud(self, surface);
    if ((self->input & KEY_PAUSE) != 0U) {
        const char *pause_text = self->pause_reason == PAUSE_DISCONNECTED ?
                                 "DISCONNECTED" :
                                 (self->pause_reason == PAUSE_INPUT_LOST ?
                                  "INPUT LOST" : "PAUSED");
        centered_text(surface, self->width,
                      (int16_t)(self->height / 2U - 6 * self->scale),
                      pause_text, (uint8_t)(self->scale * 2U), 15U);
    }
    if (self->combo >= 2U && self->combo_ms != 0U) {
        char count[4];
        int16_t combo_x;
        self->libc->snprintf(count, sizeof(count), "%u",
                            (unsigned int)self->combo);
        combo_x = (int16_t)(self->cpu.x + FIGHTER_W * self->scale / 2 -
                  5 * 4 * self->scale / 2);
        if (combo_x < 4 * self->scale) combo_x = (int16_t)(4 * self->scale);
        if (combo_x + 20 * self->scale > (int16_t)self->width)
            combo_x = (int16_t)(self->width - 20 * self->scale);
        text(surface, combo_x, (int16_t)(34 * self->scale),
             count, self->scale, 15U);
        text(surface, (int16_t)(combo_x + 6 * self->scale),
             (int16_t)(34 * self->scale), "HIT", self->scale,
             self->combo >= 3U ? 15U : 13U);
    }
}

static void draw_slice(game_t *self, gm_plugin_framebuffer_surface_t *surface)
{
    self->libc->memset(surface->pixels, 0,
                       (size_t)surface->height * surface->stride);
    if (self->screen == SCREEN_TITLE) draw_title(self, surface);
    else if (self->screen == SCREEN_DIFFICULTY)
        draw_select(self, surface);
    else if (self->screen == SCREEN_FIGHT ||
             self->screen == SCREEN_ROUND_OVER) {
        draw_fight(self, surface);
        if (self->screen == SCREEN_ROUND_OVER)
            draw_interstitial(self, surface);
    } else if (self->screen == SCREEN_INTRO) {
        draw_stage(self, surface, 0);
        draw_fighter(surface, &self->player, 0, self->scale);
        draw_fighter(surface, &self->cpu, 0, self->scale);
        draw_hud(self, surface);
        draw_interstitial(self, surface);
    } else {
        draw_interstitial(self, surface);
    }
    if (self->holding_exit) draw_exit_countdown(self, surface);
}

static gm_plugin_result_t render(game_t *self)
{
    gm_plugin_framebuffer_surface_t surface;
    uint16_t next_y = 0;
    while (next_y < self->height) {
        gm_plugin_rect_t dirty;
        uint32_t surface_end;
        uint16_t end_y;
        gm_plugin_result_t result =
            self->host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) return result;
        surface_end = (uint32_t)surface.y + surface.height;
        end_y = (uint16_t)surface_end;
        if (surface.pixels == 0 || surface.height == 0U ||
            surface.width < self->width || surface.y > next_y ||
            surface.stride < (surface.width + 1U) / 2U ||
            surface_end <= next_y || surface_end > self->height) {
            (void)self->host->graphics.framebuffer.unlock(0, false);
            return GM_PLUGIN_ESTATE;
        }
        draw_slice(self, &surface);
        dirty.x = 0;
        dirty.y = (int16_t)surface.y;
        dirty.width = self->width;
        dirty.height = surface.height;
        result = self->host->graphics.framebuffer.unlock(
            &dirty, end_y == self->height);
        if (result != GM_PLUGIN_OK) return result;
        next_y = end_y;
    }
    return GM_PLUGIN_OK;
}

static void reset_fighter(fighter_t *fighter, uint8_t character,
                          int16_t x, int16_t ground, bool facing_right,
                          uint8_t scale)
{
    game.libc->memset(fighter, 0, sizeof(*fighter));
    fighter->x = x;
    fighter->y = (int16_t)(ground - FIGHTER_H * scale);
    fighter->health = characters[character].health;
    fighter->energy = 50U;
    fighter->guard = 100U;
    fighter->character = character;
    fighter->facing_right = facing_right;
}

static void reset_round(game_t *self)
{
    reset_fighter(&self->player, self->selected_character,
                  (int16_t)(self->width / 4U - FIGHTER_W * self->scale / 2U),
                  self->ground_y, true, self->scale);
    reset_fighter(&self->cpu, self->cpu_character,
                  (int16_t)(self->width * 3U / 4U -
                            FIGHTER_W * self->scale / 2U),
                  self->ground_y, false, self->scale);
    self->libc->memset(self->projectiles, 0, sizeof(self->projectiles));
    self->round_left_ms = ROUND_MS;
    self->hit_stop_ms = 0U;
    self->impact_ms = 0U;
    self->combo_ms = 0U;
    self->cpu_think_ms = 0U;
    self->cpu_reaction_ms = 0U;
    self->cpu_retreat_ms = 0U;
    self->combo = 0U;
    self->rush_owner = 0U;
    self->round_result = 0U;
    self->cpu_observed_attack = ATTACK_NONE;
    self->cpu_defense_decided = false;
}

static void begin_match(game_t *self)
{
    self->player_rounds = 0U;
    self->cpu_rounds = 0U;
    self->max_combo = 0U;
    reset_round(self);
    self->screen = SCREEN_INTRO;
    self->screen_ms = INTRO_MS;
    send_fight_event(self, EVENT_MUSIC, MUSIC_FIGHT);
}

static void start_attack(game_t *self, fighter_t *fighter, uint8_t attack)
{
    const move_t *move = &moves[attack];
    int16_t floor = (int16_t)(self->ground_y - FIGHTER_H * self->scale);
    if (fighter->attack != ATTACK_NONE || fighter->hurt_ms != 0U ||
        fighter->block_stun_ms != 0U || fighter->blocking ||
        fighter->landing_ms != 0U || fighter->turning ||
        fighter->y != floor || fighter->velocity_y != 0) return;
    if (fighter->energy < move->energy_cost) return;
    fighter->energy = (uint16_t)(fighter->energy - move->energy_cost);
    fighter->attack = attack;
    fighter->queued_attack = ATTACK_NONE;
    fighter->attack_ms = 0U;
    fighter->hit_landed = false;
    fighter->special_spawned = false;
    send_fight_event(self, EVENT_ATTACK, attack);
}

static bool chain_transition_allowed(uint8_t current, uint8_t next)
{
    if (current == ATTACK_LIGHT)
        return next == ATTACK_LIGHT || next == ATTACK_HEAVY ||
               next == ATTACK_KICK;
    if (current == ATTACK_HEAVY || current == ATTACK_KICK)
        return next == ATTACK_SWEEP;
    return false;
}

static void request_player_attack(game_t *self, uint8_t attack)
{
    fighter_t *fighter = &self->player;
    if (fighter->attack != ATTACK_NONE) {
        if (chain_transition_allowed(fighter->attack, attack))
            fighter->queued_attack = attack;
        return;
    }
    start_attack(self, fighter, attack);
}

static uint16_t attack_start(const fighter_t *fighter)
{
    return moves[fighter->attack].startup_ms;
}

static uint16_t attack_active_end(const fighter_t *fighter)
{
    return moves[fighter->attack].active_end_ms;
}

static uint16_t attack_duration(const fighter_t *fighter)
{
    return moves[fighter->attack].duration_ms;
}

static uint8_t combo_stage_at(uint16_t attack_ms)
{
    if (attack_ms >= 200U && attack_ms <= 250U) return 0U;
    if (attack_ms >= 350U && attack_ms <= 400U) return 1U;
    if (attack_ms >= 450U && attack_ms <= 500U) return 2U;
    if (attack_ms >= 550U && attack_ms <= 600U) return 3U;
    if (attack_ms >= 650U && attack_ms <= 700U) return 4U;
    if (attack_ms >= 800U && attack_ms <= 850U) return 5U;
    return UINT8_MAX;
}

static bool attack_active(const fighter_t *fighter)
{
    if (fighter->attack == ATTACK_COMBO)
        return combo_stage_at(fighter->attack_ms) != UINT8_MAX;
    return fighter->attack != ATTACK_NONE &&
           fighter->attack_ms >= attack_start(fighter) &&
           fighter->attack_ms <= attack_active_end(fighter);
}

static int16_t fighter_distance(const fighter_t *first,
                                const fighter_t *second)
{
    return absolute((int16_t)(first->x - second->x));
}

static void spawn_projectile(game_t *self, fighter_t *fighter, uint8_t owner)
{
    uint8_t index;
    for (index = 0; index < PROJECTILE_COUNT; ++index) {
        projectile_t *projectile = &self->projectiles[index];
        if (projectile->active) continue;
        projectile->active = true;
        projectile->owner = owner;
        projectile->age_ms = 0U;
        projectile->x = (int16_t)(fighter->x + FIGHTER_W * self->scale / 2 +
            (fighter->facing_right ? 12 * self->scale : -12 * self->scale));
        projectile->y = (int16_t)(fighter->y + 30 * self->scale);
        projectile->velocity_x = (int16_t)((fighter->facing_right ? 1 : -1) *
                                           8 * self->scale);
        projectile->shade = characters[fighter->character].shade;
        break;
    }
}

static void show_impact(game_t *self, fighter_t *attacker,
                        fighter_t *defender, const move_t *move,
                        bool blocked, bool guard_broken)
{
    bool strong = move->strong || guard_broken;
    self->hit_stop_ms = strong ? 100U : 50U;
    self->impact_ms = strong ? 100U : 50U;
    self->impact_x = attacker->x < defender->x ? defender->x :
                     (int16_t)(defender->x + FIGHTER_W * self->scale);
    self->impact_y = (int16_t)(defender->y +
        (move->attack_level == ATTACK_LEVEL_LOW ? 50 : 28) * self->scale);
    self->impact_strong = strong;
    self->impact_kind = guard_broken ? IMPACT_GUARD_BREAK :
                        (blocked ? IMPACT_BLOCK : IMPACT_HIT);
}

static bool attacker_is_in_front(const fighter_t *attacker,
                                 const fighter_t *defender)
{
    return defender->facing_right ? attacker->x > defender->x :
                                    attacker->x < defender->x;
}

static bool can_block(const fighter_t *attacker, const fighter_t *defender,
                      const move_t *move)
{
    if (!defender->blocking || !attacker_is_in_front(attacker, defender))
        return false;
    if (move->attack_level == ATTACK_LEVEL_LOW) return defender->crouching;
    if (move->attack_level == ATTACK_LEVEL_OVERHEAD)
        return !defender->crouching;
    return true;
}

static void damage_fighter(game_t *self, fighter_t *attacker,
                           fighter_t *defender, const move_t *move,
                           uint8_t move_id)
{
    bool blocked = move_id != ATTACK_COMBO &&
                   can_block(attacker, defender, move);
    bool combo_finisher = move_id == ATTACK_COMBO &&
                          attacker->attack_ms >= 800U;
    uint8_t event_move_id = move_id;
    uint16_t damage = blocked ? move->chip_damage : move->damage;
    uint16_t attacker_energy_gain = blocked ? 5U : 12U;
    int16_t direction = attacker->x < defender->x ? 1 : -1;
    int16_t push;
    defender->hurt_right = direction > 0;
    if (blocked) {
        defender->block_stun_ms = move->block_stun_ms;
    } else {
        damage = (uint16_t)(damage *
                 characters[attacker->character].damage_percent / 100U);
        defender->blocking = false;
        if (move_id == ATTACK_COMBO) {
            self->rush_owner = attacker == &self->player ? 1U : 2U;
            defender->hurt_kind = combo_finisher ? HURT_KNOCKDOWN :
                ((attacker->attack_ms >= 450U &&
                  attacker->attack_ms <= 500U) ||
                 (attacker->attack_ms >= 650U &&
                  attacker->attack_ms <= 700U) ? HURT_KICK : HURT_HEAVY);
            defender->hurt_ms = combo_finisher ? 700U : 180U;
            if (combo_finisher) self->rush_owner = 0U;
        } else {
            defender->hurt_kind = move_id == ATTACK_LIGHT ? HURT_LIGHT :
                                  (move_id == ATTACK_KICK ? HURT_KICK :
                                  ((move_id == ATTACK_SPECIAL ||
                                    move_id == ATTACK_SWEEP) ?
                                   HURT_KNOCKDOWN : HURT_HEAVY));
            defender->hurt_ms = move_id == ATTACK_SPECIAL ? 900U :
                                (move_id == ATTACK_SWEEP ? 800U :
                                                         move->hit_stun_ms);
        }
        defender->hurt_total_ms = defender->hurt_ms;
        if (move_id == ATTACK_SPECIAL || combo_finisher ||
            move_id == ATTACK_SWEEP)
            defender->velocity_y = (int16_t)(
                (combo_finisher ? -6 :
                (move_id == ATTACK_SWEEP ? -3 : -6)) * self->scale);
    }
    defender->health = defender->health > damage ?
                       (uint16_t)(defender->health - damage) : 0U;
    if (defender->health == 0U) {
        self->rush_owner = 0U;
        defender->blocking = false;
        defender->crouching = false;
        defender->hurt_kind = HURT_KO;
        defender->hurt_ms = ROUND_OVER_MS;
        defender->hurt_total_ms = ROUND_OVER_MS;
        defender->velocity_y = (int16_t)(-6 * self->scale);
    }
    push = (int16_t)(direction * move->knockback * self->scale);
    if (move_id == ATTACK_COMBO && !combo_finisher) push = 0;
    if (blocked) push = (int16_t)(push / 2);
    defender->x = (int16_t)(defender->x + push);
    if (move_id == ATTACK_COMBO)
        attacker_energy_gain = blocked ? 2U : (combo_finisher ? 5U : 3U);
    attacker->energy = clamp_add(attacker->energy, attacker_energy_gain, 100U);
    defender->energy = clamp_add(defender->energy,
                                 blocked ? 10U : 5U, 100U);
    if (move_id == ATTACK_COMBO)
        event_move_id = attacker->attack_ms < 350U ? ATTACK_LIGHT :
                        (attacker->attack_ms < 450U ? ATTACK_HEAVY :
                        ((attacker->attack_ms < 550U ||
                          (attacker->attack_ms >= 650U &&
                           attacker->attack_ms < 800U)) ? ATTACK_KICK :
                        (combo_finisher ? ATTACK_SWEEP : ATTACK_LIGHT)));
    show_impact(self, attacker, defender, move, blocked, false);
    send_fight_event(self, blocked ? EVENT_BLOCK : EVENT_HIT, event_move_id);
    if (defender->health == 0U)
        send_fight_event(self, EVENT_KO,
                         defender == &self->cpu ? 1U : 2U);
    if (attacker == &self->player && !blocked) {
        self->combo = self->combo_ms != 0U ?
                      (uint8_t)(self->combo + 1U) : 1U;
        if (self->combo > self->max_combo) self->max_combo = self->combo;
        self->combo_ms = COMBO_WINDOW_MS;
    }
}

static void apply_melee_hit(game_t *self, fighter_t *attacker,
                            fighter_t *defender)
{
    const move_t *move = &moves[attacker->attack];
    uint8_t combo_stage = UINT8_MAX;
    int16_t reach;
    if (attacker->hit_landed || !attack_active(attacker)) return;
    if (attacker->attack == ATTACK_SPECIAL) return;
    if (attacker->attack == ATTACK_COMBO) {
        combo_stage = combo_stage_at(attacker->attack_ms);
        if (combo_stage == UINT8_MAX) return;
        if (combo_stage > 0U && self->rush_owner !=
            (attacker == &self->player ? 1U : 2U)) return;
        move = &combo_hits[combo_stage];
    }
    reach = (int16_t)move->reach;
    reach = (int16_t)(reach * self->scale *
                      characters[attacker->character].reach_percent / 100U);
    if (!(attacker->attack == ATTACK_COMBO && combo_stage > 0U) &&
        fighter_distance(attacker, defender) > reach) return;
    if (defender->y + FIGHTER_H * self->scale <
            attacker->y + 18 * self->scale ||
        attacker->y + FIGHTER_H * self->scale <
            defender->y + 18 * self->scale) return;
    damage_fighter(self, attacker, defender, move, attacker->attack);
    attacker->hit_landed = true;
}

static void update_projectiles(game_t *self)
{
    uint8_t index;
    for (index = 0; index < PROJECTILE_COUNT; ++index) {
        projectile_t *projectile = &self->projectiles[index];
        fighter_t *attacker;
        fighter_t *defender;
        if (!projectile->active) continue;
        projectile->age_ms = (uint16_t)(projectile->age_ms + FRAME_MS);
        projectile->x = (int16_t)(projectile->x + projectile->velocity_x);
        if (projectile->x < 0 || projectile->x >= (int16_t)self->width) {
            projectile->active = false;
            continue;
        }
        attacker = projectile->owner == 0U ? &self->player : &self->cpu;
        defender = projectile->owner == 0U ? &self->cpu : &self->player;
        if (projectile->age_ms >= 100U &&
            absolute((int16_t)(projectile->x - defender->x -
                     FIGHTER_W * self->scale / 2)) < 18 * self->scale &&
            absolute((int16_t)(projectile->y - defender->y -
                     30 * self->scale)) < 25 * self->scale) {
            damage_fighter(self, attacker, defender, &moves[ATTACK_SPECIAL],
                           ATTACK_SPECIAL);
            projectile->active = false;
        }
    }
}

static void update_hurt(fighter_t *fighter)
{
    if (fighter->hurt_ms != 0U)
        fighter->hurt_ms = fighter->hurt_ms > FRAME_MS ?
                           (uint16_t)(fighter->hurt_ms - FRAME_MS) : 0U;
    if (fighter->hurt_ms == 0U) fighter->hurt_kind = HURT_NONE;
}

static void set_moving(fighter_t *fighter, bool moving)
{
    fighter->moving = moving;
    if (!moving) {
        fighter->walk_anim_ms = 0U;
        fighter->walk_phase = false;
        return;
    }
    fighter->walk_anim_ms = (uint16_t)(fighter->walk_anim_ms + FRAME_MS);
    if (fighter->walk_anim_ms >= WALK_FRAME_MS) {
        fighter->walk_anim_ms = 0U;
        fighter->walk_phase = !fighter->walk_phase;
    }
}

static void update_attack(game_t *self, fighter_t *fighter, uint8_t owner)
{
    update_hurt(fighter);
    if (fighter->block_stun_ms != 0U)
        fighter->block_stun_ms = fighter->block_stun_ms > FRAME_MS ?
              (uint16_t)(fighter->block_stun_ms - FRAME_MS) : 0U;
    if (fighter->landing_ms != 0U)
        fighter->landing_ms = fighter->landing_ms > FRAME_MS ?
              (uint16_t)(fighter->landing_ms - FRAME_MS) : 0U;
    if (fighter->attack == ATTACK_NONE) return;
    fighter->attack_ms = (uint16_t)(fighter->attack_ms + FRAME_MS);
    if (fighter->attack == ATTACK_COMBO && self->rush_owner == owner + 1U) {
        fighter_t *defender = owner == 0U ? &self->cpu : &self->player;
        int16_t direction = fighter->facing_right ? 1 : -1;
        int16_t floor = (int16_t)(self->ground_y - FIGHTER_H * self->scale);
        defender->x = (int16_t)(fighter->x + direction * 32 * self->scale);
        defender->y = floor;
        defender->velocity_y = 0;
    } else if (fighter->attack == ATTACK_COMBO &&
               fighter->attack_ms <= 200U) {
        fighter_t *defender = owner == 0U ? &self->cpu : &self->player;
        int16_t direction = defender->x > fighter->x ? 1 : -1;
        int16_t gap = (int16_t)(absolute((int16_t)(defender->x - fighter->x)) -
                                32 * self->scale);
        int16_t step = (int16_t)(20 * self->scale);
        if (gap > 0) {
            if (step > gap) step = gap;
            fighter->x = (int16_t)(fighter->x + direction * step);
        }
    }
    if (!fighter->hit_landed && fighter->attack != ATTACK_SPECIAL &&
        fighter->attack != ATTACK_COMBO &&
        fighter->attack_ms == attack_start(fighter))
        fighter->x = (int16_t)(fighter->x +
                     (fighter->facing_right ? 2 * self->scale :
                                              -2 * self->scale));
    if (fighter->attack == ATTACK_COMBO &&
        (fighter->attack_ms == 200U || fighter->attack_ms == 350U ||
         fighter->attack_ms == 450U || fighter->attack_ms == 550U ||
         fighter->attack_ms == 650U || fighter->attack_ms == 800U)) {
        uint8_t stage = combo_stage_at(fighter->attack_ms);
        fighter->hit_landed = false;
        if (stage == 1U)
            send_fight_event(self, EVENT_ATTACK, ATTACK_HEAVY);
        else if (stage == 2U || stage == 4U)
            send_fight_event(self, EVENT_ATTACK, ATTACK_KICK);
        else if (stage > 2U && stage < 5U)
            send_fight_event(self, EVENT_ATTACK, ATTACK_LIGHT);
        else if (stage == 5U)
            send_fight_event(self, EVENT_ATTACK, ATTACK_SWEEP);
    }
    if (fighter->attack == ATTACK_SPECIAL && !fighter->special_spawned &&
        fighter->attack_ms >= attack_start(fighter)) {
        spawn_projectile(self, fighter, owner);
        send_fight_event(self, EVENT_SPECIAL_LAUNCH, owner);
        fighter->special_spawned = true;
    }
    if (fighter->queued_attack != ATTACK_NONE &&
        fighter->attack_ms > attack_active_end(fighter)) {
        uint8_t next = fighter->queued_attack;
        fighter->attack = ATTACK_NONE;
        fighter->queued_attack = ATTACK_NONE;
        fighter->attack_ms = 0U;
        fighter->hit_landed = false;
        fighter->special_spawned = false;
        if (next == ATTACK_KICK)
            fighter->x = (int16_t)(fighter->x +
                (fighter->facing_right ? 6 * self->scale :
                                         -6 * self->scale));
        start_attack(self, fighter, next);
        return;
    }
    if (fighter->attack_ms > attack_duration(fighter)) {
        if (fighter->attack == ATTACK_COMBO &&
            self->rush_owner == owner + 1U)
            self->rush_owner = 0U;
        fighter->attack = ATTACK_NONE;
        fighter->attack_ms = 0U;
        fighter->hit_landed = false;
        fighter->special_spawned = false;
        fighter->queued_attack = ATTACK_NONE;
        if (owner == 1U) {
            self->cpu_retreat_ms = difficulties[self->difficulty].retreat_ms;
            self->cpu_think_ms = 0U;
        }
    }
}

static void update_vertical(game_t *self, fighter_t *fighter)
{
    int16_t floor = (int16_t)(self->ground_y - FIGHTER_H * self->scale);
    if (fighter->y < floor || fighter->velocity_y != 0) {
        fighter->y = (int16_t)(fighter->y + fighter->velocity_y);
        fighter->velocity_y = (int16_t)(fighter->velocity_y + 2 * self->scale);
        if (fighter->y >= floor) {
            fighter->y = floor;
            fighter->velocity_y = 0;
            if (fighter->health != 0U && fighter->hurt_ms == 0U)
                fighter->landing_ms = LANDING_RECOVERY_MS + FRAME_MS;
        }
    }
}

static void update_fighter_facing(fighter_t *fighter, bool face_right,
                                  int16_t floor)
{
    if (fighter->facing_right == face_right) {
        fighter->turn_ms = 0U;
        fighter->turning = false;
        return;
    }
    if (fighter->attack != ATTACK_NONE || fighter->hurt_ms != 0U ||
        fighter->y != floor || fighter->landing_ms != 0U) {
        fighter->turn_ms = 0U;
        fighter->turning = false;
        return;
    }
    fighter->turning = true;
    fighter->blocking = false;
    fighter->crouching = false;
    fighter->turn_ms = (uint16_t)(fighter->turn_ms + FRAME_MS);
    if (fighter->turn_ms >= TURN_DELAY_MS) {
        fighter->facing_right = face_right;
        fighter->turn_ms = 0U;
        fighter->turning = false;
    }
}

static void update_facing(game_t *self)
{
    int16_t floor = (int16_t)(self->ground_y - FIGHTER_H * self->scale);
    if (self->player.x == self->cpu.x) return;
    update_fighter_facing(&self->player,
                          self->player.x < self->cpu.x, floor);
    update_fighter_facing(&self->cpu,
                          self->cpu.x < self->player.x, floor);
}

static uint8_t display_scale(uint16_t width, uint16_t height)
{
    uint32_t height_scale = (height * 43U / 100U) / FIGHTER_H;
    uint32_t width_scale = width / (2U * (FIGHTER_W + ARENA_MARGIN));
    uint32_t scale = height_scale < width_scale ? height_scale : width_scale;
    if (scale < 1U) scale = 1U;
    if (scale > 8U) scale = 8U;
    return (uint8_t)scale;
}

static void constrain_fighters(game_t *self)
{
    int16_t margin = (int16_t)(ARENA_MARGIN * self->scale);
    int16_t minimum = margin;
    int16_t maximum = (int16_t)(self->width -
                                FIGHTER_W * self->scale - margin);
    int16_t separation = (int16_t)(FIGHTER_W * self->scale);
    int16_t midpoint;
    int16_t left;
    int16_t right;
    int16_t floor = (int16_t)(self->ground_y - FIGHTER_H * self->scale);
    bool both_grounded = self->player.y == floor && self->cpu.y == floor;
    if (self->player.x < minimum) self->player.x = minimum;
    if (self->cpu.x < minimum) self->cpu.x = minimum;
    if (self->player.x > maximum) self->player.x = maximum;
    if (self->cpu.x > maximum) self->cpu.x = maximum;
    if (self->rush_owner == 0U && both_grounded &&
        absolute((int16_t)(self->player.x - self->cpu.x)) <
                         separation) {
        midpoint = (int16_t)((self->player.x + self->cpu.x) / 2);
        left = (int16_t)(midpoint - separation / 2);
        right = (int16_t)(left + separation);
        if (left < minimum) {
            left = minimum;
            right = (int16_t)(minimum + separation);
        } else if (right > maximum) {
            right = maximum;
            left = (int16_t)(maximum - separation);
        }
        if (self->player.x <= self->cpu.x) {
            self->player.x = left;
            self->cpu.x = right;
        } else {
            self->player.x = right;
            self->cpu.x = left;
        }
    }
    if (self->player.x < minimum) self->player.x = minimum;
    if (self->cpu.x < minimum) self->cpu.x = minimum;
    if (self->player.x > maximum) self->player.x = maximum;
    if (self->cpu.x > maximum) self->cpu.x = maximum;
}

static bool player_is_threatened(const game_t *self)
{
    const fighter_t *cpu = &self->cpu;
    int16_t distance = fighter_distance(&self->player, cpu);
    uint16_t next_attack_ms;
    uint8_t index;
    if (cpu->attack != ATTACK_NONE && cpu->attack != ATTACK_SPECIAL) {
        int16_t reach = (int16_t)(moves[cpu->attack].reach * self->scale);
        next_attack_ms = (uint16_t)(cpu->attack_ms + FRAME_MS);
        if (distance <= reach + 4 * self->scale &&
            (cpu->attack == ATTACK_COMBO ?
             combo_stage_at(next_attack_ms) != UINT8_MAX :
             (next_attack_ms >= attack_start(cpu) &&
              next_attack_ms <= attack_active_end(cpu))))
            return true;
    }
    for (index = 0U; index < PROJECTILE_COUNT; ++index) {
        const projectile_t *projectile = &self->projectiles[index];
        int16_t player_center;
        int16_t horizontal;
        bool approaching;
        if (!projectile->active || projectile->owner != 1U) continue;
        player_center = (int16_t)(self->player.x +
                                  FIGHTER_W * self->scale / 2);
        horizontal = absolute((int16_t)(projectile->x - player_center));
        approaching = (projectile->velocity_x < 0 &&
                       projectile->x > player_center) ||
                      (projectile->velocity_x > 0 &&
                       projectile->x < player_center);
        if (approaching && horizontal <= 28 * self->scale) return true;
    }
    return false;
}

static void update_player(game_t *self, uint16_t pressed)
{
    fighter_t *player = &self->player;
    int16_t floor = (int16_t)(self->ground_y - FIGHTER_H * self->scale);
    bool free = player->attack == ATTACK_NONE && player->hurt_ms == 0U &&
                player->block_stun_ms == 0U && player->landing_ms == 0U &&
                !player->turning;
    bool moved = false;
    uint16_t back = player->facing_right ? KEY_LEFT : KEY_RIGHT;
    player->moving = false;
    if (player->block_stun_ms == 0U) {
        player->blocking = free && player->y == floor &&
                           (self->input & back) != 0U &&
                           player_is_threatened(self);
        player->crouching = free && player->y == floor &&
                            (self->input & KEY_DOWN) != 0U;
    }
    if (free && !player->blocking && !player->crouching) {
        int16_t movement = (int16_t)(characters[player->character].speed *
                                     self->scale);
        if ((self->input & KEY_LEFT) != 0U &&
            (self->input & KEY_RIGHT) == 0U) {
            player->x = (int16_t)(player->x - movement);
            moved = true;
        } else if ((self->input & KEY_RIGHT) != 0U &&
                   (self->input & KEY_LEFT) == 0U) {
            player->x = (int16_t)(player->x + movement);
            moved = true;
        }
        if ((pressed & KEY_UP) != 0U && player->y == floor) {
            player->velocity_y = (int16_t)(-13 * self->scale);
            send_fight_event(self, EVENT_JUMP, 0U);
        }
    }
    if (moved) set_moving(player, true);
    if ((pressed & KEY_LIGHT) != 0U)
        request_player_attack(self, ATTACK_LIGHT);
    else if ((pressed & KEY_HEAVY) != 0U)
        request_player_attack(self, ATTACK_HEAVY);
    else if ((pressed & KEY_KICK) != 0U)
        request_player_attack(self, ATTACK_KICK);
    else if ((pressed & KEY_BLOCK) != 0U)
        request_player_attack(self, ATTACK_SWEEP);
    else if ((pressed & KEY_UPPERCUT) != 0U)
        request_player_attack(self, ATTACK_SPECIAL);
    else if ((pressed & KEY_SWEEP) != 0U)
        request_player_attack(self, ATTACK_COMBO);
}

static void update_cpu(game_t *self)
{
    fighter_t *cpu = &self->cpu;
    const character_t *data = &characters[cpu->character];
    const difficulty_t *difficulty = &difficulties[self->difficulty];
    int16_t distance = fighter_distance(&self->player, cpu);
    int16_t direction = cpu->x > self->player.x ? -1 : 1;
    uint32_t choice;
    cpu->moving = false;
    if (cpu->hurt_ms != 0U || cpu->block_stun_ms != 0U ||
        cpu->attack != ATTACK_NONE || cpu->landing_ms != 0U ||
        cpu->turning) return;
    if (self->cpu_retreat_ms != 0U) {
        self->cpu_retreat_ms = self->cpu_retreat_ms > FRAME_MS ?
            (uint16_t)(self->cpu_retreat_ms - FRAME_MS) : 0U;
        cpu->blocking = false;
        cpu->crouching = false;
        cpu->x = (int16_t)(cpu->x - direction * data->speed * self->scale);
        set_moving(cpu, true);
        return;
    }
    self->cpu_think_ms = (uint16_t)(self->cpu_think_ms + FRAME_MS);
    if (self->player.attack == ATTACK_NONE) {
        self->cpu_observed_attack = ATTACK_NONE;
        self->cpu_reaction_ms = 0U;
        self->cpu_defense_decided = false;
        cpu->blocking = false;
        cpu->crouching = false;
    } else {
        if (self->cpu_observed_attack != self->player.attack) {
            self->cpu_observed_attack = self->player.attack;
            self->cpu_reaction_ms = 0U;
            self->cpu_defense_decided = false;
            cpu->blocking = false;
        } else if (self->cpu_reaction_ms < UINT16_MAX - FRAME_MS) {
            self->cpu_reaction_ms =
                (uint16_t)(self->cpu_reaction_ms + FRAME_MS);
        }
        if (!self->cpu_defense_decided &&
            self->cpu_reaction_ms >= difficulty->reaction_ms) {
            self->cpu_defense_decided = true;
            cpu->blocking = distance < 65 * self->scale &&
                random_next(self) % 100U < difficulty->block_percent;
            cpu->crouching = cpu->blocking &&
                moves[self->player.attack].attack_level == ATTACK_LEVEL_LOW;
        }
    }
    if (cpu->blocking) return;
    if (distance > difficulty->preferred_distance * self->scale) {
        if (self->cpu_think_ms >= difficulty->decision_ms) {
            self->cpu_think_ms = 0U;
            choice = random_next(self) % 100U;
            if (cpu->energy >= moves[ATTACK_SPECIAL].energy_cost &&
                choice < difficulty->special_percent) {
                start_attack(self, cpu, ATTACK_SPECIAL);
                return;
            }
        }
        if (((self->round_left_ms / FRAME_MS) % 3U) != 0U) {
            cpu->x = (int16_t)(cpu->x +
                               direction * data->speed * self->scale);
            set_moving(cpu, true);
        }
        return;
    }
    if (self->cpu_think_ms < difficulty->decision_ms) return;
    self->cpu_think_ms = 0U;
    choice = random_next(self) % 100U;
    if (cpu->energy >= moves[ATTACK_SPECIAL].energy_cost &&
        choice < difficulty->special_percent)
        start_attack(self, cpu, ATTACK_SPECIAL);
    else if (choice < 55U)
        start_attack(self, cpu, ATTACK_LIGHT);
    else if (choice < 75U)
        start_attack(self, cpu, ATTACK_HEAVY);
    else if (choice < 83U)
        start_attack(self, cpu, ATTACK_KICK);
    else if (choice < 90U &&
             cpu->energy >= moves[ATTACK_COMBO].energy_cost)
        start_attack(self, cpu, ATTACK_COMBO);
    else
        start_attack(self, cpu, ATTACK_SWEEP);
}

static void finish_round(game_t *self, uint8_t result)
{
    if (self->screen != SCREEN_FIGHT) return;
    self->impact_ms = 0U;
    self->round_result = result;
    if (result == 1U) ++self->player_rounds;
    else if (result == 2U) ++self->cpu_rounds;
    send_fight_event(self, EVENT_ROUND_END, result);
    self->screen = SCREEN_ROUND_OVER;
    self->screen_ms = ROUND_OVER_MS;
}

static void advance_after_round(game_t *self)
{
    if (self->player_rounds >= 2U) {
        self->screen = SCREEN_ENDING;
        self->screen_ms = 0U;
        send_fight_event(self, EVENT_MUSIC, MUSIC_VICTORY);
    } else if (self->cpu_rounds >= 2U) {
        self->screen = SCREEN_GAME_OVER;
        self->screen_ms = 0U;
        send_fight_event(self, EVENT_MUSIC, MUSIC_DEFEAT);
    } else {
        reset_round(self);
        self->screen = SCREEN_INTRO;
        self->screen_ms = INTRO_MS;
    }
}

static void step_fight(game_t *self, uint16_t pressed)
{
    update_player(self, pressed);
    update_cpu(self);
    update_vertical(self, &self->player);
    update_vertical(self, &self->cpu);
    update_attack(self, &self->player, 0U);
    update_attack(self, &self->cpu, 1U);
    update_projectiles(self);
    constrain_fighters(self);
    update_facing(self);
    apply_melee_hit(self, &self->player, &self->cpu);
    apply_melee_hit(self, &self->cpu, &self->player);
    constrain_fighters(self);
    if (self->player.health == 0U && self->cpu.health == 0U)
        finish_round(self, 3U);
    else if (self->player.health == 0U) finish_round(self, 2U);
    else if (self->cpu.health == 0U) finish_round(self, 1U);
}

static void confirm_screen(game_t *self)
{
    if (self->screen == SCREEN_TITLE) {
        self->screen = SCREEN_DIFFICULTY;
        self->screen_ms = 0U;
        send_fight_event(self, EVENT_MUSIC, MUSIC_SELECT);
    } else if (self->screen == SCREEN_DIFFICULTY) {
        begin_match(self);
    } else if (self->screen == SCREEN_INTRO) {
        self->screen = SCREEN_FIGHT;
        self->screen_ms = 0U;
        send_fight_event(self, EVENT_ROUND_START, 0U);
    } else if (self->screen == SCREEN_ROUND_OVER) {
        advance_after_round(self);
    } else if (self->screen == SCREEN_ENDING ||
               self->screen == SCREEN_GAME_OVER) {
        self->screen = SCREEN_TITLE;
        self->screen_ms = 0U;
        send_fight_event(self, EVENT_MUSIC, MUSIC_TITLE);
    }
}

static void step_game(game_t *self)
{
    uint16_t pressed = (uint16_t)(self->input & ~self->previous_input);
    self->previous_input = self->input;
    if (self->screen == SCREEN_DIFFICULTY) {
        if ((pressed & KEY_LEFT) != 0U) {
            self->difficulty = self->difficulty == 0U ? 2U :
                               (uint8_t)(self->difficulty - 1U);
            send_fight_event(self, EVENT_MENU, 0U);
        } else if ((pressed & KEY_RIGHT) != 0U) {
            self->difficulty = self->difficulty == 2U ? 0U :
                               (uint8_t)(self->difficulty + 1U);
            send_fight_event(self, EVENT_MENU, 0U);
        }
    }
    if ((pressed & (KEY_LIGHT | KEY_START)) != 0U &&
        self->screen != SCREEN_FIGHT) {
        send_fight_event(self, EVENT_MENU, 1U);
        confirm_screen(self);
    }
    if (self->screen == SCREEN_FIGHT) {
        if ((self->input & KEY_PAUSE) != 0U) return;
        step_fight(self, pressed);
        if (self->screen == SCREEN_FIGHT) {
            self->round_left_ms = self->round_left_ms > FRAME_MS ?
                                  self->round_left_ms - FRAME_MS : 0U;
            if (self->round_left_ms == 0U)
            {
                uint32_t player_score =
                    (uint32_t)self->player.health *
                    characters[self->cpu.character].health;
                uint32_t cpu_score =
                    (uint32_t)self->cpu.health *
                    characters[self->player.character].health;
                finish_round(self, player_score > cpu_score ? 1U :
                    (player_score < cpu_score ? 2U : 3U));
            }
        }
    } else if (self->screen_ms != 0U) {
        if (self->screen == SCREEN_ROUND_OVER) {
            update_vertical(self, &self->player);
            update_vertical(self, &self->cpu);
            update_hurt(&self->player);
            update_hurt(&self->cpu);
        }
        self->screen_ms = self->screen_ms > FRAME_MS ?
                          (uint16_t)(self->screen_ms - FRAME_MS) : 0U;
        if (self->screen_ms == 0U) {
            if (self->screen == SCREEN_INTRO) {
                self->screen = SCREEN_FIGHT;
                send_fight_event(self, EVENT_ROUND_START, 0U);
            }
            else if (self->screen == SCREEN_ROUND_OVER) advance_after_round(self);
        }
    }
    if (self->combo_ms != 0U) {
        self->combo_ms = self->combo_ms > FRAME_MS ?
                         (uint16_t)(self->combo_ms - FRAME_MS) : 0U;
        if (self->combo_ms == 0U) self->combo = 0U;
    }
}

static gm_plugin_result_t plugin_start(void *opaque)
{
    game_t *self = opaque;
    gm_plugin_display_info_t display;
    gm_plugin_result_t result;
    uint32_t before;
    if (self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width < MIN_WIDTH || display.height < MIN_HEIGHT ||
        display.pixel_format != GM_PLUGIN_PIXEL_GRAY_4)
        return GM_PLUGIN_ENOTSUP;
    self->width = display.width;
    self->height = display.height;
    self->scale = display_scale(display.width, display.height);
    self->ground_y = (int16_t)(display.height - self->scale * 7U);
    self->random_state ^= self->host->monotonic_ms() | 1U;
    self->selected_character = 0U;
    self->cpu_character = 1U;
    self->difficulty = 1U;
    self->event_sequence = 0U;
    self->event_head = 0U;
    self->event_count = 0U;
    self->player_rounds = 0U;
    self->cpu_rounds = 0U;
    reset_round(self);
    self->screen = SCREEN_TITLE;
    self->screen_ms = 0U;
    self->input = 0U;
    self->previous_input = 0U;
    self->pause_reason = PAUSE_NONE;
    self->frame_accumulator = 0U;
    self->exit_hold_ms = 0U;
    self->input_active = false;
    self->holding_exit = false;
    if (self->host->log != 0)
        self->host->log("fighter_arena: start %ux%u scale=%u\n",
                        (unsigned int)self->width,
                        (unsigned int)self->height,
                        (unsigned int)self->scale);
    before = self->host->monotonic_ms();
    result = render(self);
    if (result == GM_PLUGIN_OK)
        send_fight_event(self, EVENT_MUSIC, MUSIC_TITLE);
    if (self->host->log != 0)
        self->host->log("fighter_arena: first frame result=%d time=%u ms\n",
                        (int)result,
                        (unsigned int)(self->host->monotonic_ms() - before));
    return result;
}

static void plugin_loop(void *opaque, uint32_t elapsed_ms)
{
    game_t *self = opaque;
    uint32_t now = self->host->monotonic_ms();
    bool render_needed = false;
    if (elapsed_ms > MAX_CATCHUP_MS) elapsed_ms = MAX_CATCHUP_MS;
    if (self->holding_exit) {
        self->exit_hold_ms += elapsed_ms;
        if (self->exit_hold_ms >= EXIT_HOLD_MS) {
            self->host->app_exit();
            return;
        }
        render_needed = true;
    }
    if (self->input_active && now - self->input_last_ms > INPUT_TIMEOUT_MS) {
        self->input = KEY_PAUSE;
        self->previous_input = KEY_PAUSE;
        self->pause_reason = PAUSE_INPUT_LOST;
        self->input_active = false;
    }
    self->frame_accumulator += elapsed_ms;
    while (self->frame_accumulator >= FRAME_MS) {
        self->frame_accumulator -= FRAME_MS;
        render_needed = true;
        if (self->impact_ms != 0U)
            self->impact_ms = self->impact_ms > FRAME_MS ?
                              (uint16_t)(self->impact_ms - FRAME_MS) : 0U;
        if (self->hit_stop_ms != 0U)
            self->hit_stop_ms = self->hit_stop_ms > FRAME_MS ?
                                (uint16_t)(self->hit_stop_ms - FRAME_MS) : 0U;
        else
            step_game(self);
    }
    if (render_needed) (void)render(self);
    flush_fight_event(self);
}

static bool plugin_event(void *opaque, const gm_plugin_event_t *event)
{
    game_t *self = opaque;
    const uint8_t *data;
    uint16_t buttons;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        if (!event->data.connection.connected) {
            self->input = KEY_PAUSE;
            self->previous_input = KEY_PAUSE;
            self->pause_reason = PAUSE_DISCONNECTED;
            self->input_active = false;
        }
        return true;
    }
    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != INPUT_CHANNEL ||
            event->data.bt.data == 0 || event->data.bt.length != INPUT_SIZE)
            return false;
        data = event->data.bt.data;
        if (data[0] != INPUT_VERSION) return false;
        buttons = (uint16_t)(((uint16_t)data[2] << 8) | data[3]);
        if ((buttons & UINT16_C(0xF000)) != 0U)
            return false;
        self->input = buttons;
        self->pause_reason = (buttons & KEY_PAUSE) != 0U ?
                             PAUSE_REMOTE : PAUSE_NONE;
        self->input_active = true;
        self->input_last_ms = self->host->monotonic_ms();
        return true;
    }
    if (event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_LONG ||
        event->data.button.action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
        if (!self->holding_exit) {
            self->holding_exit = true;
            self->exit_hold_ms = 0U;
            (void)render(self);
        }
        return true;
    }
    if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_RELEASE) {
        self->holding_exit = false;
        self->exit_hold_ms = 0U;
        (void)render(self);
        return true;
    }
    if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_SINGLE &&
        self->screen != SCREEN_FIGHT) {
        confirm_screen(self);
        return true;
    }
    return false;
}

static void plugin_stop(void *opaque)
{
    game_t *self = opaque;
    self->input = 0U;
    self->pause_reason = PAUSE_NONE;
    self->input_active = false;
    self->holding_exit = false;
    self->exit_hold_ms = 0U;
    self->event_head = 0U;
    self->event_count = 0U;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_DISPLAY_BITMAP |
                                              GM_PLUGIN_CAP_BLUETOOTH |
                                              GM_PLUGIN_CAP_BUTTON;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 ||
        host->display_get_info == 0 || host->monotonic_ms == 0 ||
        host->app_exit == 0 ||
        (host->capabilities & required) != required ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;
    game.host = host;
    if (gm_plugin_libc_get(host, &game.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    game.random_state = UINT32_C(0x4152454E);
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &game;
    plugin->on_start = plugin_start;
    plugin->on_loop = plugin_loop;
    plugin->on_event = plugin_event;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
