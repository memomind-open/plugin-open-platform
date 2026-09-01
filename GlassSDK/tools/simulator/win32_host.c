#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gm_plugin_protocol.h"
#include "simulator.h"

#define WINDOW_SCALE 2
#define PANEL_WIDTH 310
#define SLICE_HEIGHT 30U
#define INPUT_CHANNEL UINT16_C(0x4647)
#define EVENT_CHANNEL UINT16_C(0x4648)
#define INPUT_VERSION 2U
#define KEY_LEFT UINT16_C(1)
#define KEY_RIGHT UINT16_C(2)
#define KEY_UP UINT16_C(4)
#define KEY_DOWN UINT16_C(8)
#define KEY_LIGHT UINT16_C(16)
#define KEY_HEAVY UINT16_C(32)
#define KEY_KICK UINT16_C(64)
#define KEY_BLOCK UINT16_C(128)
#define KEY_START UINT16_C(256)
#define KEY_PAUSE UINT16_C(512)
#define KEY_UPPERCUT UINT16_C(1024)
#define KEY_SWEEP UINT16_C(2048)
#define FIGHT_EVENT_HIT 1U
#define FIGHT_EVENT_ATTACK 6U

typedef struct {
    uint8_t framebuffer[SIM_DISPLAY_HEIGHT][SIM_DISPLAY_WIDTH / 2U];
    uint32_t display_pixels[SIM_DISPLAY_WIDTH * SIM_DISPLAY_HEIGHT];
    gm_plugin_host_api_t host;
    gm_plugin_descriptor_t plugin;
    uint32_t clock_ms;
    uint32_t present_count;
    uint16_t input;
    uint16_t locked_y;
    uint16_t locked_height;
    uint8_t sequence;
    uint8_t speed_index;
    bool locked;
    bool running;
    bool paused;
    bool overlay;
    bool exit_requested;
    HWND window;
    LARGE_INTEGER frequency;
    LARGE_INTEGER last_counter;
    double real_accumulator_ms;
} simulator_t;

static simulator_t simulator;
static unsigned int fight_event_counts[16];
static unsigned int fight_event_failures;
static const double speed_values[] = {0.25, 0.5, 1.0, 2.0};
static const char *screen_names[] = {
    "TITLE", "DIFFICULTY", "UNKNOWN", "INTRO", "FIGHT",
    "ROUND OVER", "ENDING", "GAME OVER"
};
static const char *attack_names[] = {
    "NONE", "LIGHT", "HEAVY", "LIGHT KICK", "SPECIAL", "COMBO",
    "HEAVY KICK"
};
static const char *phase_names[] = {
    "IDLE", "STARTUP", "ACTIVE", "RECOVERY"
};

static void host_log(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
}

static uint32_t host_monotonic_ms(void)
{
    return simulator.clock_ms;
}

static gm_plugin_result_t host_display_get_info(gm_plugin_display_info_t *info)
{
    if (info == NULL) return GM_PLUGIN_EINVAL;
    info->width = SIM_DISPLAY_WIDTH;
    info->height = SIM_DISPLAY_HEIGHT;
    info->refresh_hz = 20U;
    info->pixel_format = GM_PLUGIN_PIXEL_GRAY_4;
    info->logical_display_count = 1U;
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t host_framebuffer_lock(
    uint16_t y, gm_plugin_framebuffer_surface_t *surface)
{
    uint16_t slice_y;
    uint16_t height;
    if (surface == NULL || y >= SIM_DISPLAY_HEIGHT) return GM_PLUGIN_EINVAL;
    if (simulator.locked) return GM_PLUGIN_ESTATE;
    slice_y = (uint16_t)(y / SLICE_HEIGHT * SLICE_HEIGHT);
    height = SLICE_HEIGHT;
    if ((uint32_t)slice_y + height > SIM_DISPLAY_HEIGHT)
        height = (uint16_t)(SIM_DISPLAY_HEIGHT - slice_y);
    surface->pixels = simulator.framebuffer[slice_y];
    surface->y = slice_y;
    surface->width = SIM_DISPLAY_WIDTH;
    surface->height = height;
    surface->stride = SIM_DISPLAY_WIDTH / 2U;
    simulator.locked = true;
    simulator.locked_y = slice_y;
    simulator.locked_height = height;
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t host_framebuffer_unlock(
    const gm_plugin_rect_t *dirty, bool present)
{
    bool valid = true;
    if (!simulator.locked) return GM_PLUGIN_ESTATE;
    if (dirty != NULL) {
        valid = dirty->width != 0U && dirty->height != 0U &&
            dirty->x >= 0 && dirty->y >= (int16_t)simulator.locked_y &&
            (uint32_t)dirty->x + dirty->width <= SIM_DISPLAY_WIDTH &&
            (uint32_t)dirty->y + dirty->height <=
                (uint32_t)simulator.locked_y + simulator.locked_height;
    }
    simulator.locked = false;
    if (!valid) return GM_PLUGIN_EINVAL;
    if (dirty != NULL && present) {
        ++simulator.present_count;
        if (simulator.window != NULL)
            InvalidateRect(simulator.window, NULL, FALSE);
    }
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t host_bt_send(gm_plugin_bt_channel_t channel,
                                       const void *data, uint32_t length)
{
    const uint8_t *bytes = data;
    if (data == NULL || length == 0U) return GM_PLUGIN_EINVAL;
    fprintf(stderr, "plugin uplink: service=0x%02x command=0x%02x\n",
            (unsigned int)GM_PLUGIN_SERVICE_ID,
            (unsigned int)GM_PLUGIN_COMMAND_GLASSES_TO_PHONE);
    if (channel == EVENT_CHANNEL && length == 4U) {
        if (fight_event_failures != 0U) {
            --fight_event_failures;
            return GM_PLUGIN_EBUSY;
        }
        if (bytes[2] < sizeof(fight_event_counts) /
                       sizeof(fight_event_counts[0]))
            ++fight_event_counts[bytes[2]];
        fprintf(stderr, "fighter event: sequence=%u event=%u value=%u\n",
                (unsigned int)bytes[1], (unsigned int)bytes[2],
                (unsigned int)bytes[3]);
    } else
        fprintf(stderr, "plugin message: channel=0x%04x length=%u\n",
                (unsigned int)channel, (unsigned int)length);
    return GM_PLUGIN_OK;
}

static void host_app_exit(void)
{
    simulator.exit_requested = true;
}

static gm_plugin_result_t host_extension_get(
    gm_plugin_extension_id_t extension_id, const void **api)
{
    (void)extension_id;
    if (api == NULL) return GM_PLUGIN_EINVAL;
    *api = NULL;
    return GM_PLUGIN_ENOTSUP;
}

static void initialize_host(void)
{
    memset(&simulator.host, 0, sizeof(simulator.host));
    simulator.host.struct_size = (uint16_t)sizeof(simulator.host);
    simulator.host.abi_version = GM_PLUGIN_ABI_VERSION;
    simulator.host.capabilities = GM_PLUGIN_CAP_DISPLAY_BITMAP |
                                  GM_PLUGIN_CAP_BLUETOOTH |
                                  GM_PLUGIN_CAP_BUTTON;
    simulator.host.log = host_log;
    simulator.host.monotonic_ms = host_monotonic_ms;
    simulator.host.alloc = malloc;
    simulator.host.free = free;
    simulator.host.display_get_info = host_display_get_info;
    simulator.host.graphics.framebuffer.lock = host_framebuffer_lock;
    simulator.host.graphics.framebuffer.unlock = host_framebuffer_unlock;
    simulator.host.bt_send = host_bt_send;
    simulator.host.app_exit = host_app_exit;
    simulator.host.extension_get = host_extension_get;
}

static void stop_plugin(void)
{
    if (!simulator.running) return;
    if (simulator.plugin.on_stop != NULL)
        simulator.plugin.on_stop(simulator.plugin.context);
    if (simulator.plugin.on_unload != NULL)
        simulator.plugin.on_unload(simulator.plugin.context);
    simulator.running = false;
}

static bool start_plugin(void)
{
    gm_plugin_result_t result;
    stop_plugin();
    memset(simulator.framebuffer, 0, sizeof(simulator.framebuffer));
    memset(&simulator.plugin, 0, sizeof(simulator.plugin));
    simulator.plugin.struct_size = (uint16_t)sizeof(simulator.plugin);
    simulator.clock_ms = 0U;
    simulator.present_count = 0U;
    simulator.input = 0U;
    simulator.sequence = 0U;
    simulator.locked = false;
    simulator.paused = false;
    simulator.exit_requested = false;
    result = sim_plugin_entry(&simulator.host, &simulator.plugin);
    if (result != GM_PLUGIN_OK) {
        fprintf(stderr, "plugin entry failed: %d\n", (int)result);
        return false;
    }
    if (simulator.plugin.on_load != NULL) {
        result = simulator.plugin.on_load(simulator.plugin.context);
        if (result != GM_PLUGIN_OK) {
            fprintf(stderr, "plugin load failed: %d\n", (int)result);
            if (simulator.plugin.on_unload != NULL)
                simulator.plugin.on_unload(simulator.plugin.context);
            return false;
        }
    }
    if (simulator.plugin.on_start == NULL) return false;
    result = simulator.plugin.on_start(simulator.plugin.context);
    if (result != GM_PLUGIN_OK) {
        fprintf(stderr, "plugin start failed: %d\n", (int)result);
        if (simulator.plugin.on_unload != NULL)
            simulator.plugin.on_unload(simulator.plugin.context);
        return false;
    }
    simulator.running = true;
    return true;
}

static void send_input(uint16_t input)
{
    gm_plugin_event_t event;
    uint8_t payload[4];
    if (!simulator.running || simulator.plugin.on_event == NULL) return;
    payload[0] = INPUT_VERSION;
    payload[1] = simulator.sequence++;
    payload[2] = (uint8_t)(input >> 8);
    payload[3] = (uint8_t)input;
    memset(&event, 0, sizeof(event));
    event.struct_size = (uint16_t)sizeof(event);
    event.type = GM_PLUGIN_EVENT_BT_MESSAGE;
    event.timestamp_ms = simulator.clock_ms;
    event.data.bt.channel = INPUT_CHANNEL;
    event.data.bt.data = payload;
    event.data.bt.length = sizeof(payload);
    simulator.plugin.on_event(simulator.plugin.context, &event);
}

static void send_connection(bool connected)
{
    gm_plugin_event_t event;
    if (!simulator.running || simulator.plugin.on_event == NULL) return;
    memset(&event, 0, sizeof(event));
    event.struct_size = (uint16_t)sizeof(event);
    event.type = GM_PLUGIN_EVENT_CONNECTION;
    event.timestamp_ms = simulator.clock_ms;
    event.data.connection.connected = connected;
    simulator.plugin.on_event(simulator.plugin.context, &event);
}

static void send_button(gm_plugin_button_action_t action)
{
    gm_plugin_event_t event;
    if (!simulator.running || simulator.plugin.on_event == NULL) return;
    memset(&event, 0, sizeof(event));
    event.struct_size = (uint16_t)sizeof(event);
    event.type = GM_PLUGIN_EVENT_BUTTON;
    event.timestamp_ms = simulator.clock_ms;
    event.data.button.button = GM_PLUGIN_BUTTON_PRIMARY;
    event.data.button.action = action;
    simulator.plugin.on_event(simulator.plugin.context, &event);
}

static void run_logic_frame(uint16_t input)
{
    if (!simulator.running || simulator.plugin.on_loop == NULL) return;
    simulator.clock_ms += SIM_FRAME_MS;
    send_input(input);
    simulator.plugin.on_loop(simulator.plugin.context, SIM_FRAME_MS);
}

static void run_unfed_logic_frame(void)
{
    if (!simulator.running || simulator.plugin.on_loop == NULL) return;
    simulator.clock_ms += SIM_FRAME_MS;
    simulator.plugin.on_loop(simulator.plugin.context, SIM_FRAME_MS);
}

static void set_paused(bool paused)
{
    simulator.paused = paused;
    send_input((uint16_t)(simulator.input | (paused ? KEY_PAUSE : 0U)));
    (void)sim_plugin_render();
    if (simulator.window != NULL)
        InvalidateRect(simulator.window, NULL, FALSE);
}

static void single_step(void)
{
    if (!simulator.paused) return;
    run_logic_frame(simulator.input);
    send_input((uint16_t)(simulator.input | KEY_PAUSE));
    (void)sim_plugin_render();
}

static uint16_t game_key(UINT virtual_key)
{
    switch (virtual_key) {
    case 'A': return KEY_LEFT;
    case 'D': return KEY_RIGHT;
    case 'W': return KEY_UP;
    case 'S': return KEY_DOWN;
    case 'J': return KEY_LIGHT;
    case 'K': return KEY_HEAVY;
    case 'U': return KEY_KICK;
    case 'I': return KEY_BLOCK;
    case 'L': return KEY_UPPERCUT;
    case 'O': return KEY_SWEEP;
    case VK_RETURN: return KEY_START;
    default: return 0U;
    }
}

static void convert_framebuffer(void)
{
    uint32_t y;
    for (y = 0U; y < SIM_DISPLAY_HEIGHT; ++y) {
        uint32_t x;
        for (x = 0U; x < SIM_DISPLAY_WIDTH; ++x) {
            uint8_t packed = simulator.framebuffer[y][x >> 1U];
            uint8_t gray = (x & 1U) == 0U ? (uint8_t)(packed >> 4U) :
                                            (uint8_t)(packed & 0x0FU);
            uint32_t value = (uint32_t)gray * 17U;
            simulator.display_pixels[y * SIM_DISPLAY_WIDTH + x] =
                value | (value << 8U) | (value << 16U);
        }
    }
}

static void draw_rect(HDC dc, const sim_rect_t *rect, COLORREF color,
                      int thickness)
{
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, rect->x * WINDOW_SCALE, rect->y * WINDOW_SCALE,
              (rect->x + rect->width) * WINDOW_SCALE,
              (rect->y + rect->height) * WINDOW_SCALE);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static const char *safe_name(const char *const *names, size_t count,
                             unsigned int index)
{
    return index < count ? names[index] : "UNKNOWN";
}

static void draw_text_line(HDC dc, int *y, const char *format, ...)
{
    char line[192];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    TextOutA(dc, SIM_DISPLAY_WIDTH * WINDOW_SCALE + 16, *y,
             line, (int)strlen(line));
    *y += 20;
}

static void draw_debug_overlay(HDC dc)
{
    sim_debug_snapshot_t debug;
    uint8_t index;
    int y = 18;
    sim_plugin_debug_snapshot(&debug);
    if (debug.screen == 4U) {
        draw_rect(dc, &debug.player.body, RGB(40, 220, 80), 2);
        draw_rect(dc, &debug.cpu.body, RGB(235, 60, 60), 2);
        draw_rect(dc, &debug.player.hurtbox, RGB(40, 160, 255), 1);
        draw_rect(dc, &debug.cpu.hurtbox, RGB(220, 80, 255), 1);
        if (debug.player.attack_range.width != 0)
            draw_rect(dc, &debug.player.attack_range,
                      debug.player.active_range ? RGB(255, 230, 0) :
                                                  RGB(160, 130, 0), 2);
        if (debug.cpu.attack_range.width != 0)
            draw_rect(dc, &debug.cpu.attack_range,
                      debug.cpu.active_range ? RGB(255, 160, 0) :
                                               RGB(140, 80, 0), 2);
        for (index = 0U; index < SIM_MAX_PROJECTILES; ++index) {
            if (debug.projectiles[index].active)
                draw_rect(dc, &debug.projectiles[index].collision,
                          RGB(0, 220, 255), 2);
        }
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 235, 235));
    draw_text_line(dc, &y, "Fighter Arena simulator");
    draw_text_line(dc, &y, "Screen: %s",
        safe_name(screen_names, sizeof(screen_names) / sizeof(screen_names[0]),
                  debug.screen));
    draw_text_line(dc, &y, "Difficulty: %u  Round: %u-%u",
        (unsigned int)debug.difficulty, (unsigned int)debug.player_rounds,
        (unsigned int)debug.cpu_rounds);
    draw_text_line(dc, &y, "Time: %u.%us  Combo: %u",
        (unsigned int)(debug.round_left_ms / 1000U),
        (unsigned int)(debug.round_left_ms % 1000U / 100U),
        (unsigned int)debug.combo);
    y += 10;
    draw_text_line(dc, &y, "PLAYER  HP %u  EN %u",
        debug.player.health, debug.player.energy);
    draw_text_line(dc, &y, "%s / %s  %u ms%s%s%s",
        safe_name(attack_names, sizeof(attack_names) / sizeof(attack_names[0]),
                  debug.player.attack),
        safe_name(phase_names, sizeof(phase_names) / sizeof(phase_names[0]),
                  debug.player.attack_phase), debug.player.attack_ms,
        debug.player.blocking ? " BLOCK" : "",
        debug.player.crouching ? " CROUCH" : "",
        debug.player.turning ? " TURN" : "");
    draw_text_line(dc, &y, "Pose %u  Hurt %u  Land %u  Turn %u  Face %s",
        debug.player.render_frame, debug.player.hurt_kind,
        debug.player.landing_ms, debug.player.turn_ms,
        debug.player.facing_right ? "RIGHT" : "LEFT");
    y += 10;
    draw_text_line(dc, &y, "CPU     HP %u  EN %u",
        debug.cpu.health, debug.cpu.energy);
    draw_text_line(dc, &y, "%s / %s  %u ms%s%s%s",
        safe_name(attack_names, sizeof(attack_names) / sizeof(attack_names[0]),
                  debug.cpu.attack),
        safe_name(phase_names, sizeof(phase_names) / sizeof(phase_names[0]),
                  debug.cpu.attack_phase), debug.cpu.attack_ms,
        debug.cpu.blocking ? " BLOCK" : "",
        debug.cpu.crouching ? " CROUCH" : "",
        debug.cpu.turning ? " TURN" : "");
    draw_text_line(dc, &y, "Pose %u  Hurt %u  Land %u  Turn %u  Face %s",
        debug.cpu.render_frame, debug.cpu.hurt_kind,
        debug.cpu.landing_ms, debug.cpu.turn_ms,
        debug.cpu.facing_right ? "RIGHT" : "LEFT");
    y += 16;
    draw_text_line(dc, &y, "Speed %.2fx%s",
        speed_values[simulator.speed_index],
        simulator.paused ? "  PAUSED" : "");
    draw_text_line(dc, &y, "Input 0x%03x  Presents %u",
        debug.input, (unsigned int)simulator.present_count);
    draw_text_line(dc, &y, "Pause reason %u", debug.pause_reason);
    y += 16;
    draw_text_line(dc, &y, "Esc pause  F5 restart");
    draw_text_line(dc, &y, "F6 overlay  F7/F8 speed");
    draw_text_line(dc, &y, "F10 step  F12 screenshot");
}

static void draw_window_contents(HDC dc, const RECT *client)
{
    BITMAPINFO info;
    HBRUSH background = CreateSolidBrush(RGB(25, 25, 28));
    FillRect(dc, client, background);
    DeleteObject(background);
    convert_framebuffer();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = SIM_DISPLAY_WIDTH;
    info.bmiHeader.biHeight = -(LONG)SIM_DISPLAY_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, 0, SIM_DISPLAY_WIDTH * WINDOW_SCALE,
                  SIM_DISPLAY_HEIGHT * WINDOW_SCALE,
                  0, 0, SIM_DISPLAY_WIDTH, SIM_DISPLAY_HEIGHT,
                  simulator.display_pixels, &info, DIB_RGB_COLORS, SRCCOPY);
    if (simulator.overlay) draw_debug_overlay(dc);
}

static void paint_window(HWND window)
{
    PAINTSTRUCT paint;
    RECT client;
    HDC dc = BeginPaint(window, &paint);
    GetClientRect(window, &client);
    draw_window_contents(dc, &client);
    EndPaint(window, &paint);
}

static bool output_path(const char *folder, const char *filename,
                        char path[MAX_PATH])
{
    char directory[MAX_PATH];
    char *separator;
    if (GetModuleFileNameA(NULL, directory, MAX_PATH) == 0U) return false;
    separator = strrchr(directory, '\\');
    if (separator == NULL) return false;
    *separator = '\0';
    if (strlen(directory) + strlen(folder) + strlen(filename) + 3U >= MAX_PATH)
        return false;
    strcat(directory, "\\");
    strcat(directory, folder);
    if (!CreateDirectoryA(directory, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) return false;
    snprintf(path, MAX_PATH, "%s\\%s", directory, filename);
    return true;
}

static bool save_screenshot_file(HWND window, const char *path)
{
    RECT client;
    BITMAPINFOHEADER header;
    BITMAPFILEHEADER file_header;
    int width;
    int height;
    DWORD row_bytes;
    DWORD image_bytes;
    uint8_t *pixels;
    HDC window_dc;
    HDC memory_dc;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    FILE *file;
    GetClientRect(window, &client);
    width = client.right;
    height = client.bottom;
    row_bytes = (DWORD)width * 4U;
    image_bytes = row_bytes * (DWORD)height;
    pixels = malloc(image_bytes);
    if (pixels == NULL) return false;
    window_dc = GetDC(window);
    memory_dc = CreateCompatibleDC(window_dc);
    bitmap = CreateCompatibleBitmap(window_dc, width, height);
    old_bitmap = SelectObject(memory_dc, bitmap);
    draw_window_contents(memory_dc, &client);
    SelectObject(memory_dc, old_bitmap);
    memset(&header, 0, sizeof(header));
    header.biSize = sizeof(header);
    header.biWidth = width;
    header.biHeight = height;
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = image_bytes;
    if (GetDIBits(memory_dc, bitmap, 0, (UINT)height, pixels,
                  (BITMAPINFO *)&header, DIB_RGB_COLORS) == 0) {
        free(pixels);
        DeleteObject(bitmap);
        DeleteDC(memory_dc);
        ReleaseDC(window, window_dc);
        return false;
    }
    memset(&file_header, 0, sizeof(file_header));
    file_header.bfType = 0x4D42U;
    file_header.bfOffBits = sizeof(file_header) + sizeof(header);
    file_header.bfSize = file_header.bfOffBits + image_bytes;
    file = fopen(path, "wb");
    if (file != NULL) {
        fwrite(&file_header, sizeof(file_header), 1U, file);
        fwrite(&header, sizeof(header), 1U, file);
        fwrite(pixels, image_bytes, 1U, file);
        fclose(file);
        fprintf(stderr, "screenshot saved: %s\n", path);
    }
    free(pixels);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(window, window_dc);
    return file != NULL;
}

static bool save_screenshot(HWND window)
{
    SYSTEMTIME time;
    char filename[96];
    char path[MAX_PATH];
    GetLocalTime(&time);
    snprintf(filename, sizeof(filename),
        "fighter-arena-%04u%02u%02u-%02u%02u%02u-overlay-%s.bmp",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, simulator.overlay ? "on" : "off");
    return output_path("screenshots", filename, path) &&
           save_screenshot_file(window, path);
}

static bool save_test_screenshot(HWND window, const char *filename)
{
    char path[MAX_PATH];
    return output_path("test-results", filename, path) &&
           save_screenshot_file(window, path);
}

static bool save_lifecycle_screenshot(HWND window, const char *filename)
{
    char path[MAX_PATH];
    return output_path("lifecycle-results", filename, path) &&
           save_screenshot_file(window, path);
}

static void update_simulation(void)
{
    LARGE_INTEGER counter;
    double elapsed_ms;
    double frame_real_ms;
    QueryPerformanceCounter(&counter);
    elapsed_ms = (double)(counter.QuadPart - simulator.last_counter.QuadPart) *
                 1000.0 / (double)simulator.frequency.QuadPart;
    simulator.last_counter = counter;
    if (elapsed_ms > 250.0) elapsed_ms = 250.0;
    if (simulator.paused || !simulator.running) return;
    simulator.real_accumulator_ms += elapsed_ms;
    frame_real_ms = SIM_FRAME_MS / speed_values[simulator.speed_index];
    while (simulator.real_accumulator_ms >= frame_real_ms) {
        simulator.real_accumulator_ms -= frame_real_ms;
        run_logic_frame(simulator.input);
        if (simulator.exit_requested) {
            PostMessage(simulator.window, WM_CLOSE, 0, 0);
            break;
        }
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    uint16_t key;
    (void)lparam;
    switch (message) {
    case WM_TIMER:
        update_simulation();
        return 0;
    case WM_KEYDOWN:
        if ((lparam & (1L << 30)) != 0 && wparam != VK_F10) return 0;
        switch (wparam) {
        case VK_ESCAPE:
            set_paused(!simulator.paused);
            return 0;
        case VK_F5:
            (void)start_plugin();
            return 0;
        case VK_F6:
            simulator.overlay = !simulator.overlay;
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case VK_F7:
            if (simulator.speed_index > 0U) --simulator.speed_index;
            simulator.real_accumulator_ms = 0.0;
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case VK_F8:
            if (simulator.speed_index + 1U <
                sizeof(speed_values) / sizeof(speed_values[0]))
                ++simulator.speed_index;
            simulator.real_accumulator_ms = 0.0;
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case VK_F10:
            single_step();
            return 0;
        case VK_F12:
            InvalidateRect(window, NULL, FALSE);
            UpdateWindow(window);
            (void)save_screenshot(window);
            return 0;
        default:
            key = game_key((UINT)wparam);
            if (key != 0U) simulator.input |= key;
            return 0;
        }
    case WM_KEYUP:
        key = game_key((UINT)wparam);
        if (key != 0U) simulator.input &= (uint16_t)~key;
        return 0;
    case WM_KILLFOCUS:
        simulator.input = 0U;
        set_paused(true);
        return 0;
    case WM_PAINT:
        paint_window(window);
        return 0;
    case WM_DESTROY:
        stop_plugin();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window, message, wparam, lparam);
    }
}

static bool framebuffer_self_test(void)
{
    gm_plugin_framebuffer_surface_t surface;
    gm_plugin_rect_t dirty;
    if (host_framebuffer_unlock(NULL, false) != GM_PLUGIN_ESTATE) return false;
    if (host_framebuffer_lock(31U, &surface) != GM_PLUGIN_OK) return false;
    if (surface.y != 30U || surface.height != SLICE_HEIGHT ||
        surface.width != SIM_DISPLAY_WIDTH ||
        surface.stride != SIM_DISPLAY_WIDTH / 2U) return false;
    if (host_framebuffer_lock(0U, &surface) != GM_PLUGIN_ESTATE) return false;
    dirty.x = 0;
    dirty.y = 0;
    dirty.width = 1U;
    dirty.height = 1U;
    if (host_framebuffer_unlock(&dirty, false) != GM_PLUGIN_EINVAL) return false;
    if (host_framebuffer_lock(0U, &surface) != GM_PLUGIN_OK) return false;
    surface.pixels[0] = 0xF1U;
    if ((surface.pixels[0] >> 4U) != 15U ||
        (surface.pixels[0] & 0x0FU) != 1U) return false;
    dirty.x = 0;
    dirty.y = 0;
    dirty.width = 2U;
    dirty.height = 1U;
    return host_framebuffer_unlock(&dirty, true) == GM_PLUGIN_OK;
}

static void smoke_press(uint16_t key)
{
    run_logic_frame(key);
    run_logic_frame(0U);
}

static int run_smoke_test(void)
{
    sim_debug_snapshot_t debug;
    unsigned int index;
    initialize_host();
    if (!framebuffer_self_test()) {
        fprintf(stderr, "smoke: framebuffer contract failed\n");
        return 1;
    }
    if (!start_plugin()) return 1;
    memset(fight_event_counts, 0, sizeof(fight_event_counts));
    fight_event_failures = 2U;
    smoke_press(KEY_LIGHT);
    smoke_press(KEY_RIGHT);
    smoke_press(KEY_START);
    smoke_press(KEY_START);
    for (index = 0U; index < 10U; ++index) run_logic_frame(KEY_RIGHT);
    smoke_press(KEY_LIGHT);
    for (index = 0U; index < 8U; ++index) run_logic_frame(KEY_LEFT);
    run_logic_frame(KEY_PAUSE);
    sim_plugin_debug_snapshot(&debug);
    if (debug.screen != 4U || simulator.present_count == 0U ||
        debug.player.health == 0U || !simulator.running) {
        fprintf(stderr, "smoke: gameplay state failed screen=%u presents=%u\n",
                (unsigned int)debug.screen,
                (unsigned int)simulator.present_count);
        stop_plugin();
        return 1;
    }
    stop_plugin();
    fprintf(stderr, "smoke: passed (%u presents, input=0x%03x)\n",
            (unsigned int)simulator.present_count, debug.input);
    return 0;
}

static FILE *combat_report;
static unsigned int combat_passes;
static unsigned int combat_failures;

static void combat_check(bool passed, const char *name, const char *format, ...)
{
    va_list arguments;
    if (passed) ++combat_passes;
    else ++combat_failures;
    fprintf(combat_report, "%s %s: ", passed ? "PASS" : "FAIL", name);
    va_start(arguments, format);
    vfprintf(combat_report, format, arguments);
    va_end(arguments);
    fputc('\n', combat_report);
}

static void combat_frames(unsigned int count, uint16_t input)
{
    unsigned int index;
    for (index = 0U; index < count; ++index) run_logic_frame(input);
}

static unsigned int active_projectiles(const sim_debug_snapshot_t *debug)
{
    unsigned int index;
    unsigned int count = 0U;
    for (index = 0U; index < SIM_MAX_PROJECTILES; ++index) {
        if (debug->projectiles[index].active) ++count;
    }
    return count;
}

static unsigned int visible_shoe_pixels(const sim_fighter_debug_t *fighter)
{
    int16_t y = (int16_t)(fighter->body.y + fighter->body.height - 1);
    int16_t start = (int16_t)(fighter->body.x - 40);
    int16_t end = (int16_t)(fighter->body.x + fighter->body.width + 40);
    unsigned int count = 0U;
    int16_t x;
    if (start < 0) start = 0;
    if (end > (int16_t)SIM_DISPLAY_WIDTH) end = (int16_t)SIM_DISPLAY_WIDTH;
    for (x = start; x < end; ++x) {
        uint8_t packed = simulator.framebuffer[y][(uint16_t)x / 2U];
        uint8_t gray = (x & 1) == 0 ? (uint8_t)(packed >> 4) :
                                      (uint8_t)(packed & 0x0FU);
        if (gray > 4U) ++count;
    }
    return count;
}

static HWND create_test_window(HINSTANCE instance)
{
    WNDCLASSA window_class;
    RECT window_rect = {0, 0,
        SIM_DISPLAY_WIDTH * WINDOW_SCALE + PANEL_WIDTH,
        SIM_DISPLAY_HEIGHT * WINDOW_SCALE};
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = "GMFighterArenaAutotest";
    if (!RegisterClassA(&window_class)) return NULL;
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
    return CreateWindowA(window_class.lpszClassName,
        "GM Fighter Arena Autotest", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL, NULL, instance, NULL);
}

static int run_combat_test(HINSTANCE instance)
{
    char report_path[MAX_PATH];
    sim_debug_snapshot_t debug;
    int16_t attack_start_x;
    int16_t constrained_player_x;
    int16_t constrained_cpu_x;
    uint32_t paused_time;
    unsigned int index;
    bool screenshot_ok;
    simulator.window = create_test_window(instance);
    if (simulator.window == NULL) return 1;
    initialize_host();
    simulator.overlay = true;
    simulator.speed_index = 2U;
    if (!start_plugin()) return 1;
    if (!output_path("test-results", "report.txt", report_path)) return 1;
    combat_report = fopen(report_path, "w");
    if (combat_report == NULL) return 1;
    fprintf(combat_report,
        "Fighter Arena deterministic combat test\n"
        "All scenarios run production combat and rendering code.\n\n");

    combat_check(sim_plugin_test_display_scale(280U, 1340U) == 3U,
                 "tall_narrow_display_scale", "expected=3 actual=%u",
                 sim_plugin_test_display_scale(280U, 1340U));
    combat_check(
        !sim_plugin_test_hurt_uses_source_direction(0U, 0U, false) &&
        sim_plugin_test_hurt_uses_source_direction(0U, 0U, true) &&
        sim_plugin_test_hurt_uses_source_direction(0U, 1U, false) &&
        !sim_plugin_test_hurt_uses_source_direction(0U, 1U, true) &&
        sim_plugin_test_hurt_uses_source_direction(0U, 2U, false) &&
        !sim_plugin_test_hurt_uses_source_direction(0U, 2U, true) &&
        !sim_plugin_test_hurt_uses_source_direction(0U, 3U, false) &&
        sim_plugin_test_hurt_uses_source_direction(0U, 3U, true) &&
        !sim_plugin_test_hurt_uses_source_direction(1U, 0U, false) &&
        sim_plugin_test_hurt_uses_source_direction(1U, 0U, true) &&
        sim_plugin_test_hurt_uses_source_direction(1U, 1U, false) &&
        !sim_plugin_test_hurt_uses_source_direction(1U, 1U, true) &&
        !sim_plugin_test_hurt_uses_source_direction(1U, 2U, false) &&
        sim_plugin_test_hurt_uses_source_direction(1U, 2U, true) &&
        !sim_plugin_test_hurt_uses_source_direction(1U, 3U, false) &&
        sim_plugin_test_hurt_uses_source_direction(1U, 3U, true),
        "hurt_sprite_source_orientation",
        "hurt-sheet source orientation must be defined per frame");
    sim_plugin_test_constrain_display(280U, 1340U, 30, 30,
                                      &constrained_player_x,
                                      &constrained_cpu_x);
    combat_check(constrained_player_x == 30 && constrained_cpu_x == 138,
                 "tall_narrow_left_separation", "player=%d cpu=%d",
                 constrained_player_x, constrained_cpu_x);
    sim_plugin_test_constrain_display(280U, 1340U, 142, 142,
                                      &constrained_player_x,
                                      &constrained_cpu_x);
    combat_check(constrained_player_x == 34 && constrained_cpu_x == 142,
                 "tall_narrow_right_separation", "player=%d cpu=%d",
                 constrained_player_x, constrained_cpu_x);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_debug_snapshot(&debug);
    combat_check(visible_shoe_pixels(&debug.player) >= 4U,
                 "shoe_ground_alignment", "pixels=%u",
                 visible_shoe_pixels(&debug.player));
    attack_start_x = debug.player.body.x;
    run_logic_frame(KEY_LIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.attack == 1U &&
                 debug.player.attack_phase == 1U && debug.cpu.health == 105U,
                 "light_startup", "phase=%u attack_ms=%u cpu_hp=%u",
                 debug.player.attack_phase, debug.player.attack_ms,
                 debug.cpu.health);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.body.x == attack_start_x + 4,
                 "startup_lunge", "expected_x=%d actual_x=%d attack_ms=%u",
                 attack_start_x + 4, debug.player.body.x,
                 debug.player.attack_ms);
    combat_check(debug.player.attack_range.x == debug.player.body.x,
                 "attack_overlay_origin",
                 "collision_origin_x=%d overlay_origin_x=%d",
                 debug.player.body.x, debug.player.attack_range.x);
    combat_check(debug.player.attack_phase == 2U && debug.cpu.health == 101U &&
                 active_projectiles(&debug) == 0U,
                 "light_hit", "phase=%u cpu_hp=%u combo=%u",
                 debug.player.attack_phase, debug.cpu.health, debug.combo);
    combat_check(fight_event_counts[FIGHT_EVENT_ATTACK] == 1U &&
                 fight_event_counts[FIGHT_EVENT_HIT] == 1U,
                 "event_retry_delivery", "attack=%u hit=%u",
                 fight_event_counts[FIGHT_EVENT_ATTACK],
                 fight_event_counts[FIGHT_EVENT_HIT]);
    screenshot_ok = save_test_screenshot(simulator.window,
                                         "01-light-hit.bmp");
    combat_check(screenshot_ok, "light_screenshot", "01-light-hit.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, true, false);
    run_logic_frame(KEY_HEAVY);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 104U && debug.cpu.guard == 100U,
                 "heavy_stand_block", "cpu_hp=%u guard=%u",
                 debug.cpu.health, debug.cpu.guard);
    (void)save_test_screenshot(simulator.window, "02-heavy-stand-block.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, true, true);
    sim_plugin_test_set_distance(72);
    run_logic_frame(KEY_KICK);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 105U && debug.cpu.guard == 100U &&
                 debug.player.render_frame == 8U,
                 "light_kick_crouch_block", "cpu_hp=%u guard=%u frame=%u",
                 debug.cpu.health, debug.cpu.guard,
                 debug.player.render_frame);
    (void)save_test_screenshot(simulator.window, "03-low-crouch-block.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(90);
    run_logic_frame(KEY_KICK);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 105U,
                 "light_kick_whiffs_at_range", "cpu_hp=%u distance=%d",
                 debug.cpu.health,
                 debug.cpu.body.x - debug.player.body.x);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, true, false);
    sim_plugin_test_set_distance(72);
    run_logic_frame(KEY_KICK);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 102U && debug.cpu.guard == 100U &&
                 debug.cpu.hurt_kind == 6U,
                 "light_kick_beats_stand_block", "cpu_hp=%u guard=%u hurt=%u",
                 debug.cpu.health, debug.cpu.guard, debug.cpu.hurt_kind);
    (void)save_test_screenshot(simulator.window, "04-low-stand-hit.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(220);
    simulator.overlay = false;
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = debug.player.body.x;
    run_logic_frame(KEY_SWEEP);
    run_logic_frame(0U);
    (void)save_test_screenshot(simulator.window, "05-rush-dash.bmp");
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.attack == 5U && debug.player.energy == 18U &&
                 debug.cpu.health == 104U && debug.cpu.hurt_kind == 2U &&
                 debug.player.render_frame == 5U &&
                 debug.player.body.x - attack_start_x >= 100,
                 "rush_combo_tracks_target",
                 "energy=%u cpu_hp=%u hurt=%u frame=%u dash=%d",
                 debug.player.energy, debug.cpu.health, debug.cpu.hurt_kind,
                 debug.player.render_frame,
                 debug.player.body.x - attack_start_x);
    (void)save_test_screenshot(simulator.window, "05-rush-connect.bmp");
    combat_frames(4U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 102U && debug.combo == 2U &&
                 debug.player.render_frame == 7U,
                 "rush_heavy", "cpu_hp=%u combo=%u frame=%u",
                 debug.cpu.health, debug.combo, debug.cpu.render_frame);
    (void)save_test_screenshot(simulator.window, "05-rush-heavy.bmp");
    combat_frames(9U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 97U && debug.combo == 5U &&
                 debug.player.render_frame == 9U,
                 "rush_flurry", "cpu_hp=%u combo=%u frame=%u",
                 debug.cpu.health, debug.combo, debug.cpu.render_frame);
    (void)save_test_screenshot(simulator.window, "05-rush-flurry.bmp");
    combat_frames(4U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 93U && debug.player.energy == 35U &&
                 debug.combo == 6U &&
                 debug.cpu.hurt_kind == 3U && debug.cpu.velocity_y < 0 &&
                 debug.player.velocity_y == 0 &&
                 debug.player.render_frame == 10U &&
                 active_projectiles(&debug) == 0U,
                 "rush_palm_finisher",
                 "cpu_hp=%u energy=%u combo=%u hurt=%u velocity=%d/%d",
                 debug.cpu.health, debug.player.energy, debug.combo,
                 debug.cpu.hurt_kind,
                 debug.player.velocity_y, debug.cpu.velocity_y);
    (void)save_test_screenshot(simulator.window, "05-rush-finisher.bmp");
    simulator.overlay = true;

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(400);
    run_logic_frame(KEY_SWEEP);
    combat_frames(22U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 105U && debug.combo == 0U &&
                 active_projectiles(&debug) == 0U,
                 "rush_whiff_has_no_followup_hits",
                 "cpu_hp=%u combo=%u projectiles=%u",
                 debug.cpu.health, debug.combo, active_projectiles(&debug));

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    run_logic_frame(KEY_LIGHT);
    run_logic_frame(0U);
    combat_frames(2U, 0U);
    run_logic_frame(KEY_LIGHT);
    run_logic_frame(0U);
    run_logic_frame(0U);
    run_logic_frame(0U);
    combat_frames(2U, 0U);
    run_logic_frame(KEY_HEAVY);
    run_logic_frame(0U);
    combat_frames(3U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 90U && debug.combo == 3U &&
                 debug.player.attack == 2U &&
                 active_projectiles(&debug) == 0U,
                 "target_combo_j_j_k",
                 "cpu_hp=%u combo=%u attack=%u",
                 debug.cpu.health, debug.combo, debug.player.attack);
    (void)save_test_screenshot(simulator.window, "05-target-combo-j-j-k.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    run_logic_frame(KEY_LIGHT);
    run_logic_frame(0U);
    combat_frames(2U, 0U);
    run_logic_frame(KEY_KICK);
    run_logic_frame(0U);
    run_logic_frame(0U);
    combat_frames(2U, 0U);
    run_logic_frame(KEY_BLOCK);
    combat_frames(4U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 93U && debug.combo == 3U &&
                 debug.cpu.hurt_kind == 3U &&
                 active_projectiles(&debug) == 0U,
                 "target_combo_j_u_i",
                 "cpu_hp=%u combo=%u hurt=%u",
                 debug.cpu.health, debug.combo, debug.cpu.hurt_kind);
    (void)save_test_screenshot(simulator.window, "05-target-combo-j-u-i.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(100);
    run_logic_frame(KEY_BLOCK);
    combat_frames(3U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 105U,
                 "heavy_kick_whiffs_at_range", "cpu_hp=%u distance=%d",
                 debug.cpu.health,
                 debug.cpu.body.x - debug.player.body.x);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    run_logic_frame(KEY_BLOCK);
    combat_frames(3U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.attack == 6U && debug.cpu.health == 100U &&
                 debug.cpu.hurt_kind == 3U && debug.cpu.velocity_y < 0 &&
                 debug.player.render_frame == 9U && debug.cpu.hurt_right,
                 "heavy_kick_knockdown",
                 "cpu_hp=%u hurt=%u velocity_y=%d frame=%u hurt_right=%u",
                 debug.cpu.health, debug.cpu.hurt_kind,
                 debug.cpu.velocity_y, debug.player.render_frame,
                 debug.cpu.hurt_right);
    (void)save_test_screenshot(simulator.window, "06-sweep.bmp");
    combat_frames(6U, 0U);
    (void)save_test_screenshot(simulator.window,
                               "06-knockdown-right-late.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_swap_sides();
    run_logic_frame(KEY_BLOCK);
    combat_frames(3U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 100U && !debug.cpu.hurt_right &&
                 debug.player.body.x > debug.cpu.body.x,
                 "knockdown_direction_after_side_switch",
                 "player_x=%d cpu_x=%d cpu_hp=%u hurt_right=%u",
                 debug.player.body.x, debug.cpu.body.x, debug.cpu.health,
                 debug.cpu.hurt_right);
    (void)save_test_screenshot(simulator.window,
                               "06-side-switched-knockdown.bmp");
    combat_frames(6U, 0U);
    (void)save_test_screenshot(simulator.window,
                               "06-knockdown-left-late.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_cpu_attack(2U, 100U);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.health == 93U && !debug.player.hurt_right,
                 "player_recoils_left_from_right_attack",
                 "player_hp=%u hurt_right=%u",
                 debug.player.health, debug.player.hurt_right);
    (void)save_test_screenshot(simulator.window, "06-player-hurt-left.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_swap_sides();
    sim_plugin_test_set_cpu_attack(2U, 100U);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.health == 93U && debug.player.hurt_right,
                 "player_recoils_right_from_left_attack",
                 "player_hp=%u hurt_right=%u",
                 debug.player.health, debug.player.hurt_right);
    (void)save_test_screenshot(simulator.window, "06-player-hurt-right.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(220);
    run_logic_frame(KEY_UPPERCUT);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    screenshot_ok = save_test_screenshot(simulator.window,
                                         "05-large-energy-wave.bmp");
    combat_check(active_projectiles(&debug) == 1U && screenshot_ok,
                 "large_energy_wave_visible",
                 "projectiles=%u screenshot=%u",
                 active_projectiles(&debug), screenshot_ok);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    run_logic_frame(KEY_UPPERCUT);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.attack == 4U && debug.player.energy == 0U &&
                 active_projectiles(&debug) == 1U,
                 "special_launch", "energy=%u projectiles=%u",
                 debug.player.energy, active_projectiles(&debug));
    (void)save_test_screenshot(simulator.window, "05-special-launch.bmp");
    combat_frames(6U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 96U && debug.cpu.hurt_kind == 3U &&
                 active_projectiles(&debug) == 0U,
                 "special_hit", "cpu_hp=%u projectiles=%u hurt=%u",
                 debug.cpu.health, active_projectiles(&debug),
                 debug.cpu.hurt_kind);
    (void)save_test_screenshot(simulator.window, "06-special-hit.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 20U, true, false);
    run_logic_frame(KEY_HEAVY);
    combat_frames(2U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 104U && debug.cpu.guard == 20U &&
                 debug.cpu.hurt_ms == 0U,
                 "blocking_ignores_legacy_guard_value",
                 "cpu_hp=%u guard=%u hurt_ms=%u",
                 debug.cpu.health, debug.cpu.guard, debug.cpu.hurt_ms);
    (void)save_test_screenshot(simulator.window, "07-block-no-meter.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, true, false);
    run_logic_frame(KEY_HEAVY);
    combat_frames(15U, 0U);
    sim_plugin_test_set_distance(64);
    sim_plugin_test_set_cpu_blocking(true);
    run_logic_frame(KEY_HEAVY);
    combat_frames(15U, 0U);
    sim_plugin_test_set_distance(64);
    sim_plugin_test_set_cpu_blocking(true);
    run_logic_frame(KEY_HEAVY);
    combat_frames(3U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 102U && debug.cpu.guard == 100U &&
                 debug.cpu.hurt_kind == 0U && debug.cpu.hurt_ms == 0U,
                 "repeated_blocks_do_not_guard_break",
                 "cpu_hp=%u guard=%u hurt=%u hurt_ms=%u attack=%u ms=%u distance=%d",
                 debug.cpu.health, debug.cpu.guard,
                 debug.cpu.hurt_kind, debug.cpu.hurt_ms,
                 debug.player.attack, debug.player.attack_ms,
                 debug.cpu.body.x - debug.player.body.x);
    (void)save_test_screenshot(simulator.window,
                               "07-repeated-blocks.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = debug.player.body.x;
    run_logic_frame(KEY_LEFT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(!debug.player.blocking && debug.player.moving &&
                 debug.player.body.x < attack_start_x,
                 "backward_walk_without_threat",
                 "start_x=%d current_x=%d blocking=%u moving=%u",
                 attack_start_x, debug.player.body.x,
                 debug.player.blocking, debug.player.moving);
    sim_plugin_test_set_cpu_attack(2U, 100U);
    run_logic_frame(KEY_LEFT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.blocking && debug.player.render_frame == 4U &&
                 debug.player.health == 99U,
                 "back_direction_blocks_active_attack",
                 "blocking=%u frame=%u hp=%u",
                 debug.player.blocking, debug.player.render_frame,
                 debug.player.health);
    (void)save_test_screenshot(simulator.window, "08-stand-guard.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(200);
    sim_plugin_test_enable_cpu(0U);
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = debug.cpu.body.x;
    combat_frames(6U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(attack_start_x - debug.cpu.body.x == 32 &&
                 debug.cpu.moving,
                 "cpu_paced_approach",
                 "start_x=%d current_x=%d delta=%d",
                 attack_start_x, debug.cpu.body.x,
                 attack_start_x - debug.cpu.body.x);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_set_distance(100);
    sim_plugin_test_enable_cpu(0U);
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = (int16_t)(debug.cpu.body.x - debug.player.body.x);
    combat_frames(6U, KEY_LEFT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.body.x - debug.player.body.x > attack_start_x &&
                 debug.player.moving && !debug.player.blocking,
                 "player_can_disengage_from_cpu",
                 "start_distance=%d current_distance=%d blocking=%u",
                 attack_start_x,
                 debug.cpu.body.x - debug.player.body.x,
                 debug.player.blocking);

    sim_plugin_test_set_cpu_attack(2U, 600U);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = debug.cpu.body.x;
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.body.x > attack_start_x && debug.cpu.moving,
                 "cpu_retreats_after_attack",
                 "start_x=%d current_x=%d moving=%u",
                 attack_start_x, debug.cpu.body.x, debug.cpu.moving);

    sim_plugin_test_set_cpu_hurt(2U, 450U, 450U);
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = debug.cpu.render_frame;
    sim_plugin_test_set_cpu_hurt(2U, 450U, 200U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(attack_start_x == 0x81U && debug.cpu.render_frame == 0x83U,
                 "heavy_hurt_pose_progression", "first=%u second=%u",
                 (unsigned int)attack_start_x, debug.cpu.render_frame);
    (void)save_test_screenshot(simulator.window, "09-heavy-recoil.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    run_logic_frame((uint16_t)(KEY_UP | KEY_RIGHT));
    combat_frames(13U, KEY_RIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.body.x > debug.cpu.body.x &&
                 debug.player.landing_ms != 0U && debug.player.facing_right,
                 "side_switch_landing_recovery",
                 "player_x=%d cpu_x=%d landing=%u facing=%u",
                 debug.player.body.x, debug.cpu.body.x,
                 debug.player.landing_ms, debug.player.facing_right);
    run_logic_frame(KEY_LIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.attack == 0U && debug.player.landing_ms != 0U,
                 "landing_blocks_attack", "attack=%u landing=%u",
                 debug.player.attack, debug.player.landing_ms);
    combat_frames(5U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(!debug.player.facing_right && debug.cpu.facing_right &&
                 !debug.player.turning &&
                 debug.player.body.x - debug.cpu.body.x >=
                     debug.player.body.width,
                 "delayed_turn_after_switch",
                 "facing=%u/%u turn=%u separation=%d",
                 debug.player.facing_right, debug.cpu.facing_right,
                 debug.player.turn_ms,
                 debug.player.body.x - debug.cpu.body.x);
    attack_start_x = debug.player.body.x;
    run_logic_frame(KEY_RIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(!debug.player.blocking && !debug.player.facing_right &&
                 debug.player.body.x > attack_start_x,
                 "switched_backward_walk",
                 "blocking=%u facing=%u start_x=%d current_x=%d",
                 debug.player.blocking, debug.player.facing_right,
                 attack_start_x, debug.player.body.x);
    sim_plugin_test_set_cpu_attack(2U, 100U);
    run_logic_frame(KEY_RIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.blocking && !debug.player.facing_right,
                 "switched_back_blocks_active_attack",
                 "blocking=%u facing=%u hp=%u",
                 debug.player.blocking, debug.player.facing_right,
                 debug.player.health);
    combat_frames(12U, 0U);
    run_logic_frame(KEY_LIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.player.attack == 1U &&
                 debug.player.attack_range.x < debug.player.body.x,
                 "switched_attack_faces_opponent", "attack=%u range_x=%d body_x=%d",
                 debug.player.attack, debug.player.attack_range.x,
                 debug.player.body.x);
    (void)save_test_screenshot(simulator.window, "10-side-switch.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    run_logic_frame(KEY_RIGHT);
    run_logic_frame(KEY_RIGHT);
    sim_plugin_debug_snapshot(&debug);
    attack_start_x = debug.player.render_frame;
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(attack_start_x == 1U && !debug.player.moving &&
                 debug.player.render_frame == 0U,
                 "walk_stops_on_idle", "walk_frame=%u idle_frame=%u moving=%u",
                 (unsigned int)attack_start_x, debug.player.render_frame,
                 debug.player.moving);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_debug_snapshot(&debug);
    paused_time = debug.round_left_ms;
    combat_frames(5U, KEY_PAUSE);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.round_left_ms == paused_time,
                 "pause_freezes_game", "before=%u after=%u",
                 paused_time, debug.round_left_ms);
    (void)save_test_screenshot(simulator.window, "11-paused.bmp");
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.round_left_ms + SIM_FRAME_MS == paused_time,
                 "single_frame_progress", "before=%u after=%u",
                 paused_time, debug.round_left_ms);
    (void)save_test_screenshot(simulator.window, "12-resumed.bmp");

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    for (index = 0U; index < 20U; ++index) {
        combat_frames(4U, KEY_RIGHT);
        run_logic_frame(0U);
        run_logic_frame(KEY_HEAVY);
        combat_frames(15U, 0U);
        sim_plugin_debug_snapshot(&debug);
        if (debug.screen != 4U) break;
    }
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 0U && debug.screen == 5U &&
                 debug.player_rounds == 1U,
                 "complete_round_from_live_combat",
                 "attacks=%u cpu_hp=%u screen=%u rounds=%u-%u",
                 index + 1U, debug.cpu.health, debug.screen,
                 debug.player_rounds, debug.cpu_rounds);
    (void)save_test_screenshot(simulator.window, "13-live-round-finish.bmp");

    sim_plugin_test_reset(100U, 4U, 50U, 100U, false, false);
    run_logic_frame(KEY_LIGHT);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.cpu.health == 0U && debug.screen == 5U &&
                 debug.player_rounds == 1U,
                 "round_finish", "cpu_hp=%u screen=%u rounds=%u-%u",
                 debug.cpu.health, debug.screen, debug.player_rounds,
                 debug.cpu_rounds);
    (void)save_test_screenshot(simulator.window, "14-round-finish.bmp");

    fprintf(combat_report, "\nSUMMARY pass=%u fail=%u\n",
            combat_passes, combat_failures);
    fclose(combat_report);
    stop_plugin();
    DestroyWindow(simulator.window);
    simulator.window = NULL;
    return combat_failures == 0U ? 0 : 1;
}

static int run_lifecycle_test(HINSTANCE instance)
{
    char report_path[MAX_PATH];
    sim_debug_snapshot_t debug;
    uint32_t frozen_time;
    gm_plugin_result_t restart_result;
    simulator.window = create_test_window(instance);
    if (simulator.window == NULL) return 1;
    initialize_host();
    simulator.overlay = true;
    simulator.speed_index = 2U;
    if (!start_plugin()) return 1;
    if (!output_path("lifecycle-results", "report.txt", report_path)) return 1;
    combat_report = fopen(report_path, "w");
    if (combat_report == NULL) return 1;
    combat_passes = 0U;
    combat_failures = 0U;
    fprintf(combat_report,
        "Fighter Arena complete hidden lifecycle test\n"
        "Menus use real v2 input events; screenshots are rendered offscreen.\n\n");

    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 0U && simulator.present_count > 0U,
                 "start_title", "screen=%u presents=%u",
                 debug.screen, (unsigned int)simulator.present_count);
    combat_check(save_lifecycle_screenshot(simulator.window, "01-title.bmp"),
                 "title_screenshot", "01-title.bmp");
    combat_frames(10U, 0U);
    (void)save_lifecycle_screenshot(simulator.window, "01-title-bob.bmp");

    smoke_press(KEY_LIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 1U, "open_difficulty", "screen=%u",
                 debug.screen);
    smoke_press(KEY_LEFT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.difficulty == 0U, "difficulty_left", "value=%u",
                 debug.difficulty);
    smoke_press(KEY_LEFT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.difficulty == 2U, "difficulty_wrap_left",
                 "value=%u", debug.difficulty);
    smoke_press(KEY_RIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.difficulty == 0U, "difficulty_wrap_right",
                 "value=%u", debug.difficulty);
    (void)save_lifecycle_screenshot(simulator.window, "02-difficulty.bmp");

    smoke_press(KEY_LIGHT);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 3U, "begin_match_intro", "screen=%u",
                 debug.screen);
    (void)save_lifecycle_screenshot(simulator.window, "03-round-ready.bmp");
    combat_frames(12U, 0U);
    (void)save_lifecycle_screenshot(simulator.window, "04-fight-call.bmp");
    combat_frames(14U, 0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 4U, "intro_to_fight", "screen=%u",
                 debug.screen);
    (void)save_lifecycle_screenshot(simulator.window, "05-fight.bmp");

    combat_frames(7U, 0U);
    /* Stop feeding snapshots; the plugin must fail safe after 300 ms. */
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    sim_plugin_debug_snapshot(&debug);
    frozen_time = debug.round_left_ms;
    combat_check(debug.input == KEY_PAUSE && debug.pause_reason == 2U,
                 "input_timeout_pauses", "input=0x%03x reason=%u time=%u",
                 debug.input, debug.pause_reason, frozen_time);
    run_unfed_logic_frame();
    run_unfed_logic_frame();
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.round_left_ms == frozen_time,
                 "timeout_pause_freezes", "before=%u after=%u",
                 frozen_time, debug.round_left_ms);
    (void)save_lifecycle_screenshot(simulator.window, "06-input-timeout.bmp");
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.input == 0U && debug.pause_reason == 0U &&
                 debug.round_left_ms + SIM_FRAME_MS == frozen_time,
                 "snapshot_resumes", "input=0x%03x reason=%u time=%u",
                 debug.input, debug.pause_reason, debug.round_left_ms);

    frozen_time = debug.round_left_ms;
    send_connection(false);
    run_unfed_logic_frame();
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.input == KEY_PAUSE && debug.pause_reason == 3U &&
                 debug.round_left_ms == frozen_time,
                 "disconnect_pauses", "input=0x%03x reason=%u time=%u",
                 debug.input, debug.pause_reason, debug.round_left_ms);
    (void)save_lifecycle_screenshot(simulator.window, "07-disconnected.bmp");
    send_connection(true);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.input == 0U && debug.pause_reason == 0U,
                 "reconnect_snapshot_resumes", "input=0x%03x reason=%u",
                 debug.input, debug.pause_reason);

    sim_plugin_test_prepare_player_win();
    run_logic_frame(KEY_LIGHT);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 5U && debug.player_rounds == 1U,
                 "round_one_win", "screen=%u rounds=%u-%u",
                 debug.screen, debug.player_rounds, debug.cpu_rounds);
    (void)save_lifecycle_screenshot(simulator.window, "08-round-one-win.bmp");
    run_logic_frame(0U);
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 3U, "next_round_intro", "screen=%u",
                 debug.screen);
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 4U && debug.player_rounds == 1U,
                 "skip_intro_round_two", "screen=%u rounds=%u-%u",
                 debug.screen, debug.player_rounds, debug.cpu_rounds);

    sim_plugin_test_prepare_player_win();
    run_logic_frame(KEY_LIGHT);
    run_logic_frame(0U);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 5U && debug.player_rounds == 2U,
                 "round_two_win", "screen=%u rounds=%u-%u",
                 debug.screen, debug.player_rounds, debug.cpu_rounds);
    (void)save_lifecycle_screenshot(simulator.window, "09-match-win.bmp");
    run_logic_frame(0U);
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 6U, "champion_screen", "screen=%u",
                 debug.screen);
    (void)sim_plugin_render();
    (void)save_lifecycle_screenshot(simulator.window, "10-champion.bmp");
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 0U, "champion_back_to_title", "screen=%u",
                 debug.screen);

    sim_plugin_test_reset(100U, 105U, 50U, 100U, false, false);
    sim_plugin_test_finish_round(2U);
    smoke_press(KEY_START);
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 4U && debug.cpu_rounds == 1U,
                 "loss_round_two_start", "screen=%u rounds=%u-%u",
                 debug.screen, debug.player_rounds, debug.cpu_rounds);
    sim_plugin_test_finish_round(2U);
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 7U && debug.cpu_rounds == 2U,
                 "game_over_screen", "screen=%u rounds=%u-%u",
                 debug.screen, debug.player_rounds, debug.cpu_rounds);
    (void)sim_plugin_render();
    (void)save_lifecycle_screenshot(simulator.window, "11-game-over.bmp");
    smoke_press(KEY_START);
    sim_plugin_debug_snapshot(&debug);
    combat_check(debug.screen == 0U, "game_over_back_to_title", "screen=%u",
                 debug.screen);

    simulator.plugin.on_stop(simulator.plugin.context);
    restart_result = simulator.plugin.on_start(simulator.plugin.context);
    sim_plugin_debug_snapshot(&debug);
    combat_check(restart_result == GM_PLUGIN_OK && debug.screen == 0U,
                 "stop_start_cycle", "result=%d screen=%u",
                 (int)restart_result, debug.screen);
    send_button(GM_PLUGIN_BUTTON_ACTION_LONG);
    combat_check(!simulator.exit_requested, "long_press_countdown_started",
                 "exit_requested=%u", simulator.exit_requested ? 1U : 0U);
    combat_frames(20U, 0U);
    (void)save_lifecycle_screenshot(simulator.window,
                                    "12-exit-countdown.bmp");
    send_button(GM_PLUGIN_BUTTON_ACTION_RELEASE);
    combat_frames(40U, 0U);
    combat_check(!simulator.exit_requested, "long_press_release_cancels",
                 "exit_requested=%u", simulator.exit_requested ? 1U : 0U);
    send_button(GM_PLUGIN_BUTTON_ACTION_LONG);
    combat_frames(39U, 0U);
    combat_check(!simulator.exit_requested, "long_press_countdown_pending",
                 "exit_requested=%u", simulator.exit_requested ? 1U : 0U);
    combat_frames(1U, 0U);
    combat_check(simulator.exit_requested, "long_press_countdown_exit",
                 "exit_requested=%u", simulator.exit_requested ? 1U : 0U);

    fprintf(combat_report, "\nSUMMARY pass=%u fail=%u\n",
            combat_passes, combat_failures);
    fclose(combat_report);
    stop_plugin();
    DestroyWindow(simulator.window);
    simulator.window = NULL;
    return combat_failures == 0U ? 0 : 1;
}

static int run_window(HINSTANCE instance, int show_command)
{
    WNDCLASSA window_class;
    RECT window_rect = {0, 0,
        SIM_DISPLAY_WIDTH * WINDOW_SCALE + PANEL_WIDTH,
        SIM_DISPLAY_HEIGHT * WINDOW_SCALE};
    MSG message;
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = "GMFighterArenaSimulator";
    if (!RegisterClassA(&window_class)) return 1;
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);
    simulator.window = CreateWindowA(window_class.lpszClassName,
        "GM Fighter Arena Simulator", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL, NULL, instance, NULL);
    if (simulator.window == NULL) return 1;
    initialize_host();
    simulator.speed_index = 2U;
    simulator.overlay = true;
    QueryPerformanceFrequency(&simulator.frequency);
    QueryPerformanceCounter(&simulator.last_counter);
    if (!start_plugin()) return 1;
    ShowWindow(simulator.window, show_command);
    UpdateWindow(simulator.window);
    SetTimer(simulator.window, 1U, 10U, NULL);
    while (GetMessage(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return (int)message.wParam;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
                   int show_command)
{
    (void)previous;
    if (strstr(command_line, "--smoke") != NULL) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE *stream;
            freopen_s(&stream, "CONOUT$", "w", stdout);
            freopen_s(&stream, "CONOUT$", "w", stderr);
        }
        return run_smoke_test();
    }
    if (strstr(command_line, "--test-combat") != NULL)
        return run_combat_test(instance);
    if (strstr(command_line, "--test-lifecycle") != NULL)
        return run_lifecycle_test(instance);
    return run_window(instance, show_command);
}
