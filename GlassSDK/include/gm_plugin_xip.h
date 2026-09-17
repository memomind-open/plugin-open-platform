#ifndef GM_PLUGIN_XIP_H
#define GM_PLUGIN_XIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GM_XIP_FORMAT 2U
#define GM_XIP_ALIGNMENT 64U
#define GM_XIP_HEADER_BYTES 120U
#define GM_XIP_MAX_SLOTS 128U
#define GM_XIP_MAX_FIXUPS 4096U
#define GM_XIP_MAX_FLASH (500U * 1024U)
/* Static RAM must be strictly below 100 KiB; dynamic heap is separate. */
#define GM_XIP_MAX_RAM (100U * 1024U - 1U)
#define GM_XIP_MAX_PACKAGE (3U * 1024U * 1024U - 65536U)
#define GM_XIP_RAM_TAG UINT32_C(0x80000000)
#define GM_XIP_RAM_VMA UINT32_C(0x10000000)

/* Little-endian GMP v2. Prefix metadata, immutable
 * Flash image, then RAM initial bytes. BSS consumes no package bytes. */
typedef struct {
    uint8_t magic[4];
    uint16_t format;
    uint16_t abi;
    uint32_t package_size;
    uint32_t flash_size;
    uint32_t ram_init_size;
    uint32_t ram_size;
    uint32_t entry;
    uint32_t slot_count;
    uint32_t fixup_count;
    uint32_t pointer_count;
    uint32_t flash_offset;
    uint32_t ram_offset;
    uint32_t crc;
    uint32_t version;
    char name[64];
} gm_xip_header_t;

typedef struct {
    uint32_t offset; /* Relative to the Flash image, two adjacent instructions. */
    uint16_t slot;
    uint8_t reg;
    uint8_t reserved;
} gm_xip_fixup_t;

typedef struct {
    uint32_t offset; /* Relative to RAM. */
    uint32_t target; /* RAM_TAG | RAM offset, or Flash offset. */
} gm_xip_pointer_t;

typedef struct {
    const uint8_t *package; /* Mapped Flash; caller keeps it pinned. */
    gm_xip_header_t header;
} gm_xip_view_t;

/* Only the prefix must be present. Does not read the image or allocate RAM. */
bool gm_xip_open_prefix(gm_xip_view_t *, const void *prefix,
                        uint32_t available, uint32_t package_size);
/* Validate each affected input byte before replacing it. Handles fragments
 * splitting either instruction at any byte. binding is the static slot array. */
bool gm_xip_transform(const gm_xip_view_t *, uint32_t offset, void *, uint32_t,
                       uint32_t binding, bool install);
/* Full package must be present. Installed form is checked against binding
 * BEFORE canonical CRC calculation, so corrupted executable bytes cannot hide. */
bool gm_xip_verify(const gm_xip_view_t *, uint32_t binding, bool installed);
/* Caller supplies only ram_size bytes and a fixed slot array. No Flash writes.
 * Explicit addresses allow host testing of RV32 relocation arithmetic. */
bool gm_xip_load_ram(const gm_xip_view_t *, void *ram, uint32_t ram_address,
                      uint32_t flash_address, uint32_t slots[GM_XIP_MAX_SLOTS]);
#endif
