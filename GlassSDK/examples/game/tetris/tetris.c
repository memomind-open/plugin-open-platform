#include "gm_plugin_lvgl_api.h"
#include "tetris_translations.h"

#define COLS 10
#define ROWS 20
#define PIECES 7
#define FRAME_MS 50U
#define NORMAL_DROP_MS 300U
#define FAST_DROP_MS 100U
#define MOVE_FILTER_MS 100U
#define GYRO_THRESHOLD 15
#define EXIT_PITCH 30
#define SCREEN_EDGE 5
#define BOARD_RIGHT_MARGIN 16
#define INFO_LEFT 16
#define INFO_GAP 16
#define INFO_MIN_WIDTH 120
#define MIN_CELL_SIZE 6

typedef struct {
    int8_t type;
    int8_t rotation;
    int8_t x;
    int8_t y;
} piece_t;

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const tetris_strings_t *strings;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *board_obj;
    gm_plugin_lvgl_obj_t *cells[ROWS][COLS];
    gm_plugin_lvgl_obj_t *score_label;
    gm_plugin_lvgl_obj_t *info_label;
    gm_plugin_lvgl_obj_t *exit_label;
    uint8_t board[ROWS][COLS];
    uint8_t shown[ROWS][COLS];
    piece_t piece;
    uint32_t random_state;
    uint32_t frame_accumulator;
    uint32_t drop_accumulator;
    uint32_t move_wait_ms;
    uint32_t exit_ms;
    uint8_t exit_seconds;
    uint16_t score;
    int16_t screen_width;
    int16_t screen_height;
    int16_t board_width;
    int16_t board_height;
    int16_t cell_size;
    bool game_over;
    bool fast_drop;
} tetris_t;

static tetris_t game;

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

static const tetris_strings_t *select_strings(const char *tag)
{
    uint32_t index;
    uint32_t count = sizeof(tetris_translations) /
                     sizeof(tetris_translations[0]);
    for (index = 0; index < count; ++index)
        if (tag_matches(tag, tetris_translations[index].tag, false))
            return &tetris_translations[index];
    for (index = 0; index < count; ++index)
        if (tag_matches(tag, tetris_translations[index].tag, true))
            return &tetris_translations[index];
    return &tetris_translations[1];
}

static void append_text(char **cursor, const char *text)
{
    while (*text != '\0') *(*cursor)++ = *text++;
}

/* 7 pieces x 4 rotations. One bit per cell instead of 448 byte matrices. */
static const uint16_t shapes[PIECES][4] = {
    {0x0F00U, 0x2222U, 0x00F0U, 0x4444U},
    {0x8E00U, 0x6440U, 0x0E20U, 0x44C0U},
    {0x2E00U, 0x4460U, 0x0E80U, 0xC440U},
    {0x6600U, 0x6600U, 0x6600U, 0x6600U},
    {0x6C00U, 0x4620U, 0x06C0U, 0x8C40U},
    {0x4E00U, 0x4640U, 0x0E40U, 0x4C40U},
    {0xC600U, 0x2640U, 0x0C60U, 0x4C80U},
};

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static void set_style(tetris_t *self, gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value)
{
    self->ui->style_set(object, property, value, GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_box(tetris_t *self, gm_plugin_lvgl_obj_t *object,
                      uint8_t fill, uint8_t border)
{
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(fill ? 255 : 0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xFF));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA, number(255));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_RADIUS, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_TOP, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_LEFT, number(0));
    set_style(self, object, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT, number(0));
}

static bool occupied(int8_t type, int8_t rotation, int8_t y, int8_t x)
{
    uint8_t shift = (uint8_t)(15 - ((uint8_t)y * 4U + (uint8_t)x));
    return ((shapes[(uint8_t)type][(uint8_t)rotation] >> shift) & 1U) != 0U;
}

static uint32_t random_next(tetris_t *self)
{
    self->random_state = self->random_state * UINT32_C(1664525) +
                         UINT32_C(1013904223);
    return self->random_state;
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

static void show_exit_countdown(tetris_t *self, uint8_t seconds)
{
    char text[48];
    char *cursor = text;
    append_text(&cursor, self->strings->exit_prefix);
    append_uint(&cursor, seconds);
    append_text(&cursor, self->strings->exit_suffix);
    *cursor = '\0';
    {
        gm_plugin_lvgl_point_t size = {0, 0};
        self->ui->text_get_size(&size, text, self->ui->font_default,
                                0, 0, self->screen_width - 32,
                                GM_PLUGIN_LVGL_TEXT_FLAG_NONE);
        self->ui->obj_set_size(self->exit_label,
                               (int16_t)size.x + 16,
                               (int16_t)size.y + 8);
        self->ui->obj_align(self->exit_label,
                            GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 0);
    }
    self->ui->label_set_text(self->exit_label, text);
    self->ui->obj_clear_flag(self->exit_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static void show_score(tetris_t *self)
{
    char text[40];
    char *cursor = text;
    append_text(&cursor, self->strings->score);
    append_uint(&cursor, self->score);
    *cursor = '\0';
    self->ui->label_set_text(self->score_label, text);
}

static bool can_place(const tetris_t *self, int8_t type, int8_t rotation,
                      int8_t x, int8_t y)
{
    int8_t py;
    int8_t px;
    for (py = 0; py < 4; ++py) {
        for (px = 0; px < 4; ++px) {
            int8_t bx;
            int8_t by;
            if (!occupied(type, rotation, py, px)) continue;
            bx = (int8_t)(x + px);
            by = (int8_t)(y + py);
            if (bx < 0 || bx >= COLS || by < 0 || by >= ROWS) return false;
            if (self->board[(uint8_t)by][(uint8_t)bx] != 0U) return false;
        }
    }
    return true;
}

static void spawn(tetris_t *self)
{
    self->piece.type = (int8_t)(random_next(self) % PIECES);
    self->piece.rotation = 0;
    self->piece.x = COLS / 2 - 2;
    self->piece.y = 0;
    if (!can_place(self, self->piece.type, 0, self->piece.x, 0)) {
        self->game_over = true;
        self->ui->label_set_text(self->info_label, self->strings->game_over);
    }
}

static void render(tetris_t *self)
{
    int8_t y;
    int8_t x;
    for (y = 0; y < ROWS; ++y) {
        for (x = 0; x < COLS; ++x) {
            bool filled = self->board[(uint8_t)y][(uint8_t)x] != 0U;
            int8_t py;
            int8_t px;
            if (!self->game_over) {
                for (py = 0; py < 4; ++py)
                    for (px = 0; px < 4; ++px)
                        if (occupied(self->piece.type, self->piece.rotation,
                                     py, px) &&
                            y == self->piece.y + py && x == self->piece.x + px)
                            filled = true;
            }
            if (self->shown[(uint8_t)y][(uint8_t)x] != filled) {
                self->shown[(uint8_t)y][(uint8_t)x] = filled;
                set_style(self, self->cells[(uint8_t)y][(uint8_t)x],
                          GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                          color(filled ? 0xF0 : 0x00));
                set_style(self, self->cells[(uint8_t)y][(uint8_t)x],
                          GM_PLUGIN_LVGL_STYLE_BG_OPA,
                          number(255));
            }
        }
    }
}

static void clear_lines(tetris_t *self)
{
    int8_t y;
    for (y = ROWS - 1; y >= 0; --y) {
        uint8_t x;
        bool full = true;
        for (x = 0; x < COLS; ++x)
            if (self->board[(uint8_t)y][x] == 0U) full = false;
        if (full) {
            int8_t row;
            for (row = y; row > 0; --row)
                for (x = 0; x < COLS; ++x)
                    self->board[(uint8_t)row][x] =
                        self->board[(uint8_t)(row - 1)][x];
            for (x = 0; x < COLS; ++x) self->board[0][x] = 0;
            self->score = (uint16_t)(self->score + 100U);
            ++y;
        }
    }
    show_score(self);
}

static void lock_piece(tetris_t *self)
{
    int8_t py;
    int8_t px;
    for (py = 0; py < 4; ++py)
        for (px = 0; px < 4; ++px)
            if (occupied(self->piece.type, self->piece.rotation, py, px) != 0U)
                self->board[(uint8_t)(self->piece.y + py)]
                           [(uint8_t)(self->piece.x + px)] = 1;
    clear_lines(self);
    spawn(self);
}

static void step_down(tetris_t *self)
{
    if (can_place(self, self->piece.type, self->piece.rotation,
                  self->piece.x, (int8_t)(self->piece.y + 1)) != 0U)
        ++self->piece.y;
    else
        lock_piece(self);
}

static void move_horizontal(tetris_t *self, int8_t delta)
{
    int8_t next = (int8_t)(self->piece.x + delta);
    if (can_place(self, self->piece.type, self->piece.rotation,
                  next, self->piece.y) != 0U) self->piece.x = next;
}

static void rotate(tetris_t *self)
{
    int8_t rotation = (int8_t)((self->piece.rotation + 1) & 3);
    int8_t dx;
    for (dx = 0; dx >= -1; --dx) {
        if (can_place(self, self->piece.type, rotation,
                      (int8_t)(self->piece.x + dx), self->piece.y) != 0U) {
            self->piece.x = (int8_t)(self->piece.x + dx);
            self->piece.rotation = rotation;
            return;
        }
    }
    if (can_place(self, self->piece.type, rotation,
                  (int8_t)(self->piece.x + 1), self->piece.y) != 0U) {
        ++self->piece.x;
        self->piece.rotation = rotation;
    }
}

static void reset_game(tetris_t *self)
{
    uint8_t y;
    uint8_t x;
    for (y = 0; y < ROWS; ++y)
        for (x = 0; x < COLS; ++x) {
            self->board[y][x] = 0;
            self->shown[y][x] = 2;
        }
    self->score = 0;
    self->drop_accumulator = 0;
    self->move_wait_ms = 0;
    self->game_over = false;
    self->fast_drop = false;
    self->exit_ms = 0;
    self->exit_seconds = 3;
    self->ui->obj_add_flag(self->exit_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->random_state ^= self->host->monotonic_ms() | 1U;
    self->ui->label_set_text(self->info_label, self->strings->controls);
    show_score(self);
    spawn(self);
    render(self);
}

static gm_plugin_result_t create_ui(tetris_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *root = self->ui->root_get();
    int16_t board_x;
    int16_t width_cell_size;
    int16_t exit_width;
    uint8_t y;
    uint8_t x;
    if (root == 0 || self->host->display_get_info(&display) != GM_PLUGIN_OK)
        return GM_PLUGIN_ESTATE;
    if (display.width <= INFO_LEFT + INFO_MIN_WIDTH + INFO_GAP +
                         BOARD_RIGHT_MARGIN ||
        display.height <= SCREEN_EDGE * 2U)
        return GM_PLUGIN_ENOTSUP;
    self->screen_width = (int16_t)display.width;
    self->screen_height = (int16_t)display.height;
    self->cell_size = (self->screen_height - SCREEN_EDGE * 2) / ROWS;
    width_cell_size = (self->screen_width - INFO_LEFT - INFO_MIN_WIDTH -
                       INFO_GAP - BOARD_RIGHT_MARGIN) / COLS;
    if (self->cell_size > width_cell_size)
        self->cell_size = width_cell_size;
    if (self->cell_size < MIN_CELL_SIZE)
        return GM_PLUGIN_ENOTSUP;
    self->board_width = self->cell_size * COLS;
    self->board_height = self->cell_size * ROWS;
    self->ui->obj_clean(root);
    self->root = self->ui->obj_create(root);
    if (self->root == 0) return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_size(self->root, self->screen_width,
                           self->screen_height);
    self->ui->obj_align(self->root, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_box(self, self->root, 0, 0);
    set_style(self, self->root, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    self->ui->obj_clear_flag(self->root, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    board_x = self->screen_width - self->board_width - BOARD_RIGHT_MARGIN;
    self->board_obj = self->ui->obj_create(self->root);
    self->score_label = self->ui->label_create(self->root);
    self->info_label = self->ui->label_create(self->root);
    self->exit_label = self->ui->label_create(self->root);
    if (self->board_obj == 0 || self->score_label == 0 ||
        self->info_label == 0 || self->exit_label == 0)
        return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_pos(self->board_obj, board_x, SCREEN_EDGE);
    self->ui->obj_set_size(self->board_obj, self->board_width,
                           self->board_height);
    style_box(self, self->board_obj, 0, 1);
    set_style(self, self->board_obj, GM_PLUGIN_LVGL_STYLE_BORDER_POST,
              number(1));
    set_style(self, self->board_obj, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(0x10));
    set_style(self, self->board_obj, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    self->ui->obj_clear_flag(self->board_obj, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    self->ui->obj_set_pos(self->score_label, INFO_LEFT, 20);
    set_style(self, self->score_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xF0));
    self->ui->obj_set_pos(self->info_label, INFO_LEFT, 55);
    self->ui->obj_set_size(self->info_label,
                           board_x - INFO_LEFT - INFO_GAP,
                           self->screen_height - 70);
    self->ui->label_set_long_mode(self->info_label, GM_PLUGIN_LVGL_LABEL_WRAP);
    set_style(self, self->info_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
              color(0xA0));
    exit_width = self->screen_width - 32;
    if (exit_width > 180) exit_width = 180;
    self->ui->obj_set_size(self->exit_label, exit_width, 32);
    self->ui->obj_align(self->exit_label, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 0);
    set_style(self, self->exit_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    style_box(self, self->exit_label, 0, 1);
    self->ui->obj_add_flag(self->exit_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    for (y = 0; y < ROWS; ++y) {
        for (x = 0; x < COLS; ++x) {
            gm_plugin_lvgl_obj_t *cell = self->ui->obj_create(self->board_obj);
            if (cell == 0) return GM_PLUGIN_ENOMEM;
            self->cells[y][x] = cell;
            self->ui->obj_set_pos(cell, x * self->cell_size,
                                  y * self->cell_size);
            self->ui->obj_set_size(cell, self->cell_size - 1,
                                   self->cell_size - 1);
            style_box(self, cell, 0, 0);
            set_style(self, cell, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
            self->ui->obj_clear_flag(cell, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
        }
    }
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t plugin_start(void *opaque)
{
    tetris_t *self = opaque;
    gm_plugin_result_t result;
    char language_tag[GM_PLUGIN_LOCALE_TAG_MAX] = {0};
    if (self->host->locale_get(language_tag) != GM_PLUGIN_OK)
        language_tag[0] = '\0';
    self->strings = select_strings(language_tag);
    result = self->host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW |
                                    GM_PLUGIN_IMU_ENABLE_GESTURES);
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
    tetris_t *self = opaque;
    gm_plugin_imu_sample_t imu;
    uint32_t interval;
    if (elapsed_ms > 200U) elapsed_ms = 200U;
    if (self->move_wait_ms > elapsed_ms) self->move_wait_ms -= elapsed_ms;
    else self->move_wait_ms = 0;
    if (self->host->imu_read(&imu) == GM_PLUGIN_OK) {
        int32_t gx = imu.gyro_raw[0];
        int32_t gy = imu.gyro_raw[1];
        int32_t ax = gx < 0 ? -gx : gx;
        int32_t ay = gy < 0 ? -gy : gy;
        if (imu.pitch_degrees > EXIT_PITCH) {
            self->exit_ms += elapsed_ms;
            while (self->exit_ms >= 1000U) {
                self->exit_ms -= 1000U;
                if (self->exit_seconds == 0U) {
                    self->host->app_exit();
                    return;
                }
                show_exit_countdown(self, self->exit_seconds);
                --self->exit_seconds;
            }
        } else {
            self->exit_ms = 0;
            self->exit_seconds = 3;
            self->ui->obj_add_flag(self->exit_label,
                                   GM_PLUGIN_LVGL_FLAG_HIDDEN);
        }
        if (!self->game_over && self->move_wait_ms == 0U &&
            (ax > GYRO_THRESHOLD || ay > GYRO_THRESHOLD)) {
            if (gx > 0 && gy < 0) move_horizontal(self, 1);
            else if (gx < 0 && gy > 0) move_horizontal(self, -1);
            self->move_wait_ms = MOVE_FILTER_MS;
        }
    }
    if (self->game_over) return;
    self->frame_accumulator += elapsed_ms;
    if (self->frame_accumulator < FRAME_MS) return;
    elapsed_ms = self->frame_accumulator;
    self->frame_accumulator = 0;
    self->drop_accumulator += elapsed_ms;
    interval = self->fast_drop ? FAST_DROP_MS : NORMAL_DROP_MS;
    while (self->drop_accumulator >= interval && !self->game_over) {
        self->drop_accumulator -= interval;
        step_down(self);
    }
    render(self);
}

static bool plugin_event(void *opaque, const gm_plugin_event_t *event)
{
    tetris_t *self = opaque;
    gm_plugin_button_action_t action;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_BUTTON) return false;
    action = event->data.button.action;
    if (action == GM_PLUGIN_BUTTON_ACTION_RELEASE) {
        self->fast_drop = false;
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_LONG ||
        action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
        if (!self->game_over) self->fast_drop = true;
        return true;
    }
    if (action == GM_PLUGIN_BUTTON_ACTION_SINGLE ||
        action == GM_PLUGIN_BUTTON_ACTION_DOUBLE) {
        if (self->game_over) reset_game(self);
        else {
            rotate(self);
            render(self);
        }
        return true;
    }
    return false;
}

static void plugin_stop(void *opaque)
{
    tetris_t *self = opaque;
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
    if (game.ui == 0 || game.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(game.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    game.random_state = UINT32_C(0x54455452);
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &game;
    plugin->on_start = plugin_start;
    plugin->on_loop = plugin_loop;
    plugin->on_event = plugin_event;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
