#ifndef GM_PLUGIN_PACKAGE_H
#define GM_PLUGIN_PACKAGE_H

#include <stdint.h>

#define GM_PLUGIN_PACKAGE_MAGIC "GMPK"
#define GM_PLUGIN_PACKAGE_FORMAT_VERSION 1u
#define GM_PLUGIN_PACKAGE_HEADER_SIZE 28u

/* PUBLISHED ON-WIRE FORMAT -- GMP v1 IS IMMUTABLE AFTER RELEASE.
 * Do not change the magic, version value, field order/type/endianness, header
 * size, alignment, CRC coverage or existing field meaning. Add a new package
 * format version and keep the v1 parser when a different layout is required.
 *
 * GMP v1 has a fixed little-endian layout:
 *   header, load image, 4-byte alignment, uint32_t base relocations, tail/BSS.
 * Offsets and package size are therefore derived instead of stored. */
#pragma pack(push, 1)
typedef struct {
    uint8_t magic[4]; /**< Exact GM_PLUGIN_PACKAGE_MAGIC byte sequence. */
    uint16_t format_version; /**< GM_PLUGIN_PACKAGE_FORMAT_VERSION. */
    uint16_t abi_version; /**< Packed Host ABI required by this image. */
    uint32_t image_size; /**< Initialized load-image bytes after the header. */
    uint32_t memory_size; /**< Total runtime image plus zeroed BSS bytes. */
    uint32_t entry_offset; /**< Byte offset of gm_plugin_entry in load image. */
    uint32_t relocation_count; /**< Number of trailing uint32_t relocations. */
    uint32_t package_crc32; /**< CRC32 of the complete package with this zero. */
} gm_plugin_package_header_t;
#pragma pack(pop)

typedef char gm_plugin_package_header_size_must_match[
    sizeof(gm_plugin_package_header_t) == GM_PLUGIN_PACKAGE_HEADER_SIZE ? 1 : -1];

#endif
