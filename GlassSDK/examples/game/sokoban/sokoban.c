#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"
#include "sokoban_levels.h"
#include "sokoban_sprites.h"

#define COLS 10U
#define ROWS SOKOBAN_LEVEL_ROWS
#define LEVEL_COUNT SOKOBAN_LEVEL_COUNT
#define MAX_BOXES 5U
#define HISTORY_DEPTH 64U
#define FRAME_MS 25U
#define VERTICAL_TRIGGER_THRESHOLD 55
#define VERTICAL_RETURN_THRESHOLD 25
#define HORIZONTAL_TRIGGER_THRESHOLD 40
#define HORIZONTAL_RETURN_THRESHOLD 20
#define GYRO_RELEASE_THRESHOLD 20
#define REARM_QUIET_MS 100U
#define SIDE_MARGIN 10
#define HEADER_HEIGHT 34
#define FOOTER_HEIGHT 28
#define CELL_GAP 2
#define MIN_CELL_SIZE 18
#define EXIT_HOLD_MS 2000U

typedef enum {
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} direction_t;

typedef enum {
    CELL_FLOOR,
    CELL_WALL,
    CELL_GOAL,
    CELL_BOX,
    CELL_BOX_ON_GOAL,
    CELL_PLAYER,
    CELL_PLAYER_ON_GOAL
} cell_state_t;

typedef struct {
    uint16_t boxes[ROWS];
    uint16_t moves;
    uint16_t pushes;
    uint8_t player_x;
    uint8_t player_y;
} history_entry_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *board;
    gm_plugin_lvgl_obj_t *status_label;
    gm_plugin_lvgl_obj_t *help_label;
    gm_plugin_lvgl_obj_t *message_label;
    gm_plugin_lvgl_obj_t *exit_arc;
    gm_plugin_lvgl_obj_t *cells[ROWS][COLS];
    gm_plugin_lvgl_obj_t *cell_labels[ROWS][COLS];
    gm_plugin_lvgl_obj_t *box_sprites[MAX_BOXES];
    gm_plugin_lvgl_obj_t *player_sprite;
    uint16_t walls[ROWS];
    uint16_t goals[ROWS];
    uint16_t boxes[ROWS];
    history_entry_t history[HISTORY_DEPTH];
    uint8_t shown[ROWS][COLS];
    uint16_t moves;
    uint16_t pushes;
    uint32_t frame_accumulator;
    uint32_t exit_hold_ms;
    uint16_t rearm_quiet_ms;
    int16_t cell_size;
    int16_t board_width;
    int16_t board_height;
    uint8_t player_x;
    uint8_t player_y;
    uint8_t level_index;
    uint8_t history_count;
    direction_t gesture_direction;
    bool imu_armed;
    bool return_seen;
    bool level_complete;
    bool holding_exit;
} sokoban_game_t;

static sokoban_game_t game;


#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static uint16_t cell_bit(uint8_t x)
{
    return (uint16_t)(UINT16_C(1) << x);
}

static bool row_has(const uint16_t rows[ROWS], uint8_t x, uint8_t y)
{
    return (rows[y] & cell_bit(x)) != 0U;
}

static void set_style(sokoban_game_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value,
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_box(sokoban_game_t *self, gm_plugin_lvgl_obj_t *object,
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

static void update_status(sokoban_game_t *self)
{
    char text[64];
    self->libc->snprintf(text, sizeof(text),
                         "LEVEL %u/%u   MOVES %u   PUSHES %u",
                         (unsigned int)self->level_index + 1U,
                         (unsigned int)LEVEL_COUNT,
                         (unsigned int)self->moves,
                         (unsigned int)self->pushes);
    self->ui->label_set_text(self->status_label, text);
}

static cell_state_t get_cell_state(const sokoban_game_t *self,
                                   uint8_t x, uint8_t y)
{
    bool goal;
    if (row_has(self->walls, x, y)) return CELL_WALL;
    goal = row_has(self->goals, x, y);
    if (row_has(self->boxes, x, y))
        return goal ? CELL_BOX_ON_GOAL : CELL_BOX;
    if (self->player_x == x && self->player_y == y)
        return goal ? CELL_PLAYER_ON_GOAL : CELL_PLAYER;
    return goal ? CELL_GOAL : CELL_FLOOR;
}

static void render_cell(sokoban_game_t *self, uint8_t x, uint8_t y,
                        cell_state_t state)
{
    gm_plugin_lvgl_obj_t *cell = self->cells[y][x];
    gm_plugin_lvgl_obj_t *label = self->cell_labels[y][x];
    const char *text = "";
    uint8_t fill = 0x08U;
    uint8_t text_color = 0xD0U;
    uint8_t border = 0U;
    bool goal = state == CELL_GOAL || state == CELL_BOX_ON_GOAL ||
                state == CELL_PLAYER_ON_GOAL;

    if (state == CELL_WALL) {
        fill = 0x48U;
        border = 1U;
    } else if (goal) {
        text = "+";
        fill = 0x10U;
        border = 1U;
    }

    set_style(self, cell, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(self, cell, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(self, cell, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
              color(goal ? 0xD0U : 0x90U));
    set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(text_color));
    self->ui->label_set_text(label, text);
    self->ui->obj_align(label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
}

static gm_plugin_lvgl_obj_t *create_piece_sprite(
    sokoban_game_t *self, const sokoban_sprite_rect_t *rectangles,
    size_t rectangle_count)
{
    gm_plugin_lvgl_obj_t *sprite = self->ui->obj_create(self->board);
    int16_t content_size = self->cell_size - 4;
    size_t index;
    if (sprite == 0) return 0;
    self->ui->obj_set_size(sprite, self->cell_size, self->cell_size);
    style_box(self, sprite, 0x00, 0, 0);
    set_style(self, sprite, GM_PLUGIN_LVGL_STYLE_BG_OPA,
              number(GM_PLUGIN_LVGL_OPA_TRANSPARENT));
    for (index = 0U; index < rectangle_count; ++index) {
        const sokoban_sprite_rect_t *source = &rectangles[index];
        gm_plugin_lvgl_obj_t *part = self->ui->obj_create(sprite);
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
        if (part == 0) return 0;
        left = (int16_t)(2 + source->x * content_size /
                              (int16_t)SOKOBAN_SPRITE_SIZE);
        top = (int16_t)(2 + source->y * content_size /
                             (int16_t)SOKOBAN_SPRITE_SIZE);
        right = (int16_t)(2 + (source->x + source->width) * content_size /
                               (int16_t)SOKOBAN_SPRITE_SIZE);
        bottom = (int16_t)(2 + (source->y + source->height) * content_size /
                                (int16_t)SOKOBAN_SPRITE_SIZE);
        self->ui->obj_set_pos(part, left, top);
        self->ui->obj_set_size(part, right > left ? right - left : 1,
                               bottom > top ? bottom - top : 1);
        style_box(self, part, source->gray, 0, 0);
    }
    return sprite;
}

static void position_piece(gm_plugin_lvgl_obj_t *sprite,
                           sokoban_game_t *self, uint8_t x, uint8_t y)
{
    self->ui->obj_set_pos(sprite,
        CELL_GAP + x * (self->cell_size + CELL_GAP),
        CELL_GAP + y * (self->cell_size + CELL_GAP));
    self->ui->obj_clear_flag(sprite, GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static void update_piece_sprites(sokoban_game_t *self)
{
    uint8_t box_index = 0U;
    uint8_t y;
    uint8_t x;
    for (y = 0U; y < ROWS; ++y) {
        for (x = 0U; x < COLS; ++x) {
            if (!row_has(self->boxes, x, y)) continue;
            if (box_index < MAX_BOXES)
                position_piece(self->box_sprites[box_index], self, x, y);
            ++box_index;
        }
    }
    while (box_index < MAX_BOXES) {
        self->ui->obj_add_flag(self->box_sprites[box_index],
                               GM_PLUGIN_LVGL_FLAG_HIDDEN);
        ++box_index;
    }
    position_piece(self->player_sprite, self,
                   self->player_x, self->player_y);
}

static void render(sokoban_game_t *self)
{
    uint8_t y;
    uint8_t x;
    for (y = 0U; y < ROWS; ++y) {
        for (x = 0U; x < COLS; ++x) {
            cell_state_t state = get_cell_state(self, x, y);
            if (self->shown[y][x] == (uint8_t)state) continue;
            self->shown[y][x] = (uint8_t)state;
            render_cell(self, x, y, state);
        }
    }
    update_piece_sprites(self);
}

static bool boxes_on_goals(const sokoban_game_t *self)
{
    uint8_t y;
    for (y = 0U; y < ROWS; ++y)
        if ((self->boxes[y] & (uint16_t)~self->goals[y]) != 0U)
            return false;
    return true;
}

static void reset_recognizer(sokoban_game_t *self)
{
    self->imu_armed = false;
    self->return_seen = true;
    self->rearm_quiet_ms = 0U;
    self->frame_accumulator = 0U;
}

static bool load_level(sokoban_game_t *self, uint8_t index)
{
    const sokoban_level_t *level;
    uint8_t y;
    uint8_t x;
    uint8_t box_count = 0U;
    uint8_t goal_count = 0U;
    if (index >= LEVEL_COUNT) return false;
    level = &sokoban_levels[index];
    self->libc->memcpy(self->walls, level->walls, sizeof(self->walls));
    self->libc->memcpy(self->goals, level->goals, sizeof(self->goals));
    self->libc->memcpy(self->boxes, level->boxes, sizeof(self->boxes));
    self->player_x = level->player_x;
    self->player_y = level->player_y;
    for (y = 0U; y < ROWS; ++y) {
        for (x = 0U; x < COLS; ++x) {
            if (row_has(self->goals, x, y)) ++goal_count;
            if (row_has(self->boxes, x, y)) ++box_count;
        }
    }
    if (self->player_x >= COLS || self->player_y >= ROWS ||
        row_has(self->walls, self->player_x, self->player_y) ||
        box_count == 0U || box_count > MAX_BOXES ||
        box_count != goal_count)
        return false;

    self->level_index = index;
    self->moves = 0U;
    self->pushes = 0U;
    self->history_count = 0U;
    self->level_complete = false;
    self->holding_exit = false;
    self->exit_hold_ms = 0U;
    self->libc->memset(self->shown, 0xFF, sizeof(self->shown));
    self->ui->obj_add_flag(self->message_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    reset_recognizer(self);
    update_status(self);
    render(self);
    return true;
}

static void save_history(sokoban_game_t *self)
{
    history_entry_t *entry;
    if (self->history_count == HISTORY_DEPTH) {
        self->libc->memmove(&self->history[0], &self->history[1],
                            (HISTORY_DEPTH - 1U) * sizeof(self->history[0]));
        self->history_count = HISTORY_DEPTH - 1U;
    }
    entry = &self->history[self->history_count++];
    self->libc->memcpy(entry->boxes, self->boxes, sizeof(self->boxes));
    entry->moves = self->moves;
    entry->pushes = self->pushes;
    entry->player_x = self->player_x;
    entry->player_y = self->player_y;
}

static void undo_move(sokoban_game_t *self)
{
    history_entry_t *entry;
    if (self->level_complete || self->history_count == 0U) return;
    entry = &self->history[--self->history_count];
    self->libc->memcpy(self->boxes, entry->boxes, sizeof(self->boxes));
    self->moves = entry->moves;
    self->pushes = entry->pushes;
    self->player_x = entry->player_x;
    self->player_y = entry->player_y;
    update_status(self);
    render(self);
}

static void finish_level(sokoban_game_t *self)
{
    self->level_complete = true;
    if (self->level_index + 1U < LEVEL_COUNT)
        self->ui->label_set_text(self->message_label,
                                 "LEVEL CLEAR!\nClick: next level");
    else
        self->ui->label_set_text(self->message_label,
                                 "ALL LEVELS CLEAR!\nClick: play again");
    self->ui->obj_clear_flag(self->message_label,
                             GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static bool apply_move(sokoban_game_t *self, direction_t direction)
{
    static const int8_t dx[4] = {0, 1, 0, -1};
    static const int8_t dy[4] = {-1, 0, 1, 0};
    int16_t next_x;
    int16_t next_y;
    int16_t beyond_x = 0;
    int16_t beyond_y = 0;
    bool pushing;

    if (self->level_complete) return false;
    next_x = (int16_t)self->player_x + dx[direction];
    next_y = (int16_t)self->player_y + dy[direction];
    if (next_x < 0 || next_y < 0 || next_x >= (int16_t)COLS ||
        next_y >= (int16_t)ROWS ||
        row_has(self->walls, (uint8_t)next_x, (uint8_t)next_y))
        return false;

    pushing = row_has(self->boxes, (uint8_t)next_x, (uint8_t)next_y);
    if (pushing) {
        beyond_x = next_x + dx[direction];
        beyond_y = next_y + dy[direction];
        if (beyond_x < 0 || beyond_y < 0 ||
            beyond_x >= (int16_t)COLS || beyond_y >= (int16_t)ROWS ||
            row_has(self->walls, (uint8_t)beyond_x, (uint8_t)beyond_y) ||
            row_has(self->boxes, (uint8_t)beyond_x, (uint8_t)beyond_y))
            return false;
    }

    save_history(self);
    if (pushing) {
        self->boxes[next_y] &= (uint16_t)~cell_bit((uint8_t)next_x);
        self->boxes[beyond_y] |= cell_bit((uint8_t)beyond_x);
        ++self->pushes;
    }
    self->player_x = (uint8_t)next_x;
    self->player_y = (uint8_t)next_y;
    ++self->moves;
    update_status(self);
    render(self);
    if (boxes_on_goals(self)) finish_level(self);
    return true;
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

static void read_controls(sokoban_game_t *self, uint32_t elapsed_ms)
{
    gm_plugin_imu_sample_t imu;
    int32_t gyro_x;
    int32_t gyro_y;
    int32_t gyro_z;
    int32_t abs_x;
    int32_t abs_y;
    int32_t abs_z;
    int32_t horizontal;
    direction_t detected = DIR_UP;
    bool valid;

    if (self->host->imu_read(&imu) != GM_PLUGIN_OK) return;
    gyro_x = imu.gyro_raw[0];
    gyro_y = imu.gyro_raw[1];
    gyro_z = imu.gyro_raw[2];
    abs_x = gyro_x < 0 ? -gyro_x : gyro_x;
    abs_y = gyro_y < 0 ? -gyro_y : gyro_y;
    abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    horizontal = gyro_x - gyro_y;
    valid = classify_motion(gyro_x, gyro_y, gyro_z, &detected);

    if (!self->imu_armed) {
        if (!self->level_complete && valid &&
            detected != self->gesture_direction &&
            (((uint8_t)detected + 2U) & 3U) !=
                (uint8_t)self->gesture_direction) {
            self->gesture_direction = detected;
            self->return_seen = false;
            self->rearm_quiet_ms = 0U;
            (void)apply_move(self, detected);
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
                self->rearm_quiet_ms = 0U;
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
                self->rearm_quiet_ms = 0U;
            }
        } else self->rearm_quiet_ms = 0U;
        return;
    }
    if (self->level_complete || !valid) return;
    self->gesture_direction = detected;
    self->imu_armed = false;
    self->return_seen = false;
    self->rearm_quiet_ms = 0U;
    (void)apply_move(self, detected);
}

static gm_plugin_result_t create_ui(sokoban_game_t *self)
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
    available_width = (int16_t)display.width - SIDE_MARGIN * 2;
    available_height = (int16_t)display.height - HEADER_HEIGHT -
                       FOOTER_HEIGHT;
    self->cell_size = (available_width - CELL_GAP * (COLS + 1U)) / COLS;
    if (self->cell_size > (int16_t)
        ((available_height - CELL_GAP * (int16_t)(ROWS + 1U)) /
         (int16_t)ROWS))
        self->cell_size =
            (available_height - CELL_GAP * (int16_t)(ROWS + 1U)) /
            (int16_t)ROWS;
    if (self->cell_size < MIN_CELL_SIZE) return GM_PLUGIN_ENOTSUP;
    self->board_width = self->cell_size * COLS + CELL_GAP * (COLS + 1U);
    self->board_height = self->cell_size * ROWS + CELL_GAP * (ROWS + 1U);
    board_x = ((int16_t)display.width - self->board_width) / 2;
    board_y = HEADER_HEIGHT +
              (available_height - self->board_height) / 2;

    self->ui->obj_clean(host_root);
    self->root = self->ui->obj_create(host_root);
    if (self->root == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->root, display.width, display.height);
    self->ui->obj_align(self->root, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->root, 0x00, 0, 0);
    self->board = self->ui->obj_create(self->root);
    self->status_label = self->ui->label_create(self->root);
    self->help_label = self->ui->label_create(self->root);
    self->message_label = self->ui->label_create(self->root);
    if (self->board == 0 || self->status_label == 0 ||
        self->help_label == 0 || self->message_label == 0)
        return GM_PLUGIN_ENOMEM;

    self->ui->obj_set_pos(self->board, board_x, board_y);
    self->ui->obj_set_size(self->board, self->board_width,
                           self->board_height);
    style_box(self, self->board, 0x04, 1, 3);
    self->ui->obj_set_pos(self->status_label, SIDE_MARGIN, 8);
    set_style(self, self->status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    self->ui->label_set_text(self->help_label,
                             "Head/accessory: move   Click: undo   2x: reset");
    self->ui->obj_align(self->help_label, GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID,
                        0, -6);
    set_style(self, self->help_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0x90));
    message_width = (int16_t)display.width - SIDE_MARGIN * 2;
    if (message_width > 240) message_width = 240;
    self->ui->obj_set_size(self->message_label, message_width, 76);
    self->ui->obj_align(self->message_label, GM_PLUGIN_LVGL_ALIGN_CENTER,
                        0, 0);
    style_box(self, self->message_label, 0x08, 2, 5);
    set_style(self, self->message_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    set_style(self, self->message_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));

    for (y = 0U; y < ROWS; ++y) {
        for (x = 0U; x < COLS; ++x) {
            gm_plugin_lvgl_obj_t *cell = self->ui->obj_create(self->board);
            gm_plugin_lvgl_obj_t *label;
            if (cell == 0) return GM_PLUGIN_ENOMEM;
            label = self->ui->label_create(cell);
            if (label == 0) return GM_PLUGIN_ENOMEM;
            self->cells[y][x] = cell;
            self->cell_labels[y][x] = label;
            self->ui->obj_set_pos(cell,
                CELL_GAP + x * (self->cell_size + CELL_GAP),
                CELL_GAP + y * (self->cell_size + CELL_GAP));
            self->ui->obj_set_size(cell, self->cell_size, self->cell_size);
            style_box(self, cell, 0x08, 0, 2);
            self->ui->obj_align(label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
            set_style(self, label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                      number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
        }
    }

    for (x = 0U; x < MAX_BOXES; ++x) {
        self->box_sprites[x] = create_piece_sprite(
            self, sokoban_box_sprite, SOKOBAN_BOX_SPRITE_RECT_COUNT);
        if (self->box_sprites[x] == 0) return GM_PLUGIN_ENOMEM;
        self->ui->obj_add_flag(self->box_sprites[x],
                               GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
    self->player_sprite = create_piece_sprite(
        self, sokoban_player_sprite, SOKOBAN_PLAYER_SPRITE_RECT_COUNT);
    if (self->player_sprite == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_add_flag(self->player_sprite,
                           GM_PLUGIN_LVGL_FLAG_HIDDEN);

    self->exit_arc = self->ui->arc_create(self->root);
    if (self->exit_arc == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->exit_arc, 48, 48);
    self->ui->obj_align(self->exit_arc, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    self->ui->arc_set_range(self->exit_arc, 0, EXIT_HOLD_MS);
    self->ui->arc_set_value(self->exit_arc, 0);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_WIDTH,
                        number(4), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_COLOR,
                        color(0x20), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_WIDTH,
                        number(4), GM_PLUGIN_LVGL_SELECTOR_INDICATOR);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_COLOR,
                        color(0xF0), GM_PLUGIN_LVGL_SELECTOR_INDICATOR);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_OPA,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_KNOB);
    self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t plugin_start(void *opaque)
{
    sokoban_game_t *self = opaque;
    gm_plugin_result_t result = self->host->imu_enable(
        GM_PLUGIN_IMU_ENABLE_RAW);
    if (result != GM_PLUGIN_OK) return result;
    result = create_ui(self);
    if (result == GM_PLUGIN_OK && !load_level(self, 0U))
        result = GM_PLUGIN_ESTATE;
    if (result != GM_PLUGIN_OK) {
        (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
        self->ui->obj_clean(self->ui->root_get());
    }
    return result;
}

static void plugin_loop(void *opaque, uint32_t elapsed_ms)
{
    sokoban_game_t *self = opaque;
    if (elapsed_ms > 150U) elapsed_ms = 150U;
    if (self->holding_exit) {
        self->exit_hold_ms += elapsed_ms;
        if (self->exit_hold_ms >= EXIT_HOLD_MS) {
            self->host->app_exit();
            return;
        }
        self->ui->arc_set_value(self->exit_arc,
                                (int16_t)self->exit_hold_ms);
    }
    self->frame_accumulator += elapsed_ms;
    if (self->frame_accumulator < FRAME_MS) return;
    read_controls(self, self->frame_accumulator);
    self->frame_accumulator = 0U;
}

static bool plugin_event(void *opaque, const gm_plugin_event_t *event)
{
    sokoban_game_t *self = opaque;
    gm_plugin_button_action_t action;
    gm_plugin_button_t button;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    action = event->data.button.action;
    button = event->data.button.button;
    if (action == GM_PLUGIN_BUTTON_ACTION_LONG ||
        action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
        if (!self->holding_exit) {
            self->holding_exit = true;
            self->exit_hold_ms = 0U;
            self->ui->arc_set_value(self->exit_arc, 0);
            self->ui->obj_clear_flag(self->exit_arc,
                                     GM_PLUGIN_LVGL_FLAG_HIDDEN);
        }
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_RELEASE) {
        self->holding_exit = false;
        self->exit_hold_ms = 0U;
        self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_SINGLE) {
        if (self->level_complete) {
            uint8_t next = (uint8_t)(self->level_index + 1U);
            if (next >= LEVEL_COUNT) next = 0U;
            (void)load_level(self, next);
        } else undo_move(self);
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_DOUBLE) {
        (void)load_level(self, self->level_index);
        return true;
    }
    if (action != GM_PLUGIN_BUTTON_ACTION_TRIGGER) return false;
    switch (button) {
    case GM_PLUGIN_BUTTON_UP:
        (void)apply_move(self, DIR_UP);
        return true;
    case GM_PLUGIN_BUTTON_DOWN:
        (void)apply_move(self, DIR_DOWN);
        return true;
    case GM_PLUGIN_BUTTON_LEFT:
        (void)apply_move(self, DIR_LEFT);
        return true;
    case GM_PLUGIN_BUTTON_RIGHT:
        (void)apply_move(self, DIR_RIGHT);
        return true;
    default:
        return false;
    }
}

static void plugin_suspend(void *opaque)
{
    sokoban_game_t *self = opaque;
    self->holding_exit = false;
    self->exit_hold_ms = 0U;
    self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
}

static void plugin_resume(void *opaque)
{
    sokoban_game_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
    reset_recognizer(self);
}

static void plugin_stop(void *opaque)
{
    sokoban_game_t *self = opaque;
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
        host->imu_enable == 0 || host->imu_read == 0 ||
        host->app_exit == 0)
        return GM_PLUGIN_EVERSION;
    game.host = host;
    game.ui = host->graphics.lvgl;
    if (gm_plugin_libc_get(host, &game.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    if (game.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(game.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
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
