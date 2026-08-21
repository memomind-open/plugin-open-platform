#include "gm_plugin.h"

static const gm_plugin_host_api_t *s_host;

static void set_gray4(gm_plugin_framebuffer_surface_t *surface,
                      uint16_t x, uint16_t y, uint8_t gray)
{
    uint8_t *pixel = surface->pixels +
        (uint32_t)(y - surface->y) * surface->stride + (x >> 1);
    if ((x & 1U) == 0U)
        *pixel = (uint8_t)((*pixel & 0x0FU) | ((gray & 0x0FU) << 4));
    else
        *pixel = (uint8_t)((*pixel & 0xF0U) | (gray & 0x0FU));
}

static void draw_slice_marker(gm_plugin_framebuffer_surface_t *surface,
                              const gm_plugin_rect_t *dirty)
{
    uint16_t x;
    uint16_t y;

    for (y = (uint16_t)dirty->y;
         y < (uint16_t)(dirty->y + dirty->height); ++y) {
        for (x = (uint16_t)dirty->x;
             x < (uint16_t)(dirty->x + dirty->width); ++x) {
            uint8_t gray = (uint8_t)(((x >> 4) + (y >> 3)) & 1U
                ? 15U : 4U);
            set_gray4(surface, x, y, gray);
        }
    }
}

static gm_plugin_result_t framebuffer_draw(void)
{
    gm_plugin_display_info_t display;
    gm_plugin_framebuffer_surface_t surface;
    gm_plugin_result_t result;
    uint16_t next_y = 0;
    if (s_host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width == 0U || display.height == 0U) {
        s_host->log("framebuffer: invalid display information\n");
        return GM_PLUGIN_ENOTSUP;
    }

    while (next_y < display.height) {
        uint16_t slice_end;
        uint16_t marker_height;
        gm_plugin_rect_t dirty;

        result = s_host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) {
            s_host->log("framebuffer: lock y=%u failed: %d\n",
                        (unsigned int)next_y, (int)result);
            return result;
        }

        slice_end = (uint16_t)(surface.y + surface.height);
        if (surface.pixels == 0 || surface.width < display.width ||
            surface.height == 0U || surface.y > next_y ||
            slice_end <= next_y || slice_end > display.height) {
            (void)s_host->graphics.framebuffer.unlock(0, false);
            s_host->log("framebuffer: invalid surface y=%u height=%u\n",
                        (unsigned int)surface.y,
                        (unsigned int)surface.height);
            return GM_PLUGIN_ESTATE;
        }

        /* Draw a small stripe in every Host-selected synchronization slice.
         * The plugin never assumes where a slice boundary is. */
        marker_height = surface.height < 16U ? surface.height : 16U;
        dirty.x = 0;
        dirty.y = (int16_t)(surface.y +
                            (surface.height - marker_height) / 2U);
        dirty.width = display.width;
        dirty.height = marker_height;
        draw_slice_marker(&surface, &dirty);

        /* Earlier slices are queued without display SYNC. SPI can consume
         * them while the plugin draws the next slice. Only the final slice
         * presents the complete update. */
        result = s_host->graphics.framebuffer.unlock(
            &dirty, slice_end == display.height);
        if (result != GM_PLUGIN_OK) {
            s_host->log("framebuffer: unlock y=%u failed: %d\n",
                        (unsigned int)dirty.y, (int)result);
            return result;
        }
        next_y = slice_end;
    }

    s_host->log("framebuffer: checker stripes displayed\n");
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t framebuffer_start(void *context)
{
    (void)context;
    return framebuffer_draw();
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    if (host == 0 || plugin == 0 ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 ||
        host->display_get_info == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & GM_PLUGIN_CAP_DISPLAY_BITMAP) == 0U)
        return GM_PLUGIN_ENOTSUP;
    s_host = host;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->on_start = framebuffer_start;
    return GM_PLUGIN_OK;
}
