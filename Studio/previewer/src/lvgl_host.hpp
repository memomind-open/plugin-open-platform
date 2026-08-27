#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

extern "C" {
#include "lvgl.h"
}

namespace gmpreview {

class LvglHost {
public:
    static constexpr int kWidth = 600;
    static constexpr int kHeight = 350;
    static constexpr int kStride = 320;

    LvglHost(std::vector<uint8_t> &framebuffer, uint32_t root_handle,
             uint32_t first_object_handle, uint32_t default_font_handle,
             uint32_t large_font_handle);
    ~LvglHost();

    void setFontData(const std::vector<uint8_t> *default_font,
                     const std::vector<uint8_t> *large_font);
    void start();
    void stop();
    void tick(uint32_t elapsed_ms);
    void refresh();

    uint32_t rootHandle() const;
    uint32_t createObject(uint32_t parent, uint8_t type);
    void deleteObject(uint32_t handle);
    void cleanObject(uint32_t handle);
    void setPosition(uint32_t handle, int32_t x, int32_t y);
    void setSize(uint32_t handle, int32_t width, int32_t height);
    void align(uint32_t handle, uint8_t alignment, int32_t x, int32_t y);
    void addFlag(uint32_t handle, uint32_t flag);
    void clearFlag(uint32_t handle, uint32_t flag);
    void invalidate(uint32_t handle);
    int32_t width(uint32_t handle);
    int32_t height(uint32_t handle);
    void setStyle(uint32_t handle, uint32_t property, uint32_t value,
                  uint32_t selector);
    void setLabelText(uint32_t handle, const char *text);
    void setLabelLongMode(uint32_t handle, uint8_t mode);
    void setArcRange(uint32_t handle, int16_t minimum, int16_t maximum);
    void setArcValue(uint32_t handle, int16_t value);
    void setLinePoints(uint32_t handle, const std::vector<int32_t> &xy);
    int32_t fontLineHeight(uint32_t font_handle) const;
    void textSize(int32_t &width, int32_t &height, const char *text,
                  uint32_t font_handle, int32_t letter_space,
                  int32_t line_space, int32_t max_width) const;
    uint32_t textNextLine(const char *text, uint32_t font_handle,
                          int32_t letter_space, int32_t max_width,
                          int32_t &used_width) const;
    void setSelectionStart(uint32_t handle, uint32_t index);
    void setSelectionEnd(uint32_t handle, uint32_t index);
    size_t objectCount() const;

private:
    struct Point { int32_t x; int32_t y; };

    std::vector<uint8_t> &framebuffer_;
    uint32_t root_handle_;
    uint32_t next_handle_;
    uint32_t default_font_handle_;
    uint32_t large_font_handle_;
    lv_disp_t *display_ = nullptr;
    lv_obj_t *root_ = nullptr;
    lv_disp_draw_buf_t *draw_buffer_descriptor_ = nullptr;
    lv_disp_drv_t *display_driver_ = nullptr;
    lv_color_t *draw_pixels_ = nullptr;
    std::unordered_map<uint32_t, lv_obj_t *> objects_;
    std::unordered_map<lv_obj_t *, uint32_t> handles_;
    std::unordered_map<uint32_t, std::vector<Point>> line_points_;

    lv_obj_t *object(uint32_t handle) const;
    uint32_t registerObject(lv_obj_t *object);
    void forgetTree(lv_obj_t *object);
    const lv_font_t *font(uint32_t handle) const;
    static void flush(lv_disp_drv_t *driver, const lv_area_t *area,
                      lv_color_t *color_pixels);
};

} // namespace gmpreview
