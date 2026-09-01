#include "gm_plugin_lvgl_api.h"

#define COLS 24
#define ROWS 14
#define MAX_SNAKE (COLS * ROWS)
#define FRAME_MS 25U
#define START_STEP_MS 260U
#define MIN_STEP_MS 90U
#define SPEEDUP_MS 8U
#define VERTICAL_TRIGGER_THRESHOLD 55
#define VERTICAL_RETURN_THRESHOLD 25
#define HORIZONTAL_TRIGGER_THRESHOLD 40
#define HORIZONTAL_RETURN_THRESHOLD 20
#define GYRO_RELEASE_THRESHOLD 20
#define REARM_QUIET_MS 100U
#define SIDE_MARGIN 12
#define TOP_MARGIN 38
#define BOTTOM_MARGIN 10
#define MIN_CELL_SIZE 5

typedef enum {
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} direction_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} point_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *board;
    gm_plugin_lvgl_obj_t *score_label;
    gm_plugin_lvgl_obj_t *help_label;
    gm_plugin_lvgl_obj_t *message_label;
    gm_plugin_lvgl_obj_t *cells[ROWS][COLS];
    point_t snake[MAX_SNAKE];
    uint8_t shown[ROWS][COLS];
    point_t food;
    uint16_t length;
    uint16_t score;
    uint32_t random_state;
    uint32_t frame_accumulator;
    uint32_t step_accumulator;
    uint16_t step_ms;
    uint16_t rearm_quiet_ms;
    int16_t cell_size;
    int16_t board_width;
    int16_t board_height;
    direction_t direction;
    direction_t pending_direction;
    bool imu_armed;
    bool return_seen;
    bool turn_queued;
    direction_t gesture_direction;
    bool game_over;
} snake_game_t;

static snake_game_t game;

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void set_style(snake_game_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value, GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_box(snake_game_t *self, gm_plugin_lvgl_obj_t *object,
                      uint8_t fill, uint8_t border, uint8_t radius)
{
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xC0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_RADIUS, number(radius));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_TOP, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_LEFT, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT, number(0));
    self->ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
}

static void append_text(char **cursor, const char *text)
{
    while (*text != '\0') *(*cursor)++ = *text++;
}

static void append_uint(char **cursor, uint32_t value)
{
    char reverse[10];
    uint8_t count = 0;
    uint8_t index;
    do {
        reverse[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < sizeof(reverse));
    for (index = 0; index < count; ++index)
        *(*cursor)++ = reverse[count - index - 1U];
}

static uint32_t random_next(snake_game_t *self)
{
    self->random_state = self->random_state * UINT32_C(1664525) +
                         UINT32_C(1013904223);
    return self->random_state;
}

static bool snake_contains(const snake_game_t *self, uint8_t x, uint8_t y,
                           uint16_t count)
{
    uint16_t index;
    for (index = 0; index < count; ++index)
        if (self->snake[index].x == x && self->snake[index].y == y) return true;
    return false;
}

static void show_score(snake_game_t *self)
{
    char text[32];
    char *cursor = text;
    append_text(&cursor, "SCORE  ");
    append_uint(&cursor, self->score);
    *cursor = '\0';
    self->ui->label_set_text(self->score_label, text);
}

static void place_food(snake_game_t *self)
{
    uint16_t free_cells = (uint16_t)(MAX_SNAKE - self->length);
    uint16_t target;
    uint8_t y;
    uint8_t x;
    if (free_cells == 0U) return;
    target = (uint16_t)(random_next(self) % free_cells);
    for (y = 0; y < ROWS; ++y) {
        for (x = 0; x < COLS; ++x) {
            if (snake_contains(self, x, y, self->length)) continue;
            if (target == 0U) {
                self->food.x = x;
                self->food.y = y;
                return;
            }
            --target;
        }
    }
}

static void render(snake_game_t *self)
{
    uint8_t y;
    uint8_t x;
    for (y = 0; y < ROWS; ++y) {
        for (x = 0; x < COLS; ++x) {
            uint8_t state = 0;
            if (self->food.x == x && self->food.y == y) state = 2;
            if (snake_contains(self, x, y, self->length)) state = 1;
            if (self->shown[y][x] != state) {
                uint8_t shade = state == 1U ? 0xB0U :
                                (state == 2U ? 0xFFU : 0x08U);
                self->shown[y][x] = state;
                set_style(self, self->cells[y][x],
                          GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(shade));
            }
        }
    }
}

static void finish_game(snake_game_t *self)
{
    self->game_over = true;
    self->ui->label_set_text(self->message_label,
                             "GAME OVER\nClick: restart\nHold: exit");
    self->ui->obj_clear_flag(self->message_label,
                             GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static void reset_game(snake_game_t *self)
{
    uint16_t index;
    uint8_t y;
    uint8_t x;
    self->length = 5;
    for (index = 0; index < self->length; ++index) {
        self->snake[index].x = (uint8_t)(COLS / 2 - index);
        self->snake[index].y = ROWS / 2;
    }
    for (y = 0; y < ROWS; ++y)
        for (x = 0; x < COLS; ++x) self->shown[y][x] = 3;
    self->direction = DIR_RIGHT;
    self->pending_direction = DIR_RIGHT;
    self->score = 0;
    self->step_ms = START_STEP_MS;
    self->frame_accumulator = 0;
    self->step_accumulator = 0;
    self->rearm_quiet_ms = 0;
    self->imu_armed = false;
    self->return_seen = true;
    self->turn_queued = false;
    self->game_over = false;
    self->random_state ^= self->host->monotonic_ms() | 1U;
    self->ui->obj_add_flag(self->message_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    place_food(self);
    show_score(self);
    render(self);
}

static bool classify_motion(int32_t gyro_x, int32_t gyro_y, int32_t gyro_z,
                            direction_t *direction)
{
    int32_t horizontal = gyro_x - gyro_y;
    int32_t abs_horizontal = horizontal < 0 ? -horizontal : horizontal;
    int32_t abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    if (abs_z >= VERTICAL_TRIGGER_THRESHOLD && abs_z > abs_horizontal) {
        *direction = gyro_z > 0 ? DIR_UP : DIR_DOWN;
        return true;
    }
    if (abs_horizontal < HORIZONTAL_TRIGGER_THRESHOLD ||
        abs_horizontal <= abs_z) return false;
    *direction = horizontal < 0 ? DIR_LEFT : DIR_RIGHT;
    return true;
}

static void queue_direction(snake_game_t *self, direction_t direction)
{
    if (self->game_over || self->turn_queued || direction == self->direction ||
        (((uint8_t)direction + 2U) & 3U) == (uint8_t)self->direction)
        return;
    self->pending_direction = direction;
    self->turn_queued = true;
}

static void read_controls(snake_game_t *self, uint32_t elapsed_ms)
{
    gm_plugin_imu_sample_t imu;
    int32_t gyro_x;
    int32_t gyro_y;
    int32_t abs_x;
    int32_t abs_y;
    int32_t gyro_z;
    int32_t abs_z;
    int32_t horizontal;
    direction_t detected = DIR_UP;
    bool valid = false;
    if (self->host->imu_read(&imu) != GM_PLUGIN_OK) return;
    gyro_x = imu.gyro_raw[0];
    gyro_y = imu.gyro_raw[1];
    gyro_z = imu.gyro_raw[2];
    abs_x = gyro_x < 0 ? -gyro_x : gyro_x;
    abs_y = gyro_y < 0 ? -gyro_y : gyro_y;
    abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    /* Sensor X/Y are rotated relative to the wearer's horizontal axis. The
     * captured left/right motion projects with opposite signs, so subtracting
     * Y from X combines both useful components instead of gating each axis. */
    horizontal = gyro_x - gyro_y;
    valid = classify_motion(gyro_x, gyro_y, gyro_z, &detected);

    /* Startup is armed by a quiet sample. After a command, quiet at the outer
     * end of the motion must NOT rearm it: first wait for the opposite gyro
     * peak produced by returning to the absolute center. */
    if (!self->imu_armed) {
        /* A strong perpendicular motion is a new intentional command, not a
         * return. This permits LEFT -> UP even when the user returns slowly or
         * changes direction before reaching a perfectly quiet center pose. */
        if (!self->turn_queued && valid &&
            detected != self->gesture_direction &&
            (((uint8_t)detected + 2U) & 3U) !=
                (uint8_t)self->gesture_direction) {
            self->gesture_direction = detected;
            self->return_seen = false;
            self->rearm_quiet_ms = 0;
            queue_direction(self, detected);
            return;
        }
        if (!self->return_seen) {
            if ((self->gesture_direction == DIR_UP &&
                 gyro_z < -VERTICAL_RETURN_THRESHOLD) ||
                (self->gesture_direction == DIR_DOWN &&
                 gyro_z > VERTICAL_RETURN_THRESHOLD) ||
                (self->gesture_direction == DIR_LEFT &&
                 horizontal > HORIZONTAL_RETURN_THRESHOLD) ||
                (self->gesture_direction == DIR_RIGHT &&
                 horizontal < -HORIZONTAL_RETURN_THRESHOLD)) {
                self->return_seen = true;
                self->rearm_quiet_ms = 0;
            }
            return;
        }
        if (abs_x < GYRO_RELEASE_THRESHOLD &&
            abs_y < GYRO_RELEASE_THRESHOLD &&
            abs_z < GYRO_RELEASE_THRESHOLD) {
            uint32_t quiet = (uint32_t)self->rearm_quiet_ms + elapsed_ms;
            self->rearm_quiet_ms = quiet > REARM_QUIET_MS ?
                                   REARM_QUIET_MS : (uint16_t)quiet;
            if (self->rearm_quiet_ms >= REARM_QUIET_MS) {
                self->imu_armed = true;
                self->return_seen = false;
                self->rearm_quiet_ms = 0;
            }
        } else self->rearm_quiet_ms = 0;
        return;
    }
    if (self->turn_queued || self->game_over) return;

    if (!valid) return;

    self->gesture_direction = detected;
    self->imu_armed = false;
    self->return_seen = false;
    self->rearm_quiet_ms = 0;
    queue_direction(self, detected);
}

static void step(snake_game_t *self)
{
    point_t next = self->snake[0];
    bool eating;
    uint16_t collision_count;
    uint16_t index;
    self->direction = self->pending_direction;
    self->turn_queued = false;
    if (self->direction == DIR_UP) {
        if (next.y == 0U) { finish_game(self); return; }
        --next.y;
    } else if (self->direction == DIR_RIGHT) {
        if (next.x + 1U >= COLS) { finish_game(self); return; }
        ++next.x;
    } else if (self->direction == DIR_DOWN) {
        if (next.y + 1U >= ROWS) { finish_game(self); return; }
        ++next.y;
    } else {
        if (next.x == 0U) { finish_game(self); return; }
        --next.x;
    }
    eating = next.x == self->food.x && next.y == self->food.y;
    collision_count = eating ? self->length : (uint16_t)(self->length - 1U);
    if (snake_contains(self, next.x, next.y, collision_count)) {
        finish_game(self);
        return;
    }
    if (eating && self->length < MAX_SNAKE) ++self->length;
    for (index = self->length - 1U; index > 0U; --index)
        self->snake[index] = self->snake[index - 1U];
    self->snake[0] = next;
    if (eating) {
        self->score = (uint16_t)(self->score + 10U);
        if (self->step_ms > MIN_STEP_MS + SPEEDUP_MS)
            self->step_ms = (uint16_t)(self->step_ms - SPEEDUP_MS);
        show_score(self);
        if (self->length == MAX_SNAKE) {
            self->ui->label_set_text(self->message_label,
                                     "YOU WIN!\nClick: restart\nHold: exit");
            self->ui->obj_clear_flag(self->message_label,
                                     GM_PLUGIN_LVGL_FLAG_HIDDEN);
            self->game_over = true;
        } else {
            place_food(self);
        }
    }
    render(self);
}

static gm_plugin_result_t create_ui(snake_game_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *host_root = self->ui->root_get();
    int16_t available_width;
    int16_t available_height;
    int16_t board_x;
    int16_t message_width;
    int16_t message_height;
    uint8_t y;
    uint8_t x;
    if (host_root == 0 ||
        self->host->display_get_info(&display) != GM_PLUGIN_OK)
        return GM_PLUGIN_ESTATE;
    available_width = (int16_t)display.width - SIDE_MARGIN * 2;
    available_height = (int16_t)display.height - TOP_MARGIN - BOTTOM_MARGIN;
    self->cell_size = available_width / COLS;
    if (self->cell_size > available_height / ROWS)
        self->cell_size = available_height / ROWS;
    if (self->cell_size < MIN_CELL_SIZE) return GM_PLUGIN_ENOTSUP;
    self->board_width = self->cell_size * COLS;
    self->board_height = self->cell_size * ROWS;
    board_x = ((int16_t)display.width - self->board_width) / 2;
    self->ui->obj_clean(host_root);
    self->root = self->ui->obj_create(host_root);
    if (self->root == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->root, display.width, display.height);
    self->ui->obj_align(self->root, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->root, 0, 0, 0);
    self->board = self->ui->obj_create(self->root);
    self->score_label = self->ui->label_create(self->root);
    self->help_label = self->ui->label_create(self->root);
    self->message_label = self->ui->label_create(self->root);
    if (self->board == 0 || self->score_label == 0 || self->help_label == 0 ||
        self->message_label == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_pos(self->board, board_x, TOP_MARGIN);
    self->ui->obj_set_size(self->board, self->board_width, self->board_height);
    style_box(self, self->board, 0x08, 1, 0);
    self->ui->obj_set_pos(self->score_label, SIDE_MARGIN, 8);
    set_style(self, self->score_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    self->ui->label_set_text(self->help_label,
                             "Head/accessory: U/D/L/R  Hold: exit");
    self->ui->obj_align(self->help_label, GM_PLUGIN_LVGL_ALIGN_TOP_RIGHT,
                        -SIDE_MARGIN, 8);
    set_style(self, self->help_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0x90));
    message_width = (int16_t)display.width - SIDE_MARGIN * 2;
    if (message_width > 220) message_width = 220;
    message_height = (int16_t)display.height - SIDE_MARGIN * 2;
    if (message_height > 70) message_height = 70;
    self->ui->obj_set_size(self->message_label, message_width, message_height);
    self->ui->obj_align(self->message_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->message_label, 0x10, 1, 4);
    set_style(self, self->message_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    set_style(self, self->message_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    for (y = 0; y < ROWS; ++y) {
        for (x = 0; x < COLS; ++x) {
            gm_plugin_lvgl_obj_t *cell = self->ui->obj_create(self->board);
            if (cell == 0) return GM_PLUGIN_ENOMEM;
            self->cells[y][x] = cell;
            self->ui->obj_set_pos(cell, x * self->cell_size,
                                  y * self->cell_size);
            self->ui->obj_set_size(cell, self->cell_size - 1,
                                   self->cell_size - 1);
            style_box(self, cell, 0x08, 0, 1);
        }
    }
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t plugin_start(void *opaque)
{
    snake_game_t *self = opaque;
    gm_plugin_result_t result = self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
    if (result != GM_PLUGIN_OK) return result;
    result = create_ui(self);
    if (result == GM_PLUGIN_OK) reset_game(self);
    else {
        (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
        self->ui->obj_clean(self->ui->root_get());
    }
    return result;
}

static void plugin_loop(void *opaque, uint32_t elapsed_ms)
{
    snake_game_t *self = opaque;
    if (elapsed_ms > 150U) elapsed_ms = 150U;
    read_controls(self, elapsed_ms);
    if (self->game_over) return;
    self->frame_accumulator += elapsed_ms;
    if (self->frame_accumulator < FRAME_MS) return;
    self->step_accumulator += self->frame_accumulator;
    self->frame_accumulator = 0;
    while (self->step_accumulator >= self->step_ms && !self->game_over) {
        self->step_accumulator -= self->step_ms;
        step(self);
    }
}

static bool plugin_event(void *opaque, const gm_plugin_event_t *event)
{
    snake_game_t *self = opaque;
    gm_plugin_button_action_t action;
    gm_plugin_button_t button;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    action = event->data.button.action;
    button = event->data.button.button;
    if (action == GM_PLUGIN_BUTTON_ACTION_LONG ||
        action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
        self->host->app_exit();
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_SINGLE) {
        if (self->game_over) {
            reset_game(self);
            return true;
        }
        return false;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_TRIGGER) {
        switch (button) {
        case GM_PLUGIN_BUTTON_UP:
            queue_direction(self, DIR_UP);
            return true;
        case GM_PLUGIN_BUTTON_DOWN:
            queue_direction(self, DIR_DOWN);
            return true;
        case GM_PLUGIN_BUTTON_LEFT:
            queue_direction(self, DIR_LEFT);
            return true;
        case GM_PLUGIN_BUTTON_RIGHT:
            queue_direction(self, DIR_RIGHT);
            return true;
        default:
            return false;
        }
    }
    return false;
}

static void plugin_suspend(void *opaque)
{
    snake_game_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
}

static void plugin_resume(void *opaque)
{
    snake_game_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
    self->imu_armed = false;
    self->return_seen = true;
    self->rearm_quiet_ms = 0;
    self->turn_queued = false;
    self->frame_accumulator = 0;
    self->step_accumulator = 0;
}

static void plugin_stop(void *opaque)
{
    snake_game_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    self->ui->obj_clean(self->ui->root_get());
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_IMU_RAW |
                                              GM_PLUGIN_CAP_BUTTON;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required ||
        host->graphics.lvgl == 0 || host->display_get_info == 0 ||
        host->monotonic_ms == 0 || host->imu_enable == 0 ||
        host->imu_read == 0 || host->app_exit == 0)
        return GM_PLUGIN_EVERSION;
    game.host = host;
    game.ui = host->graphics.lvgl;
    if (game.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(game.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    game.random_state = UINT32_C(0x534E414B);
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &game;
    plugin->on_start = plugin_start;
    plugin->on_resume = plugin_resume;
    plugin->on_loop = plugin_loop;
    plugin->on_event = plugin_event;
    plugin->on_suspend = plugin_suspend;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
