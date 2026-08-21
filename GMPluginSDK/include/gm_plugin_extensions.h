#ifndef GM_PLUGIN_EXTENSIONS_H
#define GM_PLUGIN_EXTENSIONS_H

#include <stdint.h>

/*
 * EXTENSION ID REGISTRY -- APPEND ONLY AFTER THE FIRST PUBLIC RELEASE.
 *
 * Allocate the next unused positive value. Never renumber, reuse or change
 * the meaning of an existing ID. An incompatible table needs a new ID while
 * firmware continues serving the old table for already-built plugins.
 */
#define GM_PLUGIN_EXTENSION_DEMO UINT32_C(1)
#define GM_PLUGIN_EXTENSION_LZ4 UINT32_C(2)

/* Minimal end-to-end example for adding a future extension. Real extensions
 * should define a descriptive table type and allocate a new ID above. Once an
 * ID is published, keep its table layout and behavior available forever. */
typedef struct gm_plugin_demo_extension_api {
    /**
     * Demonstration operation that adds two signed integers.
     * @param left Left operand.
     * @param right Right operand.
     * @return `left + right`. This demo assumes the sum is representable by
     *         int32_t; callers must avoid signed overflow.
     */
    int32_t (*add)(int32_t left, int32_t right);
} gm_plugin_demo_extension_api_t;

/**
 * Raw LZ4 block compression services.
 *
 * This fixed table follows the LZ4 block API return conventions. It does not
 * read or write the LZ4 frame format and does not prepend the uncompressed
 * size. Callers must carry the compressed and uncompressed sizes separately.
 */
typedef struct gm_plugin_lz4_extension_api {
    /**
     * Return the maximum compressed size for a source block.
     * @return A positive bound, or zero when source_size is invalid.
     */
    int32_t (*compress_bound)(int32_t source_size);

    /**
     * Compress one independent raw LZ4 block.
     *
     * Source and destination must be non-NULL, non-overlapping buffers.
     * @return The compressed byte count, or zero on failure, including an
     *         insufficient destination capacity or temporary Host memory.
     */
    int32_t (*compress_default)(const void *source, void *destination,
                                int32_t source_size,
                                int32_t destination_capacity);

    /**
     * Safely decompress one independent raw LZ4 block.
     *
     * Source and destination must be non-NULL, non-overlapping buffers.
     * @return The decompressed byte count on success, or a negative value for
     *         malformed input or insufficient destination capacity.
     */
    int32_t (*decompress_safe)(const void *source, void *destination,
                               int32_t compressed_size,
                               int32_t destination_capacity);
} gm_plugin_lz4_extension_api_t;

#endif
