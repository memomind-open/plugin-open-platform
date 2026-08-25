#ifndef GM_SIMULATOR_H
#define GM_SIMULATOR_H

#include "gm_plugin.h"

#define SIM_DISPLAY_WIDTH 640U
#define SIM_DISPLAY_HEIGHT 360U
#define SIM_FRAME_MS 50U
#define SIM_MAX_PROJECTILES 3U

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} sim_rect_t;

typedef struct {
    sim_rect_t body;
    sim_rect_t hurtbox;
    sim_rect_t attack_range;
    uint16_t health;
    uint16_t energy;
    uint16_t guard;
    uint16_t attack_ms;
    uint16_t hurt_ms;
    uint16_t landing_ms;
    uint16_t turn_ms;
    int16_t velocity_y;
    uint8_t attack;
    uint8_t attack_phase;
    uint8_t hurt_kind;
    uint8_t render_frame;
    bool facing_right;
    bool moving;
    bool turning;
    bool blocking;
    bool crouching;
    bool hurt_right;
    bool active_range;
} sim_fighter_debug_t;

typedef struct {
    sim_rect_t collision;
    bool active;
} sim_projectile_debug_t;

typedef struct {
    sim_fighter_debug_t player;
    sim_fighter_debug_t cpu;
    sim_projectile_debug_t projectiles[SIM_MAX_PROJECTILES];
    uint32_t round_left_ms;
    uint16_t input;
    uint8_t screen;
    uint8_t difficulty;
    uint8_t player_rounds;
    uint8_t cpu_rounds;
    uint8_t combo;
    uint8_t pause_reason;
} sim_debug_snapshot_t;

gm_plugin_result_t sim_plugin_entry(const gm_plugin_host_api_t *host,
                                    gm_plugin_descriptor_t *plugin);
void sim_plugin_debug_snapshot(sim_debug_snapshot_t *snapshot);
gm_plugin_result_t sim_plugin_render(void);
uint8_t sim_plugin_test_display_scale(uint16_t width, uint16_t height);
bool sim_plugin_test_hurt_uses_source_direction(uint8_t character,
                                                bool hurt_right);
void sim_plugin_test_constrain_display(uint16_t width, uint16_t height,
                                       int16_t player_x, int16_t cpu_x,
                                       int16_t *player_result,
                                       int16_t *cpu_result);
void sim_plugin_test_reset(uint16_t player_health, uint16_t cpu_health,
                           uint16_t player_energy, uint16_t cpu_guard,
                           bool cpu_blocking, bool cpu_crouching);
void sim_plugin_test_prepare_player_win(void);
void sim_plugin_test_finish_round(uint8_t result);
void sim_plugin_test_set_cpu_hurt(uint8_t kind, uint16_t total_ms,
                                  uint16_t remaining_ms);
void sim_plugin_test_set_distance(int16_t distance);
void sim_plugin_test_set_cpu_attack(uint8_t attack, uint16_t attack_ms);
void sim_plugin_test_set_cpu_blocking(bool blocking);
void sim_plugin_test_enable_cpu(uint16_t energy);
void sim_plugin_test_swap_sides(void);

#endif
