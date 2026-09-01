#include "gm_plugin_lvgl_api.h"
#include "jet_runner_translations.h"

#define OBSTACLE_COUNT 4
#define DASH_COUNT 9
#define FRAME_MS 50U
#define MAX_DELTA_MS 100U
#define Q 256
#define HEADER_H 38
#define FOOTER_H 32
#define MARGIN 10
#define PLAYER_X 72
#define PLAYER_W 30
#define PLAYER_H 30
#define HIT_X 5
#define HIT_Y 2
#define HIT_W 22
#define HIT_H 26
#define OBSTACLE_W 23
#define OBSTACLE_SPACING 190
#define GAP_H 90
#define GAP_MARGIN 20
#define MIN_BOARD_W (PLAYER_X + PLAYER_W + OBSTACLE_W + 20)
#define MIN_BOARD_H (GAP_H + GAP_MARGIN * 2)
#define PITCH_DEAD_ZONE 2
#define PITCH_LIMIT 38
#define PITCH_TO_PIXEL 3
#define ACCESSORY_PLAYER_STEP 18
#define BASE_SPEED_Q (125 * Q)
#define SPEED_STEP_Q (4 * Q)
#define MAX_SPEED_Q (220 * Q)

typedef struct {
    int32_t x_q;
    int16_t gap_top;
    bool passed;
    gm_plugin_lvgl_obj_t *column;
    gm_plugin_lvgl_obj_t *top;
    gm_plugin_lvgl_obj_t *bottom;
} obstacle_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const jet_runner_strings_t *strings;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *board;
    gm_plugin_lvgl_obj_t *player;
    gm_plugin_lvgl_obj_t *flame;
    gm_plugin_lvgl_obj_t *flame_core;
    gm_plugin_lvgl_obj_t *score_label;
    gm_plugin_lvgl_obj_t *end_label;
    gm_plugin_lvgl_obj_t *exit_arc;
    obstacle_t obstacles[OBSTACLE_COUNT];
    int32_t player_y_q;
    int16_t width;
    int16_t height;
    int16_t board_w;
    int16_t board_h;
    int16_t neutral_pitch;
    int16_t accessory_offset_y;
    uint16_t score;
    uint32_t random_state;
    uint32_t frame_accumulator;
    uint32_t hold_ms;
    bool game_over;
    bool calibrated;
    bool flame_long;
    bool holding_exit;
} jet_t;

static jet_t game;

static bool tag_matches(const char *left, const char *right,
                        bool primary_only)
{
    while (*left != '\0' && *right != '\0') {
        if (primary_only &&
            (*left == '-' || *left == '_' || *right == '-' || *right == '_'))
            break;
        if (*left++ != *right++) return false;
    }
    if (primary_only)
        return (*left == '\0' || *left == '-' || *left == '_') &&
               (*right == '\0' || *right == '-' || *right == '_');
    return *left == '\0' && *right == '\0';
}

static const jet_runner_strings_t *select_strings(const char *tag)
{
    uint32_t index;
    uint32_t count = sizeof(jet_runner_translations) /
                     sizeof(jet_runner_translations[0]);
    for (index = 0; index < count; ++index)
        if (tag_matches(tag, jet_runner_translations[index].tag, false))
            return &jet_runner_translations[index];
    for (index = 0; index < count; ++index)
        if (tag_matches(tag, jet_runner_translations[index].tag, true))
            return &jet_runner_translations[index];
    return &jet_runner_translations[1];
}

static void append_text(char **cursor, const char *text)
{
    while (*text != '\0') *(*cursor)++ = *text++;
}

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void set_style(jet_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value, GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_rect(jet_t *self, gm_plugin_lvgl_obj_t *object,
                       uint8_t fill, uint8_t opacity,
                       uint8_t border, uint8_t radius)
{
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(opacity));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xFF));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_RADIUS, number(radius));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_TOP, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_LEFT, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT, number(0));
    self->ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
}

static gm_plugin_lvgl_obj_t *rectangle(jet_t *self,
                                       gm_plugin_lvgl_obj_t *parent,
                                       int16_t x, int16_t y,
                                       int16_t width, int16_t height,
                                       uint8_t fill, uint8_t opacity)
{
    gm_plugin_lvgl_obj_t *object = self->ui->obj_create(parent);
    if (object != 0) {
        self->ui->obj_set_pos(object, x, y);
        self->ui->obj_set_size(object, width, height);
        style_rect(self, object, fill, opacity, 0, 0);
    }
    return object;
}

static uint32_t random_next(jet_t *self)
{
    self->random_state = self->random_state * UINT32_C(1664525) +
                         UINT32_C(1013904223);
    return self->random_state;
}

static int16_t random_gap(jet_t *self)
{
    int16_t maximum = self->board_h - GAP_H - GAP_MARGIN;
    uint16_t range;
    if (maximum < GAP_MARGIN) return (self->board_h - GAP_H) / 2;
    range = (uint16_t)(maximum - GAP_MARGIN + 1);
    return (int16_t)(GAP_MARGIN + random_next(self) % range);
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

static void show_score(jet_t *self)
{
    char text[40];
    char *cursor = text;
    append_text(&cursor, self->strings->score);
    append_uint(&cursor, self->score);
    *cursor = '\0';
    self->ui->label_set_text(self->score_label, text);
}

static void position_player(jet_t *self)
{
    int16_t player_y = (int16_t)(self->player_y_q / Q);
    self->ui->obj_set_pos(self->player, PLAYER_X, player_y);
    self->ui->obj_set_pos(self->flame,
                          PLAYER_X + (self->flame_long ? -9 : -4),
                          player_y + 12);
    self->ui->obj_set_pos(self->flame_core,
                          PLAYER_X + (self->flame_long ? -6 : -1),
                          player_y + 13);
}

static void layout_obstacle(jet_t *self, obstacle_t *obstacle)
{
    int16_t bottom_y = obstacle->gap_top + GAP_H;
    self->ui->obj_set_pos(obstacle->top, 0, 0);
    self->ui->obj_set_size(obstacle->top, OBSTACLE_W, obstacle->gap_top);
    self->ui->obj_set_pos(obstacle->bottom, 0, bottom_y);
    self->ui->obj_set_size(obstacle->bottom, OBSTACLE_W,
                           self->board_h - bottom_y);
}

static int32_t rightmost_x(const jet_t *self)
{
    int32_t rightmost = 0;
    uint8_t index;
    for (index = 0; index < OBSTACLE_COUNT; ++index)
        if (self->obstacles[index].x_q > rightmost)
            rightmost = self->obstacles[index].x_q;
    return rightmost;
}

static void reset_game(jet_t *self)
{
    uint8_t index;
    self->player_y_q = ((self->board_h - PLAYER_H) / 2) * Q;
    self->score = 0;
    self->game_over = false;
    self->calibrated = false;
    self->accessory_offset_y = 0;
    self->frame_accumulator = 0;
    self->hold_ms = 0;
    self->holding_exit = false;
    self->random_state ^= self->host->monotonic_ms() | 1U;
    self->ui->obj_add_flag(self->end_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->flame_long = false;
    self->ui->obj_set_size(self->flame, 8, 4);
    position_player(self);
    for (index = 0; index < OBSTACLE_COUNT; ++index) {
        obstacle_t *obstacle = &self->obstacles[index];
        obstacle->x_q = (self->board_w + 70 +
                         index * OBSTACLE_SPACING) * Q;
        obstacle->gap_top = random_gap(self);
        obstacle->passed = false;
        layout_obstacle(self, obstacle);
        self->ui->obj_set_pos(obstacle->column,
                              (int16_t)(obstacle->x_q / Q), 0);
    }
    show_score(self);
}

static void finish(jet_t *self)
{
    char text[160];
    char *cursor = text;
    self->game_over = true;
    append_text(&cursor, self->strings->collision);
    append_uint(&cursor, self->score);
    append_text(&cursor, self->strings->restart);
    *cursor = '\0';
    self->ui->label_set_text(self->end_label, text);
    self->ui->obj_clear_flag(self->end_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static bool collision(const jet_t *self, const obstacle_t *obstacle)
{
    int16_t player_y = (int16_t)(self->player_y_q / Q);
    int16_t obstacle_x = (int16_t)(obstacle->x_q / Q);
    int16_t player_left = PLAYER_X + HIT_X;
    int16_t player_right = player_left + HIT_W;
    int16_t player_top = player_y + HIT_Y;
    int16_t player_bottom = player_top + HIT_H;
    if (player_right <= obstacle_x ||
        player_left >= obstacle_x + OBSTACLE_W) return false;
    return player_top < obstacle->gap_top ||
           player_bottom > obstacle->gap_top + GAP_H;
}

static void update_player(jet_t *self, const gm_plugin_imu_sample_t *imu)
{
    int16_t delta;
    int32_t target;
    int32_t maximum = (self->board_h - PLAYER_H) * Q;
    if (!self->calibrated) {
        self->neutral_pitch = imu->pitch_degrees;
        self->calibrated = true;
    }
    delta = (int16_t)(imu->pitch_degrees - self->neutral_pitch);
    if (delta > -PITCH_DEAD_ZONE && delta < PITCH_DEAD_ZONE) delta = 0;
    if (delta > PITCH_LIMIT) delta = PITCH_LIMIT;
    if (delta < -PITCH_LIMIT) delta = -PITCH_LIMIT;
    target = ((self->board_h - PLAYER_H) / 2 - delta * PITCH_TO_PIXEL +
              self->accessory_offset_y) * Q;
    if (target < 0) target = 0;
    if (target > maximum) target = maximum;
    self->player_y_q += (target - self->player_y_q) * 28 / 100;
    position_player(self);
}

static void move_player_with_accessory(jet_t *self, int16_t direction)
{
    int16_t center = (self->board_h - PLAYER_H) / 2;
    int16_t maximum_y = self->board_h - PLAYER_H;
    int32_t maximum_q = maximum_y * Q;
    self->accessory_offset_y += direction * ACCESSORY_PLAYER_STEP;
    if (self->accessory_offset_y < -center)
        self->accessory_offset_y = -center;
    else if (self->accessory_offset_y > maximum_y - center)
        self->accessory_offset_y = maximum_y - center;
    self->player_y_q += direction * ACCESSORY_PLAYER_STEP * Q;
    if (self->player_y_q < 0) self->player_y_q = 0;
    else if (self->player_y_q > maximum_q) self->player_y_q = maximum_q;
    position_player(self);
}

static void update_flame(jet_t *self)
{
    self->flame_long = !self->flame_long;
    self->ui->obj_set_size(self->flame, self->flame_long ? 12 : 7, 4);
    position_player(self);
}

static void update_obstacles(jet_t *self, uint32_t elapsed_ms)
{
    int32_t speed = BASE_SPEED_Q + (int32_t)self->score * SPEED_STEP_Q;
    int32_t distance;
    uint8_t index;
    if (speed > MAX_SPEED_Q) speed = MAX_SPEED_Q;
    distance = speed * (int32_t)elapsed_ms / 1000;
    for (index = 0; index < OBSTACLE_COUNT; ++index) {
        obstacle_t *obstacle = &self->obstacles[index];
        obstacle->x_q -= distance;
        if (!obstacle->passed &&
            obstacle->x_q + OBSTACLE_W * Q < PLAYER_X * Q) {
            obstacle->passed = true;
            ++self->score;
            show_score(self);
        }
        if (obstacle->x_q + OBSTACLE_W * Q < 0) {
            obstacle->x_q = rightmost_x(self) + OBSTACLE_SPACING * Q;
            obstacle->gap_top = random_gap(self);
            obstacle->passed = false;
            layout_obstacle(self, obstacle);
        }
        self->ui->obj_set_pos(obstacle->column,
                              (int16_t)(obstacle->x_q / Q), 0);
        if (collision(self, obstacle)) {
            finish(self);
            return;
        }
    }
}

static gm_plugin_result_t create_ui(jet_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *host_root = self->ui->root_get();
    gm_plugin_lvgl_obj_t *title;
    gm_plugin_lvgl_obj_t *controls;
    int16_t end_width;
    int16_t end_height;
    uint8_t index;
    if (host_root == 0 || self->host->display_get_info(&display) != GM_PLUGIN_OK)
        return GM_PLUGIN_ESTATE;
    if (display.width <= MARGIN * 2U ||
        display.height <= HEADER_H + FOOTER_H)
        return GM_PLUGIN_ENOTSUP;
    self->width = (int16_t)display.width;
    self->height = (int16_t)display.height;
    self->board_w = self->width - MARGIN * 2;
    self->board_h = self->height - HEADER_H - FOOTER_H;
    if (self->board_w < MIN_BOARD_W || self->board_h < MIN_BOARD_H)
        return GM_PLUGIN_ENOTSUP;
    self->ui->obj_clean(host_root);
    self->root = self->ui->obj_create(host_root);
    if (self->root == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->root, self->width, self->height);
    self->ui->obj_align(self->root, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_rect(self, self->root, 0x00, 255, 0, 0);
    title = self->ui->label_create(self->root);
    self->score_label = self->ui->label_create(self->root);
    self->board = self->ui->obj_create(self->root);
    controls = self->ui->label_create(self->root);
    if (title == 0 || self->score_label == 0 || self->board == 0 ||
        controls == 0) return GM_PLUGIN_ENOMEM;
    self->ui->label_set_text(title, self->strings->title);
    self->ui->obj_set_pos(title, 18, 10);
    set_style(self, title, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xF0));
    self->ui->obj_align(self->score_label, GM_PLUGIN_LVGL_ALIGN_TOP_RIGHT,
                        -18, 10);
    set_style(self, self->score_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xF0));
    self->ui->obj_set_pos(self->board, MARGIN, HEADER_H);
    self->ui->obj_set_size(self->board, self->board_w, self->board_h);
    style_rect(self, self->board, 0x07, 255, 2, 3);
    self->ui->label_set_text(controls, self->strings->controls);
    self->ui->obj_set_size(controls, self->width - MARGIN * 2, FOOTER_H);
    self->ui->obj_align(controls, GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID, 0, 0);
    set_style(self, controls, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xA0));
    set_style(self, controls, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    for (index = 0; index < DASH_COUNT; ++index)
        if (rectangle(self, self->board, 20 + index * 72,
                      24 + (index * 47) % (self->board_h - 48),
                      20, 1, 0x70, GM_PLUGIN_LVGL_OPA_50) == 0)
            return GM_PLUGIN_ENOMEM;
    for (index = 0; index < OBSTACLE_COUNT; ++index) {
        obstacle_t *obstacle = &self->obstacles[index];
        obstacle->column = rectangle(self, self->board, 0, 0,
                                     OBSTACLE_W, self->board_h, 0, 0);
        obstacle->top = rectangle(self, obstacle->column, 0, 0,
                                  OBSTACLE_W, 40, 0xF0, 255);
        obstacle->bottom = rectangle(self, obstacle->column, 0,
                                     self->board_h - 80,
                                     OBSTACLE_W, 80, 0xF0, 255);
        if (obstacle->column == 0 || obstacle->top == 0 ||
            obstacle->bottom == 0) return GM_PLUGIN_ENOMEM;
        set_style(self, obstacle->top, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                  number(1));
        set_style(self, obstacle->bottom, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                  number(1));
    }
    self->player = rectangle(self, self->board, PLAYER_X,
                             (self->board_h - PLAYER_H) / 2,
                             PLAYER_W, PLAYER_H, 0, 0);
    if (self->player == 0) return GM_PLUGIN_ENOMEM;
    if (rectangle(self, self->player, 15, 1, 7, 7, 0xF0, 255) == 0 ||
        rectangle(self, self->player, 13, 8, 9, 12, 0xF0, 255) == 0 ||
        rectangle(self, self->player, 7, 8, 6, 13, 0x88, 255) == 0 ||
        rectangle(self, self->player, 20, 11, 7, 3, 0xF0, 255) == 0 ||
        rectangle(self, self->player, 15, 20, 4, 8, 0xF0, 255) == 0 ||
        rectangle(self, self->player, 20, 19, 7, 4, 0xF0, 255) == 0)
        return GM_PLUGIN_ENOMEM;
    self->flame = rectangle(self, self->board, PLAYER_X - 4,
                            (self->board_h - PLAYER_H) / 2 + 12,
                            7, 4, 0xF0, 255);
    self->flame_core = rectangle(self, self->board, PLAYER_X - 1,
                                 (self->board_h - PLAYER_H) / 2 + 13,
                                 5, 2, 0xFF, 255);
    if (self->flame == 0 || self->flame_core == 0)
        return GM_PLUGIN_ENOMEM;
    self->end_label = self->ui->label_create(self->board);
    if (self->end_label == 0) return GM_PLUGIN_ENOMEM;
    end_width = self->board_w - 20;
    if (end_width > 300) end_width = 300;
    end_height = self->board_h - 20;
    if (end_height > 120) end_height = 120;
    self->ui->obj_set_size(self->end_label, end_width, end_height);
    self->ui->obj_align(self->end_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xFF));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(0));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_BG_OPA,
              number(GM_PLUGIN_LVGL_OPA_90));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
              color(0xF0));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
              number(2));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_RADIUS, number(4));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_PAD_TOP, number(16));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM, number(16));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_PAD_LEFT, number(16));
    set_style(self, self->end_label, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT, number(16));
    self->ui->label_set_long_mode(self->end_label, GM_PLUGIN_LVGL_LABEL_WRAP);
    self->ui->obj_add_flag(self->end_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->exit_arc = self->ui->arc_create(self->root);
    if (self->exit_arc == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->exit_arc, 48, 48);
    self->ui->obj_align(self->exit_arc, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    self->ui->arc_set_range(self->exit_arc, 0, 2000);
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
    jet_t *self = opaque;
    gm_plugin_result_t result;
    char language_tag[GM_PLUGIN_LOCALE_TAG_MAX] = {0};
    if (self->host->locale_get(language_tag) != GM_PLUGIN_OK)
        language_tag[0] = '\0';
    self->strings = select_strings(language_tag);
    result = self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
    if (result != GM_PLUGIN_OK) return result;
    result = create_ui(self);
    if (result == GM_PLUGIN_OK) {
        reset_game(self);
    } else {
        (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
        self->ui->obj_clean(self->ui->root_get());
    }
    return result;
}

static void plugin_loop(void *opaque, uint32_t elapsed_ms)
{
    jet_t *self = opaque;
    gm_plugin_imu_sample_t imu;
    if (elapsed_ms > MAX_DELTA_MS) elapsed_ms = MAX_DELTA_MS;
    if (self->holding_exit) {
        self->hold_ms += elapsed_ms;
        if (self->hold_ms >= 2000U) {
            self->host->app_exit();
            return;
        }
        self->ui->arc_set_value(self->exit_arc, (int16_t)self->hold_ms);
    }
    if (self->game_over) return;
    self->frame_accumulator += elapsed_ms;
    if (self->frame_accumulator < FRAME_MS) return;
    elapsed_ms = self->frame_accumulator;
    self->frame_accumulator = 0;
    if (self->host->imu_read(&imu) != GM_PLUGIN_OK) return;
    update_player(self, &imu);
    update_obstacles(self, elapsed_ms);
    if (!self->game_over) update_flame(self);
}

static bool plugin_event(void *opaque, const gm_plugin_event_t *event)
{
    jet_t *self = opaque;
    gm_plugin_button_action_t action;
    gm_plugin_button_t button;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    action = event->data.button.action;
    button = event->data.button.button;
    if (action == GM_PLUGIN_BUTTON_ACTION_LONG ||
        action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
        if (!self->holding_exit) {
            self->holding_exit = true;
            self->hold_ms = 0;
            self->ui->arc_set_value(self->exit_arc, 0);
            self->ui->obj_clear_flag(self->exit_arc,
                                     GM_PLUGIN_LVGL_FLAG_HIDDEN);
        }
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_RELEASE) {
        self->holding_exit = false;
        self->hold_ms = 0;
        self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_TRIGGER &&
        (button == GM_PLUGIN_BUTTON_UP ||
         button == GM_PLUGIN_BUTTON_DOWN)) {
        if (!self->game_over && !self->holding_exit)
            move_player_with_accessory(
                self, button == GM_PLUGIN_BUTTON_UP ? -1 : 1);
        return true;
    }
    if (self->game_over &&
        (action == GM_PLUGIN_BUTTON_ACTION_SINGLE ||
         action == GM_PLUGIN_BUTTON_ACTION_DOUBLE)) {
        reset_game(self);
        return true;
    }
    return false;
}

static void plugin_stop(void *opaque)
{
    jet_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    self->ui->obj_clean(self->ui->root_get());
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_IMU_RAW |
                                              GM_PLUGIN_CAP_BUTTON |
                                              GM_PLUGIN_CAP_LOCALE;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->graphics.lvgl == 0 || host->display_get_info == 0 ||
        host->monotonic_ms == 0 || host->imu_enable == 0 ||
        host->imu_read == 0 || host->locale_get == 0 || host->app_exit == 0 ||
        (host->capabilities & required) != required ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;
    game.host = host;
    game.ui = host->graphics.lvgl;
    game.random_state = UINT32_C(0x4A455452);
    if (game.ui == 0 || game.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(game.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &game;
    plugin->on_start = plugin_start;
    plugin->on_loop = plugin_loop;
    plugin->on_event = plugin_event;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
