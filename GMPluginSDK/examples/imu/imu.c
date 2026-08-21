#include "gm_plugin.h"

static const gm_plugin_host_api_t *s_host;
static uint32_t s_elapsed_ms;

static gm_plugin_result_t imu_start(void *context)
{
    (void)context;
    s_elapsed_ms = 0;
    return s_host->imu_enable(GM_PLUGIN_IMU_ENABLE_GESTURES |
                              GM_PLUGIN_IMU_ENABLE_RAW);
}

static void imu_loop(void *context, uint32_t elapsed_ms)
{
    gm_plugin_imu_sample_t sample;
    (void)context;
    s_elapsed_ms += elapsed_ms;
    if (s_elapsed_ms < 250U) return;
    s_elapsed_ms = 0;
    if (s_host->imu_read(&sample) == GM_PLUGIN_OK)
        s_host->log("imu pitch=%d gyro=%d,%d,%d", sample.pitch_degrees,
                    sample.gyro_raw[0], sample.gyro_raw[1], sample.gyro_raw[2]);
}

static bool imu_event(void *context, const gm_plugin_event_t *event)
{
    (void)context;
    if (event == 0 || event->type != GM_PLUGIN_EVENT_IMU_GESTURE) return false;
    s_host->log("imu gesture=%u active=%u", event->data.imu_gesture.gesture,
                event->data.imu_gesture.active);
    return true;
}

static void imu_stop(void *context)
{
    (void)context;
    (void)s_host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_IMU_EVENTS |
                                               GM_PLUGIN_CAP_IMU_RAW;
    if (host == 0 || plugin == 0 || host->imu_enable == 0 ||
        host->imu_read == 0 || host->log == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required)
        return GM_PLUGIN_ENOTSUP;
    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = imu_start;
    plugin->on_loop = imu_loop;
    plugin->on_event = imu_event;
    plugin->on_stop = imu_stop;
    return GM_PLUGIN_OK;
}
