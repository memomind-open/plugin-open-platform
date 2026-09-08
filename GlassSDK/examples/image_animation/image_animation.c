#include "gm_plugin_lvgl_api.h"

#include "../game/fighter_arena/zen_combat_sprites.h"

#define IMAGE_WIDTH ((PF_SPRITE_WIDTH * 2U + 1U) / 3U)
#define IMAGE_HEIGHT ((PF_SPRITE_HEIGHT * 2U + 1U) / 3U)
#define IMAGE_ROW_BYTES ((IMAGE_WIDTH + 1U) / 2U)
#define IMAGE_DATA_SIZE (64U + IMAGE_ROW_BYTES * IMAGE_HEIGHT)
#define FRAME_COUNT 4U
#define PANEL_WIDTH 276
#define PANEL_HEIGHT 230

static const uint8_t s_fighter_frame_indexes[FRAME_COUNT] = {0U, 5U, 8U, 1U};
static const gm_plugin_lvgl_api_t *s_ui;
static gm_plugin_lvgl_obj_t *s_static_image;
static gm_plugin_lvgl_obj_t *s_animation;
static uint8_t s_frame_data[FRAME_COUNT][IMAGE_DATA_SIZE];
static gm_plugin_lvgl_image_dsc_t s_frame_descriptors[FRAME_COUNT];

static void set_pixel(uint8_t *data, uint16_t x, uint16_t y, uint8_t index)
{
    uint8_t *packed = &data[64U + (uint32_t)y * IMAGE_ROW_BYTES + x / 2U];
    if ((x & 1U) == 0U)
        *packed = (uint8_t)((*packed & 0x0fU) | (uint8_t)(index << 4));
    else
        *packed = (uint8_t)((*packed & 0xf0U) | (index & 0x0fU));
}

static void build_palette(uint8_t *data)
{
    uint8_t shade;
    uint32_t index;
    for (index = 0U; index < IMAGE_DATA_SIZE; ++index) data[index] = 0U;
    for (shade = 1U; shade < 16U; ++shade) {
        const uint8_t brightness = (uint8_t)(shade * 17U);
        data[(uint32_t)shade * 4U] = brightness;
        data[(uint32_t)shade * 4U + 1U] = brightness;
        data[(uint32_t)shade * 4U + 2U] = brightness;
        data[(uint32_t)shade * 4U + 3U] = 255U;
    }
}

static uint8_t read_fighter_pixel(uint32_t row_offset, uint16_t source_x)
{
    const uint8_t start = pf_sprite_data[row_offset];
    const uint8_t length = pf_sprite_data[row_offset + 1U];
    uint16_t pixel_index;
    uint8_t packed;
    if (start == 0xffU || source_x < start ||
        source_x >= (uint16_t)(start + length))
        return 0U;
    pixel_index = (uint16_t)(source_x - start);
    packed = pf_sprite_data[row_offset + 2U + pixel_index / 2U];
    return (pixel_index & 1U) == 0U
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fU);
}

static void decode_fighter_frame(uint8_t *destination, uint8_t frame)
{
    uint32_t offset = pf_sprite_offsets[frame];
    uint32_t row_offsets[PF_SPRITE_HEIGHT];
    uint16_t source_y;
    uint16_t destination_y;
    build_palette(destination);

    for (source_y = 0U; source_y < PF_SPRITE_HEIGHT; ++source_y) {
        const uint8_t length = pf_sprite_data[offset + 1U];
        row_offsets[source_y] = offset;
        offset += 2U + (uint32_t)(length + 1U) / 2U;
    }

    for (destination_y = 0U; destination_y < IMAGE_HEIGHT;
         ++destination_y) {
        uint16_t destination_x;
        source_y = (uint16_t)(((uint32_t)(destination_y * 2U + 1U) *
                               PF_SPRITE_HEIGHT) /
                              (IMAGE_HEIGHT * 2U));
        for (destination_x = 0U; destination_x < IMAGE_WIDTH;
             ++destination_x) {
            const uint16_t source_x = (uint16_t)(
                ((uint32_t)(destination_x * 2U + 1U) * PF_SPRITE_WIDTH) /
                (IMAGE_WIDTH * 2U));
            const uint8_t shade =
                read_fighter_pixel(row_offsets[source_y], source_x);
            if (shade != 0U)
                set_pixel(destination, destination_x, destination_y, shade);
        }
    }
}

static gm_plugin_lvgl_obj_t *create_label(gm_plugin_lvgl_obj_t *root,
                                           const char *text,
                                           gm_plugin_lvgl_align_t alignment,
                                           int16_t x, int16_t y)
{
    gm_plugin_lvgl_obj_t *label = s_ui->label_create(root);
    if (label == 0) return 0;
    s_ui->label_set_text(label, text);
    s_ui->obj_align(label, alignment, x, y);
    return label;
}

static gm_plugin_lvgl_obj_t *create_bar(gm_plugin_lvgl_obj_t *parent,
                                         int16_t x, int16_t y,
                                         int16_t width, uint8_t shade)
{
    gm_plugin_lvgl_obj_t *bar = s_ui->obj_create(parent);
    if (bar == 0) return 0;
    s_ui->obj_set_pos(bar, x, y);
    s_ui->obj_set_size(bar, width, 7);
    s_ui->style_set(bar, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                    gm_plugin_lvgl_style_color(shade), 0);
    s_ui->style_set(bar, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                    gm_plugin_lvgl_style_number(255), 0);
    s_ui->style_set(bar, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                    gm_plugin_lvgl_style_number(0), 0);
    s_ui->style_set(bar, GM_PLUGIN_LVGL_STYLE_RADIUS,
                    gm_plugin_lvgl_style_number(4), 0);
    return bar;
}

static gm_plugin_lvgl_obj_t *create_background_panel(
    gm_plugin_lvgl_obj_t *root, int16_t x, const char *status)
{
    gm_plugin_lvgl_obj_t *panel = s_ui->obj_create(root);
    gm_plugin_lvgl_obj_t *layer_label;
    gm_plugin_lvgl_obj_t *status_label;
    gm_plugin_lvgl_obj_t *bar_back;
    gm_plugin_lvgl_obj_t *bar_value;
    if (panel == 0) return 0;

    s_ui->obj_set_pos(panel, x, 100);
    s_ui->obj_set_size(panel, PANEL_WIDTH, PANEL_HEIGHT);
    s_ui->style_set(panel, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                    gm_plugin_lvgl_style_number(0), 0);
    s_ui->style_set(panel, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR,
                    gm_plugin_lvgl_style_color(0x92U), 0);
    s_ui->style_set(panel, GM_PLUGIN_LVGL_STYLE_BORDER_OPA,
                    gm_plugin_lvgl_style_number(255), 0);
    s_ui->style_set(panel, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                    gm_plugin_lvgl_style_number(2), 0);
    s_ui->style_set(panel, GM_PLUGIN_LVGL_STYLE_RADIUS,
                    gm_plugin_lvgl_style_number(12), 0);

    layer_label = create_label(panel, "LOWER UI LAYER",
                               GM_PLUGIN_LVGL_ALIGN_CENTER, 0, -8);
    status_label = create_label(panel, status,
                                GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID, 0, -12);
    bar_back = create_bar(panel, 25, 36, 226, 0x49U);
    bar_value = create_bar(panel, 25, 36, 158, 0xb6U);
    if (layer_label == 0 || status_label == 0 || bar_back == 0 ||
        bar_value == 0)
        return 0;

    s_ui->style_set(layer_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                    gm_plugin_lvgl_style_color(0x92U), 0);
    s_ui->style_set(status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                    gm_plugin_lvgl_style_color(0xb6U), 0);
    return panel;
}

static gm_plugin_result_t plugin_start(void *context)
{
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *title;
    gm_plugin_lvgl_obj_t *memory_warning;
    gm_plugin_lvgl_obj_t *static_label;
    gm_plugin_lvgl_obj_t *animation_label;
    gm_plugin_lvgl_obj_t *static_background;
    gm_plugin_lvgl_obj_t *animation_background;
    const gm_plugin_lvgl_image_dsc_t *frames[FRAME_COUNT];
    uint8_t index;
    (void)context;

    root = s_ui->root_get();
    if (root == 0) return GM_PLUGIN_ESTATE;
    s_ui->obj_clean(root);
    for (index = 0U; index < FRAME_COUNT; ++index) {
        decode_fighter_frame(s_frame_data[index],
                             s_fighter_frame_indexes[index]);
        s_frame_descriptors[index].struct_size =
            (uint16_t)sizeof(gm_plugin_lvgl_image_dsc_t);
        s_frame_descriptors[index].width = IMAGE_WIDTH;
        s_frame_descriptors[index].height = IMAGE_HEIGHT;
        s_frame_descriptors[index].format = GM_PLUGIN_LVGL_IMAGE_INDEXED_4BIT;
        s_frame_descriptors[index].reserved = 0U;
        s_frame_descriptors[index].data = s_frame_data[index];
        s_frame_descriptors[index].data_size = IMAGE_DATA_SIZE;
        frames[index] = &s_frame_descriptors[index];
    }

    title = create_label(root, "IMAGE AND ANIMATION API",
                         GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 18);
    memory_warning = create_label(root, "ANIMATE SPARINGLY - SAVE MEMORY",
                                  GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 43);
    static_label = create_label(root, "STATIC IMAGE",
                                GM_PLUGIN_LVGL_ALIGN_TOP_LEFT, 92, 72);
    animation_label = create_label(root, "FRAME ANIMATION",
                                   GM_PLUGIN_LVGL_ALIGN_TOP_RIGHT, -62, 72);

    /* Earlier siblings form the background. The image objects are created
     * last so their opaque fighter pixels cover these fixed UI components,
     * while transparent palette index 0 reveals the components underneath. */
    static_background = create_background_panel(root, 12, "STATIC / READY");
    animation_background = create_background_panel(
        root, 312, "ANIMATION / RUNNING");
    s_static_image = s_ui->image_create(root, frames[1]);
    s_animation = s_ui->anim_image_create(
        root, frames, FRAME_COUNT, 400U, 0U);
    if (title == 0 || memory_warning == 0 || static_label == 0 ||
        animation_label == 0 ||
        static_background == 0 || animation_background == 0 ||
        s_static_image == 0 || s_animation == 0)
        goto no_memory;

    s_ui->obj_align(s_static_image, GM_PLUGIN_LVGL_ALIGN_CENTER, -150, 35);
    s_ui->obj_align(s_animation, GM_PLUGIN_LVGL_ALIGN_CENTER, 150, 35);
    if (s_ui->image_set_source(s_static_image, frames[0]) != GM_PLUGIN_OK ||
        s_ui->anim_image_set_sources(s_animation, frames, FRAME_COUNT) !=
            GM_PLUGIN_OK ||
        s_ui->anim_image_set_frame_duration(s_animation, 320U) !=
            GM_PLUGIN_OK ||
        s_ui->anim_image_set_repeat_count(
            s_animation, GM_PLUGIN_LVGL_ANIM_REPEAT_INFINITE) != GM_PLUGIN_OK ||
        s_ui->anim_image_start(s_animation) != GM_PLUGIN_OK)
        goto no_memory;
    return GM_PLUGIN_OK;

no_memory:
    s_static_image = 0;
    s_animation = 0;
    s_ui->obj_clean(root);
    return GM_PLUGIN_ENOMEM;
}

static void plugin_stop(void *context)
{
    gm_plugin_lvgl_obj_t *root;
    (void)context;
    if (s_animation != 0) (void)s_ui->anim_image_stop(s_animation);
    root = s_ui->root_get();
    if (root != 0) s_ui->obj_clean(root);
    s_static_image = 0;
    s_animation = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 || host->graphics.lvgl == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_EVERSION;
    s_ui = host->graphics.lvgl;
    if (!GM_PLUGIN_VERSION_COMPATIBLE(s_ui->api_version,
                                      GM_PLUGIN_VERSION(1U, 1U)) ||
        s_ui->struct_size < GM_PLUGIN_LVGL_API_1_1_SIZE ||
        s_ui->image_create == 0 || s_ui->image_set_source == 0 ||
        s_ui->anim_image_create == 0 || s_ui->anim_image_set_sources == 0 ||
        s_ui->anim_image_set_frame_duration == 0 ||
        s_ui->anim_image_set_repeat_count == 0 ||
        s_ui->anim_image_start == 0 || s_ui->anim_image_stop == 0)
        return GM_PLUGIN_EVERSION;

    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = plugin_start;
    plugin->on_stop = plugin_stop;
    return GM_PLUGIN_OK;
}
