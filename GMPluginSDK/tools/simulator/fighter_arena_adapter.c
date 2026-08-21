#include <string.h>

#include "simulator.h"

/* Compile the production game source directly into the simulator. This keeps
 * the simulator on the same combat and rendering implementation as the GMP. */
#include "../../examples/game/fighter_arena/fighter_arena.c"

static uint8_t attack_phase(const fighter_t *fighter)
{
    if (fighter->attack == ATTACK_NONE) return 0U;
    if (fighter->attack_ms < attack_start(fighter)) return 1U;
    if (fighter->attack_ms <= attack_active_end(fighter)) return 2U;
    return 3U;
}

static void copy_fighter_debug(const game_t *self, const fighter_t *fighter,
                               sim_fighter_debug_t *debug)
{
    int16_t reach = 0;
    int16_t center;
    memset(debug, 0, sizeof(*debug));
    debug->body.x = fighter->x;
    debug->body.y = fighter->y;
    debug->body.width = (int16_t)(FIGHTER_W * self->scale);
    debug->body.height = (int16_t)(FIGHTER_H * self->scale);
    debug->hurtbox.x = (int16_t)(fighter->x + 8 * self->scale);
    debug->hurtbox.y = (int16_t)(fighter->y + 6 * self->scale);
    debug->hurtbox.width = (int16_t)((FIGHTER_W - 16) * self->scale);
    debug->hurtbox.height = (int16_t)((FIGHTER_H - 8) * self->scale);
    debug->health = fighter->health;
    debug->energy = fighter->energy;
    debug->guard = fighter->guard;
    debug->attack_ms = fighter->attack_ms;
    debug->hurt_ms = fighter->hurt_ms;
    debug->landing_ms = fighter->landing_ms;
    debug->turn_ms = fighter->turn_ms;
    debug->velocity_y = fighter->velocity_y;
    debug->attack = fighter->attack;
    debug->attack_phase = attack_phase(fighter);
    debug->hurt_kind = fighter->hurt_kind;
    debug->render_frame = fighter_render_frame(fighter);
    debug->facing_right = fighter->facing_right;
    debug->moving = fighter->moving;
    debug->turning = fighter->turning;
    debug->blocking = fighter->blocking;
    debug->crouching = fighter->crouching;
    debug->hurt_right = fighter->hurt_right;
    debug->active_range = fighter->attack != ATTACK_NONE &&
                          fighter->attack != ATTACK_SPECIAL &&
                          attack_active(fighter);
    if (fighter->attack == ATTACK_NONE ||
        fighter->attack == ATTACK_SPECIAL) return;
    reach = (int16_t)(moves[fighter->attack].reach * self->scale *
        characters[fighter->character].reach_percent / 100U);
    center = fighter->x;
    debug->attack_range.x = fighter->facing_right ? center :
                            (int16_t)(center - reach);
    debug->attack_range.y = (int16_t)(fighter->y + 18 * self->scale);
    debug->attack_range.width = reach;
    debug->attack_range.height = (int16_t)(54 * self->scale);
}

gm_plugin_result_t sim_plugin_entry(const gm_plugin_host_api_t *host,
                                    gm_plugin_descriptor_t *plugin)
{
    return gm_plugin_entry(host, plugin);
}

void sim_plugin_debug_snapshot(sim_debug_snapshot_t *snapshot)
{
    uint8_t index;
    if (snapshot == NULL) return;
    memset(snapshot, 0, sizeof(*snapshot));
    copy_fighter_debug(&game, &game.player, &snapshot->player);
    copy_fighter_debug(&game, &game.cpu, &snapshot->cpu);
    for (index = 0U; index < PROJECTILE_COUNT; ++index) {
        const projectile_t *projectile = &game.projectiles[index];
        const fighter_t *defender = projectile->owner == 0U ?
                                    &game.cpu : &game.player;
        sim_projectile_debug_t *debug = &snapshot->projectiles[index];
        if (!projectile->active) continue;
        debug->active = true;
        debug->collision.x = (int16_t)(defender->x +
            FIGHTER_W * game.scale / 2 - 14 * game.scale);
        debug->collision.y = (int16_t)(defender->y +
            30 * game.scale - 25 * game.scale);
        debug->collision.width = (int16_t)(28 * game.scale);
        debug->collision.height = (int16_t)(50 * game.scale);
    }
    snapshot->round_left_ms = game.round_left_ms;
    snapshot->input = game.input;
    snapshot->screen = game.screen;
    snapshot->difficulty = game.difficulty;
    snapshot->player_rounds = game.player_rounds;
    snapshot->cpu_rounds = game.cpu_rounds;
    snapshot->combo = game.combo;
    snapshot->pause_reason = game.pause_reason;
}

gm_plugin_result_t sim_plugin_render(void)
{
    return render(&game);
}

uint8_t sim_plugin_test_display_scale(uint16_t width, uint16_t height)
{
    return display_scale(width, height);
}

void sim_plugin_test_constrain_display(uint16_t width, uint16_t height,
                                       int16_t player_x, int16_t cpu_x,
                                       int16_t *player_result,
                                       int16_t *cpu_result)
{
    game_t test;
    int16_t floor;
    memset(&test, 0, sizeof(test));
    test.width = width;
    test.height = height;
    test.scale = display_scale(width, height);
    test.ground_y = (int16_t)(height - test.scale * 7U);
    floor = (int16_t)(test.ground_y - FIGHTER_H * test.scale);
    test.player.x = player_x;
    test.cpu.x = cpu_x;
    test.player.y = floor;
    test.cpu.y = floor;
    constrain_fighters(&test);
    *player_result = test.player.x;
    *cpu_result = test.cpu.x;
}

void sim_plugin_test_reset(uint16_t player_health, uint16_t cpu_health,
                           uint16_t player_energy, uint16_t cpu_guard,
                           bool cpu_blocking, bool cpu_crouching)
{
    int16_t floor = (int16_t)(game.ground_y - FIGHTER_H * game.scale);
    game.selected_character = 0U;
    game.cpu_character = 1U;
    game.difficulty = 0U;
    reset_round(&game);
    game.screen = SCREEN_FIGHT;
    game.screen_ms = 0U;
    game.player.x = (int16_t)(game.width / 2U - 100U);
    game.cpu.x = (int16_t)(game.player.x + 80U);
    game.player.y = floor;
    game.cpu.y = floor;
    game.player.health = player_health;
    game.cpu.health = cpu_health;
    game.player.energy = player_energy;
    game.cpu.energy = 0U;
    game.cpu.guard = cpu_guard;
    game.cpu.blocking = cpu_blocking;
    game.cpu.crouching = cpu_crouching;
    /* Keep focused tests deterministic without changing production AI code. */
    game.cpu.hurt_ms = 0U;
    game.cpu.hurt_total_ms = 0U;
    game.cpu.hurt_kind = HURT_NONE;
    game.cpu.block_stun_ms = 5000U;
    game.cpu.guard_recovery_ms = 0U;
    game.cpu.attack = ATTACK_NONE;
    game.cpu_think_ms = 0U;
    game.player.attack = ATTACK_NONE;
    game.player.attack_ms = 0U;
    game.player.hurt_ms = 0U;
    game.player.block_stun_ms = 0U;
    game.player.blocking = false;
    game.player.crouching = false;
    game.player.velocity_y = 0;
    game.player.landing_ms = 0U;
    game.player.turn_ms = 0U;
    game.player.turning = false;
    game.player.moving = false;
    game.input = 0U;
    game.previous_input = 0U;
    game.pause_reason = PAUSE_NONE;
    game.input_active = false;
    game.frame_accumulator = 0U;
    game.round_left_ms = ROUND_MS;
    game.player_rounds = 0U;
    game.cpu_rounds = 0U;
    game.combo = 0U;
    game.max_combo = 0U;
    game.combo_ms = 0U;
    game.hit_stop_ms = 0U;
    game.impact_ms = 0U;
    clear_projectiles(&game);
    constrain_fighters(&game);
    (void)render(&game);
}

void sim_plugin_test_prepare_player_win(void)
{
    int16_t floor = (int16_t)(game.ground_y - FIGHTER_H * game.scale);
    game.player.x = (int16_t)(game.width / 2U - 100U);
    game.cpu.x = (int16_t)(game.player.x + 80U);
    game.player.y = floor;
    game.cpu.y = floor;
    game.player.attack = ATTACK_NONE;
    game.player.attack_ms = 0U;
    game.player.hurt_ms = 0U;
    game.player.hurt_kind = HURT_NONE;
    game.player.blocking = false;
    game.player.crouching = false;
    game.cpu.health = moves[ATTACK_LIGHT].damage;
    game.cpu.attack = ATTACK_NONE;
    game.cpu.attack_ms = 0U;
    game.cpu.hurt_ms = 5000U;
    game.cpu.hurt_total_ms = 5000U;
    game.cpu.hurt_kind = HURT_LIGHT;
    game.cpu.blocking = false;
    game.cpu.crouching = false;
    game.input = 0U;
    game.previous_input = 0U;
    game.frame_accumulator = 0U;
    constrain_fighters(&game);
    (void)render(&game);
}

void sim_plugin_test_finish_round(uint8_t result)
{
    finish_round(&game, result);
    (void)render(&game);
}

void sim_plugin_test_set_cpu_hurt(uint8_t kind, uint16_t total_ms,
                                  uint16_t remaining_ms)
{
    game.cpu.hurt_kind = kind;
    game.cpu.hurt_total_ms = total_ms;
    game.cpu.hurt_ms = remaining_ms;
    game.cpu.blocking = false;
    game.cpu.crouching = false;
    (void)render(&game);
}

void sim_plugin_test_set_distance(int16_t distance)
{
    game.cpu.x = (int16_t)(game.player.x + distance);
    constrain_fighters(&game);
    (void)render(&game);
}

void sim_plugin_test_set_cpu_attack(uint8_t attack, uint16_t attack_ms)
{
    game.cpu.block_stun_ms = 0U;
    game.cpu.hurt_ms = 0U;
    game.cpu.attack = attack;
    game.cpu.attack_ms = attack_ms;
    game.cpu.hit_landed = false;
    game.cpu.blocking = false;
    game.cpu.crouching = false;
    (void)render(&game);
}

void sim_plugin_test_set_cpu_blocking(bool blocking)
{
    game.cpu.blocking = blocking;
    game.cpu.crouching = false;
    game.cpu.block_stun_ms = 5000U;
}

void sim_plugin_test_enable_cpu(uint16_t energy)
{
    game.cpu.block_stun_ms = 0U;
    game.cpu.energy = energy;
    game.cpu_think_ms = 0U;
    game.cpu_retreat_ms = 0U;
    game.random_state = UINT32_C(1);
    (void)render(&game);
}

void sim_plugin_test_swap_sides(void)
{
    game.cpu.x = (int16_t)(game.width / 2U - 80U);
    game.player.x = (int16_t)(game.cpu.x + 80U);
    game.player.facing_right = false;
    game.cpu.facing_right = true;
    game.player.turning = false;
    game.cpu.turning = false;
    game.player.turn_ms = 0U;
    game.cpu.turn_ms = 0U;
    constrain_fighters(&game);
    (void)render(&game);
}
