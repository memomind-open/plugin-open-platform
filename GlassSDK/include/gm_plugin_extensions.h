#ifndef GM_PLUGIN_EXTENSIONS_H
#define GM_PLUGIN_EXTENSIONS_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*
 * EXTENSION ID REGISTRY -- APPEND ONLY AFTER THE FIRST PUBLIC RELEASE.
 *
 * Allocate the next unused positive value. Never renumber, reuse or change
 * the meaning of an existing ID. An incompatible table needs a new ID while
 * firmware continues serving the old table for already-built plugins.
 */
#define GM_PLUGIN_EXTENSION_RANDOM UINT32_C(1)
#define GM_PLUGIN_EXTENSION_LZ4 UINT32_C(2)
#define GM_PLUGIN_EXTENSION_LIBC UINT32_C(3)

/**
 * Pseudo-random number services.
 *
 * The generated values are suitable for non-security uses such as randomized
 * UI behavior. They are not a cryptographically secure random source and must
 * not be used to generate keys, nonces or other security-sensitive values.
 */
typedef struct gm_plugin_random_extension_api {
    /** Return the next pseudo-random value in the inclusive range 0..UINT32_MAX. */
    uint32_t (*get_u32)(void);
} gm_plugin_random_extension_api_t;

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

/**
 * Host C runtime services.
 *
 * This table gives freestanding plugins access to common memory, byte-string,
 * and bounded formatting operations without linking a private C runtime into
 * every package. Every table member uses the corresponding familiar C/POSIX
 * library name and, unless stated otherwise, follows that function's standard
 * contract. Every pointer and NUL-terminated string supplied by a plugin must
 * refer to readable or writable memory owned by that plugin, or to memory
 * explicitly loaned to it by the Host.
 *
 * The table is immutable and valid until plugin unload. Its layout is frozen;
 * an incompatible revision will use a new extension ID rather than changing
 * this structure. Functions do not allocate memory and retain no arguments.
 */
typedef struct gm_plugin_libc_extension_api {
    /**
     * Copy exactly size bytes from source to destination.
     * The regions must not overlap. Returns destination.
     */
    void *(*memcpy)(void *destination, const void *source, size_t size);
    /**
     * Copy exactly size bytes, including between overlapping regions.
     * Returns destination.
     */
    void *(*memmove)(void *destination, const void *source, size_t size);
    /** Fill size bytes at destination with the low eight bits of value. */
    void *(*memset)(void *destination, int value, size_t size);
    /**
     * Compare size bytes as unsigned bytes. Returns less than, equal to, or
     * greater than zero according to the first differing byte.
     */
    int (*memcmp)(const void *left, const void *right, size_t size);
    /**
     * Find the first byte equal to the low eight bits of value within size
     * bytes. Returns its address, or NULL when absent.
     */
    void *(*memchr)(const void *memory, int value, size_t size);

    /** Return the byte length before text's terminating NUL. */
    size_t (*strlen)(const char *text);
    /**
     * Return the byte length before NUL, examining at most maximum bytes.
     * A maximum return value means that no NUL was found in that range.
     */
    size_t (*strnlen)(const char *text, size_t maximum);
    /**
     * Copy source, including its terminating NUL, to destination and return
     * destination. The caller must provide enough non-overlapping storage.
     */
    char *(*strcpy)(char *destination, const char *source);
    /**
     * Copy exactly count bytes, padding with NUL bytes when source is shorter.
     * The result is not NUL-terminated when source has count or more bytes.
     * The regions must not overlap. Returns destination.
     */
    char *(*strncpy)(char *destination, const char *source, size_t count);
    /**
     * Append source, including its terminating NUL, and return destination.
     * Destination must already be NUL-terminated and have sufficient space;
     * source and destination must not overlap.
     */
    char *(*strcat)(char *destination, const char *source);
    /**
     * Append at most count source bytes and always terminate the result with
     * NUL. Destination must have room for its old contents, appended bytes,
     * and the terminator. The regions must not overlap.
     */
    char *(*strncat)(char *destination, const char *source, size_t count);
    /** Compare two NUL-terminated byte strings using unsigned bytes. */
    int (*strcmp)(const char *left, const char *right);
    /** Compare at most count bytes of two NUL-terminated byte strings. */
    int (*strncmp)(const char *left, const char *right, size_t count);
    /** Return the first occurrence of character in text, or NULL. */
    char *(*strchr)(const char *text, int character);
    /** Return the last occurrence of character in text, or NULL. */
    char *(*strrchr)(const char *text, int character);
    /**
     * Return the first occurrence of the NUL-terminated needle in haystack,
     * or NULL. An empty needle returns haystack.
     */
    char *(*strstr)(const char *haystack, const char *needle);
    /**
     * Search only the first maximum bytes of haystack for the NUL-terminated
     * needle. A match must end within that bound. Returns the match or NULL.
     */
    char *(*strnstr)(const char *haystack, const char *needle,
                     size_t maximum);
    /** Return the length of the prefix containing only bytes from accept. */
    size_t (*strspn)(const char *text, const char *accept);
    /** Return the length of the prefix containing no byte from reject. */
    size_t (*strcspn)(const char *text, const char *reject);
    /** Return the first byte in text that occurs in accept, or NULL. */
    char *(*strpbrk)(const char *text, const char *accept);
    /**
     * Split text using delimiter bytes. The first call passes text; later
     * calls pass NULL and reuse save_pointer. This function writes NUL bytes
     * into text, is reentrant only with separate save_pointer values, and
     * returns the next token or NULL.
     */
    char *(*strtok_r)(char *text, const char *delimiters,
                      char **save_pointer);

    /**
     * Format into at most capacity bytes, including the terminating NUL.
     * Returns the number of bytes that would have been written excluding NUL,
     * or a negative value on error. A return value at least capacity means
     * truncation. Format arguments must exactly match their conversion
     * specifiers. Floating-point conversion support is Host-dependent.
     */
    int (*snprintf)(char *destination, size_t capacity,
                    const char *format, ...);
    /** Same contract as snprintf, taking an initialized va_list. */
    int (*vsnprintf)(char *destination, size_t capacity,
                     const char *format, va_list arguments);
} gm_plugin_libc_extension_api_t;

#endif
