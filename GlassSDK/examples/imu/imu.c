#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"
#include "head_sprites.h"

#define SAMPLE_INTERVAL_MS 50U
#define LOG_INTERVAL_MS 250U
#define VERTICAL_MOTION_THRESHOLD 55
#define HORIZONTAL_MOTION_THRESHOLD 60
#define HORIZONTAL_POSE_DIVISOR 7
#define POSE_LIMIT 30
#define POSE_ENTER_THRESHOLD 9
#define POSE_EXIT_THRESHOLD 5
#define HEAD_SPRITE_ROW_BYTES ((HEAD_SPRITE_WIDTH + 1U) / 2U)

static const gm_plugin_host_api_t *s_host;
static const gm_plugin_lvgl_api_t *s_lvgl;
static const gm_plugin_libc_extension_api_t *s_libc;
static gm_plugin_display_info_t s_display;
static gm_plugin_lvgl_obj_t *s_motion_label;
static gm_plugin_lvgl_obj_t *s_pitch_label;
static gm_plugin_lvgl_obj_t *s_gyro_label;
static gm_plugin_lvgl_obj_t *s_accel_label;
static gm_plugin_lvgl_obj_t *s_gesture_label;
static int16_t s_visual_yaw;
static int16_t s_visual_pitch;
static uint8_t s_sprite_row;
static uint8_t s_sprite_column;
static int16_t s_sprite_x;
static int16_t s_sprite_y;
static uint8_t s_sprite_pixels[HEAD_SPRITE_ROW_BYTES * HEAD_SPRITE_HEIGHT];
static uint32_t s_sample_elapsed_ms;
static uint32_t s_log_elapsed_ms;
static bool s_sprite_dirty;

static int32_t clamp_value(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int16_t smooth_toward(int16_t current, int16_t target)
{
    int32_t delta = (int32_t)target - current;
    if (delta > 0) return (int16_t)(current + (delta + 2) / 3);
    if (delta < 0) return (int16_t)(current - ((-delta) + 2) / 3);
    return current;
}

static const char *motion_text(const gm_plugin_imu_sample_t *sample)
{
    int32_t horizontal = (int32_t)sample->gyro_raw[0] -
                         sample->gyro_raw[1];
    int32_t abs_horizontal = horizontal < 0 ? -horizontal : horizontal;
    int32_t gyro_z = sample->gyro_raw[2];
    int32_t abs_z = gyro_z < 0 ? -gyro_z : gyro_z;
    if (abs_z >= VERTICAL_MOTION_THRESHOLD && abs_z > abs_horizontal)
        return gyro_z > 0 ? "RAISING HEAD" : "LOWERING HEAD";
    if (abs_horizontal >= HORIZONTAL_MOTION_THRESHOLD &&
        abs_horizontal > abs_z)
        return horizontal < 0 ? "TURNING LEFT" : "TURNING RIGHT";
    return "HEAD STILL";
}

static const char *gesture_text(gm_plugin_imu_gesture_t gesture)
{
    switch (gesture) {
    case GM_PLUGIN_IMU_GESTURE_NOD:
        return "NOD";
    case GM_PLUGIN_IMU_GESTURE_HEAD_RAISE:
        return "HEAD RAISE";
    case GM_PLUGIN_IMU_GESTURE_HEAD_LOWER:
        return "HEAD LOWER";
    case GM_PLUGIN_IMU_GESTURE_SHAKE:
        return "SHAKE";
    case GM_PLUGIN_IMU_GESTURE_HEAD_RAISE_TIMEOUT:
        return "RAISE TIMEOUT";
    case GM_PLUGIN_IMU_GESTURE_HEAD_LOWER_TIMEOUT:
        return "LOWER TIMEOUT";
    case GM_PLUGIN_IMU_GESTURE_LEFT:
        return "LEFT";
    case GM_PLUGIN_IMU_GESTURE_RIGHT:
        return "RIGHT";
    default:
        return "UNKNOWN";
    }
}

static void set_axis_text(gm_plugin_lvgl_obj_t *label, const char *prefix,
                          const int16_t axes[3])
{
    char text[64];
    s_libc->snprintf(text, sizeof(text), "%s X %+d  Y %+d  Z %+d",
                          prefix, (int)axes[0], (int)axes[1], (int)axes[2]);
    s_lvgl->label_set_text(label, text);
}

static uint8_t select_pose_axis(int16_t value, uint8_t current)
{
    if (current == 0U) {
        if (value <= -POSE_EXIT_THRESHOLD) return 0;
        return 1;
    }
    if (current == 2U) {
        if (value >= POSE_EXIT_THRESHOLD) return 2;
        return 1;
    }
    if (value <= -POSE_ENTER_THRESHOLD) return 0;
    if (value >= POSE_ENTER_THRESHOLD) return 2;
    return 1;
}

static void decode_sprite(void)
{
    uint8_t frame = (uint8_t)(s_sprite_row * 3U + s_sprite_column);
    const uint8_t *encoded = head_sprites[frame];
    uint32_t encoded_size = head_sprite_sizes[frame];
    uint32_t offset = 0U;
    uint32_t pixel = 0U;
    const uint32_t pixel_count = HEAD_SPRITE_WIDTH * HEAD_SPRITE_HEIGHT;
    while (offset + 1U < encoded_size && pixel < pixel_count) {
        uint32_t run = encoded[offset++];
        uint8_t gray = (uint8_t)(encoded[offset++] & 0x0FU);
        uint8_t packed = (uint8_t)((gray << 4) | gray);
        uint32_t bytes;
        if (run > pixel_count - pixel) run = pixel_count - pixel;
        if ((pixel & 1U) != 0U && run != 0U) {
            s_sprite_pixels[pixel >> 1] |= gray;
            ++pixel;
            --run;
        }
        bytes = run >> 1;
        if (bytes != 0U) {
            s_libc->memset(s_sprite_pixels + (pixel >> 1), packed, bytes);
            pixel += bytes << 1;
            run &= 1U;
        }
        if (run != 0U) {
            s_sprite_pixels[pixel >> 1] = (uint8_t)(gray << 4);
            ++pixel;
        }
    }
    if ((pixel & 1U) != 0U) {
        s_sprite_pixels[pixel >> 1] &= 0xF0U;
        ++pixel;
    }
    if (pixel < pixel_count)
        s_libc->memset(s_sprite_pixels + (pixel >> 1), 0,
                       (pixel_count - pixel) >> 1);
}

static gm_plugin_result_t render_sprite(void)
{
    uint16_t next_y = (uint16_t)s_sprite_y;
    uint16_t sprite_bottom = (uint16_t)(s_sprite_y + HEAD_SPRITE_HEIGHT);
    while (next_y < sprite_bottom) {
        gm_plugin_framebuffer_surface_t surface;
        gm_plugin_rect_t dirty;
        uint32_t surface_end;
        uint16_t draw_top;
        uint16_t draw_bottom;
        uint16_t row;
        gm_plugin_result_t result =
            s_host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) return result;
        surface_end = (uint32_t)surface.y + surface.height;
        if (surface.pixels == 0 || surface.height == 0U ||
            surface.y > next_y || surface_end <= next_y ||
            surface_end > s_display.height ||
            (uint32_t)s_sprite_x + HEAD_SPRITE_WIDTH > surface.width ||
            surface.stride < (surface.width + 1U) / 2U) {
            (void)s_host->graphics.framebuffer.unlock(0, false);
            return GM_PLUGIN_ESTATE;
        }
        draw_top = surface.y > (uint16_t)s_sprite_y
            ? surface.y : (uint16_t)s_sprite_y;
        draw_bottom = (uint16_t)surface_end;
        if (draw_bottom > sprite_bottom) draw_bottom = sprite_bottom;
        if (draw_top >= draw_bottom) {
            (void)s_host->graphics.framebuffer.unlock(0, false);
            return GM_PLUGIN_ESTATE;
        }
        for (row = draw_top; row < draw_bottom; ++row) {
            const uint8_t *source = s_sprite_pixels +
                (uint32_t)(row - (uint16_t)s_sprite_y) *
                HEAD_SPRITE_ROW_BYTES;
            uint8_t *target = surface.pixels +
                (uint32_t)(row - surface.y) * surface.stride +
                ((uint16_t)s_sprite_x >> 1);
            s_libc->memcpy(target, source, HEAD_SPRITE_ROW_BYTES);
        }
        dirty.x = s_sprite_x;
        dirty.y = (int16_t)draw_top;
        dirty.width = HEAD_SPRITE_WIDTH;
        dirty.height = (uint16_t)(draw_bottom - draw_top);
        result = s_host->graphics.framebuffer.unlock(
            &dirty, draw_bottom == sprite_bottom);
        if (result != GM_PLUGIN_OK) return result;
        next_y = draw_bottom;
    }
    return GM_PLUGIN_OK;
}

static void update_pose(const gm_plugin_imu_sample_t *sample)
{
    int32_t horizontal = (int32_t)sample->gyro_raw[0] -
                         sample->gyro_raw[1];
    int32_t gyro_z = sample->gyro_raw[2];
    int16_t target_yaw = (int16_t)(
        clamp_value(horizontal, -150, 150) / HORIZONTAL_POSE_DIVISOR);
    int16_t target_pitch = (int16_t)(
        -clamp_value(sample->pitch_degrees, -30, 30) -
        clamp_value(gyro_z, -120, 120) / 8);
    uint8_t next_row;
    uint8_t next_column;
    target_pitch = (int16_t)clamp_value(target_pitch, -POSE_LIMIT, POSE_LIMIT);
    s_visual_yaw = smooth_toward(s_visual_yaw, target_yaw);
    s_visual_pitch = smooth_toward(s_visual_pitch, target_pitch);
    next_row = select_pose_axis(s_visual_pitch, s_sprite_row);
    next_column = select_pose_axis(s_visual_yaw, s_sprite_column);
    if (next_row != s_sprite_row || next_column != s_sprite_column) {
        s_sprite_row = next_row;
        s_sprite_column = next_column;
        s_sprite_dirty = true;
    }
}

static void update_sample(const gm_plugin_imu_sample_t *sample)
{
    char text[32];
    s_lvgl->label_set_text(s_motion_label, motion_text(sample));
    s_libc->snprintf(text, sizeof(text), "Pitch %+d deg",
                          (int)sample->pitch_degrees);
    s_lvgl->label_set_text(s_pitch_label, text);
    set_axis_text(s_gyro_label, "Gyro", sample->gyro_raw);
    set_axis_text(s_accel_label, "Accel", sample->accel_raw);
    update_pose(sample);
}

static bool create_ui(void)
{
    gm_plugin_lvgl_obj_t *root = s_lvgl->root_get();
    gm_plugin_lvgl_obj_t *title;
    gm_plugin_lvgl_style_value_t large_font = {0};
    if (root == 0) return false;
    title = s_lvgl->label_create(root);
    s_motion_label = s_lvgl->label_create(root);
    s_pitch_label = s_lvgl->label_create(root);
    s_gyro_label = s_lvgl->label_create(root);
    s_accel_label = s_lvgl->label_create(root);
    s_gesture_label = s_lvgl->label_create(root);
    if (title == 0 || s_motion_label == 0 || s_pitch_label == 0 ||
        s_gyro_label == 0 || s_accel_label == 0 || s_gesture_label == 0) {
        s_lvgl->obj_clean(root);
        return false;
    }
    s_lvgl->label_set_text(title, "IMU HEAD TRACKING");
    s_lvgl->label_set_text(s_motion_label, "WAITING FOR SAMPLE");
    s_lvgl->label_set_text(s_pitch_label, "Pitch +0 deg");
    s_lvgl->label_set_text(s_gyro_label, "Gyro X +0  Y +0  Z +0");
    s_lvgl->label_set_text(s_accel_label, "Accel X +0  Y +0  Z +0");
    s_lvgl->label_set_text(s_gesture_label, "Gesture: none");
    large_font.ptr = s_lvgl->font_large;
    s_lvgl->style_set(s_motion_label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
                      large_font, GM_PLUGIN_LVGL_SELECTOR_MAIN);
    s_lvgl->obj_align(title, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 18);
    s_lvgl->obj_align(s_motion_label, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 55);
    s_lvgl->obj_align(s_pitch_label, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 92);
    s_lvgl->obj_align(s_gyro_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 90, 0);
    s_lvgl->obj_align(s_accel_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 90, 38);
    s_lvgl->obj_align(s_gesture_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 90, 76);
    return true;
}

static gm_plugin_result_t imu_start(void *context)
{
    gm_plugin_result_t result;
    (void)context;
    s_sample_elapsed_ms = SAMPLE_INTERVAL_MS;
    s_log_elapsed_ms = 0;
    s_visual_yaw = 0;
    s_visual_pitch = 0;
    s_sprite_row = 1;
    s_sprite_column = 1;
    s_sprite_dirty = true;
    if (s_host->display_get_info(&s_display) != GM_PLUGIN_OK ||
        s_display.pixel_format != GM_PLUGIN_PIXEL_GRAY_4 ||
        s_display.width < HEAD_SPRITE_WIDTH ||
        s_display.height < HEAD_SPRITE_HEIGHT)
        return GM_PLUGIN_ENOTSUP;
    s_sprite_x = s_display.width >= HEAD_SPRITE_WIDTH + 64U ? 32 : 0;
    s_sprite_y = s_display.height >= HEAD_SPRITE_HEIGHT + 135U
        ? 135 : (int16_t)(s_display.height - HEAD_SPRITE_HEIGHT);
    if (!create_ui()) return GM_PLUGIN_ENOMEM;
    result = s_host->imu_enable(GM_PLUGIN_IMU_ENABLE_GESTURES |
                                GM_PLUGIN_IMU_ENABLE_RAW);
    if (result != GM_PLUGIN_OK) s_lvgl->obj_clean(s_lvgl->root_get());
    return result;
}

static void imu_loop(void *context, uint32_t elapsed_ms)
{
    gm_plugin_imu_sample_t sample;
    bool sample_updated = false;
    (void)context;
    if (s_sample_elapsed_ms < SAMPLE_INTERVAL_MS) {
        uint32_t remaining = SAMPLE_INTERVAL_MS - s_sample_elapsed_ms;
        s_sample_elapsed_ms = elapsed_ms >= remaining
            ? SAMPLE_INTERVAL_MS : s_sample_elapsed_ms + elapsed_ms;
    }
    if (s_log_elapsed_ms < LOG_INTERVAL_MS) {
        uint32_t remaining = LOG_INTERVAL_MS - s_log_elapsed_ms;
        s_log_elapsed_ms = elapsed_ms >= remaining
            ? LOG_INTERVAL_MS : s_log_elapsed_ms + elapsed_ms;
    }
    if (s_sample_elapsed_ms >= SAMPLE_INTERVAL_MS) {
        s_sample_elapsed_ms = 0;
        if (s_host->imu_read(&sample) == GM_PLUGIN_OK) {
            update_sample(&sample);
            sample_updated = true;
        }
    }
    if (s_sprite_dirty) {
        decode_sprite();
        if (render_sprite() == GM_PLUGIN_OK) s_sprite_dirty = false;
    }
    if (sample_updated && s_log_elapsed_ms >= LOG_INTERVAL_MS) {
        s_log_elapsed_ms = 0;
        s_host->log("imu pitch=%d accel=%d,%d,%d gyro=%d,%d,%d",
                    sample.pitch_degrees, sample.accel_raw[0],
                    sample.accel_raw[1], sample.accel_raw[2],
                    sample.gyro_raw[0], sample.gyro_raw[1],
                    sample.gyro_raw[2]);
    }
}

static bool imu_event(void *context, const gm_plugin_event_t *event)
{
    char text[48];
    (void)context;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_IMU_GESTURE) return false;
    s_libc->snprintf(
        text, sizeof(text), "Gesture: %s %s",
        gesture_text(event->data.imu_gesture.gesture),
        event->data.imu_gesture.active ? "active" : "released");
    s_lvgl->label_set_text(s_gesture_label, text);
    s_host->log("imu gesture=%u active=%u",
                (unsigned int)event->data.imu_gesture.gesture,
                (unsigned int)event->data.imu_gesture.active);
    return true;
}

static void imu_stop(void *context)
{
    (void)context;
    (void)s_host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    s_lvgl->obj_clean(s_lvgl->root_get());
    s_motion_label = 0;
    s_pitch_label = 0;
    s_gyro_label = 0;
    s_accel_label = 0;
    s_gesture_label = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_DISPLAY_BITMAP |
                                               GM_PLUGIN_CAP_IMU_EVENTS |
                                               GM_PLUGIN_CAP_IMU_RAW;
    if (host == 0 || plugin == 0 || host->imu_enable == 0 ||
        host->imu_read == 0 || host->log == 0 || host->display_get_info == 0 ||
        host->graphics.lvgl == 0 ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required)
        return GM_PLUGIN_ENOTSUP;
    s_host = host;
    s_lvgl = host->graphics.lvgl;
    if (gm_plugin_libc_get(host, &s_libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    if (s_lvgl->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(s_lvgl->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = imu_start;
    plugin->on_loop = imu_loop;
    plugin->on_event = imu_event;
    plugin->on_stop = imu_stop;
    return GM_PLUGIN_OK;
}
