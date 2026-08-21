#include "gm_plugin_lvgl_api.h"

#define BOARD_SIZE 4U
#define CELL_COUNT (BOARD_SIZE * BOARD_SIZE)
#define SCREEN_MARGIN 12
#define HEADER_HEIGHT 42
#define FOOTER_HEIGHT 30
#define CELL_GAP 4
#define MIN_CELL_SIZE 42
#define VERTICAL_TRIGGER_THRESHOLD 55
#define VERTICAL_RETURN_THRESHOLD 20
#define HORIZONTAL_TRIGGER_THRESHOLD 40
#define HORIZONTAL_RETURN_THRESHOLD 18
#define GYRO_QUIET_THRESHOLD 20
#define REARM_QUIET_MS 100U

typedef enum {
    MOVE_UP,
    MOVE_RIGHT,
    MOVE_DOWN,
    MOVE_LEFT
} move_direction_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *board_object;
    gm_plugin_lvgl_obj_t *score_label;
    gm_plugin_lvgl_obj_t *help_label;
    gm_plugin_lvgl_obj_t *message_label;
    gm_plugin_lvgl_obj_t *cells[BOARD_SIZE][BOARD_SIZE];
    gm_plugin_lvgl_obj_t *cell_labels[BOARD_SIZE][BOARD_SIZE];
    uint32_t board[BOARD_SIZE][BOARD_SIZE];
    uint32_t shown[BOARD_SIZE][BOARD_SIZE];
    uint32_t score;
    uint32_t random_state;
    int16_t screen_width;
    int16_t screen_height;
    int16_t board_pixels;
    int16_t cell_size;
    uint16_t rearm_quiet_ms;
    move_direction_t gesture_direction;
    bool imu_armed;
    bool return_seen;
    bool action_pending;
    bool game_over;
} game_2048_t;

static game_2048_t game;

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void set_style(game_2048_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value,
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_box(game_2048_t *self, gm_plugin_lvgl_obj_t *object,
                      uint8_t fill, uint8_t border, uint8_t radius)
{
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xA0));
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

static uint32_t random_next(game_2048_t *self)
{
    self->random_state = self->random_state * UINT32_C(1664525) +
                         UINT32_C(1013904223);
    return self->random_state;
}

static void show_score(game_2048_t *self)
{
    char text[32];
    char *cursor = text;
    append_text(&cursor, "SCORE  ");
    append_uint(&cursor, self->score);
    *cursor = '\0';
    self->ui->label_set_text(self->score_label, text);
}

static uint8_t tile_shade(uint32_t value)
{
    uint8_t exponent = 0;
    while (value > 1U && exponent < 15U) {
        value >>= 1;
        ++exponent;
    }
    if (exponent == 0U) return 0x12;
    if (exponent >= 11U) return 0xFF;
    return (uint8_t)(0x30U + exponent * 18U);
}

static void render(game_2048_t *self)
{
    uint8_t y;
    uint8_t x;
    for (y = 0; y < BOARD_SIZE; ++y) {
        for (x = 0; x < BOARD_SIZE; ++x) {
            uint32_t value = self->board[y][x];
            char text[12];
            char *cursor = text;
            uint8_t shade;
            if (self->shown[y][x] == value) continue;
            self->shown[y][x] = value;
            shade = tile_shade(value);
            set_style(self, self->cells[y][x],
                      GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(shade));
            if (value != 0U) append_uint(&cursor, value);
            *cursor = '\0';
            self->ui->label_set_text(self->cell_labels[y][x], text);
            self->ui->obj_align(self->cell_labels[y][x],
                                GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
            set_style(self, self->cell_labels[y][x],
                      GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                      color(shade >= 0xB0U ? 0x00U : 0xFFU));
        }
    }
}

static bool add_random_tile(game_2048_t *self)
{
    uint8_t free_count = 0;
    uint8_t target;
    uint8_t y;
    uint8_t x;
    for (y = 0; y < BOARD_SIZE; ++y)
        for (x = 0; x < BOARD_SIZE; ++x)
            if (self->board[y][x] == 0U) ++free_count;
    if (free_count == 0U) return false;
    target = (uint8_t)(random_next(self) % free_count);
    for (y = 0; y < BOARD_SIZE; ++y) {
        for (x = 0; x < BOARD_SIZE; ++x) {
            if (self->board[y][x] != 0U) continue;
            if (target == 0U) {
                self->board[y][x] =
                    random_next(self) % 10U == 0U ? 4U : 2U;
                return true;
            }
            --target;
        }
    }
    return false;
}

static bool moves_available(const game_2048_t *self)
{
    uint8_t y;
    uint8_t x;
    for (y = 0; y < BOARD_SIZE; ++y) {
        for (x = 0; x < BOARD_SIZE; ++x) {
            uint32_t value = self->board[y][x];
            if (value == 0U) return true;
            if (x + 1U < BOARD_SIZE && self->board[y][x + 1U] == value)
                return true;
            if (y + 1U < BOARD_SIZE && self->board[y + 1U][x] == value)
                return true;
        }
    }
    return false;
}

static void merge_line(game_2048_t *self, uint32_t line[BOARD_SIZE])
{
    uint32_t compact[BOARD_SIZE] = {0U, 0U, 0U, 0U};
    uint8_t input;
    uint8_t count = 0;
    uint8_t output = 0;
    for (input = 0; input < BOARD_SIZE; ++input)
        if (line[input] != 0U) compact[count++] = line[input];
    input = 0;
    while (input < count) {
        if (input + 1U < count && compact[input] == compact[input + 1U]) {
            line[output] = compact[input] * 2U;
            self->score += line[output];
            input = (uint8_t)(input + 2U);
        } else {
            line[output] = compact[input++];
        }
        ++output;
    }
    while (output < BOARD_SIZE) line[output++] = 0U;
}

static bool move_board(game_2048_t *self, move_direction_t direction)
{
    uint8_t outer;
    uint8_t index;
    bool changed = false;
    for (outer = 0; outer < BOARD_SIZE; ++outer) {
        uint32_t original[BOARD_SIZE];
        uint32_t line[BOARD_SIZE];
        for (index = 0; index < BOARD_SIZE; ++index) {
            uint8_t y;
            uint8_t x;
            if (direction == MOVE_LEFT || direction == MOVE_RIGHT) {
                y = outer;
                x = direction == MOVE_LEFT ? index :
                    (uint8_t)(BOARD_SIZE - 1U - index);
            } else {
                x = outer;
                y = direction == MOVE_UP ? index :
                    (uint8_t)(BOARD_SIZE - 1U - index);
            }
            original[index] = self->board[y][x];
            line[index] = original[index];
        }
        merge_line(self, line);
        for (index = 0; index < BOARD_SIZE; ++index) {
            uint8_t y;
            uint8_t x;
            if (line[index] != original[index]) changed = true;
            if (direction == MOVE_LEFT || direction == MOVE_RIGHT) {
                y = outer;
                x = direction == MOVE_LEFT ? index :
                    (uint8_t)(BOARD_SIZE - 1U - index);
            } else {
                x = outer;
                y = direction == MOVE_UP ? index :
                    (uint8_t)(BOARD_SIZE - 1U - index);
            }
            self->board[y][x] = line[index];
        }
    }
    return changed;
}

static void reset_game(game_2048_t *self)
{
    uint8_t y;
    uint8_t x;
    for (y = 0; y < BOARD_SIZE; ++y) {
        for (x = 0; x < BOARD_SIZE; ++x) {
            self->board[y][x] = 0U;
            self->shown[y][x] = UINT32_MAX;
        }
    }
    self->score = 0U;
    self->game_over = false;
    self->imu_armed = false;
    self->return_seen = true;
    self->rearm_quiet_ms = 0U;
    self->action_pending = false;
    self->random_state ^= self->host->monotonic_ms() | 1U;
    self->ui->obj_add_flag(self->message_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    (void)add_random_tile(self);
    (void)add_random_tile(self);
    show_score(self);
    render(self);
}

static void apply_move(game_2048_t *self, move_direction_t direction)
{
    uint32_t score_before;
    if (self->game_over) return;
    score_before = self->score;
    if (move_board(self, direction)) {
        (void)add_random_tile(self);
        show_score(self);
        render(self);
    } else {
        self->score = score_before;
    }
    if (!moves_available(self)) {
        self->game_over = true;
        self->ui->label_set_text(self->message_label,
                                 "GAME OVER\nClick to restart");
        self->ui->obj_clear_flag(self->message_label,
                                 GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
}

static bool classify_motion(int32_t gyro_x, int32_t gyro_y, int32_t gyro_z,
                            move_direction_t *direction)
{
    int32_t horizontal = gyro_x - gyro_y;
    int32_t abs_horizontal = horizontal < 0 ? -horizontal : horizontal;
    int32_t abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    if (abs_z >= VERTICAL_TRIGGER_THRESHOLD && abs_z > abs_horizontal) {
        *direction = gyro_z > 0 ? MOVE_UP : MOVE_DOWN;
        return true;
    }
    if (abs_horizontal < HORIZONTAL_TRIGGER_THRESHOLD ||
        abs_horizontal <= abs_z)
        return false;
    *direction = horizontal < 0 ? MOVE_LEFT : MOVE_RIGHT;
    return true;
}

static void read_controls(game_2048_t *self, uint32_t elapsed_ms)
{
    gm_plugin_imu_sample_t imu;
    int32_t gyro_x;
    int32_t gyro_y;
    int32_t gyro_z;
    int32_t abs_x;
    int32_t abs_y;
    int32_t abs_z;
    int32_t horizontal;
    move_direction_t detected = MOVE_UP;
    if (self->host->imu_read(&imu) != GM_PLUGIN_OK) return;
    gyro_x = imu.gyro_raw[0];
    gyro_y = imu.gyro_raw[1];
    gyro_z = imu.gyro_raw[2];
    abs_x = gyro_x < 0 ? -gyro_x : gyro_x;
    abs_y = gyro_y < 0 ? -gyro_y : gyro_y;
    abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    horizontal = gyro_x - gyro_y;

    if (self->imu_armed) {
        if (!self->game_over &&
            classify_motion(gyro_x, gyro_y, gyro_z, &detected)) {
            self->gesture_direction = detected;
            self->imu_armed = false;
            self->return_seen = false;
            self->rearm_quiet_ms = 0U;
            self->action_pending = true;
        }
        return;
    }

    /* The opposite angular-velocity peak belongs to returning the head to its
     * neutral pose. Consume it only as an unlock signal; never as a move. */
    if (!self->return_seen) {
        if ((self->gesture_direction == MOVE_UP &&
             gyro_z < -VERTICAL_RETURN_THRESHOLD) ||
            (self->gesture_direction == MOVE_DOWN &&
             gyro_z > VERTICAL_RETURN_THRESHOLD) ||
            (self->gesture_direction == MOVE_LEFT &&
             horizontal > HORIZONTAL_RETURN_THRESHOLD) ||
            (self->gesture_direction == MOVE_RIGHT &&
             horizontal < -HORIZONTAL_RETURN_THRESHOLD)) {
            self->return_seen = true;
            self->rearm_quiet_ms = 0U;
        }
        return;
    }

    if (abs_x < GYRO_QUIET_THRESHOLD && abs_y < GYRO_QUIET_THRESHOLD &&
        abs_z < GYRO_QUIET_THRESHOLD) {
        uint32_t quiet = (uint32_t)self->rearm_quiet_ms + elapsed_ms;
        self->rearm_quiet_ms = quiet >= REARM_QUIET_MS ?
                               REARM_QUIET_MS : (uint16_t)quiet;
        if (self->rearm_quiet_ms >= REARM_QUIET_MS) {
            if (self->action_pending && !self->game_over)
                apply_move(self, self->gesture_direction);
            self->action_pending = false;
            self->imu_armed = true;
            self->return_seen = false;
            self->rearm_quiet_ms = 0U;
        }
    } else {
        self->rearm_quiet_ms = 0U;
    }
}

static gm_plugin_result_t create_ui(game_2048_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *host_root = self->ui->root_get();
    int16_t available_width;
    int16_t available_height;
    int16_t board_x;
    int16_t board_y;
    int16_t message_width;
    uint8_t y;
    uint8_t x;
    if (host_root == 0 ||
        self->host->display_get_info(&display) != GM_PLUGIN_OK)
        return GM_PLUGIN_ESTATE;
    self->screen_width = (int16_t)display.width;
    self->screen_height = (int16_t)display.height;
    available_width = self->screen_width - SCREEN_MARGIN * 2;
    available_height = self->screen_height - HEADER_HEIGHT -
                       FOOTER_HEIGHT - SCREEN_MARGIN;
    self->cell_size = (available_width - CELL_GAP * 5) / 4;
    if (self->cell_size > (available_height - CELL_GAP * 5) / 4)
        self->cell_size = (available_height - CELL_GAP * 5) / 4;
    if (self->cell_size < MIN_CELL_SIZE) return GM_PLUGIN_ENOTSUP;
    self->board_pixels = self->cell_size * 4 + CELL_GAP * 5;
    board_x = (self->screen_width - self->board_pixels) / 2;
    board_y = HEADER_HEIGHT;
    self->ui->obj_clean(host_root);
    self->root = self->ui->obj_create(host_root);
    if (self->root == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->root, self->screen_width,
                           self->screen_height);
    self->ui->obj_align(self->root, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->root, 0x00, 0, 0);
    self->board_object = self->ui->obj_create(self->root);
    self->score_label = self->ui->label_create(self->root);
    self->help_label = self->ui->label_create(self->root);
    self->message_label = self->ui->label_create(self->root);
    if (self->board_object == 0 || self->score_label == 0 ||
        self->help_label == 0 || self->message_label == 0)
        return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_pos(self->board_object, board_x, board_y);
    self->ui->obj_set_size(self->board_object, self->board_pixels,
                           self->board_pixels);
    style_box(self, self->board_object, 0x20, 1, 5);
    self->ui->obj_set_pos(self->score_label, SCREEN_MARGIN, 10);
    set_style(self, self->score_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    self->ui->label_set_text(self->help_label,
                             "Head: up / down / left / right");
    self->ui->obj_align(self->help_label, GM_PLUGIN_LVGL_ALIGN_TOP_RIGHT,
                        -SCREEN_MARGIN, 10);
    set_style(self, self->help_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xA0));
    message_width = self->screen_width - SCREEN_MARGIN * 2;
    if (message_width > 240) message_width = 240;
    self->ui->obj_set_size(self->message_label, message_width, 72);
    self->ui->obj_align(self->message_label, GM_PLUGIN_LVGL_ALIGN_CENTER,
                        0, 0);
    style_box(self, self->message_label, 0x08, 2, 5);
    set_style(self, self->message_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    set_style(self, self->message_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    for (y = 0; y < BOARD_SIZE; ++y) {
        for (x = 0; x < BOARD_SIZE; ++x) {
            gm_plugin_lvgl_obj_t *cell;
            gm_plugin_lvgl_obj_t *label;
            cell = self->ui->obj_create(self->board_object);
            if (cell == 0) return GM_PLUGIN_ENOMEM;
            label = self->ui->label_create(cell);
            if (label == 0) return GM_PLUGIN_ENOMEM;
            self->cells[y][x] = cell;
            self->cell_labels[y][x] = label;
            self->ui->obj_set_pos(cell,
                CELL_GAP + x * (self->cell_size + CELL_GAP),
                CELL_GAP + y * (self->cell_size + CELL_GAP));
            self->ui->obj_set_size(cell, self->cell_size, self->cell_size);
            style_box(self, cell, 0x12, 0, 4);
            self->ui->obj_align(label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
            set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                      number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
        }
    }
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t plugin_start(void *opaque)
{
    game_2048_t *self = opaque;
    gm_plugin_result_t result =
        self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
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
    game_2048_t *self = opaque;
    if (elapsed_ms > 150U) elapsed_ms = 150U;
    read_controls(self, elapsed_ms);
}

static bool plugin_event(void *opaque, const gm_plugin_event_t *event)
{
    game_2048_t *self = opaque;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_BUTTON) {
        gm_plugin_button_action_t action = event->data.button.action;
        if (action == GM_PLUGIN_BUTTON_ACTION_LONG ||
            action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
            self->host->app_exit();
            return true;
        }
        if (action == GM_PLUGIN_BUTTON_ACTION_SINGLE && self->game_over) {
            reset_game(self);
            return true;
        }
    }
    return false;
}

static void plugin_suspend(void *opaque)
{
    game_2048_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
}

static void plugin_resume(void *opaque)
{
    game_2048_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
    self->imu_armed = false;
    self->return_seen = true;
    self->rearm_quiet_ms = 0U;
    self->action_pending = false;
}

static void plugin_stop(void *opaque)
{
    game_2048_t *self = opaque;
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
        host->graphics.lvgl == 0 || host->display_get_info == 0 ||
        host->monotonic_ms == 0 || host->imu_enable == 0 ||
        host->imu_read == 0 ||
        host->app_exit == 0 ||
        (host->capabilities & required) != required ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;
    game.host = host;
    game.ui = host->graphics.lvgl;
    if (game.ui == 0 ||
        game.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(game.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    game.random_state = UINT32_C(0x32303438);
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &game;
    plugin->on_start = plugin_start;
    plugin->on_loop = plugin_loop;
    plugin->on_event = plugin_event;
    plugin->on_suspend = plugin_suspend;
    plugin->on_resume = plugin_resume;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
