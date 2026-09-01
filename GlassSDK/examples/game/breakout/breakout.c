#include "gm_plugin_lvgl_api.h"
#include "breakout_translations.h"

#define COLS 15
#define ROWS 3
#define TRAILS 3
#define FRAME_MS 50U
#define Q 256
#define BOARD_X 10
#define BOARD_Y 40
#define BOARD_BOTTOM_MARGIN 16
#define MIN_BOARD_W 120
#define MIN_BOARD_H 120
#define BRICK_GAP 2
#define BRICK_H 14
#define BRICK_TOP 28
#define PADDLE_W 80
#define PADDLE_H 10
#define ACCESSORY_PADDLE_STEP 32
#define BALL_SIZE 10
#define GYRO_THRESHOLD 15
#define EXIT_PITCH 30
#define EXIT_MS 3000U
#define HEADER_SIDE_W 190
#define HEADER_TIMER_W 100
#define HEADER_H 24
#define MESSAGE_SIDE_MARGIN 36
#define PAUSE_PANEL_MIN_W 380
#define PAUSE_PANEL_MIN_H 120
#define RESULT_PANEL_MIN_W 380
#define RESULT_PANEL_MIN_H 152

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const breakout_strings_t *strings;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *board;
    gm_plugin_lvgl_obj_t *bricks[ROWS][COLS];
    gm_plugin_lvgl_obj_t *paddle;
    gm_plugin_lvgl_obj_t *ball;
    gm_plugin_lvgl_obj_t *trails[TRAILS];
    gm_plugin_lvgl_obj_t *score_label;
    gm_plugin_lvgl_obj_t *timer_label;
    gm_plugin_lvgl_obj_t *control_label;
    gm_plugin_lvgl_obj_t *message_label;
    int32_t ball_x;
    int32_t ball_y;
    int32_t ball_vx;
    int32_t ball_vy;
    int32_t paddle_x;
    int32_t paddle_vx;
    int16_t trail_x[TRAILS];
    int16_t trail_y[TRAILS];
    uint32_t frame_accumulator;
    uint32_t start_ms;
    uint32_t pause_ms;
    uint32_t paused_total_ms;
    uint32_t exit_start_ms;
    uint32_t last_second;
    int16_t screen_width;
    int16_t screen_height;
    int16_t board_width;
    int16_t board_height;
    int16_t paddle_y;
    int16_t brick_width;
    int16_t brick_left;
    uint16_t score;
    uint8_t bricks_remaining;
    uint8_t alive[ROWS][COLS];
    uint8_t paused;
    uint8_t ended;
    uint8_t exiting;
    uint8_t exit_seconds;
} breakout_t;

static breakout_t game;

static uint8_t tag_equals(const char *left, const char *right,
                          uint8_t primary_only)
{
    while (*left != '\0' && *right != '\0') {
        if (primary_only != 0U &&
            (*left == '-' || *left == '_' || *right == '-' || *right == '_'))
            break;
        if (*left != *right) return 0;
        ++left;
        ++right;
    }
    if (primary_only != 0U)
        return (*left == '\0' || *left == '-' || *left == '_') &&
               (*right == '\0' || *right == '-' || *right == '_');
    return *left == '\0' && *right == '\0';
}

static const breakout_strings_t *select_strings(const char *language_tag)
{
    uint32_t index;
    for (index = 0; index < sizeof(translations) / sizeof(translations[0]);
         ++index) {
        if (tag_equals(language_tag, translations[index].tag, 0U) != 0U)
            return &translations[index];
    }
    for (index = 0; index < sizeof(translations) / sizeof(translations[0]);
         ++index) {
        if (tag_equals(language_tag, translations[index].tag, 1U) != 0U)
            return &translations[index];
    }
    return &translations[1];
}

static void append_text(char **cursor, const char *text)
{
    while (*text != '\0') *(*cursor)++ = *text++;
}

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void style_box(breakout_t *self, gm_plugin_lvgl_obj_t *object,
                      uint8_t opacity, uint8_t border, uint8_t radius)
{
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                        number(opacity), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA,
                        number(255), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                        number(border), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_RADIUS,
                        number(radius), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
}

static void style_panel(breakout_t *self, gm_plugin_lvgl_obj_t *label)
{
    style_box(self, label, GM_PLUGIN_LVGL_OPA_80, 1, 4);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        color(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_OUTLINE_COLOR,
                        color(0x30U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_OUTLINE_WIDTH,
                        number(3), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_OUTLINE_OPA,
                        number(GM_PLUGIN_LVGL_OPA_40),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
                        number(12), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
                        number(12), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
                        number(12), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
                        number(12), GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void append_uint(char **cursor, uint32_t value, uint8_t minimum_digits)
{
    char reverse[10];
    uint8_t count = 0;
    uint8_t index;
    do {
        reverse[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < sizeof(reverse));
    while (count < minimum_digits) reverse[count++] = '0';
    for (index = 0; index < count; ++index)
        *(*cursor)++ = reverse[count - index - 1U];
}

static void set_score(breakout_t *self)
{
    char text[40];
    char *cursor = text;
    append_text(&cursor, self->strings->score);
    append_uint(&cursor, self->score, 1);
    *cursor = '\0';
    self->ui->label_set_text(self->score_label, text);
}

static uint32_t active_elapsed_ms(const breakout_t *self, uint32_t now)
{
    uint32_t paused = self->paused_total_ms;
    if (self->paused != 0U) paused += now - self->pause_ms;
    return now - self->start_ms - paused;
}

static void set_timer(breakout_t *self, uint32_t seconds)
{
    /* uint32_t milliseconds can represent more than 99 minutes. */
    char text[16];
    char *cursor = text;
    append_uint(&cursor, seconds / 60U, 2);
    *cursor++ = ':';
    append_uint(&cursor, seconds % 60U, 2);
    *cursor = '\0';
    self->ui->label_set_text(self->timer_label, text);
}

static void set_control_text(breakout_t *self, const char *text)
{
    self->ui->label_set_text(self->control_label, text);
}

static void set_exit_countdown(breakout_t *self, uint8_t seconds)
{
    char text[48];
    char *cursor = text;
    append_text(&cursor, self->strings->exit_prefix);
    append_uint(&cursor, seconds, 1);
    append_text(&cursor, self->strings->exit_suffix);
    *cursor = '\0';
    set_control_text(self, text);
}

static void set_message(breakout_t *self, const char *text, uint8_t visible,
                        int16_t minimum_width, int16_t minimum_height)
{
    self->ui->label_set_text(self->message_label, text);
    if (visible != 0U) {
        gm_plugin_lvgl_point_t natural_size = {0, 0};
        gm_plugin_lvgl_point_t wrapped_size = {0, 0};
        int16_t maximum_width = self->screen_width -
                                MESSAGE_SIDE_MARGIN * 2;
        int16_t width;
        int16_t height;
        self->ui->text_get_size(&natural_size, text, self->ui->font_default,
                                0, 0, maximum_width - 24,
                                GM_PLUGIN_LVGL_TEXT_FLAG_NONE);
        width = (int16_t)natural_size.x + 24;
        if (width < minimum_width) width = minimum_width;
        if (width > maximum_width) width = maximum_width;

        /* Measure again with the final content width. A translation that fits
         * the initial limit can still wrap after minimum/maximum sizing. */
        self->ui->text_get_size(&wrapped_size, text, self->ui->font_default,
                                0, 0, width - 24,
                                GM_PLUGIN_LVGL_TEXT_FLAG_NONE);
        height = (int16_t)wrapped_size.y + 32;
        if (height < minimum_height) height = minimum_height;
        if (height > self->screen_height - 48)
            height = self->screen_height - 48;
        self->ui->obj_set_size(self->message_label, width, height);
        self->ui->obj_align(self->message_label,
                            GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
        self->ui->obj_clear_flag(self->message_label,
                                 GM_PLUGIN_LVGL_FLAG_HIDDEN);
    } else {
        self->ui->obj_add_flag(self->message_label,
                               GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
}

static void initialize_state(breakout_t *self)
{
    uint8_t row;
    uint8_t col;
    self->score = 0;
    self->bricks_remaining = 0;
    for (row = 0; row < ROWS; ++row) {
        for (col = 0; col < COLS; ++col) {
            self->alive[row][col] = (col % 4U) != 3U;
            if (self->alive[row][col] != 0U) ++self->bricks_remaining;
        }
    }
    self->paddle_x = ((self->board_width - PADDLE_W) / 2) * Q;
    self->paddle_vx = 0;
    self->ball_x = (self->board_width / 2 - BALL_SIZE / 2) * Q;
    self->ball_y = (self->paddle_y - BALL_SIZE - 4) * Q;
    self->ball_vx = 1587;
    self->ball_vy = -2202;
    self->frame_accumulator = 0;
    self->start_ms = self->host->monotonic_ms();
    self->paused_total_ms = 0;
    self->pause_ms = 0;
    self->last_second = UINT32_MAX;
    self->exit_start_ms = 0;
    self->paused = 0;
    self->ended = 0;
    self->exiting = 0;
    self->exit_seconds = 0;
}

static gm_plugin_result_t create_ui(breakout_t *self)
{
    gm_plugin_lvgl_obj_t *host_root = self->ui->root_get();
    uint8_t row;
    uint8_t col;
    uint8_t index;
    int16_t header_side_width;
    int16_t position;
    if (host_root == 0) return GM_PLUGIN_ESTATE;
    self->ui->obj_clean(host_root);
    self->root = self->ui->obj_create(host_root);
    if (self->root == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->root, self->screen_width,
                           self->screen_height);
    self->ui->obj_align(self->root, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->root, 255, 0, 0);
    self->ui->style_set(self->root, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        color(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->root, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->root, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->root, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->root, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->board = self->ui->obj_create(self->root);
    self->score_label = self->ui->label_create(self->root);
    self->timer_label = self->ui->label_create(self->root);
    self->control_label = self->ui->label_create(self->root);
    self->message_label = self->ui->label_create(self->root);
    if (self->board == 0 || self->score_label == 0 ||
        self->timer_label == 0 || self->control_label == 0 ||
        self->message_label == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_pos(self->board, BOARD_X, BOARD_Y);
    self->ui->obj_set_size(self->board, self->board_width, self->board_height);
    style_box(self, self->board, GM_PLUGIN_LVGL_OPA_COVER, 2, 3);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        color(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_BG_GRAD_COLOR,
                        color(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_BG_GRAD_DIR,
                        number(GM_PLUGIN_LVGL_GRAD_DIR_VER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_OUTLINE_COLOR,
                        color(0x40U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_OUTLINE_WIDTH,
                        number(5), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_OUTLINE_OPA,
                        number(GM_PLUGIN_LVGL_OPA_50),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_SHADOW_COLOR,
                        color(0xC0U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_SHADOW_WIDTH,
                        number(8), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_SHADOW_SPREAD,
                        number(1), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_SHADOW_OPA,
                        number(GM_PLUGIN_LVGL_OPA_30),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_SHADOW_OFS_X,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_SHADOW_OFS_Y,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->board, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_clear_flag(self->board, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    for (position = 58; position < self->board_height - 34; position += 46) {
        gm_plugin_lvgl_obj_t *line = self->ui->obj_create(self->board);
        if (line == 0) return GM_PLUGIN_ENOMEM;
        self->ui->obj_set_size(line, self->board_width - 28, 1);
        self->ui->obj_set_pos(line, 14, position);
        style_box(self, line, 102, 0, 0);
        self->ui->style_set(line, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                            color(0x40U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    }
    for (row = 0; row < ROWS; ++row) {
        for (col = 0; col < COLS; ++col) {
            gm_plugin_lvgl_obj_t *brick = self->ui->obj_create(self->board);
            int16_t x = self->brick_left +
                        col * (self->brick_width + BRICK_GAP);
            int16_t y = BRICK_TOP + row * (BRICK_H + BRICK_GAP);
            if (brick == 0) return GM_PLUGIN_ENOMEM;
            self->bricks[row][col] = brick;
            self->ui->obj_set_pos(brick, x, y);
            self->ui->obj_set_size(brick, self->brick_width, BRICK_H);
            style_box(self, brick, row == 1U ? 0 : (row == 2U ? 150 : 255),
                      row == 1U ? 2 : 1, 2);
            if (self->alive[row][col] == 0U)
                self->ui->obj_add_flag(brick, GM_PLUGIN_LVGL_FLAG_HIDDEN);
        }
    }
    self->paddle = self->ui->obj_create(self->board);
    if (self->paddle == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->paddle, PADDLE_W, PADDLE_H + 4);
    style_box(self, self->paddle, 0, 0, 0);
    self->ui->style_set(self->paddle, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->paddle, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->paddle, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->paddle, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    {
        gm_plugin_lvgl_obj_t *body = self->ui->obj_create(self->paddle);
        gm_plugin_lvgl_obj_t *left = self->ui->obj_create(self->paddle);
        gm_plugin_lvgl_obj_t *right = self->ui->obj_create(self->paddle);
        if (body == 0 || left == 0 || right == 0)
            return GM_PLUGIN_ENOMEM;
        self->ui->obj_set_size(body, PADDLE_W - 16, PADDLE_H);
        self->ui->obj_set_pos(body, 8, 2);
        style_box(self, body, 255, 1, 2);
        self->ui->obj_set_size(left, 10, PADDLE_H + 4);
        self->ui->obj_set_pos(left, 0, 0);
        style_box(self, left, 204, 1, 2);
        self->ui->obj_set_size(right, 10, PADDLE_H + 4);
        self->ui->obj_set_pos(right, PADDLE_W - 10, 0);
        style_box(self, right, 204, 1, 2);
    }
    for (index = 0; index < TRAILS; ++index) {
        int16_t size = BALL_SIZE - 2 - index * 2;
        self->trails[index] = self->ui->obj_create(self->board);
        if (self->trails[index] == 0) return GM_PLUGIN_ENOMEM;
        self->ui->obj_set_size(self->trails[index], size, size);
        style_box(self, self->trails[index], 100U - index * 30U, 0, size / 2);
        self->trail_x[index] = (int16_t)(self->ball_x / Q);
        self->trail_y[index] = (int16_t)(self->ball_y / Q);
    }
    self->ball = self->ui->obj_create(self->board);
    if (self->ball == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->ball, BALL_SIZE, BALL_SIZE);
    style_box(self, self->ball, 255, 0, BALL_SIZE / 2);
    header_side_width = (self->screen_width - HEADER_TIMER_W) / 2 - 16;
    if (header_side_width > HEADER_SIDE_W)
        header_side_width = HEADER_SIDE_W;
    self->ui->obj_set_pos(self->score_label, BOARD_X + 4, 8);
    self->ui->obj_set_size(self->score_label, header_side_width, HEADER_H);
    self->ui->label_set_long_mode(self->score_label, GM_PLUGIN_LVGL_LABEL_CLIP);
    self->ui->style_set(self->score_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xF0U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->score_label, GM_PLUGIN_LVGL_STYLE_TEXT_OPA,
                        number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_set_size(self->timer_label, HEADER_TIMER_W, HEADER_H);
    self->ui->obj_align(self->timer_label, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 8);
    self->ui->label_set_long_mode(self->timer_label, GM_PLUGIN_LVGL_LABEL_CLIP);
    self->ui->style_set(self->timer_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->timer_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->timer_label, GM_PLUGIN_LVGL_STYLE_TEXT_OPA,
                        number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->control_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xF0U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->control_label, GM_PLUGIN_LVGL_STYLE_TEXT_OPA,
                        number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_set_size(self->control_label, header_side_width, HEADER_H);
    self->ui->obj_align(self->control_label, GM_PLUGIN_LVGL_ALIGN_TOP_RIGHT,
                        -12, 8);
    self->ui->label_set_long_mode(self->control_label,
                                  GM_PLUGIN_LVGL_LABEL_CLIP);
    self->ui->style_set(self->control_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_RIGHT),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    set_control_text(self, self->strings->control_hint);
    style_panel(self, self->message_label);
    self->ui->label_set_long_mode(self->message_label,
                                  GM_PLUGIN_LVGL_LABEL_WRAP);
    set_message(self, "", 0, 0, 0);
    set_score(self);
    set_timer(self, 0);
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t restart(breakout_t *self)
{
    initialize_state(self);
    return create_ui(self);
}

static void move_paddle(breakout_t *self, const gm_plugin_imu_sample_t *imu)
{
    int32_t gyro_x = imu->gyro_raw[0];
    int32_t gyro_y = imu->gyro_raw[1];
    int32_t abs_x = gyro_x < 0 ? -gyro_x : gyro_x;
    int32_t abs_y = gyro_y < 0 ? -gyro_y : gyro_y;
    int32_t magnitude = abs_x > abs_y ? abs_x : abs_y;
    int32_t target = 0;
    int32_t maximum = (self->board_width - PADDLE_W) * Q;
    if (abs_x > GYRO_THRESHOLD || abs_y > GYRO_THRESHOLD) {
        int32_t speed = magnitude / 5;
        if (speed < 6) speed = 6;
        if (speed > 70) speed = 70;
        if (gyro_x > 0 && gyro_y < 0) target = speed * Q;
        else if (gyro_x < 0 && gyro_y > 0) target = -speed * Q;
    }
    self->paddle_vx += (target - self->paddle_vx) / 2;
    self->paddle_x += self->paddle_vx;
    if (self->paddle_x < 0) {
        self->paddle_x = 0;
        self->paddle_vx = 0;
    } else if (self->paddle_x > maximum) {
        self->paddle_x = maximum;
        self->paddle_vx = 0;
    }
}

static void move_paddle_with_accessory(breakout_t *self, int32_t direction)
{
    int32_t maximum = (self->board_width - PADDLE_W) * Q;
    self->paddle_x += direction * ACCESSORY_PADDLE_STEP * Q;
    self->paddle_vx = 0;
    if (self->paddle_x < 0) self->paddle_x = 0;
    else if (self->paddle_x > maximum) self->paddle_x = maximum;
    self->ui->obj_set_pos(self->paddle, (int16_t)(self->paddle_x / Q),
                          self->paddle_y - 2);
}

static void brick_collision(breakout_t *self)
{
    int32_t ball_x = self->ball_x / Q;
    int32_t ball_y = self->ball_y / Q;
    uint8_t row;
    uint8_t col;
    for (row = 0; row < ROWS; ++row) {
        for (col = 0; col < COLS; ++col) {
            int32_t x;
            int32_t y;
            int32_t dx1;
            int32_t dx2;
            int32_t dy1;
            int32_t dy2;
            if (self->alive[row][col] == 0U) continue;
            x = self->brick_left + col * (self->brick_width + BRICK_GAP);
            y = BRICK_TOP + row * (BRICK_H + BRICK_GAP);
            if (ball_x + BALL_SIZE <= x || ball_x >= x + self->brick_width ||
                ball_y + BALL_SIZE <= y || ball_y >= y + BRICK_H) continue;
            self->alive[row][col] = 0;
            self->ui->obj_add_flag(self->bricks[row][col],
                                   GM_PLUGIN_LVGL_FLAG_HIDDEN);
            self->score += 10;
            --self->bricks_remaining;
            set_score(self);
            dx1 = ball_x + BALL_SIZE - x;
            dx2 = x + self->brick_width - ball_x;
            dy1 = ball_y + BALL_SIZE - y;
            dy2 = y + BRICK_H - ball_y;
            if ((dx1 < dx2 ? dx1 : dx2) < (dy1 < dy2 ? dy1 : dy2))
                self->ball_vx = -self->ball_vx;
            else
                self->ball_vy = -self->ball_vy;
            return;
        }
    }
}

static void finish_game(breakout_t *self, uint8_t won)
{
    char text[192];
    char *cursor = text;
    uint32_t seconds = active_elapsed_ms(self, self->host->monotonic_ms()) / 1000U;
    append_text(&cursor, won != 0U ? self->strings->victory
                                  : self->strings->game_over);
    *cursor++ = '\n';
    append_text(&cursor, self->strings->score);
    append_uint(&cursor, self->score, 1);
    *cursor++ = '\n';
    append_text(&cursor, self->strings->time);
    append_uint(&cursor, seconds / 60U, 2);
    *cursor++ = ':';
    append_uint(&cursor, seconds % 60U, 2);
    *cursor++ = '\n';
    append_text(&cursor, self->strings->restart);
    *cursor = '\0';
    self->ended = 1;
    set_message(self, text, 1, RESULT_PANEL_MIN_W, RESULT_PANEL_MIN_H);
}

static void render(breakout_t *self)
{
    int16_t x = (int16_t)(self->ball_x / Q);
    int16_t y = (int16_t)(self->ball_y / Q);
    int8_t index;
    for (index = TRAILS - 1; index > 0; --index) {
        self->trail_x[index] = self->trail_x[index - 1];
        self->trail_y[index] = self->trail_y[index - 1];
    }
    self->trail_x[0] = x;
    self->trail_y[0] = y;
    for (index = 0; index < TRAILS; ++index)
        self->ui->obj_set_pos(self->trails[index], self->trail_x[index],
                              self->trail_y[index]);
    self->ui->obj_set_pos(self->ball, x, y);
    self->ui->obj_set_pos(self->paddle, (int16_t)(self->paddle_x / Q),
                          self->paddle_y - 2);
}

static void frame(breakout_t *self, uint32_t now)
{
    gm_plugin_imu_sample_t imu;
    int32_t x;
    int32_t y;
    if (self->host->imu_read(&imu) != GM_PLUGIN_OK) return;
    if (imu.pitch_degrees > EXIT_PITCH) {
        if (self->exiting == 0U) {
            self->exiting = 1;
            self->exit_start_ms = now;
            self->exit_seconds = 3;
            set_exit_countdown(self, self->exit_seconds);
            self->ui->obj_clear_flag(self->control_label,
                                     GM_PLUGIN_LVGL_FLAG_HIDDEN);
        } else {
            uint32_t elapsed = now - self->exit_start_ms;
            if (elapsed >= EXIT_MS) {
                self->exiting = 2;
                self->host->app_exit();
                return;
            } else {
                uint8_t seconds = (uint8_t)((EXIT_MS - elapsed + 999U) / 1000U);
                if (seconds != self->exit_seconds) {
                    self->exit_seconds = seconds;
                    set_exit_countdown(self, seconds);
                }
            }
        }
    } else if (self->exiting != 0U) {
        self->exiting = 0;
        self->exit_seconds = 0;
        set_control_text(self, self->strings->control_hint);
    }
    if (self->paused != 0U || self->ended != 0U) return;
    move_paddle(self, &imu);
    self->ball_x += self->ball_vx;
    self->ball_y += self->ball_vy;
    if (self->ball_x < 0) {
        self->ball_x = 0;
        self->ball_vx = -self->ball_vx;
    } else if (self->ball_x + BALL_SIZE * Q > self->board_width * Q) {
        self->ball_x = (self->board_width - BALL_SIZE) * Q;
        self->ball_vx = -self->ball_vx;
    }
    if (self->ball_y < 0) {
        self->ball_y = 0;
        self->ball_vy = -self->ball_vy;
    }
    brick_collision(self);
    x = self->ball_x / Q;
    y = self->ball_y / Q;
    if (self->ball_vy > 0 && y + BALL_SIZE >= self->paddle_y &&
        y < self->paddle_y + PADDLE_H &&
        x + BALL_SIZE > self->paddle_x / Q &&
        x < self->paddle_x / Q + PADDLE_W) {
        int32_t offset = x + BALL_SIZE / 2 - self->paddle_x / Q;
        int32_t relative = offset * Q / PADDLE_W - Q / 2;
        self->ball_y = (self->paddle_y - BALL_SIZE) * Q;
        self->ball_vy = -self->ball_vy;
        self->ball_vx = relative * 2202 / Q;
        if (self->ball_vx > 1101) self->ball_vx = 1101;
        if (self->ball_vx < -1101) self->ball_vx = -1101;
        if (self->ball_vx > -220 && self->ball_vx < 220)
            self->ball_vx = self->ball_vx >= 0 ? 256 : -256;
    }
    if (self->ball_y > self->board_height * Q) finish_game(self, 0);
    else if (self->bricks_remaining == 0U) finish_game(self, 1);
    render(self);
}

static gm_plugin_result_t on_load(void *opaque)
{
    breakout_t *self = opaque;
    self->host->log("GM breakout plugin loaded");
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t on_start(void *opaque)
{
    breakout_t *self = opaque;
    gm_plugin_result_t result;
    gm_plugin_display_info_t display;
    char language_tag[GM_PLUGIN_LOCALE_TAG_MAX] = {0};
    if (self->host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width <= BOARD_X * 2U ||
        display.height <= BOARD_Y + BOARD_BOTTOM_MARGIN)
        return GM_PLUGIN_ENOTSUP;
    self->screen_width = (int16_t)display.width;
    self->screen_height = (int16_t)display.height;
    self->board_width = self->screen_width - BOARD_X * 2;
    self->board_height = self->screen_height - BOARD_Y -
                         BOARD_BOTTOM_MARGIN;
    if (self->board_width < MIN_BOARD_W ||
        self->board_height < MIN_BOARD_H)
        return GM_PLUGIN_ENOTSUP;
    self->paddle_y = self->board_height - 24;
    self->brick_width = (self->board_width - self->board_width / 5 -
        (COLS - 1) * BRICK_GAP) / COLS;
    self->brick_left = (self->board_width -
        (COLS * self->brick_width + (COLS - 1) * BRICK_GAP)) / 2;
    if (self->host->locale_get(language_tag) != GM_PLUGIN_OK)
        language_tag[0] = '\0';
    self->strings = select_strings(language_tag);
    if (self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW) != GM_PLUGIN_OK)
        return GM_PLUGIN_ESTATE;
    result = restart(self);
    if (result != GM_PLUGIN_OK) {
        (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
        self->ui->obj_clean(self->ui->root_get());
    }
    return result;
}

static void on_loop(void *opaque, uint32_t elapsed_ms)
{
    breakout_t *self = opaque;
    uint32_t now = self->host->monotonic_ms();
    uint32_t seconds = active_elapsed_ms(self, now) / 1000U;
    self->frame_accumulator += elapsed_ms;
    if (self->frame_accumulator > 150U) self->frame_accumulator = 150U;
    while (self->frame_accumulator >= FRAME_MS && self->exiting != 2U) {
        self->frame_accumulator -= FRAME_MS;
        frame(self, now);
    }
    if (self->ended == 0U && self->exiting != 2U &&
        seconds != self->last_second) {
        self->last_second = seconds;
        set_timer(self, seconds);
    }
}

static bool on_event(void *opaque, const gm_plugin_event_t *event)
{
    breakout_t *self = opaque;
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
    if (action == GM_PLUGIN_BUTTON_ACTION_TRIGGER) {
        if (button != GM_PLUGIN_BUTTON_LEFT &&
            button != GM_PLUGIN_BUTTON_RIGHT)
            return false;
        if (self->paused == 0U && self->ended == 0U && self->exiting == 0U)
            move_paddle_with_accessory(
                self, button == GM_PLUGIN_BUTTON_LEFT ? -1 : 1);
        return true;
    }
    if (action != GM_PLUGIN_BUTTON_ACTION_SINGLE) return false;
    if (self->ended != 0U) {
        if (restart(self) != GM_PLUGIN_OK) self->host->app_exit();
    } else if (self->paused != 0U) {
        self->paused_total_ms += self->host->monotonic_ms() - self->pause_ms;
        self->paused = 0;
        set_message(self, "", 0, 0, 0);
    } else {
        self->paused = 1;
        self->pause_ms = self->host->monotonic_ms();
        set_message(self, self->strings->pause, 1,
                    PAUSE_PANEL_MIN_W, PAUSE_PANEL_MIN_H);
    }
    return true;
}

static void on_suspend(void *opaque)
{
    breakout_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
}

static void on_resume(void *opaque)
{
    breakout_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW);
    self->frame_accumulator = 0;
}

static void on_stop(void *opaque)
{
    breakout_t *self = opaque;
    (void)self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    self->ui->obj_clean(self->ui->root_get());
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required =
        GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_IMU_RAW | GM_PLUGIN_CAP_LOCALE;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->graphics.lvgl == 0 || host->display_get_info == 0 ||
        host->monotonic_ms == 0 || host->imu_enable == 0 ||
        host->imu_read == 0 || host->app_exit == 0 || host->locale_get == 0 ||
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
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &game;
    plugin->on_load = on_load;
    plugin->on_start = on_start;
    plugin->on_resume = on_resume;
    plugin->on_loop = on_loop;
    plugin->on_event = on_event;
    plugin->on_suspend = on_suspend;
    plugin->on_stop = on_stop;
    return GM_PLUGIN_OK;
}
