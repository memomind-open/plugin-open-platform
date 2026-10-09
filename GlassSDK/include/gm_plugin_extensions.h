#ifndef GM_PLUGIN_EXTENSIONS_H
#define GM_PLUGIN_EXTENSIONS_H

#include "gm_plugin.h"

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
#define GM_PLUGIN_EXTENSION_INPUT UINT32_C(4)
#define GM_PLUGIN_EXTENSION_SYSTEM_EVENTS UINT32_C(5)

/** System UI notifications. Delivery continues while an overlay is visible.
 * No caller identity or phone number is exposed. Drawing remains plugin-owned. */
#define GM_PLUGIN_SYSTEM_EVENT_CALL_UI UINT16_C(1)
typedef struct {
    uint16_t struct_size;
    uint16_t type;
    uint32_t timestamp_ms;
    bool active; /**< true before call UI starts; false after it is destroyed. */
} gm_plugin_system_event_t;
typedef void (*gm_plugin_system_event_callback_t)(
    void *context, const gm_plugin_system_event_t *event);
typedef struct {
    /** Subscribe during on_start/on_resume. Replaces the previous callback.
     * Synchronously delivers the current CALL_UI state before returning.
     * Events are borrowed and serialized on the display task. This does not
     * stop loops or prohibit framebuffer writes: the plugin decides how to yield.
     * The Host revokes the subscription before stop/unload. */
    gm_plugin_result_t (*subscribe)(gm_plugin_system_event_callback_t on_event,
                                    void *context);
    void (*unsubscribe)(void);
} gm_plugin_system_events_extension_api_t;

/** HOGP input classes that a foreground plugin can claim from cooked routing. */
typedef uint32_t gm_plugin_input_classes_t;
enum {
    GM_PLUGIN_INPUT_CLASS_KEY = UINT32_C(1) << 0,
    GM_PLUGIN_INPUT_CLASS_REL = UINT32_C(1) << 1,
    GM_PLUGIN_INPUT_CLASS_ABS = UINT32_C(1) << 2,
    GM_PLUGIN_INPUT_CLASS_TOUCH = UINT32_C(1) << 3,
    GM_PLUGIN_INPUT_CLASS_ALL = UINT32_C(0x0F),
};

/** Device-independent key produced by the Host HOGP normalizer. */
typedef uint8_t gm_plugin_input_key_t;
enum {
    GM_PLUGIN_INPUT_KEY_UP = 0,
    GM_PLUGIN_INPUT_KEY_DOWN = 1,
    GM_PLUGIN_INPUT_KEY_LEFT = 2,
    GM_PLUGIN_INPUT_KEY_RIGHT = 3,
    GM_PLUGIN_INPUT_KEY_SELECT = 4,
    GM_PLUGIN_INPUT_KEY_BACK = 5,
    GM_PLUGIN_INPUT_KEY_HOME = 6,
    GM_PLUGIN_INPUT_KEY_MENU = 7,
    GM_PLUGIN_INPUT_KEY_NEXT = 8,
    GM_PLUGIN_INPUT_KEY_PREV = 9,
    GM_PLUGIN_INPUT_KEY_PLAY_PAUSE = 10,
    GM_PLUGIN_INPUT_KEY_VOL_UP = 11,
    GM_PLUGIN_INPUT_KEY_VOL_DOWN = 12,
    GM_PLUGIN_INPUT_KEY_MUTE = 13,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_A = 14,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_B = 15,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_X = 16,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_Y = 17,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_L1 = 18,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_R1 = 19,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_L2 = 20,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_R2 = 21,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_START = 22,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_SELECT = 23,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_THUMBL = 24,
    GM_PLUGIN_INPUT_KEY_GAMEPAD_THUMBR = 25,
    GM_PLUGIN_INPUT_KEY_MOUSE_LEFT = 26,
    GM_PLUGIN_INPUT_KEY_MOUSE_RIGHT = 27,
    GM_PLUGIN_INPUT_KEY_MOUSE_MIDDLE = 28,
    GM_PLUGIN_INPUT_KEY_REL_X = 29,
    GM_PLUGIN_INPUT_KEY_REL_Y = 30,
    GM_PLUGIN_INPUT_KEY_REL_WHEEL = 31,
    GM_PLUGIN_INPUT_KEY_REL_HWHEEL = 32,
    GM_PLUGIN_INPUT_KEY_ABS_X = 33,
    GM_PLUGIN_INPUT_KEY_ABS_Y = 34,
    GM_PLUGIN_INPUT_KEY_ABS_Z = 35,
    GM_PLUGIN_INPUT_KEY_ABS_RX = 36,
    GM_PLUGIN_INPUT_KEY_ABS_RY = 37,
    GM_PLUGIN_INPUT_KEY_ABS_RZ = 38,
    GM_PLUGIN_INPUT_KEY_ABS_HAT_X = 39,
    GM_PLUGIN_INPUT_KEY_ABS_HAT_Y = 40,
    GM_PLUGIN_INPUT_KEY_ABS_PRESSURE = 41,
    GM_PLUGIN_INPUT_KEY_TOUCH_DOWN = 42,
    GM_PLUGIN_INPUT_KEY_TOUCH_UP = 43,
    GM_PLUGIN_INPUT_KEY_TOUCH_MOVE = 44,
    GM_PLUGIN_INPUT_KEY_TOUCH_TAP = 45,
    GM_PLUGIN_INPUT_KEY_TOUCH_DOUBLE_TAP = 46,
    GM_PLUGIN_INPUT_KEY_TOUCH_LONG_PRESS = 47,
    GM_PLUGIN_INPUT_KEY_TOUCH_SWIPE_UP = 48,
    GM_PLUGIN_INPUT_KEY_TOUCH_SWIPE_DOWN = 49,
    GM_PLUGIN_INPUT_KEY_TOUCH_SWIPE_LEFT = 50,
    GM_PLUGIN_INPUT_KEY_TOUCH_SWIPE_RIGHT = 51,
    GM_PLUGIN_INPUT_KEY_VENDOR_USAGE = 52,
    GM_PLUGIN_INPUT_KEY_UNKNOWN = 53,
    GM_PLUGIN_INPUT_KEY_PAGE_UP = 54,
    GM_PLUGIN_INPUT_KEY_PAGE_DOWN = 55,
    GM_PLUGIN_INPUT_KEY_SCROLL_UP = 56,
    GM_PLUGIN_INPUT_KEY_SCROLL_DOWN = 57,
    GM_PLUGIN_INPUT_KEY_DOUBLE = 58,
    GM_PLUGIN_INPUT_KEY_LONG = 59,
    GM_PLUGIN_INPUT_KEY_AI_TRIGGER = 60,
};

typedef uint8_t gm_plugin_input_phase_t;
enum {
    GM_PLUGIN_INPUT_PHASE_DOWN = 0,
    GM_PLUGIN_INPUT_PHASE_UP = 1,
    GM_PLUGIN_INPUT_PHASE_REPEAT = 2,
    GM_PLUGIN_INPUT_PHASE_MOVE = 3,
    GM_PLUGIN_INPUT_PHASE_CANCEL = 4,
};

typedef uint8_t gm_plugin_input_source_t;
enum {
    GM_PLUGIN_INPUT_SOURCE_GENERIC = 0,
    GM_PLUGIN_INPUT_SOURCE_KEYBOARD = 1,
    GM_PLUGIN_INPUT_SOURCE_MOUSE = 2,
    GM_PLUGIN_INPUT_SOURCE_CONSUMER = 3,
    GM_PLUGIN_INPUT_SOURCE_DIGITIZER = 4,
    GM_PLUGIN_INPUT_SOURCE_GAMEPAD = 5,
    GM_PLUGIN_INPUT_SOURCE_VENDOR = 6,
};

typedef uint8_t gm_plugin_input_axis_t;
enum {
    GM_PLUGIN_INPUT_AXIS_NONE = 0,
    GM_PLUGIN_INPUT_AXIS_X = 1,
    GM_PLUGIN_INPUT_AXIS_Y = 2,
    GM_PLUGIN_INPUT_AXIS_Z = 3,
    GM_PLUGIN_INPUT_AXIS_RX = 4,
    GM_PLUGIN_INPUT_AXIS_RY = 5,
    GM_PLUGIN_INPUT_AXIS_RZ = 6,
    GM_PLUGIN_INPUT_AXIS_WHEEL = 7,
    GM_PLUGIN_INPUT_AXIS_HWHEEL = 8,
    GM_PLUGIN_INPUT_AXIS_HAT_X = 9,
    GM_PLUGIN_INPUT_AXIS_HAT_Y = 10,
};

enum {
    GM_PLUGIN_INPUT_MOD_LCTRL = UINT16_C(1) << 0,
    GM_PLUGIN_INPUT_MOD_LSHIFT = UINT16_C(1) << 1,
    GM_PLUGIN_INPUT_MOD_LALT = UINT16_C(1) << 2,
    GM_PLUGIN_INPUT_MOD_LGUI = UINT16_C(1) << 3,
    GM_PLUGIN_INPUT_MOD_RCTRL = UINT16_C(1) << 4,
    GM_PLUGIN_INPUT_MOD_RSHIFT = UINT16_C(1) << 5,
    GM_PLUGIN_INPUT_MOD_RALT = UINT16_C(1) << 6,
    GM_PLUGIN_INPUT_MOD_RGUI = UINT16_C(1) << 7,
};

/**
 * One normalized HOGP input sample. The sample is borrowed and remains valid
 * only for the callback duration. Relative axes use value as a delta; absolute
 * axes use value plus norm_value; digitizers use x/y and may use pressure in
 * value/norm_value. usage_page, usage_id, application and report_id preserve
 * the originating HID identity for device-specific controls.
 */
typedef struct gm_plugin_input_event {
    uint16_t struct_size;
    gm_plugin_input_key_t key;
    gm_plugin_input_phase_t phase;
    gm_plugin_input_source_t source;
    gm_plugin_input_axis_t axis;
    uint8_t report_id;
    uint8_t reserved0;
    int32_t value;
    int32_t norm_value;
    int32_t x;
    int32_t y;
    uint16_t modifiers;
    uint16_t usage_page;
    uint16_t usage_id;
    uint16_t reserved1;
    uint32_t application;
    uint32_t timestamp_ms;
} gm_plugin_input_event_t;

typedef void (*gm_plugin_input_callback_t)(
    void *context, const gm_plugin_input_event_t *event);

/**
 * Foreground HOGP input subscription.
 *
 * Subscribed classes are delivered raw and no longer produce their normal
 * cooked local navigation/button events. Subscribe only to classes the plugin
 * handles. The phone-side HOGP mirror remains independent.
 *
 * Subscribe from on_start() or on_resume(). The Host suspends delivery while
 * an overlay covers the plugin, restores it on resume, and revokes it before
 * stop/unload. Registering again replaces the callback and class mask.
 * Continuous samples are best-effort and may be rate-limited by the Host;
 * discrete DOWN/UP/CANCEL edges are preserved whenever queue capacity allows.
 */
typedef struct gm_plugin_input_extension_api {
    /**
     * Claim one or more GM_PLUGIN_INPUT_CLASS_* values for raw delivery.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL for an empty/unknown
     *         mask or NULL/invalid callback, or GM_PLUGIN_ESTATE outside a
     *         visible plugin cycle.
     */
    gm_plugin_result_t (*subscribe)(gm_plugin_input_classes_t classes,
                                    gm_plugin_input_callback_t callback,
                                    void *context);
    /** Disable raw delivery and restore normal cooked handling. Idempotent. */
    void (*unsubscribe)(void);
} gm_plugin_input_extension_api_t;

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
