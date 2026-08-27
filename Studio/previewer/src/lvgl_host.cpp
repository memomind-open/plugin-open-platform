#include "lvgl_host.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

const std::vector<uint8_t> *g_default_font_data = nullptr;
const std::vector<uint8_t> *g_large_font_data = nullptr;

uint32_t read32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           static_cast<uint32_t>(data[1]) << 8 |
           static_cast<uint32_t>(data[2]) << 16 |
           static_cast<uint32_t>(data[3]) << 24;
}

const std::vector<uint8_t> *fontData(const lv_font_t *font)
{
    return font == &lv_font_xgimi_20 ? g_large_font_data : g_default_font_data;
}

bool findGlyph(const lv_font_t *font, uint32_t unicode, size_t &descriptor,
               size_t &bitmap)
{
    const auto *bytes = fontData(font);
    if (!bytes || bytes->size() < 16) return false;
    const uint32_t count = read32(bytes->data() + 12);
    const uint64_t descriptor_table = 16ull + static_cast<uint64_t>(count) * 4ull;
    const uint64_t bitmap_table = descriptor_table + static_cast<uint64_t>(count) * 9ull;
    if (!count || bitmap_table > bytes->size()) return false;

    uint32_t low = 0;
    uint32_t high = count;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2;
        const uint32_t value = read32(bytes->data() + 16u + middle * 4u);
        if (value < unicode) low = middle + 1;
        else high = middle;
    }
    if (low >= count || read32(bytes->data() + 16u + low * 4u) != unicode)
        return false;
    descriptor = static_cast<size_t>(descriptor_table + static_cast<uint64_t>(low) * 9ull);
    bitmap = static_cast<size_t>(bitmap_table + read32(bytes->data() + descriptor));
    return descriptor + 9 <= bytes->size() && bitmap < bytes->size();
}

bool getGlyphDescriptor(const lv_font_t *font, lv_font_glyph_dsc_t *output,
                        uint32_t unicode, uint32_t)
{
    if (!output) return false;
    size_t descriptor = 0, bitmap = 0;
    if (!findGlyph(font, unicode, descriptor, bitmap)) return false;
    const auto &bytes = *fontData(font);
    output->adv_w = bytes[descriptor + 4];
    output->box_h = bytes[descriptor + 5];
    output->box_w = bytes[descriptor + 6];
    output->ofs_x = static_cast<int8_t>(bytes[descriptor + 7]);
    output->ofs_y = static_cast<int8_t>(bytes[descriptor + 8]);
    output->bpp = bytes[8];
    output->is_placeholder = 0;
    return true;
}

const uint8_t *getGlyphBitmap(const lv_font_t *font, uint32_t unicode)
{
    size_t descriptor = 0, bitmap = 0;
    if (!findGlyph(font, unicode, descriptor, bitmap)) return nullptr;
    return fontData(font)->data() + bitmap;
}

lv_font_t makeFont(lv_coord_t line_height, lv_coord_t baseline)
{
    lv_font_t font{};
    font.get_glyph_dsc = getGlyphDescriptor;
    font.get_glyph_bitmap = getGlyphBitmap;
    font.line_height = line_height;
    font.base_line = baseline;
    font.subpx = LV_FONT_SUBPX_NONE;
    return font;
}

bool isColorProperty(uint32_t property)
{
    switch (property) {
    case LV_STYLE_BG_COLOR:
    case LV_STYLE_BG_GRAD_COLOR:
    case LV_STYLE_BG_IMG_RECOLOR:
    case LV_STYLE_BORDER_COLOR:
    case LV_STYLE_OUTLINE_COLOR:
    case LV_STYLE_SHADOW_COLOR:
    case LV_STYLE_IMG_RECOLOR:
    case LV_STYLE_LINE_COLOR:
    case LV_STYLE_ARC_COLOR:
    case LV_STYLE_TEXT_COLOR:
        return true;
    default:
        return false;
    }
}

} // namespace

extern "C" const lv_font_t lv_font_xgimi_17 = makeFont(34, 7);
extern "C" const lv_font_t lv_font_xgimi_20 = makeFont(41, 9);

namespace gmpreview {

LvglHost::LvglHost(std::vector<uint8_t> &framebuffer, uint32_t root_handle,
                   uint32_t first_object_handle, uint32_t default_font_handle,
                   uint32_t large_font_handle)
    : framebuffer_(framebuffer), root_handle_(root_handle),
      next_handle_(first_object_handle), default_font_handle_(default_font_handle),
      large_font_handle_(large_font_handle)
{
}

LvglHost::~LvglHost()
{
    stop();
}

void LvglHost::setFontData(const std::vector<uint8_t> *default_font,
                           const std::vector<uint8_t> *large_font)
{
    g_default_font_data = default_font;
    g_large_font_data = large_font;
}

void LvglHost::start()
{
    if (display_) return;
    if (!g_default_font_data || !g_large_font_data)
        throw std::runtime_error("the same-source XGIMI LVGL fonts are not loaded");

    lv_init();
    try {
        draw_pixels_ = new lv_color_t[static_cast<size_t>(kWidth) * 100u]{};
        draw_buffer_descriptor_ = new lv_disp_draw_buf_t{};
        display_driver_ = new lv_disp_drv_t{};
        lv_disp_draw_buf_init(draw_buffer_descriptor_, draw_pixels_, nullptr,
                              static_cast<uint32_t>(kWidth * 100));
        lv_disp_drv_init(display_driver_);
        display_driver_->hor_res = kWidth;
        display_driver_->ver_res = kHeight;
        display_driver_->draw_buf = draw_buffer_descriptor_;
        display_driver_->flush_cb = flush;
        display_driver_->user_data = this;
        display_ = lv_disp_drv_register(display_driver_);
        if (!display_) throw std::runtime_error("LVGL display registration failed");

        lv_obj_t *screen = lv_scr_act();
        lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
        root_ = lv_obj_create(screen);
        lv_obj_set_size(root_, kWidth, kHeight);
        lv_obj_set_style_radius(root_, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(root_, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);
        lv_obj_set_style_text_font(root_, &lv_font_xgimi_17, LV_PART_MAIN);
        lv_color_t default_color{};
        default_color.full = 0xf0;
        lv_obj_set_style_text_color(root_, default_color, LV_PART_MAIN);
        lv_obj_center(root_);
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        objects_[root_handle_] = root_;
        handles_[root_] = root_handle_;
        lv_refr_now(display_);
    } catch (...) {
        stop();
        throw;
    }
}

void LvglHost::stop()
{
    objects_.clear();
    handles_.clear();
    line_points_.clear();
    root_ = nullptr;
    if (display_) {
        lv_disp_remove(display_);
        display_ = nullptr;
    }
    delete display_driver_;
    display_driver_ = nullptr;
    delete draw_buffer_descriptor_;
    draw_buffer_descriptor_ = nullptr;
    delete[] draw_pixels_;
    draw_pixels_ = nullptr;
    next_handle_ = root_handle_ + 4;
}

void LvglHost::tick(uint32_t elapsed_ms)
{
    if (!display_) return;
    lv_tick_inc(elapsed_ms);
    (void)lv_timer_handler();
}

void LvglHost::refresh()
{
    if (display_) lv_refr_now(display_);
}

uint32_t LvglHost::rootHandle() const
{
    return root_ ? root_handle_ : 0;
}

lv_obj_t *LvglHost::object(uint32_t handle) const
{
    auto found = objects_.find(handle);
    return found == objects_.end() ? nullptr : found->second;
}

uint32_t LvglHost::registerObject(lv_obj_t *object_value)
{
    if (!object_value) return 0;
    const uint32_t handle = next_handle_;
    next_handle_ += 4;
    objects_[handle] = object_value;
    handles_[object_value] = handle;
    return handle;
}

uint32_t LvglHost::createObject(uint32_t parent, uint8_t type)
{
    lv_obj_t *parent_object = object(parent);
    if (!parent_object) return 0;
    lv_obj_t *created = nullptr;
    switch (type) {
    case 1: created = lv_label_create(parent_object); break;
    case 2: created = lv_arc_create(parent_object); break;
    case 3: created = lv_line_create(parent_object); break;
    default: created = lv_obj_create(parent_object); break;
    }
    return registerObject(created);
}

void LvglHost::forgetTree(lv_obj_t *object_value)
{
    if (!object_value) return;
    const uint32_t child_count = lv_obj_get_child_cnt(object_value);
    for (uint32_t i = 0; i < child_count; ++i)
        forgetTree(lv_obj_get_child(object_value, i));
    auto found = handles_.find(object_value);
    if (found != handles_.end()) {
        line_points_.erase(found->second);
        objects_.erase(found->second);
        handles_.erase(found);
    }
}

void LvglHost::deleteObject(uint32_t handle)
{
    if (handle == root_handle_) return;
    lv_obj_t *value = object(handle);
    if (!value) return;
    forgetTree(value);
    lv_obj_del(value);
}

void LvglHost::cleanObject(uint32_t handle)
{
    lv_obj_t *value = object(handle);
    if (!value) return;
    const uint32_t child_count = lv_obj_get_child_cnt(value);
    for (uint32_t i = 0; i < child_count; ++i)
        forgetTree(lv_obj_get_child(value, i));
    lv_obj_clean(value);
}

void LvglHost::setPosition(uint32_t handle, int32_t x, int32_t y)
{
    if (auto *value = object(handle)) lv_obj_set_pos(value, x, y);
}

void LvglHost::setSize(uint32_t handle, int32_t width_value, int32_t height_value)
{
    if (auto *value = object(handle)) lv_obj_set_size(value, width_value, height_value);
}

void LvglHost::align(uint32_t handle, uint8_t alignment, int32_t x, int32_t y)
{
    if (auto *value = object(handle))
        lv_obj_align(value, static_cast<lv_align_t>(alignment), x, y);
}

void LvglHost::addFlag(uint32_t handle, uint32_t flag)
{
    if (auto *value = object(handle)) lv_obj_add_flag(value, flag);
}

void LvglHost::clearFlag(uint32_t handle, uint32_t flag)
{
    if (auto *value = object(handle)) lv_obj_clear_flag(value, flag);
}

void LvglHost::invalidate(uint32_t handle)
{
    if (auto *value = object(handle)) lv_obj_invalidate(value);
}

int32_t LvglHost::width(uint32_t handle)
{
    if (auto *value = object(handle)) {
        lv_obj_update_layout(value);
        return lv_obj_get_width(value);
    }
    return 0;
}

int32_t LvglHost::height(uint32_t handle)
{
    if (auto *value = object(handle)) {
        lv_obj_update_layout(value);
        return lv_obj_get_height(value);
    }
    return 0;
}

const lv_font_t *LvglHost::font(uint32_t handle) const
{
    if (handle == large_font_handle_) return &lv_font_xgimi_20;
    (void)default_font_handle_;
    return &lv_font_xgimi_17;
}

void LvglHost::setStyle(uint32_t handle, uint32_t property, uint32_t raw,
                        uint32_t selector)
{
    lv_obj_t *value = object(handle);
    if (!value) return;
    lv_style_value_t style_value{};
    if (property == LV_STYLE_TEXT_FONT) style_value.ptr = font(raw);
    else if (isColorProperty(property)) style_value.color.full = static_cast<uint8_t>(raw);
    else style_value.num = static_cast<int32_t>(raw);
    lv_obj_set_local_style_prop(value, static_cast<lv_style_prop_t>(property),
                                style_value, selector);
}

void LvglHost::setLabelText(uint32_t handle, const char *text)
{
    if (auto *value = object(handle)) lv_label_set_text(value, text ? text : "");
}

void LvglHost::setLabelLongMode(uint32_t handle, uint8_t mode)
{
    if (auto *value = object(handle))
        lv_label_set_long_mode(value, static_cast<lv_label_long_mode_t>(mode));
}

void LvglHost::setArcRange(uint32_t handle, int16_t minimum, int16_t maximum)
{
    if (auto *value = object(handle)) lv_arc_set_range(value, minimum, maximum);
}

void LvglHost::setArcValue(uint32_t handle, int16_t value_number)
{
    if (auto *value = object(handle)) lv_arc_set_value(value, value_number);
}

void LvglHost::setLinePoints(uint32_t handle, const std::vector<int32_t> &xy)
{
    lv_obj_t *value = object(handle);
    if (!value) return;
    auto &points = line_points_[handle];
    points.resize(xy.size() / 2);
    for (size_t i = 0; i < points.size(); ++i) {
        points[i].x = static_cast<int16_t>(xy[i * 2]);
        points[i].y = static_cast<int16_t>(xy[i * 2 + 1]);
    }
    static_assert(sizeof(Point) == sizeof(lv_point_t), "LVGL point ABI changed");
    lv_line_set_points(value, reinterpret_cast<const lv_point_t *>(points.data()),
                       static_cast<uint16_t>(points.size()));
}

int32_t LvglHost::fontLineHeight(uint32_t font_handle) const
{
    return lv_font_get_line_height(font(font_handle));
}

void LvglHost::textSize(int32_t &width_value, int32_t &height_value,
                        const char *text, uint32_t font_handle,
                        int32_t letter_space, int32_t line_space,
                        int32_t max_width) const
{
    lv_point_t size{};
    lv_txt_get_size(&size, text ? text : "", font(font_handle), letter_space,
                    line_space, max_width, LV_TEXT_FLAG_NONE);
    width_value = size.x;
    height_value = size.y;
}

uint32_t LvglHost::textNextLine(const char *text, uint32_t font_handle,
                                int32_t letter_space, int32_t max_width,
                                int32_t &used_width) const
{
    lv_coord_t used = 0;
    const uint32_t next = _lv_txt_get_next_line(text ? text : "", font(font_handle),
                                                letter_space, max_width, &used,
                                                LV_TEXT_FLAG_NONE);
    used_width = used;
    return next;
}

void LvglHost::setSelectionStart(uint32_t handle, uint32_t index)
{
    if (auto *value = object(handle)) lv_label_set_text_sel_start(value, index);
}

void LvglHost::setSelectionEnd(uint32_t handle, uint32_t index)
{
    if (auto *value = object(handle)) lv_label_set_text_sel_end(value, index);
}

size_t LvglHost::objectCount() const
{
    return objects_.size() - (root_ ? 1u : 0u);
}

void LvglHost::flush(lv_disp_drv_t *driver, const lv_area_t *area,
                     lv_color_t *color_pixels)
{
    auto *host = static_cast<LvglHost *>(driver->user_data);
    if (!host || !area || !color_pixels) {
        lv_disp_flush_ready(driver);
        return;
    }
    size_t source = 0;
    for (int32_t y = area->y1; y <= area->y2; ++y) {
        for (int32_t x = area->x1; x <= area->x2; ++x, ++source) {
            if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) continue;
            const uint8_t gray = color_pixels[source].ch.green & 0x0f;
            uint8_t &packed = host->framebuffer_[static_cast<size_t>(y) * kStride +
                                                 static_cast<size_t>(x / 2)];
            if ((x & 1) == 0) packed = static_cast<uint8_t>((packed & 0x0f) | (gray << 4));
            else packed = static_cast<uint8_t>((packed & 0xf0) | gray);
        }
    }
    lv_disp_flush_ready(driver);
}

} // namespace gmpreview
