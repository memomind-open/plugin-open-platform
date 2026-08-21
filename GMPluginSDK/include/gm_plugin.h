#ifndef GM_PLUGIN_H
#define GM_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RELEASE-CANDIDATE ABI CONTRACT.
 *
 * Before the first public release, intentional API cleanup may update this
 * file and regenerate the ABI snapshot together. After release, ABI 1.0 is
 * the frozen compatibility baseline enforced by SDK/CI:
 * - never renumber, reuse or change the meaning of an existing numeric value;
 * - never reorder, remove or change the type/signature of an existing field;
 * - core structures, enums, values and function tables are frozen as a whole;
 * - all post-release capabilities use a new extension ID and table;
 * - existing extension IDs and table prefixes remain available forever.
 *
 * Firmware implementation and function addresses may change freely as long as
 * this binary contract and the documented behavior remain unchanged.
 */

#define GM_PLUGIN_VERSION(major, minor) \
    ((uint16_t)((((uint16_t)(major)) << 8) | (uint8_t)(minor)))
#define GM_PLUGIN_VERSION_MAJOR(version) ((uint8_t)((version) >> 8))
#define GM_PLUGIN_VERSION_MINOR(version) ((uint8_t)(version))
#define GM_PLUGIN_VERSION_COMPATIBLE(available, required) \
    (GM_PLUGIN_VERSION_MAJOR(available) == GM_PLUGIN_VERSION_MAJOR(required) && \
     (uint16_t)(available) >= (uint16_t)(required))
/* Core ABI 1.0 is frozen. Future functionality uses extension_get(). */
#define GM_PLUGIN_ABI_MIN_VERSION GM_PLUGIN_VERSION(1U, 0U)
#define GM_PLUGIN_ABI_VERSION GM_PLUGIN_ABI_MIN_VERSION

/* ABI-FROZEN exported symbol and calling convention. A new major may define a
 * new entry contract; firmware must retain the old loader for old GMP files. */
#define GM_PLUGIN_ENTRY_NAME gm_plugin_entry

typedef int32_t gm_plugin_result_t;

/* ABI-FROZEN result numbers. Every API documents the subset it returns.
 * Removed results stay reserved forever. */
enum {
    GM_PLUGIN_OK = 0,         /**< Operation completed successfully. */
    GM_PLUGIN_EINVAL = -1,   /**< Invalid pointer, size, value, or layout. */
    GM_PLUGIN_ENOTSUP = -2,  /**< Host does not implement the requested item. */
    GM_PLUGIN_EBUSY = -3,    /**< Resource/sample is temporarily unavailable. */
    GM_PLUGIN_ENOMEM = -4,   /**< Required memory or output capacity is absent. */
    GM_PLUGIN_EIO = -5,      /**< Host device or transport operation failed. */
    GM_PLUGIN_EPERM = -6,    /**< Operation is not permitted in this context. */
    GM_PLUGIN_ESTATE = -7,   /**< Operation is invalid in the current state. */
    GM_PLUGIN_EVERSION = -8, /**< ABI or table version is incompatible. */
};

typedef uint32_t gm_plugin_capabilities_t;
typedef uint32_t gm_plugin_extension_id_t;

/* The LVGL drawing table is part of the initial core contract. Its concrete
 * layout lives in gm_plugin_lvgl_api.h to keep this core header lightweight. */
struct gm_plugin_lvgl_api;

/* Frozen 1.0 capability set. Do not allocate new bits after release; future
 * optional modules are discovered through extension_get(). */
enum {
    GM_PLUGIN_CAP_DISPLAY_BITMAP  = UINT32_C(1) << 0,
    GM_PLUGIN_CAP_BUTTON          = UINT32_C(1) << 1,
    GM_PLUGIN_CAP_IMU_EVENTS      = UINT32_C(1) << 2,
    GM_PLUGIN_CAP_IMU_RAW         = UINT32_C(1) << 3,
    GM_PLUGIN_CAP_BLUETOOTH       = UINT32_C(1) << 4,
    GM_PLUGIN_CAP_DEVICE_STATE    = UINT32_C(1) << 5,
    GM_PLUGIN_CAP_DISPLAY_CONTROL = UINT32_C(1) << 6,
    GM_PLUGIN_CAP_LOCALE          = UINT32_C(1) << 7,
};

#define GM_PLUGIN_LOCALE_TAG_MAX UINT32_C(16)

/* All public numeric values below are ABI-FROZEN. New values use extensions. */
typedef uint8_t gm_plugin_pixel_format_t;
enum {
    GM_PLUGIN_PIXEL_GRAY_4 = 1,
};

/* ABI-FROZEN core data layouts. Do not append, reorder or change fields. */
typedef struct {
    uint16_t width;        /**< Logical width in pixels. */
    uint16_t height;       /**< Logical height in pixels. */
    uint16_t refresh_hz;   /**< Nominal panel refresh rate in hertz. */
    gm_plugin_pixel_format_t pixel_format; /**< GM_PLUGIN_PIXEL_* encoding. */
    uint8_t logical_display_count; /**< Independently drawn logical displays. */
} gm_plugin_display_info_t;

typedef struct {
    int16_t x;       /**< Left edge in logical pixels. */
    int16_t y;       /**< Top edge in logical pixels. */
    uint16_t width;  /**< Non-zero rectangle width in pixels. */
    uint16_t height; /**< Non-zero rectangle height in pixels. */
} gm_plugin_rect_t;

/* One directly writable slice of the Host's existing framebuffer. No pixel
 * storage is allocated for this structure or for lock(): pixels points into
 * firmware-owned GRAY_4 memory. */
typedef struct {
    uint8_t *pixels; /**< First byte at logical coordinate (0, y). */
    uint16_t y;      /**< First logical display row represented by pixels. */
    uint16_t width;  /**< Writable width in pixels. */
    uint16_t height; /**< Writable row count in this synchronization slice. */
    uint16_t stride; /**< Byte distance between adjacent framebuffer rows. */
} gm_plugin_framebuffer_surface_t;

/* Direct framebuffer drawing maps one Host-selected synchronization slice at
 * a time. Drawing and scanout can alternate slices like a temporal double
 * buffer without allocating a second framebuffer.
 *
 * This low-level path is intended for specialized per-pixel renderers. Prefer
 * LVGL for normal text, controls and application UI. */
typedef struct gm_plugin_framebuffer_api {
    /**
     * Lock the framebuffer slice containing one logical display row.
     *
     * The returned pointer maps the firmware's existing GRAY_4 framebuffer;
     * draw into it directly and do not allocate another screen-sized buffer.
     * The call may wait briefly for scanout to finish consuming that slice;
     * it does not spin and allocates no memory.
     * To render a full frame, start with y=0, unlock the returned slice, then
     * repeat with `y = surface.y + surface.height` until display height. The
     * previously unlocked slice can be sent by SPI while the next is drawn.
     *
     * @warning This is direct framebuffer access, not a lock for LVGL. While
     *          this lock is held, call no function from graphics.lvgl, even
     *          for a non-overlapping object. LVGL has separate synchronization
     *          and may wait for this same slice, causing a stall or deadlock.
     *          Unlock before using LVGL and before returning from the current
     *          lifecycle/event/loop callback.
     *
     * @param y Any logical row in the slice to acquire. The Host chooses slice
     *        boundaries; plugins must use the returned y and height instead of
     *        hard-coding a half-screen size.
     * @param surface Non-NULL output receiving the mapped pointer and geometry.
     *        Its pixels pointer is writable only until the matching unlock().
     *        GRAY_4 stores the even x pixel in the high nibble and odd x pixel
     *        in the low nibble of each byte.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL for an invalid y or
     *         NULL output, GM_PLUGIN_ESTATE if a slice is already locked, or
     *         GM_PLUGIN_EBUSY if framebuffer synchronization is unavailable,
     *         for example while the display is shutting down.
     */
    gm_plugin_result_t (*lock)(uint16_t y,
                               gm_plugin_framebuffer_surface_t *surface);
    /**
     * Relinquish the mapped slice and optionally submit its modified rectangle.
     *
     * After this function returns, direct framebuffer access is no longer
     * locked and the plugin may call graphics.lvgl. A later LVGL redraw can
     * still overwrite directly drawn pixels in an overlapping display area.
     *
     * @param dirty Modified rectangle in absolute logical display coordinates.
     *        It must be fully contained in the locked surface. Pass NULL to
     *        release without sending pixels. The pointer returned by lock()
     *        becomes invalid for writes in every return path.
     * @param present true appends the display SYNC and presents every slice
     *        submitted since the previous presentation. For a multi-slice
     *        frame, pass false for every earlier slice and true only for the
     *        final slice. For an independent one-slice update, pass true.
     *        Ignored when dirty is NULL.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_ESTATE when no slice is
     *         locked, or GM_PLUGIN_EINVAL for an invalid/out-of-slice dirty
     *         rectangle. An invalid rectangle is still safely unlocked.
     */
    gm_plugin_result_t (*unlock)(const gm_plugin_rect_t *dirty, bool present);
} gm_plugin_framebuffer_api_t;

/* Core drawing services. LVGL and direct framebuffer drawing share this
 * namespace but keep separate synchronization models. They must never be
 * nested: framebuffer.lock(), LVGL call, framebuffer.unlock() is invalid. */
typedef struct gm_plugin_graphics_api {
    /** Host-owned immutable LVGL table, valid until plugin unload. Do not call
     *  it while a direct framebuffer surface is locked. */
    const struct gm_plugin_lvgl_api *lvgl;
    /** Zero-copy access to the firmware-owned GRAY_4 framebuffer. */
    gm_plugin_framebuffer_api_t framebuffer;
} gm_plugin_graphics_api_t;

typedef enum {
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_1 = 1,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_2 = 2,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_3 = 3,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_4 = 4,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_5 = 5,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_6 = 6,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_7 = 7,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_8 = 8,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_9 = 9,
    GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_10 = 10,
} gm_plugin_display_brightness_t;

typedef enum {
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_0 = 0,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_1 = 1,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_2 = 2,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_3 = 3,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_4 = 4,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_5 = 5,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_6 = 6,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_7 = 7,
    GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_8 = 8,
} gm_plugin_display_distance_t;

typedef enum {
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_0 = 0,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_1 = 1,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_2 = 2,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_3 = 3,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_4 = 4,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_5 = 5,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_6 = 6,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_7 = 7,
    GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_8 = 8,
} gm_plugin_display_height_t;

typedef struct gm_plugin_display_control_api {
    /** @return true when the display is powered on, otherwise false. */
    bool (*power_get)(void);
    /**
     * @param on true to power the display on; false to power it off.
     * @return GM_PLUGIN_OK when the request is accepted.
     */
    gm_plugin_result_t (*power_set)(bool on);
    /** @return The current fixed brightness level. */
    gm_plugin_display_brightness_t (*brightness_get)(void);
    /**
     * @param level One GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_* value.
     * @return GM_PLUGIN_OK on success or GM_PLUGIN_EINVAL for an unknown level.
     */
    gm_plugin_result_t (*brightness_set)(gm_plugin_display_brightness_t level);
    /** @return The current optical-distance level. */
    gm_plugin_display_distance_t (*distance_get)(void);
    /**
     * @param level One GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_* value.
     * @return GM_PLUGIN_OK on success or GM_PLUGIN_EINVAL for an unknown level.
     */
    gm_plugin_result_t (*distance_set)(gm_plugin_display_distance_t level);
    /** @return The current vertical display-position level. */
    gm_plugin_display_height_t (*height_get)(void);
    /**
     * @param level One GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_* value.
     * @return GM_PLUGIN_OK on success or GM_PLUGIN_EINVAL for an unknown level.
     */
    gm_plugin_result_t (*height_set)(gm_plugin_display_height_t level);
    /**
     * Temporarily block or release automatic brightness for this plugin.
     * The Host releases an outstanding block when the plugin stops.
     *
     * @param blocked true to block automatic changes; false to release them.
     * @return GM_PLUGIN_OK when the request is applied.
     */
    gm_plugin_result_t (*auto_brightness_block)(bool blocked);
} gm_plugin_display_control_api_t;

typedef struct {
    int16_t accel_raw[3];   /**< Native accelerometer X/Y/Z sample values. */
    int16_t gyro_raw[3];    /**< Native gyroscope X/Y/Z sample values. */
    int16_t temperature_raw; /**< Native, unconverted sensor temperature. */
    int16_t pitch_degrees;  /**< Current signed pitch angle in whole degrees. */
} gm_plugin_imu_sample_t;

/* ABI-FROZEN input and gesture IDs. Do not append, renumber or reuse IDs. */
typedef uint16_t gm_plugin_button_t;
enum {
    GM_PLUGIN_BUTTON_UNKNOWN = 0,
    GM_PLUGIN_BUTTON_PRIMARY = 1,
};

typedef uint16_t gm_plugin_button_action_t;
enum {
    GM_PLUGIN_BUTTON_ACTION_UNKNOWN = 0,
    GM_PLUGIN_BUTTON_ACTION_SINGLE = 1,
    GM_PLUGIN_BUTTON_ACTION_DOUBLE = 2,
    GM_PLUGIN_BUTTON_ACTION_LONG = 3,
    GM_PLUGIN_BUTTON_ACTION_VERY_LONG = 4,
    GM_PLUGIN_BUTTON_ACTION_RELEASE = 5,
};

typedef uint16_t gm_plugin_imu_gesture_t;
enum {
    GM_PLUGIN_IMU_GESTURE_NONE = 0,
    GM_PLUGIN_IMU_GESTURE_NOD = 1,
    GM_PLUGIN_IMU_GESTURE_HEAD_RAISE = 2,
    GM_PLUGIN_IMU_GESTURE_HEAD_LOWER = 3,
    GM_PLUGIN_IMU_GESTURE_SHAKE = 4,
    GM_PLUGIN_IMU_GESTURE_HEAD_RAISE_TIMEOUT = 5,
    GM_PLUGIN_IMU_GESTURE_HEAD_LOWER_TIMEOUT = 6,
    GM_PLUGIN_IMU_GESTURE_LEFT = 7,
    GM_PLUGIN_IMU_GESTURE_RIGHT = 8,
};

typedef uint8_t gm_plugin_imu_modes_t;
enum {
    GM_PLUGIN_IMU_ENABLE_NONE = 0,
    GM_PLUGIN_IMU_ENABLE_GESTURES = UINT8_C(1) << 0,
    GM_PLUGIN_IMU_ENABLE_RAW = UINT8_C(1) << 1,
};

/* Application-defined Bluetooth channel. The Host transports this value
 * unchanged; plugins should give their channel constants descriptive names. */
typedef uint16_t gm_plugin_bt_channel_t;

/* ABI-FROZEN event IDs. Removed IDs remain reserved and are never reused. */
typedef uint16_t gm_plugin_event_type_t;
enum {
    GM_PLUGIN_EVENT_BUTTON = 1,
    GM_PLUGIN_EVENT_IMU_GESTURE = 2,
    GM_PLUGIN_EVENT_BT_MESSAGE = 3,
    GM_PLUGIN_EVENT_CONNECTION = 4,
};

typedef struct {
    uint16_t struct_size; /**< Bytes available in this event structure. */
    gm_plugin_event_type_t type; /**< Selects the active data union member. */
    uint32_t timestamp_ms; /**< Wrapping monotonic event time in milliseconds. */
    union {
        struct {
            gm_plugin_button_t button; /**< GM_PLUGIN_BUTTON_* identifier. */
            gm_plugin_button_action_t action; /**< GM_PLUGIN_BUTTON_ACTION_*. */
        } button;
        struct {
            gm_plugin_imu_gesture_t gesture; /**< GM_PLUGIN_IMU_GESTURE_*. */
            bool active; /**< true on activation; false on gesture release. */
        } imu_gesture;
        struct {
            gm_plugin_bt_channel_t channel; /**< Sender-defined channel. */
            const uint8_t *data; /**< Borrowed payload; copy during callback. */
            uint32_t length; /**< Number of readable bytes at data. */
        } bt;
        struct {
            bool connected; /**< true after connect; false after disconnect. */
        } connection;
    } data;
} gm_plugin_event_t;

/* ABI-FROZEN 1.0 table. Never append/reorder/remove entries or change their
 * signatures or semantics. Add every future module through extension_get(). */
typedef struct gm_plugin_host_api {
    /** Total byte size of this Host table, including this field. */
    uint16_t struct_size;
    /** Packed GM_PLUGIN_VERSION() value implemented by the Host. */
    uint16_t abi_version;
    /** OR-ed GM_PLUGIN_CAP_* bits supported by this Host. */
    gm_plugin_capabilities_t capabilities;

    /**
     * Write one plugin log record using printf-compatible formatting.
     *
     * @param format Non-NULL printf format string. Its conversion specifiers
     *        must exactly match the following variable arguments. The Host
     *        consumes all arguments during this call and retains no pointer.
     */
    void (*log)(const char *format, ...);
    /**
     * @return Monotonic milliseconds as a wrapping uint32_t. Compute elapsed
     *         time with unsigned subtraction; this is not wall-clock time.
     */
    uint32_t (*monotonic_ms)(void);
    /**
     * Allocate from the Host heap.
     * @param size Requested byte count.
     * @return Suitably aligned memory, or NULL when allocation fails. The
     *         plugin owns the returned block and must release it with free().
     */
    void *(*alloc)(size_t size);
    /**
     * Release a block returned by this table's alloc().
     * @param memory Owned block to release, or NULL. It must not be used again.
     */
    void (*free)(void *memory);

    /**
     * Read the logical display geometry and pixel format.
     * @param info Non-NULL destination written completely by the Host.
     * @return GM_PLUGIN_OK on success or GM_PLUGIN_EINVAL if info is NULL.
     */
    gm_plugin_result_t (*display_get_info)(gm_plugin_display_info_t *info);
    /* Required core drawing APIs. Host-owned and valid until plugin unload.
     * The framebuffer path maps one synchronized Host-owned slice at a time. */
    gm_plugin_graphics_api_t graphics;
    gm_plugin_display_control_api_t display_control;

    /**
     * Send one application message to the connected phone over the plugin BT
     * service. The Host synchronously copies the payload before returning.
     *
     * @param channel Application-defined channel delivered unchanged to the
     *        phone. Channel allocation is part of the plugin/phone contract.
     * @param data Non-NULL payload bytes; ownership remains with the plugin.
     * @param length Non-zero payload length that fits one Host transport packet.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL for an invalid pointer,
     *         zero/oversized message, GM_PLUGIN_ENOMEM if packet construction
     *         fails, or GM_PLUGIN_EIO if the transport rejects the packet.
     */
    gm_plugin_result_t (*bt_send)(gm_plugin_bt_channel_t channel,
                                  const void *data, uint32_t length);
    /**
     * Select IMU services used by the plugin.
     * @param modes OR-ed GM_PLUGIN_IMU_ENABLE_* values; NONE disables both raw
     *        sampling and gesture delivery. The Host disables IMU on stop.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL for unknown bits, or
     *         GM_PLUGIN_EIO if the hardware service cannot change state.
     */
    gm_plugin_result_t (*imu_enable)(gm_plugin_imu_modes_t modes);
    /**
     * Copy the latest cached raw IMU sample without allocating or blocking.
     * @param sample Non-NULL destination written completely on success.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL if sample is NULL,
     *         GM_PLUGIN_ESTATE unless raw mode is enabled, or GM_PLUGIN_EBUSY
     *         when no sample is currently available.
     */
    gm_plugin_result_t (*imu_read)(gm_plugin_imu_sample_t *sample);

    /** @return Latest battery percentage in the inclusive range 0..100. */
    uint8_t (*battery_percent)(void);
    /** @return true when the battery is currently charging. */
    bool (*battery_charging)(void);
    /** @return true when the glasses are currently detected as worn. */
    bool (*wearing)(void);
    /**
     * Read the current UI language tag.
     * @param language_tag Non-NULL writable array of exactly
     *        GM_PLUGIN_LOCALE_TAG_MAX bytes. On success it contains a
     *        NUL-terminated tag. On GM_PLUGIN_ENOMEM its first byte is NUL.
     * @return GM_PLUGIN_OK on success, GM_PLUGIN_EINVAL for NULL,
     *         GM_PLUGIN_ESTATE if no current locale exists, or
     *         GM_PLUGIN_ENOMEM if the fixed output array is too small.
     */
    gm_plugin_result_t (*locale_get)(
        char language_tag[GM_PLUGIN_LOCALE_TAG_MAX]);

    /**
     * Request exit to the previous glasses application. This is asynchronous;
     * the current callback must return promptly and must not perform more UI
     * or service work after making the request.
     */
    void (*app_exit)(void);

    /**
     * Discover one optional extension table by registered ID.
     *
     * @param extension_id GM_PLUGIN_EXTENSION_* value from
     *        gm_plugin_extensions.h.
     * @param api Non-NULL output. On success receives a Host-owned immutable
     *        table valid until plugin unload; on failure receives NULL.
     * @return GM_PLUGIN_OK when found, GM_PLUGIN_EINVAL if api is NULL, or
     *         GM_PLUGIN_ENOTSUP when the Host does not provide the ID.
     */
    gm_plugin_result_t (*extension_get)(gm_plugin_extension_id_t extension_id,
                                        const void **api);
} gm_plugin_host_api_t;

/* ABI-FROZEN 1.0 lifecycle descriptor. Never change this layout after release. */
typedef struct gm_plugin_descriptor {
    /** Writable descriptor capacity set by the Host before gm_plugin_entry().
     *  The plugin validates and preserves this value. */
    uint16_t struct_size;
    /** Set to the ABI version required by this plugin. */
    uint16_t abi_version;
    /* Passed unchanged to every lifecycle callback. May be NULL. */
    void *context;

    /* Order: entry, on_load once, then zero or more start/resume/suspend/stop
     * cycles, then on_unload once. Callbacks are serialized on display task.
     * on_load runs before a display root exists; UI belongs in on_start. */
    /**
     * Allocate non-UI plugin resources.
     * @param context Descriptor context, possibly NULL.
     * @return GM_PLUGIN_OK to continue loading, or another GM_PLUGIN_* error
     *         to reject activation. The Host calls on_unload after this
     *         callback returns an error, so on_unload must also handle a
     *         partially initialized context. Do not release the same resources
     *         both here and in on_unload.
     */
    gm_plugin_result_t (*on_load)(void *context);
    /**
     * Create UI and start one visible application cycle.
     * @param context Descriptor context, possibly NULL.
     * @return GM_PLUGIN_OK to run, or another GM_PLUGIN_* error to abort start.
     *         On failure, release partial start/UI resources in this callback
     *         because on_stop is not called for a failed start.
     */
    gm_plugin_result_t (*on_start)(void *context);
    /** @param context Descriptor context, possibly NULL. Resume paused work. */
    void (*on_resume)(void *context);
    /**
     * Perform one short, non-blocking update iteration.
     * @param context Descriptor context, possibly NULL.
     * @param elapsed_ms Unsigned milliseconds since the preceding on_loop call.
     */
    void (*on_loop)(void *context, uint32_t elapsed_ms);
    /**
     * Handle one borrowed Host event synchronously.
     * @param context Descriptor context, possibly NULL.
     * @param event Non-NULL event valid only until this callback returns. Do not
     *        retain event or event->data.bt.data; copy needed bytes immediately.
     * @return true only if the plugin handled the event, otherwise false.
     */
    bool (*on_event)(void *context, const gm_plugin_event_t *event);
    /** @param context Descriptor context, possibly NULL. Pause active work. */
    void (*on_suspend)(void *context);
    /**
     * Stop the current visible cycle and release UI-cycle resources.
     * @param context Descriptor context, possibly NULL.
     */
    void (*on_stop)(void *context);
    /**
     * Release resources acquired by on_load before the image is unloaded.
     * This is also called when on_load returns an error and must therefore
     * safely clean up a partially initialized context.
     * @param context Descriptor context, possibly NULL.
     */
    void (*on_unload)(void *context);
} gm_plugin_descriptor_t;

/* Once released, core layouts are immutable. These constants validate the
 * complete 1.0 layouts; future functionality uses extension tables. */
#define GM_PLUGIN_MEMBER_END(type, member) \
    (offsetof(type, member) + sizeof(((type *)0)->member))
#define GM_PLUGIN_EVENT_MIN_SIZE ((uint16_t)sizeof(gm_plugin_event_t))
#define GM_PLUGIN_HOST_API_MIN_SIZE \
    ((uint16_t)GM_PLUGIN_MEMBER_END(gm_plugin_host_api_t, extension_get))
#define GM_PLUGIN_DESCRIPTOR_MIN_SIZE \
    ((uint16_t)GM_PLUGIN_MEMBER_END(gm_plugin_descriptor_t, on_unload))

#define GM_PLUGIN_STRUCT_HAS(object, type, member) \
    ((object) != NULL && (size_t)(object)->struct_size >= \
     GM_PLUGIN_MEMBER_END(type, member))

/**
 * Validate the Host ABI and populate the Host-initialized plugin descriptor.
 *
 * Entry must not allocate or acquire resources: a rejected descriptor has no
 * safe callback to release them. Acquire resources in on_load instead.
 *
 * @param host Non-NULL Host-owned immutable function table, valid until plugin
 *        unload. Validate struct_size, abi_version, capabilities, and every
 *        function pointer used by the plugin before storing it.
 * @param plugin Non-NULL output descriptor. The Host zeroes it and then sets
 *        struct_size to the writable capacity before this call. Validate and
 *        preserve struct_size; set abi_version, context, and the lifecycle
 *        callbacks used by the plugin.
 * @return GM_PLUGIN_OK when the descriptor is valid, GM_PLUGIN_EINVAL for bad
 *         pointers/layout, GM_PLUGIN_EVERSION for an incompatible ABI, or
 *         GM_PLUGIN_ENOTSUP when a required Host capability is absent.
 */
typedef gm_plugin_result_t (*gm_plugin_entry_fn)(
    const gm_plugin_host_api_t *host, gm_plugin_descriptor_t *plugin);

/* gm_plugin_entry_fn and the exported symbol have the identical contract. */
gm_plugin_result_t GM_PLUGIN_ENTRY_NAME(
    const gm_plugin_host_api_t *host, gm_plugin_descriptor_t *plugin);

#ifdef __cplusplus
}
#endif

#endif
