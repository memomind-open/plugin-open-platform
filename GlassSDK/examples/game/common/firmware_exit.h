/* Port of glassproject's circle_exit_pb_widget (09bd6ae5b).
 * Analytic antialiased ring pixels avoid the visible corners from short,
 * integer-coordinate line segments. No private LVGL or Host ABI changes. */
#ifndef FIRMWARE_GAME_EXIT_H
#define FIRMWARE_GAME_EXIT_H

#include "gm_plugin_lvgl_api.h"
#include "firmware_exit_art.h"

typedef struct {
    gm_plugin_lvgl_obj_t *overlay;
    gm_plugin_lvgl_obj_t *ring;
    gm_plugin_lvgl_obj_t *ring_full;
    uint32_t started_ms;
    uint8_t step;
    bool active;
    bool committed; /* Exit requested: keep covering the game until Host stop. */
} firmware_exit_t;

static gm_plugin_lvgl_obj_t *firmware_exit_picture(
    const gm_plugin_lvgl_api_t *ui, gm_plugin_lvgl_obj_t *parent,
    uint16_t side, const uint8_t *pixels, uint32_t pixel_size,
    const firmware_exit_run_t *runs, unsigned run_count)
{
    gm_plugin_lvgl_obj_t *picture;
    if (ui->struct_size >= GM_PLUGIN_LVGL_API_1_1_SIZE &&
        GM_PLUGIN_VERSION_COMPATIBLE(ui->api_version, GM_PLUGIN_VERSION(1U, 1U)) &&
        ui->image_create) {
        gm_plugin_lvgl_image_dsc_t image = {0};
        image.struct_size = sizeof(image);
        image.width = side;
        image.height = side;
        image.format = GM_PLUGIN_LVGL_IMAGE_INDEXED_4BIT;
        image.data = pixels;
        image.data_size = pixel_size;
        picture = ui->image_create(parent, &image);
    } else {
        /* 1.0 Hosts paint identical coverage using horizontal pixel runs. */
        picture = ui->obj_create(parent);
        if (!picture) return 0;
        ui->obj_set_size(picture, side, side);
        ui->obj_clear_flag(picture, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
        ui->style_set(picture, GM_PLUGIN_LVGL_STYLE_BG_OPA, gm_plugin_lvgl_style_number(0), 0);
        ui->style_set(picture, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, gm_plugin_lvgl_style_number(0), 0);
        ui->style_set(picture, GM_PLUGIN_LVGL_STYLE_RADIUS, gm_plugin_lvgl_style_number(0), 0);
        for (unsigned p = GM_PLUGIN_LVGL_STYLE_PAD_TOP; p <= GM_PLUGIN_LVGL_STYLE_PAD_RIGHT; ++p)
            ui->style_set(picture, p, gm_plugin_lvgl_style_number(0), 0);
        for (unsigned i = 0; i < run_count; ++i) {
            const firmware_exit_run_t *run = &runs[i];
            gm_plugin_lvgl_obj_t *pixel = ui->obj_create(picture);
            if (!pixel) return 0;
            ui->obj_set_size(pixel, run->width, 1);
            ui->obj_set_pos(pixel, run->x, run->y);
            ui->style_set(pixel, GM_PLUGIN_LVGL_STYLE_BG_COLOR, gm_plugin_lvgl_style_color(run->green), 0);
            ui->style_set(pixel, GM_PLUGIN_LVGL_STYLE_BG_OPA, gm_plugin_lvgl_style_number(run->alpha), 0);
            ui->style_set(pixel, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, gm_plugin_lvgl_style_number(0), 0);
            ui->style_set(pixel, GM_PLUGIN_LVGL_STYLE_RADIUS, gm_plugin_lvgl_style_number(0), 0);
            ui->obj_clear_flag(pixel, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
        }
    }
    if (picture) ui->obj_align(picture, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    return picture;
}

static void firmware_exit_cancel(firmware_exit_t *exit,
                                 const gm_plugin_lvgl_api_t *ui)
{
    if (exit->committed) return;
    exit->active = false;
    exit->step = 0;
    if (exit->overlay)
        ui->obj_add_flag(exit->overlay, GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static gm_plugin_result_t firmware_exit_create(
    firmware_exit_t *exit, const gm_plugin_lvgl_api_t *ui,
    gm_plugin_lvgl_obj_t *parent, int16_t width, int16_t height,
    const char *hold_text)
{
    gm_plugin_lvgl_obj_t *label;
    unsigned i;
    if (!ui->style_set ||
        !ui->obj_create || !ui->label_create || !ui->label_set_text ||
        !ui->obj_set_size || !ui->obj_set_pos || !ui->obj_align ||
        !ui->obj_add_flag || !ui->obj_clear_flag)
        return GM_PLUGIN_ENOTSUP;
    exit->active = false;
    exit->committed = false;
    exit->overlay = ui->obj_create(parent);
    if (!exit->overlay) return GM_PLUGIN_ENOMEM;
    ui->obj_set_size(exit->overlay, width, height);
    ui->obj_align(exit->overlay, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    ui->obj_clear_flag(exit->overlay, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    ui->style_set(exit->overlay, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                  gm_plugin_lvgl_style_color(0), 0);
    ui->style_set(exit->overlay, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                  gm_plugin_lvgl_style_number(255), 0);
    ui->style_set(exit->overlay, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                  gm_plugin_lvgl_style_number(0), 0);
    ui->style_set(exit->overlay, GM_PLUGIN_LVGL_STYLE_RADIUS,
                  gm_plugin_lvgl_style_number(0), 0);
    for (i = GM_PLUGIN_LVGL_STYLE_PAD_TOP;
         i <= GM_PLUGIN_LVGL_STYLE_PAD_RIGHT; ++i)
        ui->style_set(exit->overlay, i, gm_plugin_lvgl_style_number(0), 0);
    exit->ring = firmware_exit_picture(ui, exit->overlay, 48,
        firmware_exit_half_pixels, sizeof(firmware_exit_half_pixels),
        firmware_exit_half, sizeof(firmware_exit_half) / sizeof(firmware_exit_half[0]));
    if (!exit->ring) return GM_PLUGIN_ENOMEM;
    exit->ring_full = firmware_exit_picture(ui, exit->overlay, 48,
        firmware_exit_full_pixels, sizeof(firmware_exit_full_pixels),
        firmware_exit_full, sizeof(firmware_exit_full) / sizeof(firmware_exit_full[0]));
    label = ui->label_create(exit->overlay);
    if (!exit->ring_full || !label) return GM_PLUGIN_ENOMEM;
    ui->label_set_text(label, hold_text);
    ui->style_set(label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                  gm_plugin_lvgl_style_color(0xF0), 0);
    ui->obj_align(label, GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID, 0, -16);
    if (!firmware_exit_picture(ui, exit->overlay, 18,
        firmware_exit_icon_pixels, sizeof(firmware_exit_icon_pixels),
        firmware_exit_icon, sizeof(firmware_exit_icon) / sizeof(firmware_exit_icon[0])))
        return GM_PLUGIN_ENOMEM;
    firmware_exit_cancel(exit, ui);
    return GM_PLUGIN_OK;
}

static void firmware_exit_begin(firmware_exit_t *exit,
                                const gm_plugin_lvgl_api_t *ui, uint32_t now)
{
    if (exit->active || exit->committed) return;
    exit->active = true;
    exit->started_ms = now;
    exit->step = 0;
    ui->obj_add_flag(exit->ring, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    ui->obj_add_flag(exit->ring_full, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    ui->obj_clear_flag(exit->overlay, GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

/* Return true once to request asynchronous Host exit. Keep the opaque overlay
 * visible until on_stop cleans the UI; release must not uncover the game. */
static bool firmware_exit_complete(firmware_exit_t *exit,
                                   const gm_plugin_lvgl_api_t *ui, uint32_t now)
{
    uint32_t elapsed;
    uint8_t step;
    if (!exit->active || exit->committed) return false;
    elapsed = now - exit->started_ms;
    if (elapsed > 2000U) {
        exit->active = false;
        exit->committed = true;
        exit->step = 2;
        ui->obj_add_flag(exit->ring, GM_PLUGIN_LVGL_FLAG_HIDDEN);
        ui->obj_clear_flag(exit->ring_full, GM_PLUGIN_LVGL_FLAG_HIDDEN);
        return true;
    }
    step = elapsed > 800U ? (uint8_t)(elapsed / 800U) : 0;
    if (step != exit->step) {
        exit->step = step;
        ui->obj_add_flag(step == 1U ? exit->ring_full : exit->ring,
                          GM_PLUGIN_LVGL_FLAG_HIDDEN);
        ui->obj_clear_flag(step == 1U ? exit->ring : exit->ring_full,
                            GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
    return false;
}
#endif
