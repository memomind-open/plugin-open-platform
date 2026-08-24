#include "gm_plugin.h"
#include "gm_plugin_extensions.h"
#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_package.h"
#include "gm_plugin_protocol.h"

#define ABI_CHECK(name, condition) typedef char name[(condition) ? 1 : -1]

/* This SDK targets RV32. Published values and prefix sizes must never move. */
ABI_CHECK(gm_abi_pointer_is_32_bit, sizeof(void *) == 4U);
ABI_CHECK(gm_abi_initial_value,
          GM_PLUGIN_ABI_MIN_VERSION == UINT16_C(0x0100));
ABI_CHECK(gm_lvgl_initial_version,
          GM_PLUGIN_LVGL_API_MIN_VERSION == UINT16_C(0x0100));
ABI_CHECK(gm_lvgl_selector_indicator,
          GM_PLUGIN_LVGL_SELECTOR_INDICATOR == UINT32_C(0x020000));
ABI_CHECK(gm_lvgl_selector_knob,
          GM_PLUGIN_LVGL_SELECTOR_KNOB == UINT32_C(0x030000));
ABI_CHECK(gm_package_v1_value, GM_PLUGIN_PACKAGE_FORMAT_VERSION == 1U);
ABI_CHECK(gm_package_v1_header_size, GM_PLUGIN_PACKAGE_HEADER_SIZE == 28U);
ABI_CHECK(gm_plugin_service_id, GM_PLUGIN_SERVICE_ID == UINT8_C(0x0F));
ABI_CHECK(gm_plugin_downlink_command,
          GM_PLUGIN_COMMAND_PHONE_TO_GLASSES == UINT8_C(0x28));
ABI_CHECK(gm_plugin_uplink_command,
          GM_PLUGIN_COMMAND_GLASSES_TO_PHONE == UINT8_C(0x29));

ABI_CHECK(gm_cap_display_bitmap, GM_PLUGIN_CAP_DISPLAY_BITMAP == (1U << 0));
ABI_CHECK(gm_cap_button, GM_PLUGIN_CAP_BUTTON == (1U << 1));
ABI_CHECK(gm_cap_imu_events, GM_PLUGIN_CAP_IMU_EVENTS == (1U << 2));
ABI_CHECK(gm_cap_imu_raw, GM_PLUGIN_CAP_IMU_RAW == (1U << 3));
ABI_CHECK(gm_cap_bluetooth, GM_PLUGIN_CAP_BLUETOOTH == (1U << 4));
ABI_CHECK(gm_cap_device_state, GM_PLUGIN_CAP_DEVICE_STATE == (1U << 5));
ABI_CHECK(gm_cap_display_control, GM_PLUGIN_CAP_DISPLAY_CONTROL == (1U << 6));
ABI_CHECK(gm_cap_locale, GM_PLUGIN_CAP_LOCALE == (1U << 7));

ABI_CHECK(gm_pixel_gray4, GM_PLUGIN_PIXEL_GRAY_4 == 1);
ABI_CHECK(gm_brightness_first, GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_1 == 1);
ABI_CHECK(gm_brightness_last, GM_PLUGIN_DISPLAY_BRIGHTNESS_LEVEL_10 == 10);
ABI_CHECK(gm_distance_first, GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_0 == 0);
ABI_CHECK(gm_distance_last, GM_PLUGIN_DISPLAY_DISTANCE_LEVEL_8 == 8);
ABI_CHECK(gm_height_first, GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_0 == 0);
ABI_CHECK(gm_height_last, GM_PLUGIN_DISPLAY_HEIGHT_LEVEL_8 == 8);

ABI_CHECK(gm_event_button, GM_PLUGIN_EVENT_BUTTON == 1);
ABI_CHECK(gm_event_imu, GM_PLUGIN_EVENT_IMU_GESTURE == 2);
ABI_CHECK(gm_event_bt, GM_PLUGIN_EVENT_BT_MESSAGE == 3);
ABI_CHECK(gm_event_connection, GM_PLUGIN_EVENT_CONNECTION == 4);

ABI_CHECK(gm_display_info_initial_size,
          sizeof(gm_plugin_display_info_t) == 8U);
ABI_CHECK(gm_framebuffer_surface_initial_size,
          sizeof(gm_plugin_framebuffer_surface_t) == 12U);
ABI_CHECK(gm_framebuffer_api_initial_size,
          sizeof(gm_plugin_framebuffer_api_t) == 8U);
ABI_CHECK(gm_graphics_api_initial_size,
          sizeof(gm_plugin_graphics_api_t) == 12U);
ABI_CHECK(gm_display_control_api_initial_size,
          sizeof(gm_plugin_display_control_api_t) == 36U);
ABI_CHECK(gm_pixel_format_is_compact, sizeof(gm_plugin_pixel_format_t) == 1U);
ABI_CHECK(gm_extension_id_is_fixed,
          sizeof(gm_plugin_extension_id_t) == 4U);
ABI_CHECK(gm_lz4_extension_id_is_fixed,
          GM_PLUGIN_EXTENSION_LZ4 == UINT32_C(2));
ABI_CHECK(gm_lz4_extension_initial_size,
          sizeof(gm_plugin_lz4_extension_api_t) == 12U);
ABI_CHECK(gm_button_is_compact, sizeof(gm_plugin_button_t) == 2U);
ABI_CHECK(gm_button_action_is_compact,
          sizeof(gm_plugin_button_action_t) == 2U);
ABI_CHECK(gm_imu_gesture_is_compact, sizeof(gm_plugin_imu_gesture_t) == 2U);
ABI_CHECK(gm_bt_channel_is_compact, sizeof(gm_plugin_bt_channel_t) == 2U);
ABI_CHECK(gm_event_type_is_compact, sizeof(gm_plugin_event_type_t) == 2U);
ABI_CHECK(gm_event_initial_size, GM_PLUGIN_EVENT_MIN_SIZE == 20U);
ABI_CHECK(gm_host_initial_size, GM_PLUGIN_HOST_API_MIN_SIZE == 112U);
ABI_CHECK(gm_descriptor_initial_size,
          GM_PLUGIN_DESCRIPTOR_MIN_SIZE == 40U);
ABI_CHECK(gm_lvgl_initial_size, GM_PLUGIN_LVGL_API_MIN_SIZE == 116U);
