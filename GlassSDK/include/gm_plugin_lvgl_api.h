#ifndef GM_PLUGIN_LVGL_API_H
#define GM_PLUGIN_LVGL_API_H

#include "gm_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RELEASE-CANDIDATE CORE DRAWING ABI: after the first public release, version
 * baselines, value encodings and each released table prefix are immutable. Add a new
 * minor version by appending fields only. A breaking change needs a new
 * separately named table while firmware continues serving the old table. */
#define GM_PLUGIN_LVGL_API_MIN_VERSION GM_PLUGIN_VERSION(1U, 0U)
#define GM_PLUGIN_LVGL_API_VERSION GM_PLUGIN_VERSION(1U, 1U)

/* Match the native LVGL types used by this platform so compatible entries can
 * point straight at firmware functions without plugin-side LVGL code. */
typedef struct _lv_obj_t gm_plugin_lvgl_obj_t;
typedef struct _lv_font_t gm_plugin_lvgl_font_t;
typedef int32_t gm_plugin_lvgl_coord_t;
typedef uint32_t gm_plugin_lvgl_selector_t;
typedef uint32_t gm_plugin_lvgl_flag_t;

#ifdef LV_TXT_H
typedef lv_text_flag_t gm_plugin_lvgl_text_flag_t;
#else
typedef uint8_t gm_plugin_lvgl_text_flag_t;
#endif

#ifdef LV_AREA_H
typedef lv_point_t gm_plugin_lvgl_point_t;
#else
typedef struct {
    gm_plugin_lvgl_coord_t x;
    gm_plugin_lvgl_coord_t y;
} gm_plugin_lvgl_point_t;
#endif

/* ABI-FROZEN 1.1 image values and descriptor. The image payload is already in
 * the layout consumed by LVGL; it is not a PNG or a serialized lv_img_dsc_t. */
typedef uint8_t gm_plugin_lvgl_image_format_t;
enum {
    /* data begins with 16 BGRA8888 palette entries (64 bytes), followed by
     * tightly packed 4-bit indexes. Even x is in the high nibble and odd x is
     * in the low nibble. Palette alpha is composited by LVGL before G4 flush. */
    GM_PLUGIN_LVGL_IMAGE_INDEXED_4BIT = 1,
};

typedef struct {
    /** Size of this descriptor; initialize to sizeof(gm_plugin_lvgl_image_dsc_t). */
    uint16_t struct_size;
    /** Image width in pixels. */
    uint16_t width;
    /** Image height in pixels. */
    uint16_t height;
    /** One GM_PLUGIN_LVGL_IMAGE_* value. */
    gm_plugin_lvgl_image_format_t format;
    /** Must be zero. Reserved for a future compatible descriptor extension. */
    uint8_t reserved;
    /** Palette and packed indexes; must start at a 4-byte-aligned address.
     * The Host borrows this payload without copying. For packed frame arrays,
     * round each storage stride up to 4 bytes; data_size excludes that padding. */
    const uint8_t *data;
    /** Exact payload size in bytes. */
    uint32_t data_size;
} gm_plugin_lvgl_image_dsc_t;

#define GM_PLUGIN_LVGL_IMAGE_DSC_MIN_SIZE \
    ((uint16_t)GM_PLUGIN_MEMBER_END(gm_plugin_lvgl_image_dsc_t, data_size))
#define GM_PLUGIN_LVGL_ANIM_REPEAT_INFINITE UINT16_MAX
#define GM_PLUGIN_LVGL_ANIM_IMAGE_MAX_FRAMES UINT16_C(127)

/* This target uses LV_COLOR_DEPTH=8. In firmware builds use the exact native
 * type so function-table entries can point directly at LVGL. */
#if defined(LV_COLOR_DEPTH) && LV_COLOR_DEPTH == 8
typedef lv_color_t gm_plugin_lvgl_color_t;
#else
typedef union {
    uint8_t full;
} gm_plugin_lvgl_color_t;
#endif

/* ABI-FROZEN values. These are plugin ABI values, not values that may be
 * updated to follow a future LVGL release. If native LVGL values change, the
 * firmware Host must translate them while preserving these numbers. */
#define GM_PLUGIN_LVGL_FLAG_HIDDEN UINT32_C(1)
#define GM_PLUGIN_LVGL_FLAG_SCROLLABLE UINT32_C(16)

#ifdef LV_STYLE_H
typedef lv_style_prop_t gm_plugin_lvgl_style_prop_t;
typedef lv_style_value_t gm_plugin_lvgl_style_value_t;
#else
typedef uint32_t gm_plugin_lvgl_style_prop_t;
typedef union {
    int32_t num;
    const void *ptr;
    gm_plugin_lvgl_color_t color;
} gm_plugin_lvgl_style_value_t;
#endif

/* Typed style-value constructors. They compile to the same union assignment
 * as hand-written code and keep the union representation out of plugins. */
/**
 * Construct a numeric LVGL style value.
 * @param number Signed numeric value expected by the selected style property.
 * @return Value suitable for gm_plugin_lvgl_api_t::style_set.
 */
static inline gm_plugin_lvgl_style_value_t
gm_plugin_lvgl_style_number(int32_t number)
{
    gm_plugin_lvgl_style_value_t value = {0};
    value.num = number;
    return value;
}

/**
 * Construct an 8-bit native LVGL color style value.
 * @param color Packed color in the firmware's LV_COLOR_DEPTH=8 format.
 * @return Value suitable for a color property passed to style_set.
 */
static inline gm_plugin_lvgl_style_value_t
gm_plugin_lvgl_style_color(uint8_t color)
{
    gm_plugin_lvgl_style_value_t value = {0};
    value.color.full = color;
    return value;
}

#define GM_PLUGIN_LVGL_STYLE_WIDTH         1U
#define GM_PLUGIN_LVGL_STYLE_MIN_WIDTH     2U
#define GM_PLUGIN_LVGL_STYLE_MAX_WIDTH     3U
#define GM_PLUGIN_LVGL_STYLE_HEIGHT        4U
#define GM_PLUGIN_LVGL_STYLE_MIN_HEIGHT    5U
#define GM_PLUGIN_LVGL_STYLE_MAX_HEIGHT    6U
#define GM_PLUGIN_LVGL_STYLE_X             7U
#define GM_PLUGIN_LVGL_STYLE_Y             8U
#define GM_PLUGIN_LVGL_STYLE_ALIGN         9U
#define GM_PLUGIN_LVGL_STYLE_LAYOUT       10U
#define GM_PLUGIN_LVGL_STYLE_RADIUS       11U
#define GM_PLUGIN_LVGL_STYLE_PAD_TOP      16U
#define GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM   17U
#define GM_PLUGIN_LVGL_STYLE_PAD_LEFT     18U
#define GM_PLUGIN_LVGL_STYLE_PAD_RIGHT    19U
#define GM_PLUGIN_LVGL_STYLE_PAD_ROW      20U
#define GM_PLUGIN_LVGL_STYLE_PAD_COLUMN   21U
#define GM_PLUGIN_LVGL_STYLE_BASE_DIR     22U
#define GM_PLUGIN_LVGL_STYLE_CLIP_CORNER  23U
#define GM_PLUGIN_LVGL_STYLE_BG_COLOR     32U
#define GM_PLUGIN_LVGL_STYLE_BG_OPA       33U
#define GM_PLUGIN_LVGL_STYLE_BG_GRAD_COLOR 34U
#define GM_PLUGIN_LVGL_STYLE_BG_GRAD_DIR   35U
#define GM_PLUGIN_LVGL_STYLE_BG_MAIN_STOP  36U
#define GM_PLUGIN_LVGL_STYLE_BG_GRAD_STOP  37U
#define GM_PLUGIN_LVGL_STYLE_BG_DITHER_MODE 39U
#define GM_PLUGIN_LVGL_STYLE_BG_IMG_OPA       41U
#define GM_PLUGIN_LVGL_STYLE_BG_IMG_RECOLOR   42U
#define GM_PLUGIN_LVGL_STYLE_BG_IMG_RECOLOR_OPA 43U
#define GM_PLUGIN_LVGL_STYLE_BG_IMG_TILED     44U
#define GM_PLUGIN_LVGL_STYLE_BORDER_COLOR 48U
#define GM_PLUGIN_LVGL_STYLE_BORDER_OPA   49U
#define GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH 50U
#define GM_PLUGIN_LVGL_STYLE_BORDER_SIDE  51U
#define GM_PLUGIN_LVGL_STYLE_BORDER_POST  52U
#define GM_PLUGIN_LVGL_STYLE_OUTLINE_WIDTH 53U
#define GM_PLUGIN_LVGL_STYLE_OUTLINE_COLOR 54U
#define GM_PLUGIN_LVGL_STYLE_OUTLINE_OPA   55U
#define GM_PLUGIN_LVGL_STYLE_OUTLINE_PAD   56U
#define GM_PLUGIN_LVGL_STYLE_SHADOW_WIDTH  64U
#define GM_PLUGIN_LVGL_STYLE_SHADOW_OFS_X  65U
#define GM_PLUGIN_LVGL_STYLE_SHADOW_OFS_Y  66U
#define GM_PLUGIN_LVGL_STYLE_SHADOW_SPREAD 67U
#define GM_PLUGIN_LVGL_STYLE_SHADOW_COLOR  68U
#define GM_PLUGIN_LVGL_STYLE_SHADOW_OPA    69U
#define GM_PLUGIN_LVGL_STYLE_IMG_OPA        70U
#define GM_PLUGIN_LVGL_STYLE_IMG_RECOLOR    71U
#define GM_PLUGIN_LVGL_STYLE_IMG_RECOLOR_OPA 72U
#define GM_PLUGIN_LVGL_STYLE_LINE_WIDTH    73U
#define GM_PLUGIN_LVGL_STYLE_LINE_DASH_WIDTH 74U
#define GM_PLUGIN_LVGL_STYLE_LINE_DASH_GAP 75U
#define GM_PLUGIN_LVGL_STYLE_LINE_ROUNDED  76U
#define GM_PLUGIN_LVGL_STYLE_LINE_COLOR    77U
#define GM_PLUGIN_LVGL_STYLE_LINE_OPA      78U
#define GM_PLUGIN_LVGL_STYLE_ARC_WIDTH     80U
#define GM_PLUGIN_LVGL_STYLE_ARC_ROUNDED   81U
#define GM_PLUGIN_LVGL_STYLE_ARC_COLOR     82U
#define GM_PLUGIN_LVGL_STYLE_ARC_OPA       83U
#define GM_PLUGIN_LVGL_STYLE_TEXT_COLOR   85U
#define GM_PLUGIN_LVGL_STYLE_TEXT_OPA     86U
#define GM_PLUGIN_LVGL_STYLE_TEXT_FONT    87U
#define GM_PLUGIN_LVGL_STYLE_TEXT_LETTER_SPACE 88U
#define GM_PLUGIN_LVGL_STYLE_TEXT_LINE_SPACE   89U
#define GM_PLUGIN_LVGL_STYLE_TEXT_DECOR        90U
#define GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN        91U
#define GM_PLUGIN_LVGL_STYLE_OPA               96U
#define GM_PLUGIN_LVGL_STYLE_OPA_LAYERED       97U
#define GM_PLUGIN_LVGL_STYLE_COLOR_FILTER_OPA  99U
#define GM_PLUGIN_LVGL_STYLE_ANIM_TIME        101U
#define GM_PLUGIN_LVGL_STYLE_ANIM_SPEED       102U
#define GM_PLUGIN_LVGL_STYLE_BLEND_MODE       104U
#define GM_PLUGIN_LVGL_STYLE_TRANSFORM_WIDTH  105U
#define GM_PLUGIN_LVGL_STYLE_TRANSFORM_HEIGHT 106U
#define GM_PLUGIN_LVGL_STYLE_TRANSLATE_X      107U
#define GM_PLUGIN_LVGL_STYLE_TRANSLATE_Y      108U
#define GM_PLUGIN_LVGL_STYLE_TRANSFORM_ZOOM   109U
#define GM_PLUGIN_LVGL_STYLE_TRANSFORM_ANGLE  110U
#define GM_PLUGIN_LVGL_STYLE_TRANSFORM_PIVOT_X 111U
#define GM_PLUGIN_LVGL_STYLE_TRANSFORM_PIVOT_Y 112U

#define GM_PLUGIN_LVGL_GRAD_DIR_NONE UINT8_C(0)
#define GM_PLUGIN_LVGL_GRAD_DIR_VER  UINT8_C(1)
#define GM_PLUGIN_LVGL_GRAD_DIR_HOR  UINT8_C(2)
#define GM_PLUGIN_LVGL_BORDER_SIDE_NONE   UINT8_C(0x00)
#define GM_PLUGIN_LVGL_BORDER_SIDE_BOTTOM UINT8_C(0x01)
#define GM_PLUGIN_LVGL_BORDER_SIDE_TOP    UINT8_C(0x02)
#define GM_PLUGIN_LVGL_BORDER_SIDE_LEFT   UINT8_C(0x04)
#define GM_PLUGIN_LVGL_BORDER_SIDE_RIGHT  UINT8_C(0x08)
#define GM_PLUGIN_LVGL_BORDER_SIDE_FULL   UINT8_C(0x0F)
#define GM_PLUGIN_LVGL_TEXT_DECOR_NONE          UINT8_C(0x00)
#define GM_PLUGIN_LVGL_TEXT_DECOR_UNDERLINE     UINT8_C(0x01)
#define GM_PLUGIN_LVGL_TEXT_DECOR_STRIKETHROUGH UINT8_C(0x02)
#define GM_PLUGIN_LVGL_OPA_TRANSPARENT UINT8_C(0)
#define GM_PLUGIN_LVGL_OPA_20          UINT8_C(51)
#define GM_PLUGIN_LVGL_OPA_30          UINT8_C(76)
#define GM_PLUGIN_LVGL_OPA_40          UINT8_C(102)
#define GM_PLUGIN_LVGL_OPA_50          UINT8_C(127)
#define GM_PLUGIN_LVGL_OPA_60          UINT8_C(153)
#define GM_PLUGIN_LVGL_OPA_70          UINT8_C(178)
#define GM_PLUGIN_LVGL_OPA_80          UINT8_C(204)
#define GM_PLUGIN_LVGL_OPA_90          UINT8_C(229)
#define GM_PLUGIN_LVGL_OPA_COVER       UINT8_C(255)

#define GM_PLUGIN_LVGL_SELECTOR_MAIN     UINT32_C(0x000000)
#define GM_PLUGIN_LVGL_SELECTOR_INDICATOR UINT32_C(0x020000)
#define GM_PLUGIN_LVGL_SELECTOR_KNOB      UINT32_C(0x030000)
#define GM_PLUGIN_LVGL_SELECTOR_SELECTED UINT32_C(0x040000)
#define GM_PLUGIN_LVGL_TEXT_ALIGN_AUTO   UINT8_C(0)
#define GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT   UINT8_C(1)
#define GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER UINT8_C(2)
#define GM_PLUGIN_LVGL_TEXT_ALIGN_RIGHT  UINT8_C(3)
#define GM_PLUGIN_LVGL_TEXT_FLAG_NONE    UINT8_C(0)
#define GM_PLUGIN_LVGL_TEXT_SELECTION_OFF UINT32_C(0xFFFF)

typedef uint8_t gm_plugin_lvgl_align_t;
enum {
    GM_PLUGIN_LVGL_ALIGN_TOP_LEFT = 1,
    GM_PLUGIN_LVGL_ALIGN_TOP_MID = 2,
    GM_PLUGIN_LVGL_ALIGN_TOP_RIGHT = 3,
    GM_PLUGIN_LVGL_ALIGN_BOTTOM_LEFT = 4,
    GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID = 5,
    GM_PLUGIN_LVGL_ALIGN_BOTTOM_RIGHT = 6,
    GM_PLUGIN_LVGL_ALIGN_LEFT_MID = 7,
    GM_PLUGIN_LVGL_ALIGN_RIGHT_MID = 8,
    GM_PLUGIN_LVGL_ALIGN_CENTER = 9,
};

typedef uint8_t gm_plugin_lvgl_label_mode_t;
enum {
    GM_PLUGIN_LVGL_LABEL_WRAP = 0,
    GM_PLUGIN_LVGL_LABEL_DOT = 1,
    GM_PLUGIN_LVGL_LABEL_SCROLL = 2,
    GM_PLUGIN_LVGL_LABEL_SCROLL_CIRCULAR = 3,
    GM_PLUGIN_LVGL_LABEL_CLIP = 4,
};

/* LVGL is the recommended renderer for normal UI and already uses the Host's
 * display synchronization. Do not surround LVGL calls with
 * graphics.framebuffer.lock/unlock and do not call any entry in this table
 * while a direct framebuffer surface is locked. The two renderers may be used
 * only in separate, non-nested phases; a later LVGL redraw may overwrite
 * overlapping pixels written through the direct framebuffer API.
 *
 * ABI-FROZEN 1.0 prefix. This is not lv_obj_t's binary layout.
 * APPEND fields only, bump the minor version and preserve old behavior.
 * Never reorder/remove entries or expose a new LVGL private structure. */
typedef struct gm_plugin_lvgl_api {
    /** Total byte size of this LVGL table, including this field. */
    uint16_t struct_size;
    /** Packed GM_PLUGIN_VERSION() implemented by this table. */
    uint16_t api_version;

    /**
     * Get this plugin application's Host-owned LVGL root.
     * @return Root valid during the started UI cycle, or NULL when no plugin
     *         root exists. The plugin may clean it but must never delete it.
     */
    gm_plugin_lvgl_obj_t *(*root_get)(void);
    /**
     * Create a generic child object.
     * @param parent Non-NULL plugin-owned object or Host-owned plugin root.
     * @return New child object, or NULL if creation fails. Its lifetime is
     *         bounded by deletion of itself, an ancestor, or the plugin root.
     */
    gm_plugin_lvgl_obj_t *(*obj_create)(gm_plugin_lvgl_obj_t *parent);
    /**
     * Delete an object and all of its descendants.
     * @param object Non-NULL plugin-created object. Never pass the Host root;
     *        the pointer becomes invalid as soon as this function returns.
     */
    void (*obj_delete)(gm_plugin_lvgl_obj_t *object);
    /**
     * Delete every child of an object without deleting the object itself.
     * @param object Non-NULL live object, including the Host root.
     */
    void (*obj_clean)(gm_plugin_lvgl_obj_t *object);
    /**
     * Set coordinates relative to the object's parent content area.
     * @param object Non-NULL live object.
     * @param x Horizontal position in logical pixels.
     * @param y Vertical position in logical pixels.
     */
    void (*obj_set_pos)(gm_plugin_lvgl_obj_t *object,
                        gm_plugin_lvgl_coord_t x, gm_plugin_lvgl_coord_t y);
    /**
     * Set object dimensions.
     * @param object Non-NULL live object.
     * @param width Width in logical pixels.
     * @param height Height in logical pixels.
     */
    void (*obj_set_size)(gm_plugin_lvgl_obj_t *object,
                         gm_plugin_lvgl_coord_t width,
                         gm_plugin_lvgl_coord_t height);
    /**
     * Align an object to its parent.
     * @param object Non-NULL live object.
     * @param alignment One GM_PLUGIN_LVGL_ALIGN_* value.
     * @param x Horizontal offset from the selected alignment point.
     * @param y Vertical offset from the selected alignment point.
     */
    void (*obj_align)(gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_align_t alignment,
                      gm_plugin_lvgl_coord_t x, gm_plugin_lvgl_coord_t y);
    /**
     * Add one or more OR-ed GM_PLUGIN_LVGL_FLAG_* flags.
     * @param object Non-NULL live object.
     * @param flag Flag bit or OR-ed flag bits to add.
     */
    void (*obj_add_flag)(gm_plugin_lvgl_obj_t *object, gm_plugin_lvgl_flag_t flag);
    /**
     * Clear one or more OR-ed GM_PLUGIN_LVGL_FLAG_* flags.
     * @param object Non-NULL live object.
     * @param flag Flag bit or OR-ed flag bits to clear.
     */
    void (*obj_clear_flag)(gm_plugin_lvgl_obj_t *object, gm_plugin_lvgl_flag_t flag);
    /**
     * Mark an object's current area for redraw by LVGL.
     * @param object Non-NULL live object; ownership is unchanged.
     */
    void (*obj_invalidate)(const gm_plugin_lvgl_obj_t *object);
    /**
     * @param object Non-NULL live object.
     * @return Current computed width in logical pixels.
     */
    gm_plugin_lvgl_coord_t (*obj_get_width)(const gm_plugin_lvgl_obj_t *object);
    /**
     * @param object Non-NULL live object.
     * @return Current computed height in logical pixels.
     */
    gm_plugin_lvgl_coord_t (*obj_get_height)(const gm_plugin_lvgl_obj_t *object);

    /**
     * Set one local style property on an object.
     * @param object Non-NULL live object.
     * @param property One compatible GM_PLUGIN_LVGL_STYLE_* property ID.
     * @param value Typed value made with gm_plugin_lvgl_style_number() or
     *        gm_plugin_lvgl_style_color(), or a documented pointer value.
     * @param selector GM_PLUGIN_LVGL_SELECTOR_* target part/state selector.
     */
    void (*style_set)(gm_plugin_lvgl_obj_t *object,
                      gm_plugin_lvgl_style_prop_t property,
                      gm_plugin_lvgl_style_value_t value,
                      gm_plugin_lvgl_selector_t selector);

    /**
     * Create a text label.
     * @param parent Non-NULL plugin object or Host root.
     * @return New label, or NULL if creation fails.
     */
    gm_plugin_lvgl_obj_t *(*label_create)(gm_plugin_lvgl_obj_t *parent);
    /**
     * Replace a label's text.
     * @param label Non-NULL live label.
     * @param utf8 Non-NULL NUL-terminated UTF-8 string. LVGL copies the string,
     *        so the caller may reuse its buffer after this function returns.
     */
    void (*label_set_text)(gm_plugin_lvgl_obj_t *label, const char *utf8);
    /**
     * Select how text exceeding the label bounds is handled.
     * @param label Non-NULL live label.
     * @param mode One GM_PLUGIN_LVGL_LABEL_* value.
     */
    void (*label_set_long_mode)(gm_plugin_lvgl_obj_t *label,
                                gm_plugin_lvgl_label_mode_t mode);

    /**
     * Create an arc object.
     * @param parent Non-NULL plugin object or Host root.
     * @return New arc, or NULL if creation fails.
     */
    gm_plugin_lvgl_obj_t *(*arc_create)(gm_plugin_lvgl_obj_t *parent);
    /**
     * Set an arc's inclusive numeric range.
     * @param arc Non-NULL live arc.
     * @param minimum Minimum value; it must not exceed maximum.
     * @param maximum Maximum value; it must not be less than minimum.
     */
    void (*arc_set_range)(gm_plugin_lvgl_obj_t *arc,
                          int16_t minimum, int16_t maximum);
    /**
     * Set the current arc value.
     * @param arc Non-NULL live arc.
     * @param value Value interpreted within the range set by arc_set_range().
     */
    void (*arc_set_value)(gm_plugin_lvgl_obj_t *arc, int16_t value);

    /**
     * Create a line object.
     * @param parent Non-NULL plugin object or Host root.
     * @return New line, or NULL if creation fails.
     */
    gm_plugin_lvgl_obj_t *(*line_create)(gm_plugin_lvgl_obj_t *parent);
    /**
     * Replace the points referenced by a line. LVGL retains the pointer.
     * @param line Non-NULL live line.
     * @param points Non-NULL point array that remains readable and unchanged
     *        until the next line_set_points() call or deletion of the line.
     * @param point_count Number of elements in points; use at least two for a
     *        visible line.
     */
    void (*line_set_points)(gm_plugin_lvgl_obj_t *line,
                            const gm_plugin_lvgl_point_t *points,
                            uint16_t point_count);

    /* Font pointers are Host-owned and immutable. */
    const gm_plugin_lvgl_font_t *font_default;
    const gm_plugin_lvgl_font_t *font_large;
    /**
     * @param font Non-NULL Host-owned font, normally font_default/font_large.
     * @return Font line height in logical pixels.
     */
    gm_plugin_lvgl_coord_t (*font_get_line_height)(
        const gm_plugin_lvgl_font_t *font);
    /**
     * Measure a NUL-terminated UTF-8 string without creating an object.
     * @param size Non-NULL output receiving measured width in x and height in y.
     * @param utf8 Non-NULL NUL-terminated UTF-8 text.
     * @param font Non-NULL Host-owned font.
     * @param letter_space Extra logical pixels between adjacent characters.
     * @param line_space Extra logical pixels between adjacent lines.
     * @param max_width Maximum line width used for wrapping.
     * @param flags OR-ed GM_PLUGIN_LVGL_TEXT_FLAG_* values.
     */
    void (*text_get_size)(gm_plugin_lvgl_point_t *size,
                          const char *utf8,
                          const gm_plugin_lvgl_font_t *font,
                          gm_plugin_lvgl_coord_t letter_space,
                          gm_plugin_lvgl_coord_t line_space,
                          gm_plugin_lvgl_coord_t max_width,
                          gm_plugin_lvgl_text_flag_t flags);
    /**
     * Find the end of the next rendered line in a UTF-8 string.
     * @param utf8 Non-NULL NUL-terminated UTF-8 text at the line start.
     * @param font Non-NULL Host-owned font.
     * @param letter_space Extra logical pixels between adjacent characters.
     * @param max_width Maximum rendered line width before wrapping.
     * @param used_width Optional output for the selected line width; may be NULL.
     * @param flags OR-ed GM_PLUGIN_LVGL_TEXT_FLAG_* values.
     * @return Byte offset from utf8 to the start of the following line, or to
     *         the terminating NUL when this is the final line.
     */
    uint32_t (*text_get_next_line)(
        const char *utf8, const gm_plugin_lvgl_font_t *font,
        gm_plugin_lvgl_coord_t letter_space,
        gm_plugin_lvgl_coord_t max_width,
        gm_plugin_lvgl_coord_t *used_width,
        gm_plugin_lvgl_text_flag_t flags);
    /**
     * Set the first selected character in a label.
     * @param label Non-NULL live label.
     * @param index Character index, or GM_PLUGIN_LVGL_TEXT_SELECTION_OFF to
     *        disable the selection start.
     */
    void (*label_set_selection_start)(gm_plugin_lvgl_obj_t *label,
                                      uint32_t index);
    /**
     * Set the exclusive end of a label selection.
     * @param label Non-NULL live label.
     * @param index Character index, or GM_PLUGIN_LVGL_TEXT_SELECTION_OFF to
     *        disable the selection end.
     */
    void (*label_set_selection_end)(gm_plugin_lvgl_obj_t *label,
                                    uint32_t index);

    /**
     * Create an indexed image whose transparent palette entries reveal its
     * parent and lower siblings.
     * @param parent Non-NULL plugin object or Host root.
     * @param source Non-NULL image descriptor copied during this call. The
     *        source data itself remains borrowed and must stay readable and
     *        unchanged until the next image_set_source() call or object
     *        deletion.
     * @return New image object, or NULL for invalid input/allocation failure.
     */
    gm_plugin_lvgl_obj_t *(*image_create)(
        gm_plugin_lvgl_obj_t *parent,
        const gm_plugin_lvgl_image_dsc_t *source);
    /**
     * Change an image frame without copying its pixel payload.
     * @param image Non-NULL live object returned by image_create().
     * @param source Non-NULL descriptor copied during this call. Its data must
     *        remain readable and unchanged until the next source change or
     *        object deletion.
     * @return GM_PLUGIN_OK, GM_PLUGIN_EINVAL for malformed input/wrong object,
     *         or GM_PLUGIN_ESTATE when no plugin UI cycle is active.
     */
    gm_plugin_result_t (*image_set_source)(
        gm_plugin_lvgl_obj_t *image,
        const gm_plugin_lvgl_image_dsc_t *source);

    /**
     * Create a frame animation backed by static indexed image payloads.
     * Descriptor values are copied, so the frames array and descriptors may be
     * temporary; every descriptor's data remains borrowed for the lifetime of
     * the animation object or until anim_image_set_sources() replaces it.
     * @param parent Non-NULL plugin object or Host root.
     * @param frames Non-NULL array of non-NULL descriptor pointers.
     * @param frame_count Number of frames, 1..GM_PLUGIN_LVGL_ANIM_IMAGE_MAX_FRAMES.
     * @param frame_duration_ms Non-zero display duration of each frame.
     * @param repeat_count Number of repeats after the first play; use zero to
     *        play once or GM_PLUGIN_LVGL_ANIM_REPEAT_INFINITE to loop forever.
     * @return New stopped animation object, or NULL for invalid input/allocation
     *         failure. Call anim_image_start() to begin playback.
     */
    gm_plugin_lvgl_obj_t *(*anim_image_create)(
        gm_plugin_lvgl_obj_t *parent,
        const gm_plugin_lvgl_image_dsc_t *const frames[],
        uint16_t frame_count, uint32_t frame_duration_ms,
        uint16_t repeat_count);
    /** Replace all animation frames and stop playback on the first frame. */
    gm_plugin_result_t (*anim_image_set_sources)(
        gm_plugin_lvgl_obj_t *animation,
        const gm_plugin_lvgl_image_dsc_t *const frames[],
        uint16_t frame_count);
    /**
     * Set the non-zero duration of each frame in milliseconds. A running
     * animation keeps its current timing until anim_image_start() restarts it.
     */
    gm_plugin_result_t (*anim_image_set_frame_duration)(
        gm_plugin_lvgl_obj_t *animation, uint32_t frame_duration_ms);
    /**
     * Set repeats after the first play. A running animation keeps its current
     * count until anim_image_start() restarts it. Use
     * GM_PLUGIN_LVGL_ANIM_REPEAT_INFINITE for infinite playback.
     */
    gm_plugin_result_t (*anim_image_set_repeat_count)(
        gm_plugin_lvgl_obj_t *animation, uint16_t repeat_count);
    /** Start or restart playback from the first frame. */
    gm_plugin_result_t (*anim_image_start)(gm_plugin_lvgl_obj_t *animation);
    /** Stop playback while keeping the current frame visible. */
    gm_plugin_result_t (*anim_image_stop)(gm_plugin_lvgl_obj_t *animation);
} gm_plugin_lvgl_api_t;

#define GM_PLUGIN_LVGL_API_MIN_SIZE \
    ((uint16_t)GM_PLUGIN_MEMBER_END(gm_plugin_lvgl_api_t, \
                                    label_set_selection_end))
#define GM_PLUGIN_LVGL_API_1_1_SIZE \
    ((uint16_t)GM_PLUGIN_MEMBER_END(gm_plugin_lvgl_api_t, \
                                    anim_image_stop))

#ifdef __cplusplus
}
#endif

#endif
