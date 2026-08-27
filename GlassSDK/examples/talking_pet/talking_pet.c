#include "gm_plugin_lvgl_api.h"
#include "momo_sprites.h"

#define PET_STATE_CHANNEL UINT16_C(0x4D50)
#define BUTTON_CHANNEL UINT16_C(0x0100)
#define CONNECTION_CHANNEL UINT16_C(0x0103)
#define PROTOCOL_VERSION 1U
#define FRAME_MS 80U
#define ANIMATION_LEFT 182
#define ANIMATION_TOP 51
#define LEFT_PANEL_X 18
#define LEFT_PANEL_WIDTH 164
#define RIGHT_PANEL_WIDTH 164
#define PANEL_TOP 62
#define PANEL_HEIGHT 256

enum {
    MOOD_IDLE = 0,
    MOOD_HAPPY,
    MOOD_EATING,
    MOOD_PLAYING,
    MOOD_SLEEPING,
    MOOD_LISTENING,
    MOOD_TALKING,
};

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *title_label;
    gm_plugin_lvgl_obj_t *subtitle_label;
    gm_plugin_lvgl_obj_t *level_label;
    gm_plugin_lvgl_obj_t *stat_name_labels[3];
    gm_plugin_lvgl_obj_t *stat_value_labels[3];
    gm_plugin_lvgl_obj_t *stat_bar_fills[3];
    gm_plugin_lvgl_obj_t *feels_label;
    gm_plugin_lvgl_obj_t *mood_label;
    gm_plugin_lvgl_obj_t *hints_label;
    uint16_t width;
    uint16_t height;
    uint32_t sequence;
    uint32_t frame_accumulator;
    uint32_t animation_ms;
    uint32_t local_mood_ms;
    uint8_t mood;
    uint8_t happy;
    uint8_t food;
    uint8_t energy;
    uint16_t level;
} pet_t;

static pet_t s_pet;

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
        while (column + 1 < end_x) {
            *target++ = packed;
            column = (int16_t)(column + 2);
        }
        if (column < end_x) pixel(surface, column, row, gray);
    }
}

static void outline(gm_plugin_framebuffer_surface_t *surface,
                    int16_t x, int16_t y, int16_t width, int16_t height,
                    uint8_t gray)
{
    rectangle(surface, x, y, width, 1, gray);
    rectangle(surface, x, (int16_t)(y + height - 1), width, 1, gray);
    rectangle(surface, x, y, 1, height, gray);
    rectangle(surface, (int16_t)(x + width - 1), y, 1, height, gray);
}

static void rounded_panel(gm_plugin_framebuffer_surface_t *surface,
                          int16_t x, int16_t y, int16_t width, int16_t height,
                          uint8_t fill, uint8_t border)
{
    rectangle(surface, (int16_t)(x + 5), y,
              (int16_t)(width - 10), 1, border);
    rectangle(surface, (int16_t)(x + 5), (int16_t)(y + height - 1),
              (int16_t)(width - 10), 1, border);
    rectangle(surface, x, (int16_t)(y + 5), 1,
              (int16_t)(height - 10), border);
    rectangle(surface, (int16_t)(x + width - 1), (int16_t)(y + 5), 1,
              (int16_t)(height - 10), border);
    rectangle(surface, (int16_t)(x + 2), (int16_t)(y + 2), 3, 1, border);
    rectangle(surface, (int16_t)(x + width - 5), (int16_t)(y + 2),
              3, 1, border);
    rectangle(surface, (int16_t)(x + 2), (int16_t)(y + height - 3),
              3, 1, border);
    rectangle(surface, (int16_t)(x + width - 5),
              (int16_t)(y + height - 3), 3, 1, border);
    rectangle(surface, (int16_t)(x + 1), (int16_t)(y + 5),
              (int16_t)(width - 2), (int16_t)(height - 10), fill);
    rectangle(surface, (int16_t)(x + 5), (int16_t)(y + 1),
              (int16_t)(width - 10), 4, fill);
    rectangle(surface, (int16_t)(x + 5), (int16_t)(y + height - 5),
              (int16_t)(width - 10), 4, fill);
}

static void ellipse(gm_plugin_framebuffer_surface_t *surface,
                    int16_t center_x, int16_t center_y,
                    int16_t radius_x, int16_t radius_y, uint8_t gray)
{
    int16_t row;
    int32_t denominator = (int32_t)radius_y * radius_y;
    for (row = (int16_t)-radius_y; row <= radius_y; ++row) {
        int32_t shrink = (int32_t)row * row * radius_x / denominator;
        int16_t half = (int16_t)(radius_x - shrink);
        rectangle(surface, (int16_t)(center_x - half),
                  (int16_t)(center_y + row),
                  (int16_t)(half * 2 + 1), 1, gray);
    }
}

static void number_text(char *output, uint32_t value)
{
    char reverse[10];
    uint8_t count = 0;
    uint8_t index;
    do {
        reverse[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U && count < sizeof(reverse));
    for (index = 0; index < count; ++index)
        output[index] = reverse[count - index - 1U];
    output[count] = '\0';
}

static const char *mood_text(uint8_t mood)
{
    static const char *const messages[] = {
        "READY", "PURR", "EATING", "PLAYING",
        "ASLEEP", "LISTEN", "TALKING"
    };
    return messages[mood <= MOOD_TALKING ? mood : MOOD_IDLE];
}

static gm_plugin_lvgl_style_value_t font_value(
    const gm_plugin_lvgl_font_t *font)
{
    gm_plugin_lvgl_style_value_t value = {0};
    value.ptr = font;
    return value;
}

static gm_plugin_lvgl_obj_t *create_box(
    pet_t *self, int16_t x, int16_t y, int16_t width, int16_t height,
    uint8_t fill, uint8_t border, uint8_t border_width, uint8_t radius)
{
    gm_plugin_lvgl_obj_t *object = self->ui->obj_create(self->root);
    if (object == 0) return 0;
    self->ui->obj_set_pos(object, x, y);
    self->ui->obj_set_size(object, width, height);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        gm_plugin_lvgl_style_color((uint8_t)((fill << 4) | fill)),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                        gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
                        gm_plugin_lvgl_style_color(
                            (uint8_t)((border << 4) | border)),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA,
                        gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                        gm_plugin_lvgl_style_number(border_width),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_RADIUS,
                        gm_plugin_lvgl_style_number(radius),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
                        gm_plugin_lvgl_style_number(0),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
                        gm_plugin_lvgl_style_number(0),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
                        gm_plugin_lvgl_style_number(0),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(object, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
                        gm_plugin_lvgl_style_number(0),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    return object;
}

static gm_plugin_result_t create_chrome_ui(pet_t *self)
{
    static const int16_t stat_y[3] = {70, 148, 226};
    static const uint8_t stat_gray[3] = {15, 11, 8};
    int16_t right_x = (int16_t)(self->width - RIGHT_PANEL_WIDTH - 18U);
    uint8_t index;
    if (create_box(self, 18, 49, (int16_t)(self->width - 36U), 1,
                   4, 4, 0, 0) == 0 ||
        create_box(self, 18, 49, 104, 2, 12, 12, 0, 0) == 0 ||
        create_box(self, LEFT_PANEL_X, PANEL_TOP, LEFT_PANEL_WIDTH,
                   PANEL_HEIGHT, 0, 5, 1, 5) == 0 ||
        create_box(self, right_x, PANEL_TOP, RIGHT_PANEL_WIDTH,
                   PANEL_HEIGHT, 0, 5, 1, 5) == 0 ||
        create_box(self, (int16_t)(right_x + 18), 153, 128, 1,
                   5, 5, 0, 0) == 0 ||
        create_box(self, (int16_t)(right_x + 10), 174, 2, 107,
                   7, 7, 0, 0) == 0)
        return GM_PLUGIN_ENOMEM;
    for (index = 0; index < 3U; ++index) {
        uint8_t value = index == 0U ? self->happy :
            (index == 1U ? self->food : self->energy);
        if (create_box(self, 27, (int16_t)(stat_y[index] + 11), 8, 8,
                       stat_gray[index], stat_gray[index], 0, 4) == 0 ||
            create_box(self, 44, (int16_t)(stat_y[index] + 39), 122, 10,
                       3, 3, 0, 0) == 0)
            return GM_PLUGIN_ENOMEM;
        self->stat_bar_fills[index] = create_box(
            self, 46, (int16_t)(stat_y[index] + 41),
            (int16_t)(118U * value / 100U), 6,
            stat_gray[index], stat_gray[index], 0, 0);
        if (self->stat_bar_fills[index] == 0) return GM_PLUGIN_ENOMEM;
        if (create_box(self, (int16_t)(right_x + 8),
                       (int16_t)(181 + index * 38), 6, 6,
                       stat_gray[index], stat_gray[index], 0, 3) == 0)
            return GM_PLUGIN_ENOMEM;
    }
    return GM_PLUGIN_OK;
}

static gm_plugin_lvgl_obj_t *create_label(
    pet_t *self, int16_t x, int16_t y, int16_t width, int16_t height,
    uint8_t gray, const gm_plugin_lvgl_font_t *font, uint8_t alignment)
{
    gm_plugin_lvgl_obj_t *label = self->ui->label_create(self->root);
    if (label == 0) return 0;
    self->ui->obj_set_pos(label, x, y);
    self->ui->obj_set_size(label, width, height);
    self->ui->label_set_long_mode(label, GM_PLUGIN_LVGL_LABEL_CLIP);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                        gm_plugin_lvgl_style_number(
                            GM_PLUGIN_LVGL_OPA_TRANSPARENT),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        gm_plugin_lvgl_style_color(
                            (uint8_t)((gray << 4) | gray)),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_OPA,
                        gm_plugin_lvgl_style_number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
                        font_value(font), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        gm_plugin_lvgl_style_number(alignment),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    return label;
}

static void update_text_labels(pet_t *self)
{
    char number[11];
    uint8_t values[3] = {self->happy, self->food, self->energy};
    uint8_t index;
    for (index = 0; index < 3U; ++index) {
        number_text(number, values[index]);
        self->ui->label_set_text(self->stat_value_labels[index], number);
        if (self->stat_bar_fills[index] != 0)
            self->ui->obj_set_size(self->stat_bar_fills[index],
                                   (int16_t)(118U * values[index] / 100U), 6);
    }
    number[0] = 'L';
    number[1] = 'V';
    number[2] = '.';
    number[3] = ' ';
    number_text(number + 4, self->level);
    self->ui->label_set_text(self->level_label, number);
    self->ui->label_set_text(self->mood_label, mood_text(self->mood));
}

static gm_plugin_result_t create_text_ui(pet_t *self)
{
    static const char *const names[3] = {"HAPPY", "FULL", "ENERGY"};
    static const int16_t stat_y[3] = {70, 148, 226};
    int16_t right_x = (int16_t)(self->width - 182U);
    uint8_t index;
    self->root = self->ui->root_get();
    if (self->root == 0) return GM_PLUGIN_ENOTSUP;
    self->ui->obj_clean(self->root);
    if (create_chrome_ui(self) != GM_PLUGIN_OK) return GM_PLUGIN_ENOMEM;
    self->title_label = create_label(self, 28, 7, 88, 34, 15,
                                     self->ui->font_default,
                                     GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->subtitle_label = create_label(self, 116, 7, 214, 34, 8,
                                        self->ui->font_default,
                                        GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->level_label = create_label(self, (int16_t)(self->width - 108U),
                                     7, 80, 34, 13,
                                     self->ui->font_default,
                                     GM_PLUGIN_LVGL_TEXT_ALIGN_RIGHT);
    for (index = 0; index < 3U; ++index) {
        self->stat_name_labels[index] = create_label(
            self, 44, stat_y[index], 82, 34,
            (uint8_t)(15U - index * 3U), self->ui->font_default,
            GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
        self->stat_value_labels[index] = create_label(
            self, 132, stat_y[index], 34, 34, 15,
            self->ui->font_default, GM_PLUGIN_LVGL_TEXT_ALIGN_RIGHT);
    }
    self->feels_label = create_label(self, (int16_t)(right_x + 18), 70,
                                     128, 34, 9,
                                     self->ui->font_default,
                                     GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->mood_label = create_label(self, (int16_t)(right_x + 18), 105,
                                    128, 34, 14, self->ui->font_default,
                                    GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    self->hints_label = create_label(self, (int16_t)(right_x + 18), 168,
                                     128, 112, 11,
                                     self->ui->font_default,
                                     GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT);
    if (self->title_label == 0 || self->subtitle_label == 0 ||
        self->level_label == 0 ||
        self->feels_label == 0 || self->mood_label == 0 ||
        self->hints_label == 0)
        return GM_PLUGIN_ENOMEM;
    for (index = 0; index < 3U; ++index)
        if (self->stat_name_labels[index] == 0 ||
            self->stat_value_labels[index] == 0)
            return GM_PLUGIN_ENOMEM;
    self->ui->style_set(self->hints_label,
                        GM_PLUGIN_LVGL_STYLE_TEXT_LINE_SPACE,
                        gm_plugin_lvgl_style_number(2),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->label_set_text(self->title_label, "MOMO");
    self->ui->label_set_text(self->subtitle_label, "/ PLAYROOM");
    for (index = 0; index < 3U; ++index)
        self->ui->label_set_text(self->stat_name_labels[index], names[index]);
    self->ui->label_set_text(self->feels_label, "MOOD");
    self->ui->label_set_text(self->hints_label,
                             "PHONE\nFEED  PLAY\nSLEEP TALK");
    update_text_labels(self);
    return GM_PLUGIN_OK;
}

static uint8_t sprite_frame(const pet_t *self)
{
    uint32_t phase = self->animation_ms;
    switch (self->mood) {
    case MOOD_HAPPY: return (phase / 240U) % 2U == 0U ? 2U : 0U;
    case MOOD_EATING:
        if (phase < 480U) return 0U;
        return phase < 1700U ? 3U : 1U;
    case MOOD_PLAYING:
        if (phase < 520U) return 0U;
        return phase < 1850U ? 4U : 2U;
    case MOOD_SLEEPING: return 5U;
    case MOOD_LISTENING: return (phase / 320U) % 2U == 0U ? 6U : 1U;
    case MOOD_TALKING: return (phase / 140U) % 2U == 0U ? 7U : 2U;
    default: return phase % 4200U > 3900U ? 1U : 0U;
    }
}

static void sprite_offset(const pet_t *self, int16_t *x, int16_t *y)
{
    static const int8_t bounce[10] = {0,-5,-12,-20,-26,-20,-12,-5,0,2};
    uint32_t step = self->animation_ms / FRAME_MS;
    *x = (int16_t)((self->width - MOMO_SPRITE_WIDTH) / 2U);
    *y = 91;
    if (self->mood == MOOD_HAPPY) *y += bounce[step % 10U] / 3;
    else if (self->mood == MOOD_EATING) *x += (step & 1U) != 0U ? 2 : -2;
    else if (self->mood == MOOD_PLAYING) {
        *x += (step & 1U) != 0U ? 4 : -4;
        *y += bounce[step % 10U];
    } else if (self->mood == MOOD_SLEEPING) {
        *y += (step & 1U) != 0U ? 4 : 6;
    } else if (self->mood == MOOD_LISTENING) {
        *x += (step & 1U) != 0U ? 3 : -3;
    } else if (self->mood == MOOD_TALKING) {
        *y += (step & 1U) != 0U ? -2 : 1;
    } else {
        *y += (int16_t)((step / 4U) % 4U) - 2;
    }
}

static void draw_sprite(gm_plugin_framebuffer_surface_t *surface,
                        const pet_t *self)
{
    uint8_t frame = sprite_frame(self);
    const uint8_t *encoded = momo_sprites[frame];
    uint32_t encoded_size = momo_sprite_sizes[frame];
    uint32_t offset = 0;
    uint32_t index = 0;
    int16_t sprite_x;
    int16_t sprite_y;
    sprite_offset(self, &sprite_x, &sprite_y);
    while (offset + 1U < encoded_size &&
           index < MOMO_SPRITE_WIDTH * MOMO_SPRITE_HEIGHT) {
        uint8_t count = encoded[offset++];
        uint8_t value = encoded[offset++];
        uint8_t run;
        for (run = 0; run < count; ++run, ++index) {
            int16_t y;
            if (value == 0U) continue;
            y = (int16_t)(sprite_y + index / MOMO_SPRITE_WIDTH);
            if (y >= (int16_t)surface->y &&
                y < (int16_t)(surface->y + surface->height))
                pixel(surface,
                      (int16_t)(sprite_x + index % MOMO_SPRITE_WIDTH),
                      y, (uint8_t)(value - 1U));
        }
    }
}

static void sparkle(gm_plugin_framebuffer_surface_t *surface,
                    int16_t x, int16_t y, uint8_t size, uint8_t gray)
{
    rectangle(surface, (int16_t)(x - size), y,
              (int16_t)(size * 2U + 1U), 1, gray);
    rectangle(surface, x, (int16_t)(y - size), 1,
              (int16_t)(size * 2U + 1U), gray);
}

static void sleep_mark(gm_plugin_framebuffer_surface_t *surface,
                       int16_t x, int16_t y, uint8_t size, uint8_t gray)
{
    uint8_t step;
    rectangle(surface, x, y, (int16_t)(size * 3U), size, gray);
    for (step = 0; step < size * 2U; ++step)
        rectangle(surface, (int16_t)(x + size * 2U - step),
                  (int16_t)(y + size + step), size, size, gray);
    rectangle(surface, x, (int16_t)(y + size * 3U),
              (int16_t)(size * 3U), size, gray);
}

static void draw_effects(gm_plugin_framebuffer_surface_t *surface,
                         const pet_t *self)
{
    uint32_t step = self->animation_ms / FRAME_MS;
    int16_t center = (int16_t)(self->width / 2U);
    if (self->mood == MOOD_HAPPY || self->mood == MOOD_PLAYING) {
        sparkle(surface, (int16_t)(center - 94),
                (int16_t)(105 + (step * 5U) % 52U), 4, 15);
        sparkle(surface, (int16_t)(center + 92),
                (int16_t)(142 - (step * 4U) % 39U), 3, 12);
        sparkle(surface, (int16_t)(center + 112),
                (int16_t)(210 + (step * 3U) % 42U), 2, 10);
    } else if (self->mood == MOOD_EATING) {
        if (self->animation_ms >= 480U && self->animation_ms < 1700U) {
            rectangle(surface,
                      (int16_t)(center - 92 + (step % 3U) * 7U),
                      (int16_t)(220 + (step % 4U) * 5U), 4, 4, 12);
            rectangle(surface,
                      (int16_t)(center + 78 - (step % 3U) * 6U),
                      (int16_t)(206 + (step % 5U) * 4U), 3, 3, 9);
        }
    } else if (self->mood == MOOD_SLEEPING) {
        sleep_mark(surface, (int16_t)(center + 72),
                   (int16_t)(82 - (step % 4U) * 4U), 3, 15);
        sleep_mark(surface, (int16_t)(center + 98),
                   (int16_t)(66 - (step % 4U) * 3U), 2, 10);
    } else if (self->mood == MOOD_LISTENING) {
        int16_t spread = (int16_t)(6 + (step % 4U) * 5U);
        outline(surface, (int16_t)(center - 96 - spread), 122,
                (int16_t)(10 + spread), 46, 8);
        outline(surface, (int16_t)(center + 82), 122,
                (int16_t)(10 + spread), 46, 8);
    } else if (self->mood == MOOD_TALKING) {
        uint8_t width = (uint8_t)(8U + (step % 4U) * 5U);
        rectangle(surface, (int16_t)(center + 84), 125, width, 2, 13);
        rectangle(surface, (int16_t)(center + 84), 135,
                  (int16_t)(width + 8U), 2, 9);
        rectangle(surface, (int16_t)(center + 84), 145, width, 2, 6);
    }
}

static void draw_animation(gm_plugin_framebuffer_surface_t *surface,
                           const pet_t *self, uint8_t clear)
{
    if (clear != 0U)
        rectangle(surface, ANIMATION_LEFT, ANIMATION_TOP,
                  (int16_t)(self->width - ANIMATION_LEFT * 2U),
                  (int16_t)(self->height - ANIMATION_TOP), 0);
    rounded_panel(surface, (int16_t)(self->width / 2U - 108U), 57,
                  216, 262, 0, 5);
    rectangle(surface, (int16_t)(self->width / 2U - 94U), 72,
              188, 1, 3);
    ellipse(surface, (int16_t)(self->width / 2U), 288, 66, 8, 3);
    ellipse(surface, (int16_t)(self->width / 2U), 287, 48, 5, 2);
    draw_sprite(surface, self);
    draw_effects(surface, self);
}

static gm_plugin_result_t render(pet_t *self)
{
    uint16_t next_y = 0;
    while (next_y < self->height) {
        gm_plugin_framebuffer_surface_t surface;
        gm_plugin_rect_t dirty;
        uint16_t part_end;
        gm_plugin_result_t result =
            self->host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) return result;
        part_end = (uint16_t)(surface.y + surface.height);
        if (part_end > self->height) part_end = self->height;
        int16_t dirty_y = surface.y > ANIMATION_TOP
            ? (int16_t)surface.y : ANIMATION_TOP;
        draw_animation(&surface, self, 1);
        dirty.x = ANIMATION_LEFT;
        dirty.y = dirty_y;
        dirty.width = (uint16_t)(self->width - ANIMATION_LEFT * 2U);
        dirty.height = (uint16_t)(part_end - dirty_y);
        result = self->host->graphics.framebuffer.unlock(
            &dirty, part_end == self->height);
        if (result != GM_PLUGIN_OK) return result;
        next_y = part_end;
    }
    return GM_PLUGIN_OK;
}

static void set_mood(pet_t *self, uint8_t mood, uint32_t local_ms)
{
    if (mood > MOOD_TALKING) mood = MOOD_IDLE;
    if (mood != self->mood) self->animation_ms = 0;
    self->mood = mood;
    self->local_mood_ms = local_ms;
    if (self->mood_label != 0)
        self->ui->label_set_text(self->mood_label, mood_text(self->mood));
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void event_header(pet_t *self, uint8_t *data, uint8_t type,
                         uint32_t timestamp)
{
    data[0] = PROTOCOL_VERSION;
    data[1] = type;
    write_u32(data + 2, self->sequence++);
    write_u32(data + 6, timestamp);
}

static bool send_button(pet_t *self, const gm_plugin_event_t *event)
{
    uint8_t data[14];
    event_header(self, data, 1, event->timestamp_ms);
    write_u16(data + 10, event->data.button.button);
    write_u16(data + 12, event->data.button.action);
    (void)self->host->bt_send(BUTTON_CHANNEL, data, sizeof(data));
    return true;
}

static bool receive_state(pet_t *self, const uint8_t *data, uint32_t length)
{
    if (data == 0 || length != 7U || data[0] != PROTOCOL_VERSION ||
        data[1] > MOOD_TALKING || data[2] > 100U || data[3] > 100U ||
        data[4] > 100U) return false;
    self->happy = data[2];
    self->food = data[3];
    self->energy = data[4];
    self->level = (uint16_t)(((uint16_t)data[5] << 8) | data[6]);
    if (self->level == 0U) self->level = 1U;
    set_mood(self, data[1], 0);
    update_text_labels(self);
    return true;
}

static gm_plugin_result_t on_start(void *opaque)
{
    pet_t *self = opaque;
    gm_plugin_display_info_t display;
    gm_plugin_result_t result = self->host->display_get_info(&display);
    if (result != GM_PLUGIN_OK) return result;
    if (display.width < 500U || display.height < 300U)
        return GM_PLUGIN_ENOTSUP;
    self->width = display.width;
    self->height = display.height;
    self->sequence = 0;
    self->frame_accumulator = 0;
    self->animation_ms = 0;
    self->local_mood_ms = 0;
    self->mood = MOOD_IDLE;
    self->happy = 78;
    self->food = 62;
    self->energy = 84;
    self->level = 1;
    result = create_text_ui(self);
    if (result != GM_PLUGIN_OK) return result;
    return render(self);
}

static void on_loop(void *opaque, uint32_t elapsed_ms)
{
    pet_t *self = opaque;
    self->frame_accumulator += elapsed_ms;
    if (self->frame_accumulator > 320U) self->frame_accumulator = 320U;
    while (self->frame_accumulator >= FRAME_MS) {
        self->frame_accumulator -= FRAME_MS;
        self->animation_ms += FRAME_MS;
        if (self->local_mood_ms != 0U) {
            if (self->local_mood_ms <= FRAME_MS) {
                self->local_mood_ms = 0;
                set_mood(self, MOOD_IDLE, 0);
            } else {
                self->local_mood_ms -= FRAME_MS;
            }
        }
        (void)render(self);
    }
}

static bool on_event(void *opaque, const gm_plugin_event_t *event)
{
    pet_t *self = opaque;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != PET_STATE_CHANNEL) return false;
        return receive_state(self, event->data.bt.data, event->data.bt.length);
    }
    if (event->type == GM_PLUGIN_EVENT_BUTTON) {
        if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_SINGLE)
            set_mood(self, MOOD_HAPPY, 1600U);
        else if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_DOUBLE)
            set_mood(self, MOOD_EATING, 2200U);
        else if (event->data.button.action == GM_PLUGIN_BUTTON_ACTION_LONG)
            set_mood(self, MOOD_SLEEPING, 3000U);
        return send_button(self, event);
    }
    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        uint8_t data[11];
        if (!event->data.connection.connected) return true;
        event_header(self, data, 4, event->timestamp_ms);
        data[10] = 1;
        (void)self->host->bt_send(CONNECTION_CHANNEL, data, sizeof(data));
        return true;
    }
    return false;
}

static void on_suspend(void *opaque)
{
    (void)opaque;
}

static void on_resume(void *opaque)
{
    pet_t *self = opaque;
    self->frame_accumulator = 0;
    (void)render(self);
}

static void on_stop(void *opaque)
{
    pet_t *self = opaque;
    if (self->root != 0) self->ui->obj_clean(self->root);
    self->root = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_DISPLAY_BITMAP |
        GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_BLUETOOTH;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->display_get_info == 0 ||
        host->graphics.lvgl == 0 ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 ||
        host->bt_send == 0 ||
        (host->capabilities & required) != required ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_ENOTSUP;
    s_pet.host = host;
    s_pet.ui = host->graphics.lvgl;
    if (s_pet.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(s_pet.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_ENOTSUP;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &s_pet;
    plugin->on_start = on_start;
    plugin->on_resume = on_resume;
    plugin->on_loop = on_loop;
    plugin->on_event = on_event;
    plugin->on_suspend = on_suspend;
    plugin->on_stop = on_stop;
    return GM_PLUGIN_OK;
}
