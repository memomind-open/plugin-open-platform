#ifndef LV_FONT_FLASH_MANAGER_H
#define LV_FONT_FLASH_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 字体ID定义 - 根据 #font_config.json 中的实际字体更新
typedef enum {
    FONT_ID_XGIMI_17 = 4097,      // 0x1001 - lv_font_xgimi_17
    FONT_ID_XGIMI_20 = 4098,      // 0x1002 - lv_font_xgimi_20
    FONT_ID_XGIMI_EMO = 4099,     // 0x1004 - lv_font_xgimi_emo
    FONT_ID_MAX = FONT_ID_XGIMI_EMO+1
} font_id_t;

// 字体包头结构 - 移到头文件供外部使用
typedef struct {
    uint32_t magic;                    // "FONT" (0x544E4F46)
    uint32_t version;                  // 版本号
    uint32_t header_size;              // 头部大小
    uint32_t font_count;               // 字体数量
    uint32_t directory_size;           // 目录大小
    uint32_t data_size;                // 数据大小
    uint32_t total_crc32;              // 总CRC32
    uint32_t alignment;                // 对齐大小
    uint32_t padding_between_fonts;    // 字体间间隔
    uint8_t  reserved[92];             // 保留字段
} __attribute__((packed)) font_package_header_t;

// 字体目录项结构
typedef struct {
    uint32_t font_id;                  // 字体ID（4字节）
    uint32_t offset;                   // 在数据区的偏移（4字节）
    uint32_t original_size;            // 原始大小（4字节）
    uint32_t padded_size;              // 对齐后大小（4字节）
    uint32_t crc32;                    // CRC32校验（4字节）
    uint32_t flags;                    // 标志位（4字节）
    uint32_t reserved1;                // 保留字段1（4字节）
    uint32_t reserved2;                // 保留字段2（4字节）
} __attribute__((packed)) font_directory_entry_t;

// 字体信息结构 - 用于查询字体信息
typedef struct {
    uint32_t font_id;
    uint32_t original_size;
    uint32_t padded_size;
    uint32_t crc32;
    uint32_t flags;
    uint32_t offset;
    char name[32];                     // 字体名称（可选）
} font_info_t;

// 字体管理器统计信息
typedef struct {
    bool initialized;
    uint32_t font_count;
    uint32_t total_size;
    uint32_t data_size;
    uint32_t access_count;
    uint32_t version;
    uint32_t header_size;
    uint32_t directory_size;
} font_manager_stats_t;

typedef struct {
    uint32_t hit_unicode;
    uint16_t adv_w; /**< The glyph needs this space. Draw the next glyph after this width.*/
    uint16_t box_w; /**< Width of the glyph's bounding box*/
    uint16_t box_h; /**< Height of the glyph's bounding box*/
    int16_t ofs_x;  /**< x offset of the bounding box*/
    int16_t ofs_y;  /**< y offset of the bounding box*/
    uint8_t bpp;
} font_hit_glyph_dsc_t;

typedef struct {
    uint32_t hit_unicode;
    const uint8_t* bitmap_ptr;
} font_hit_bitmap_t;

// 常量定义
#define FONT_MAGIC_NUMBER           0x544E4F46  // "FONT"
#define FONT_PACKAGE_VERSION_MAX      99
#define FONT_PACKAGE_HEADER_SIZE    128
#define FONT_MAX_COUNT              64
#define FONT_EMPTY_DATA_SIZE        64

// 基础接口
bool init_font_flash_mapping(void);
uint8_t* get_font_base_address(void);
uint32_t get_font_size(void);
uint32_t get_font_count(void);
uint8_t* get_font_data(uint32_t offset, uint32_t size);
void deinit_font_flash_mapping(void);
bool is_font_initialized(void);

// 基于ID的新接口
const font_directory_entry_t* find_font_by_id(uint32_t font_id);
uint8_t* get_font_data_by_id(uint32_t font_id, uint32_t* size_out);
uint8_t* get_font_data_by_id_offset(uint32_t font_id, uint32_t offset, uint32_t size);

// 信息查询接口
bool get_font_info_by_id(uint32_t font_id, font_info_t* info);
bool get_font_header(font_package_header_t* header);
void list_available_fonts(void);

// 调试和统计接口
void get_font_manager_stats(font_manager_stats_t* stats);

// 获取字体包版本字符串，格式如 "1.2"，buf_size 建议至少 8 字节
bool get_font_version_string(char *buf, uint32_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV_FONT_FLASH_MANAGER_H */