/**
 * @file lv_font.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_font.h"
#include "../misc/lv_utils.h"
#include "../misc/lv_log.h"
#include "../misc/lv_assert.h"
#include "gm_log.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Return with the bitmap of a font.
 * @param font_p pointer to a font
 * @param letter a UNICODE character code
 * @return pointer to the bitmap of the letter
 */
static uint8_t fontData[1] = {0};

const uint8_t * lv_font_get_glyph_bitmap(const lv_font_t * font_p, uint32_t letter)
{
    LV_ASSERT_NULL(font_p);
    if(letter == 0xA0 || letter == 0xFFFC || (letter >= 0x200B && letter <= 0x200F) || (letter >= 0x202A && letter <= 0x202F) || (letter >= 0x1F3FB && letter <= 0x1F3FF))
    {
        //不支持的空格或肤色修饰符，返回空白位图
        return fontData;
    }
    return font_p->get_glyph_bitmap(font_p, letter);
}

/**
 * Get the descriptor of a glyph
 * @param font_p pointer to font
 * @param dsc_out store the result descriptor here
 * @param letter a UNICODE letter code
 * @param letter_next the next letter after `letter`. Used for kerning
 * @return true: descriptor is successfully loaded into `dsc_out`.
 *         false: the letter was not found, no data is loaded to `dsc_out`
 */
bool lv_font_get_glyph_dsc(const lv_font_t * font_p, lv_font_glyph_dsc_t * dsc_out, uint32_t letter,
                           uint32_t letter_next)
{
    // 断言参数有效性
    LV_ASSERT_NULL(font_p); // 字体指针不能为空
    LV_ASSERT_NULL(dsc_out); // 输出描述结构体指针不能为空

#if LV_USE_FONT_PLACEHOLDER
    // 占位字体指针，记录第一个返回 is_placeholder=true 的字体
    const lv_font_t * placeholder_font = NULL;
#endif

    // 当前正在查找的字体指针，初始为传入字体
    const lv_font_t * f = font_p;

    // 先清空 resolved_font，后续查找到后会赋值
    dsc_out->resolved_font = NULL;

    // 如果是控制字符、LV_SYMBOL_DUMMY 或 ZERO WIDTH NON-JOINER，宽度为0
    if(letter < 0x20 ||
       letter == 0xf8ff || /*LV_SYMBOL_DUMMY*/
       letter == 0x20E3 ||
       letter == 0xFE0F) { /*ZERO WIDTH NON-JOINER*/
        dsc_out->box_w = 0;
        dsc_out->adv_w = 0;
        // 填充剩余的字形描述字段，标记为占位符
        dsc_out->resolved_font = NULL;
        dsc_out->box_h = font_p->line_height;
        dsc_out->ofs_x = 0;
        dsc_out->ofs_y = 0;
        dsc_out->bpp   = 1;
        dsc_out->is_placeholder = true;
        // 没有找到字形，返回 false
        return false;
    }

    if(letter == 0xA0 || letter == 0xFFFC || (letter >= 0x200B && letter <= 0x200F) || (letter >= 0x202A && letter <= 0x202F) || (letter >= 0x1F3FB && letter <= 0x1F3FF))
    {
        //不支持的空格或肤色修饰符，宽度为0

        if(letter == 0xA0)
        {
            dsc_out->adv_w = 4; // non-breaking space advance width
        }
        else
        {
            dsc_out->adv_w = 0; // skin tone modifiers have no advance width
        }
        dsc_out->box_w = 0;
        // 填充剩余的字形描述字段，标记为占位符
        dsc_out->resolved_font = f;
        dsc_out->box_h = 0;
        dsc_out->ofs_x = 0;
        dsc_out->ofs_y = 0;
        dsc_out->bpp   = 1;
        dsc_out->is_placeholder = false;
        // 没有找到字形，返回 false
        return true;
    }

    // 遍历字体链表（支持字体 fallback 机制）
    while(f) {
        // 调用当前字体的 get_glyph_dsc 回调，查找 letter 的字形描述
        bool found = f->get_glyph_dsc(f, dsc_out, letter, letter_next);
        if(found) {
            // 如果找到且不是占位符，说明该字体真正包含此字形
            if(!dsc_out->is_placeholder) {
                dsc_out->resolved_font = f; // 记录找到的字体
                return true; // 查找成功，返回 true
            }
#if LV_USE_FONT_PLACEHOLDER
            // 如果是占位符，且还没记录过占位字体，则记录下来
            else if(placeholder_font == NULL) {
                placeholder_font = f;
            }
#endif
        }
        // 没找到则查找下一个 fallback 字体
        f = f->fallback;
    }

#if LV_USE_FONT_PLACEHOLDER
    // 如果有占位字体，优先返回占位符的字形描述
    if(placeholder_font != NULL) {
        placeholder_font->get_glyph_dsc(placeholder_font, dsc_out, letter, letter_next);
        dsc_out->resolved_font = placeholder_font;
        return true;
    }
#endif

#if LV_USE_FONT_PLACEHOLDER
    //如果启用占位符，返回一个默认宽度的占位字形
    dsc_out->box_w = font_p->line_height / 2;
    dsc_out->adv_w = dsc_out->box_w + 2;
#else
    // 未启用占位符则宽度为0
    dsc_out->box_w = 0;
    dsc_out->adv_w = 0;
#endif

    // 填充剩余的字形描述字段，标记为占位符
    dsc_out->resolved_font = NULL;
    dsc_out->box_h = font_p->line_height;
    dsc_out->ofs_x = 0;
    dsc_out->ofs_y = 0;
    dsc_out->bpp   = 1;
    dsc_out->is_placeholder = true;
    GMLOGD_STREAM("get_glyph_dsc: unicode=0x%04x,line_height = %d not found\n", letter,font_p->line_height);
    // 没有找到字形，返回 false
    return false;
}

/**
 * Get the width of a glyph with kerning
 * @param font pointer to a font
 * @param letter a UNICODE letter
 * @param letter_next the next letter after `letter`. Used for kerning
 * @return the width of the glyph
 */
uint16_t lv_font_get_glyph_width(const lv_font_t * font, uint32_t letter, uint32_t letter_next)
{
    LV_ASSERT_NULL(font);
    lv_font_glyph_dsc_t g;
    lv_font_get_glyph_dsc(font, &g, letter, letter_next);
    return g.adv_w;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
