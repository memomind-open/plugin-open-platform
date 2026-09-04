#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"

#define READER_CHANNEL UINT16_C(0x4E52)
#define READER_EVENT_CHANNEL UINT16_C(0x4E53)
#define PROTOCOL_VERSION 1U

#define COMMAND_OPEN 1U
#define COMMAND_WINDOW 2U
#define COMMAND_CONTROL 3U
#define COMMAND_CLOSE 4U
#define COMMAND_CHAPTER 5U
#define COMMAND_IMAGE_BEGIN 6U
#define COMMAND_IMAGE_TILE 7U

#define EVENT_NEED_WINDOW 1U
#define EVENT_PROGRESS 2U
#define EVENT_ACTION 3U
#define EVENT_NEED_IMAGE 4U
#define EVENT_IMAGE_STATUS 5U

#define CONTROL_PLAY 1U
#define CONTROL_PAUSE 2U
#define CONTROL_LINE_UP 3U
#define CONTROL_LINE_DOWN 4U
#define CONTROL_PAGE_UP 5U
#define CONTROL_PAGE_DOWN 6U
#define CONTROL_SET_FONT 7U
#define CONTROL_SET_SPEED 8U
#define CONTROL_SET_MODE 9U
#define CONTROL_SET_PAGE_INTERVAL 10U
#define ACTION_PREVIOUS_CHAPTER 1U
#define ACTION_NEXT_CHAPTER 2U
#define ACTION_BOOKMARK 3U

#define FONT_DEFAULT 0U
#define FONT_LARGE 1U
#define DEFAULT_SPEED 16U
#define MIN_SPEED 2U
#define MAX_SPEED 30U
#define MODE_SCROLL 0U
#define MODE_PAGE 1U
#define DEFAULT_PAGE_INTERVAL_SECONDS 10U
#define MIN_PAGE_INTERVAL_SECONDS 4U
#define MAX_PAGE_INTERVAL_SECONDS 20U

#define WINDOW_BYTES 12288U
#define PAGE_BYTES 4096U
#define MAX_PAGE_LINES 20U
#define VISIBLE_LINES 5
#define HISTORY_PAGES 32U
#define WINDOW_GUARD_BYTES 512U
#define IMAGE_MARKER_BYTES 17U
#define MAX_IMAGE_TILES 64U
#define PAGE_TEXT 0U
#define PAGE_IMAGE 1U
#define IMAGE_STATUS_OK 0U
#define IMAGE_STATUS_INVALID 1U
#define IMAGE_STATUS_DRAW_FAILED 2U
#define CHAPTER_TITLE_BYTES 320U
#define EXIT_HOLD_MS 3000U
#define NOTICE_DURATION_MS 1400U
#define PROGRESS_INTERVAL_MS 1000U
#define SCROLL_FRAME_MS 100U

#define SIDE_MARGIN 20
#define TOP_MARGIN 8
#define BOTTOM_MARGIN 5
#define LINE_SPACE 5

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *root;
    gm_plugin_lvgl_obj_t *viewport;
    gm_plugin_lvgl_obj_t *chapter_label;
    gm_plugin_lvgl_obj_t *footer;
    gm_plugin_lvgl_obj_t *page[2];
    gm_plugin_lvgl_obj_t *notice_label;
    gm_plugin_lvgl_obj_t *exit_label;
    gm_plugin_lvgl_obj_t *exit_arc;
    char window[WINDOW_BYTES + 1U];
    char page_text[2][PAGE_BYTES];
    char chapter_title[CHAPTER_TITLE_BYTES + 1U];
    uint32_t page_line_offset[2][MAX_PAGE_LINES];
    uint32_t history[HISTORY_PAGES];
    uint32_t window_offset;
    uint32_t window_length;
    uint32_t page_start[2];
    uint32_t page_end[2];
    uint32_t page_image_id[2];
    uint32_t image_id;
    uint32_t total_bytes;
    uint32_t session;
    uint32_t requested_offset;
    uint32_t resume_offset;
    uint32_t scroll_fraction;
    uint32_t scroll_elapsed;
    uint32_t page_elapsed;
    uint32_t progress_elapsed;
    uint32_t notice_elapsed;
    uint32_t exit_elapsed;
    int16_t viewport_width;
    int16_t viewport_height;
    uint16_t display_width;
    uint16_t display_height;
    uint16_t image_tile_count;
    uint16_t image_next_tile;
    uint16_t image_next_y;
    int16_t line_height;
    int16_t page_height;
    int16_t scroll_y;
    uint16_t page_lines[2];
    uint8_t current_page;
    uint8_t page_type[2];
    uint8_t history_count;
    uint8_t font_mode;
    uint8_t speed;
    uint8_t reading_mode;
    uint8_t page_interval_seconds;
    uint8_t active;
    uint8_t next_ready;
    uint8_t final_window;
    uint8_t waiting;
    uint8_t waiting_image;
    uint8_t image_receiving;
    uint8_t image_ready;
    uint8_t displaying_image;
    uint8_t playing;
    uint8_t connected;
    uint8_t shown_percent;
    uint8_t exit_sources;
    uint8_t exit_seconds;
} novel_reader_t;

static novel_reader_t reader;

static gm_plugin_lvgl_style_value_t number(int32_t value)
{
    return gm_plugin_lvgl_style_number(value);
}

static gm_plugin_lvgl_style_value_t color(uint8_t value)
{
    return gm_plugin_lvgl_style_color(value);
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static const gm_plugin_lvgl_font_t *active_font(novel_reader_t *self)
{
    return self->font_mode == FONT_LARGE ? self->ui->font_large
                                         : self->ui->font_default;
}

static uint32_t visible_offset(const novel_reader_t *self)
{
    uint8_t slot = self->current_page;
    uint16_t line;
    if (self->active == 0U || self->page_lines[slot] == 0U)
        return self->resume_offset;
    if (self->page_type[slot] == PAGE_IMAGE)
        return self->page_start[slot];
    line = (uint16_t)(self->scroll_y / self->line_height);
    if (line >= self->page_lines[slot]) line = self->page_lines[slot] - 1U;
    return self->page_line_offset[slot][line];
}

static void update_footer(novel_reader_t *self)
{
    char text[16];
    uint32_t percent;
    if (self->active != 0U &&
        self->page_type[self->current_page] == PAGE_IMAGE)
        return;
    percent = self->total_bytes == 0U ? 0U :
        (visible_offset(self) * 100U) / self->total_bytes;
    if (percent > 100U) percent = 100U;
    if (self->shown_percent == (uint8_t)percent) return;
    self->shown_percent = (uint8_t)percent;
    self->libc->snprintf(text, sizeof(text), "%u%%",
                         (unsigned int)percent);
    self->ui->label_set_text(self->footer, text);
}

static void send_need_window(novel_reader_t *self, uint32_t offset)
{
    uint8_t data[12];
    if (self->connected == 0U || self->session == 0U || self->waiting != 0U)
        return;
    data[0] = PROTOCOL_VERSION;
    data[1] = EVENT_NEED_WINDOW;
    write_u32(data + 2, self->session);
    write_u32(data + 6, offset);
    write_u16(data + 10, WINDOW_BYTES);
    if (self->host->bt_send(READER_EVENT_CHANNEL, data, sizeof(data)) ==
        GM_PLUGIN_OK) {
        self->waiting = 1U;
        self->requested_offset = offset;
    }
}

static void send_progress(novel_reader_t *self)
{
    uint8_t data[12];
    if (self->connected == 0U || self->session == 0U) return;
    data[0] = PROTOCOL_VERSION;
    data[1] = EVENT_PROGRESS;
    write_u32(data + 2, self->session);
    write_u32(data + 6, visible_offset(self));
    data[10] = self->playing;
    data[11] = self->font_mode;
    (void)self->host->bt_send(READER_EVENT_CHANNEL, data, sizeof(data));
}

static bool send_action(novel_reader_t *self, uint8_t action)
{
    uint8_t data[11];
    if (self->connected == 0U || self->session == 0U) return false;
    data[0] = PROTOCOL_VERSION;
    data[1] = EVENT_ACTION;
    write_u32(data + 2, self->session);
    data[6] = action;
    write_u32(data + 7, visible_offset(self));
    return self->host->bt_send(READER_EVENT_CHANNEL, data, sizeof(data)) ==
        GM_PLUGIN_OK;
}

static uint8_t hex_value(uint8_t value)
{
    if (value >= (uint8_t)'0' && value <= (uint8_t)'9')
        return (uint8_t)(value - (uint8_t)'0');
    if (value >= (uint8_t)'a' && value <= (uint8_t)'f')
        return (uint8_t)(value - (uint8_t)'a' + 10U);
    if (value >= (uint8_t)'A' && value <= (uint8_t)'F')
        return (uint8_t)(value - (uint8_t)'A' + 10U);
    return 0xffU;
}

static bool parse_image_marker(const novel_reader_t *self, uint32_t cursor,
                               uint32_t *image_id)
{
    static const char prefix[] = "GMIMG:";
    uint32_t value = 0U;
    uint8_t index;
    if (cursor + IMAGE_MARKER_BYTES > self->window_length ||
        (uint8_t)self->window[cursor] != 0x1eU ||
        self->libc->memcmp(self->window + cursor + 1U, prefix, 6U) != 0 ||
        (uint8_t)self->window[cursor + 15U] != 0x1eU ||
        self->window[cursor + 16U] != '\n')
        return false;
    for (index = 0U; index < 8U; ++index) {
        uint8_t digit = hex_value((uint8_t)self->window[cursor + 7U + index]);
        if (digit == 0xffU) return false;
        value = (value << 4) | digit;
    }
    if (value == 0U) return false;
    *image_id = value;
    return true;
}

static void send_need_image(novel_reader_t *self, uint32_t image_id)
{
    uint8_t data[14];
    if (self->connected == 0U || self->session == 0U ||
        self->waiting_image != 0U)
        return;
    data[0] = PROTOCOL_VERSION;
    data[1] = EVENT_NEED_IMAGE;
    write_u32(data + 2, self->session);
    write_u32(data + 6, image_id);
    write_u16(data + 10, self->display_width);
    write_u16(data + 12, self->display_height);
    if (self->host->bt_send(READER_EVENT_CHANNEL, data, sizeof(data)) ==
        GM_PLUGIN_OK)
        self->waiting_image = 1U;
}

static void send_image_status(novel_reader_t *self, uint32_t image_id,
                              uint16_t tile_index, uint8_t status,
                              bool complete)
{
    uint8_t data[14];
    if (self->connected == 0U || self->session == 0U) return;
    data[0] = PROTOCOL_VERSION;
    data[1] = EVENT_IMAGE_STATUS;
    write_u32(data + 2, self->session);
    write_u32(data + 6, image_id);
    write_u16(data + 10, tile_index);
    data[12] = status;
    data[13] = complete ? 1U : 0U;
    (void)self->host->bt_send(READER_EVENT_CHANNEL, data, sizeof(data));
}

static gm_plugin_result_t draw_image_tile(novel_reader_t *self,
                                          const uint8_t *pixels,
                                          uint16_t y, uint16_t height,
                                          uint16_t stride, bool present)
{
    gm_plugin_framebuffer_surface_t surface;
    uint16_t next_y = y;
    uint16_t end_y = (uint16_t)(y + height);
    while (next_y < end_y) {
        uint32_t surface_end;
        uint16_t part_end;
        uint16_t row;
        gm_plugin_result_t result =
            self->host->graphics.framebuffer.lock(next_y, &surface);
        if (result != GM_PLUGIN_OK) return result;
        surface_end = (uint32_t)surface.y + surface.height;
        part_end = surface_end > end_y ? end_y : (uint16_t)surface_end;
        if (surface.pixels == 0 || surface.height == 0U ||
            surface.width < self->display_width || surface.stride < stride ||
            surface.y > next_y || surface_end <= next_y ||
            surface_end > self->display_height || part_end <= next_y) {
            (void)self->host->graphics.framebuffer.unlock(0, false);
            return GM_PLUGIN_EINVAL;
        }
        for (row = next_y; row < part_end; ++row) {
            const uint8_t *source = pixels + (uint32_t)(row - y) * stride;
            uint8_t *target = surface.pixels +
                (uint32_t)(row - surface.y) * surface.stride;
            self->libc->memcpy(target, source, stride);
        }
        {
            gm_plugin_rect_t dirty = {
                .x = 0,
                .y = (int16_t)next_y,
                .width = self->display_width,
                .height = (uint16_t)(part_end - next_y),
            };
            result = self->host->graphics.framebuffer.unlock(
                &dirty, present && part_end == end_y);
        }
        if (result != GM_PLUGIN_OK) return result;
        next_y = part_end;
    }
    return GM_PLUGIN_OK;
}

static void show_notice(novel_reader_t *self, const char *text)
{
    if (self->notice_label == 0 || self->displaying_image != 0U) return;
    self->ui->label_set_text(self->notice_label, text);
    self->ui->obj_clear_flag(self->notice_label,
                             GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->notice_elapsed = NOTICE_DURATION_MS;
}

static void position_pages(novel_reader_t *self)
{
    uint8_t next = (uint8_t)(1U - self->current_page);
    if (self->page_type[self->current_page] == PAGE_TEXT) {
        self->ui->obj_clear_flag(self->page[self->current_page],
                                 GM_PLUGIN_LVGL_FLAG_HIDDEN);
        self->ui->obj_set_pos(self->page[self->current_page], 0,
                              (int16_t)-self->scroll_y);
    } else {
        self->ui->obj_add_flag(self->page[self->current_page],
                               GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
    if (self->next_ready != 0U && self->page_type[next] == PAGE_TEXT) {
        self->ui->obj_clear_flag(self->page[next],
                                 GM_PLUGIN_LVGL_FLAG_HIDDEN);
        self->ui->obj_set_pos(self->page[next], 0,
                              (int16_t)(self->page_height - self->scroll_y));
    } else {
        self->ui->obj_add_flag(self->page[next], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    }
}

static void set_page_font(novel_reader_t *self, uint8_t slot)
{
    gm_plugin_lvgl_style_value_t font = {0};
    font.ptr = active_font(self);
    self->ui->style_set(self->page[slot], GM_PLUGIN_LVGL_STYLE_TEXT_FONT,
                        font, GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->page[slot],
                        GM_PLUGIN_LVGL_STYLE_TEXT_LINE_SPACE,
                        number(LINE_SPACE), GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void calculate_layout(novel_reader_t *self)
{
    int16_t lines;
    int16_t maximum_lines;
    int16_t header_y;
    int16_t viewport_y;
    self->line_height = (int16_t)(self->ui->font_get_line_height(
        active_font(self)) + LINE_SPACE);
    if (self->line_height < 1) self->line_height = 1;
    maximum_lines = (int16_t)(self->viewport_height / self->line_height - 1);
    lines = self->reading_mode == MODE_PAGE ? maximum_lines : VISIBLE_LINES;
    if (lines > maximum_lines) lines = maximum_lines;
    if (lines < 2) lines = 2;
    if (lines > (int16_t)MAX_PAGE_LINES) lines = MAX_PAGE_LINES;
    self->page_height = (int16_t)(lines * self->line_height);
    viewport_y = (int16_t)(TOP_MARGIN + self->viewport_height -
        self->page_height);
    header_y = (int16_t)(viewport_y - self->line_height);
    if (header_y < TOP_MARGIN) header_y = TOP_MARGIN;
    self->ui->obj_set_pos(self->chapter_label, SIDE_MARGIN, header_y);
    self->ui->obj_set_size(self->chapter_label,
                           (int16_t)(self->viewport_width - 100),
                           self->line_height);
    self->ui->obj_set_pos(self->footer,
                          (int16_t)(SIDE_MARGIN + self->viewport_width - 92),
                          header_y);
    self->ui->obj_set_size(self->footer, 92, self->line_height);
    self->ui->obj_set_pos(self->viewport, SIDE_MARGIN, viewport_y);
    self->ui->obj_set_size(self->viewport, self->viewport_width,
                           self->page_height);
    set_page_font(self, 0);
    set_page_font(self, 1);
}

static bool build_page(novel_reader_t *self, uint8_t slot,
                       uint32_t start_offset)
{
    uint32_t local;
    uint32_t cursor;
    uint32_t output = 0;
    uint16_t lines = 0;
    uint16_t maximum_lines = (uint16_t)(self->page_height / self->line_height);
    if (start_offset < self->window_offset ||
        start_offset > self->window_offset + self->window_length)
        return false;
    local = start_offset - self->window_offset;
    if (local >= self->window_length) return false;
    cursor = local;
    while (lines < maximum_lines && cursor < self->window_length) {
        uint32_t step;
        uint32_t image_id;
        if (parse_image_marker(self, cursor, &image_id)) {
            if (lines != 0U) break;
            self->page_text[slot][0] = '\0';
            self->page_start[slot] = start_offset;
            self->page_end[slot] = self->window_offset + cursor +
                IMAGE_MARKER_BYTES;
            self->page_line_offset[slot][0] = start_offset;
            self->page_lines[slot] = 1U;
            self->page_type[slot] = PAGE_IMAGE;
            self->page_image_id[slot] = image_id;
            self->ui->label_set_text(self->page[slot], "");
            self->ui->obj_add_flag(self->page[slot],
                                   GM_PLUGIN_LVGL_FLAG_HIDDEN);
            return true;
        }
        if (self->final_window == 0U &&
            self->window_length - cursor < WINDOW_GUARD_BYTES)
            return false;
        self->page_line_offset[slot][lines] = self->window_offset + cursor;
        step = self->ui->text_get_next_line(
            self->window + cursor, active_font(self), 0,
            self->viewport_width, 0, GM_PLUGIN_LVGL_TEXT_FLAG_NONE);
        if (step == 0U || cursor + step > self->window_length ||
            output + step >= PAGE_BYTES)
            return false;
        self->libc->memcpy(self->page_text[slot] + output,
                           self->window + cursor, step);
        output += step;
        cursor += step;
        ++lines;
    }
    if (lines == 0U) return false;
    self->page_text[slot][output] = '\0';
    self->page_start[slot] = start_offset;
    self->page_end[slot] = self->window_offset + cursor;
    self->page_lines[slot] = lines;
    self->page_type[slot] = PAGE_TEXT;
    self->page_image_id[slot] = 0U;
    self->ui->label_set_text(self->page[slot], self->page_text[slot]);
    self->ui->obj_set_size(self->page[slot], self->viewport_width,
                           self->page_height);
    self->ui->obj_clear_flag(self->page[slot], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    return true;
}

static void request_seek(novel_reader_t *self, uint32_t offset)
{
    if (self->displaying_image != 0U) {
        self->displaying_image = 0U;
        self->ui->obj_invalidate(self->root);
    }
    self->active = 0U;
    self->next_ready = 0U;
    self->resume_offset = offset;
    self->scroll_y = 0;
    self->scroll_fraction = 0U;
    self->scroll_elapsed = 0U;
    self->page_elapsed = 0U;
    self->waiting = 0U;
    self->waiting_image = 0U;
    self->image_receiving = 0U;
    self->image_ready = 0U;
    self->ui->obj_add_flag(self->page[0], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_add_flag(self->page[1], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    send_need_window(self, offset);
}

static void prepare_following_page(novel_reader_t *self)
{
    uint8_t next = (uint8_t)(1U - self->current_page);
    uint32_t offset = self->page_end[self->current_page];
    if (offset >= self->total_bytes) {
        self->next_ready = 0U;
        self->playing = 0U;
        self->ui->obj_add_flag(self->page[next], GM_PLUGIN_LVGL_FLAG_HIDDEN);
        update_footer(self);
        return;
    }
    if (build_page(self, next, offset)) {
        self->next_ready = 1U;
        position_pages(self);
        return;
    }
    self->next_ready = 0U;
    self->ui->obj_add_flag(self->page[next], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->waiting = 0U;
    send_need_window(self, offset);
}

static void activate_current_page(novel_reader_t *self)
{
    uint8_t slot = self->current_page;
    if (self->page_type[slot] == PAGE_IMAGE) {
        self->image_ready = 0U;
        self->image_receiving = 0U;
        self->waiting_image = 0U;
        self->image_id = self->page_image_id[slot];
        send_need_image(self, self->image_id);
        return;
    }
    self->image_receiving = 0U;
    self->waiting_image = 0U;
    self->image_ready = 0U;
    if (self->displaying_image != 0U) {
        self->displaying_image = 0U;
        self->ui->obj_invalidate(self->root);
    }
}

static void promote_page(novel_reader_t *self)
{
    if (self->next_ready == 0U) return;
    if (self->history_count < HISTORY_PAGES)
        self->history[self->history_count++] =
            self->page_start[self->current_page];
    else {
        self->libc->memmove(self->history, self->history + 1,
                            (HISTORY_PAGES - 1U) * sizeof(self->history[0]));
        self->history[HISTORY_PAGES - 1U] =
            self->page_start[self->current_page];
    }
    self->current_page = (uint8_t)(1U - self->current_page);
    self->scroll_y = 0;
    self->scroll_fraction = 0U;
    self->page_elapsed = 0U;
    self->next_ready = 0U;
    prepare_following_page(self);
    position_pages(self);
    activate_current_page(self);
    update_footer(self);
    send_progress(self);
}

static void previous_page(novel_reader_t *self);

static void advance_pixels(novel_reader_t *self, int16_t pixels)
{
    int32_t next;
    if (self->active == 0U) return;
    if (self->page_type[self->current_page] == PAGE_IMAGE) {
        if (pixels > 0 && self->next_ready != 0U) promote_page(self);
        else if (pixels < 0) previous_page(self);
        return;
    }
    next = (int32_t)self->scroll_y + pixels;
    if (next < 0) next = 0;
    while (next >= self->page_height && self->next_ready != 0U) {
        next -= self->page_height;
        promote_page(self);
    }
    if (next >= self->page_height) next = self->page_height - self->line_height;
    self->scroll_y = (int16_t)next;
    position_pages(self);
    update_footer(self);
}

static void previous_page(novel_reader_t *self)
{
    uint32_t offset;
    if (self->history_count == 0U) {
        advance_pixels(self, (int16_t)-self->scroll_y);
        return;
    }
    offset = self->history[--self->history_count];
    request_seek(self, offset);
}

static void reset_private_data(novel_reader_t *self)
{
    if (self->displaying_image != 0U && self->root != 0)
        self->ui->obj_invalidate(self->root);
    self->libc->memset(self->window, 0, sizeof(self->window));
    self->libc->memset(self->page_text, 0, sizeof(self->page_text));
    self->libc->memset(self->chapter_title, 0,
                       sizeof(self->chapter_title));
    self->libc->memset(self->page_line_offset, 0,
                       sizeof(self->page_line_offset));
    self->libc->memset(self->page_image_id, 0,
                       sizeof(self->page_image_id));
    self->libc->memset(self->page_type, 0, sizeof(self->page_type));
    self->libc->memset(self->history, 0, sizeof(self->history));
    self->window_offset = 0U;
    self->window_length = 0U;
    self->page_start[0] = self->page_start[1] = 0U;
    self->page_end[0] = self->page_end[1] = 0U;
    self->page_lines[0] = self->page_lines[1] = 0U;
    self->history_count = 0U;
    self->active = 0U;
    self->next_ready = 0U;
    self->final_window = 0U;
    self->waiting = 0U;
    self->waiting_image = 0U;
    self->image_receiving = 0U;
    self->image_ready = 0U;
    self->displaying_image = 0U;
    self->image_id = 0U;
    self->image_tile_count = 0U;
    self->image_next_tile = 0U;
    self->image_next_y = 0U;
    self->scroll_y = 0;
    self->scroll_fraction = 0U;
    self->scroll_elapsed = 0U;
    self->page_elapsed = 0U;
    self->progress_elapsed = 0U;
    self->notice_elapsed = 0U;
    self->shown_percent = 0xffU;
    if (self->page[0] != 0) self->ui->label_set_text(self->page[0], "");
    if (self->page[1] != 0) self->ui->label_set_text(self->page[1], "");
    if (self->chapter_label != 0)
        self->ui->label_set_text(self->chapter_label, "");
    if (self->notice_label != 0)
        self->ui->obj_add_flag(self->notice_label,
                               GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static void show_exit(novel_reader_t *self)
{
    char text[24];
    uint8_t seconds = (uint8_t)((EXIT_HOLD_MS - self->exit_elapsed + 999U) /
                                1000U);
    if (seconds == self->exit_seconds) return;
    self->exit_seconds = seconds;
    self->libc->snprintf(text, sizeof(text), "EXIT IN %u",
                         (unsigned int)seconds);
    self->ui->label_set_text(self->exit_label, text);
    self->ui->obj_clear_flag(self->exit_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_clear_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
}

static void set_exit_source(novel_reader_t *self, uint8_t source, bool active)
{
    if (active) {
        if (self->exit_sources == 0U) {
            self->exit_elapsed = 0U;
            self->exit_seconds = 0xffU;
        }
        self->exit_sources |= source;
        show_exit(self);
    } else {
        self->exit_sources &= (uint8_t)~source;
        if (self->exit_sources == 0U) {
            self->exit_elapsed = 0U;
            self->ui->obj_add_flag(self->exit_label,
                                   GM_PLUGIN_LVGL_FLAG_HIDDEN);
            self->ui->obj_add_flag(self->exit_arc,
                                   GM_PLUGIN_LVGL_FLAG_HIDDEN);
        }
    }
}

static bool handle_open(novel_reader_t *self, const uint8_t *data,
                        uint32_t length)
{
    if (length < 16U || length > 18U) return false;
    reset_private_data(self);
    self->session = read_u32(data + 2);
    self->total_bytes = read_u32(data + 6);
    self->resume_offset = read_u32(data + 10);
    self->font_mode = data[14] == FONT_LARGE ? FONT_LARGE : FONT_DEFAULT;
    self->speed = data[15];
    if (self->speed < MIN_SPEED) self->speed = DEFAULT_SPEED;
    if (self->speed > MAX_SPEED) self->speed = MAX_SPEED;
    self->reading_mode = length >= 17U && data[16] == MODE_PAGE
        ? MODE_PAGE : MODE_SCROLL;
    self->page_interval_seconds = length >= 18U ? data[17]
        : DEFAULT_PAGE_INTERVAL_SECONDS;
    if (self->page_interval_seconds < MIN_PAGE_INTERVAL_SECONDS ||
        self->page_interval_seconds > MAX_PAGE_INTERVAL_SECONDS)
        self->page_interval_seconds = DEFAULT_PAGE_INTERVAL_SECONDS;
    if (self->session == 0U || self->total_bytes == 0U ||
        self->resume_offset >= self->total_bytes)
        return false;
    self->playing = 1U;
    self->current_page = 0U;
    calculate_layout(self);
    update_footer(self);
    send_need_window(self, self->resume_offset);
    return true;
}

static bool handle_window(novel_reader_t *self, const uint8_t *data,
                          uint32_t length)
{
    uint32_t session;
    uint32_t offset;
    uint32_t text_length;
    uint8_t target;
    if (length <= 11U) return false;
    session = read_u32(data + 2);
    offset = read_u32(data + 6);
    text_length = length - 11U;
    if (session != self->session || text_length > WINDOW_BYTES ||
        (self->waiting != 0U && offset != self->requested_offset))
        return false;
    self->libc->memcpy(self->window, data + 11, text_length);
    self->window[text_length] = '\0';
    self->window_offset = offset;
    self->window_length = text_length;
    self->final_window = data[10] != 0U;
    self->waiting = 0U;
    if (self->active == 0U) {
        if (!build_page(self, 0U, self->resume_offset)) return false;
        self->current_page = 0U;
        self->active = 1U;
        self->scroll_y = 0;
        prepare_following_page(self);
        position_pages(self);
        activate_current_page(self);
        update_footer(self);
        send_progress(self);
        return true;
    }
    target = (uint8_t)(1U - self->current_page);
    if (!build_page(self, target, offset)) return false;
    self->next_ready = 1U;
    position_pages(self);
    return true;
}

static bool handle_control(novel_reader_t *self, const uint8_t *data,
                           uint32_t length)
{
    uint8_t command;
    uint16_t value;
    if (length != 9U || read_u32(data + 2) != self->session) return false;
    command = data[6];
    value = read_u16(data + 7);
    switch (command) {
    case CONTROL_PLAY:
        self->playing = 1U;
        show_notice(self, "Playing");
        break;
    case CONTROL_PAUSE:
        self->playing = 0U;
        show_notice(self, "Paused");
        break;
    case CONTROL_LINE_UP:
        self->page_elapsed = 0U;
        advance_pixels(self, (int16_t)-self->line_height);
        break;
    case CONTROL_LINE_DOWN:
        self->page_elapsed = 0U;
        advance_pixels(self, self->line_height);
        break;
    case CONTROL_PAGE_UP: previous_page(self); break;
    case CONTROL_PAGE_DOWN: advance_pixels(self, self->page_height); break;
    case CONTROL_SET_FONT:
    {
        uint32_t offset = visible_offset(self);
        self->font_mode = value == FONT_LARGE ? FONT_LARGE : FONT_DEFAULT;
        calculate_layout(self);
        request_seek(self, offset);
        break;
    }
    case CONTROL_SET_SPEED:
        if (value >= MIN_SPEED && value <= MAX_SPEED) self->speed = (uint8_t)value;
        break;
    case CONTROL_SET_MODE:
    {
        uint32_t offset = visible_offset(self);
        self->reading_mode = value == MODE_PAGE ? MODE_PAGE : MODE_SCROLL;
        calculate_layout(self);
        self->scroll_fraction = 0U;
        self->scroll_elapsed = 0U;
        self->page_elapsed = 0U;
        request_seek(self, offset);
        break;
    }
    case CONTROL_SET_PAGE_INTERVAL:
        if (value >= MIN_PAGE_INTERVAL_SECONDS &&
            value <= MAX_PAGE_INTERVAL_SECONDS) {
            self->page_interval_seconds = (uint8_t)value;
            self->page_elapsed = 0U;
        }
        break;
    default: return false;
    }
    update_footer(self);
    send_progress(self);
    return true;
}

static bool handle_chapter(novel_reader_t *self, const uint8_t *data,
                           uint32_t length)
{
    uint32_t title_length;
    if (length < 6U || length > 6U + CHAPTER_TITLE_BYTES ||
        read_u32(data + 2) != self->session)
        return false;
    title_length = length - 6U;
    self->libc->memcpy(self->chapter_title, data + 6, title_length);
    self->chapter_title[title_length] = '\0';
    self->ui->label_set_text(self->chapter_label, self->chapter_title);
    return true;
}

static bool handle_image_begin(novel_reader_t *self, const uint8_t *data,
                               uint32_t length)
{
    uint32_t image_id;
    uint16_t width;
    uint16_t height;
    uint16_t tile_count;
    if (length != 16U || read_u32(data + 2) != self->session ||
        self->active == 0U ||
        self->page_type[self->current_page] != PAGE_IMAGE)
        return false;
    image_id = read_u32(data + 6);
    width = read_u16(data + 10);
    height = read_u16(data + 12);
    tile_count = read_u16(data + 14);
    if (image_id == 0U || image_id != self->page_image_id[self->current_page] ||
        width != self->display_width || height != self->display_height ||
        tile_count == 0U || tile_count > MAX_IMAGE_TILES)
        return false;
    self->image_id = image_id;
    self->image_tile_count = tile_count;
    self->image_next_tile = 0U;
    self->image_next_y = 0U;
    self->image_receiving = 1U;
    self->image_ready = 0U;
    self->waiting_image = 0U;
    return true;
}

static bool handle_image_tile(novel_reader_t *self, const uint8_t *data,
                              uint32_t length)
{
    uint32_t image_id;
    uint16_t tile_index;
    uint16_t y;
    uint16_t height;
    uint16_t stride;
    bool final;
    uint8_t status = IMAGE_STATUS_INVALID;
    gm_plugin_result_t result;
    if (length < 19U || read_u32(data + 2) != self->session) return false;
    image_id = read_u32(data + 6);
    tile_index = read_u16(data + 10);
    y = read_u16(data + 12);
    height = read_u16(data + 14);
    stride = read_u16(data + 16);
    final = data[18] != 0U;
    if (self->image_receiving == 0U || image_id != self->image_id ||
        tile_index != self->image_next_tile || y != self->image_next_y ||
        height == 0U || y + height > self->display_height ||
        stride != (self->display_width + 1U) / 2U ||
        length - 19U != (uint32_t)height * stride ||
        final != (tile_index + 1U == self->image_tile_count) ||
        (final && y + height != self->display_height)) {
        send_image_status(self, image_id, tile_index, status, false);
        return false;
    }
    result = draw_image_tile(self, data + 19U, y, height, stride, final);
    if (result != GM_PLUGIN_OK) {
        self->image_receiving = 0U;
        send_image_status(self, image_id, tile_index,
                          IMAGE_STATUS_DRAW_FAILED, false);
        return false;
    }
    self->image_next_tile++;
    self->image_next_y = (uint16_t)(y + height);
    status = IMAGE_STATUS_OK;
    if (final) {
        self->image_receiving = 0U;
        self->image_ready = 1U;
        self->displaying_image = 1U;
        self->page_elapsed = 0U;
    }
    send_image_status(self, image_id, tile_index, status, final);
    return true;
}

static bool receive_message(novel_reader_t *self, const uint8_t *data,
                            uint32_t length)
{
    if (data == 0 || length < 2U || data[0] != PROTOCOL_VERSION) return false;
    switch (data[1]) {
    case COMMAND_OPEN: return handle_open(self, data, length);
    case COMMAND_WINDOW: return handle_window(self, data, length);
    case COMMAND_CONTROL: return handle_control(self, data, length);
    case COMMAND_CHAPTER: return handle_chapter(self, data, length);
    case COMMAND_IMAGE_BEGIN: return handle_image_begin(self, data, length);
    case COMMAND_IMAGE_TILE: return handle_image_tile(self, data, length);
    case COMMAND_CLOSE:
        if (length != 6U || read_u32(data + 2) != self->session) return false;
        reset_private_data(self);
        self->session = 0U;
        self->total_bytes = 0U;
        self->ui->label_set_text(self->footer, "");
        return true;
    default: return false;
    }
}

static gm_plugin_result_t create_ui(novel_reader_t *self)
{
    gm_plugin_display_info_t display;
    gm_plugin_result_t result = self->host->display_get_info(&display);
    if (result != GM_PLUGIN_OK) return result;
    if (display.width < 500U || display.height < 300U ||
        display.pixel_format != GM_PLUGIN_PIXEL_GRAY_4)
        return GM_PLUGIN_ENOTSUP;
    self->display_width = display.width;
    self->display_height = display.height;
    self->viewport_width = (int16_t)(display.width - SIDE_MARGIN * 2);
    self->viewport_height = (int16_t)(display.height - TOP_MARGIN - BOTTOM_MARGIN);
    self->root = self->ui->root_get();
    if (self->root == 0) return GM_PLUGIN_ESTATE;
    self->ui->obj_clean(self->root);
    self->chapter_label = self->ui->label_create(self->root);
    self->footer = self->ui->label_create(self->root);
    self->viewport = self->ui->obj_create(self->root);
    self->page[0] = self->ui->label_create(self->viewport);
    self->page[1] = self->ui->label_create(self->viewport);
    self->notice_label = self->ui->label_create(self->root);
    self->exit_label = self->ui->label_create(self->root);
    self->exit_arc = self->ui->arc_create(self->root);
    if (self->chapter_label == 0 || self->footer == 0 || self->viewport == 0 ||
        self->page[0] == 0 || self->page[1] == 0 ||
        self->notice_label == 0 ||
        self->exit_label == 0 || self->exit_arc == 0)
        return GM_PLUGIN_ENOMEM;
    self->ui->obj_set_pos(self->viewport, SIDE_MARGIN, TOP_MARGIN);
    self->ui->obj_set_size(self->viewport, self->viewport_width,
                           self->viewport_height);
    self->ui->obj_clear_flag(self->viewport, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_BG_COLOR,
                        color(0x00U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_BG_OPA,
                        number(GM_PLUGIN_LVGL_OPA_COVER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_PAD_TOP,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_PAD_BOTTOM,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_PAD_LEFT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_PAD_RIGHT,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_RADIUS,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->viewport, GM_PLUGIN_LVGL_STYLE_OUTLINE_WIDTH,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->label_set_long_mode(self->chapter_label,
                                  GM_PLUGIN_LVGL_LABEL_CLIP);
    self->ui->style_set(self->chapter_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_LEFT),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->chapter_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xC0U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->label_set_long_mode(self->footer, GM_PLUGIN_LVGL_LABEL_CLIP);
    self->ui->style_set(self->footer, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_RIGHT),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->footer, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xC0U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->label_set_long_mode(self->page[0], GM_PLUGIN_LVGL_LABEL_WRAP);
    self->ui->label_set_long_mode(self->page[1], GM_PLUGIN_LVGL_LABEL_WRAP);
    self->ui->style_set(self->page[0], GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->page[1], GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_add_flag(self->page[0], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_add_flag(self->page[1], GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_set_size(self->notice_label, 240, 28);
    self->ui->obj_align(self->notice_label, GM_PLUGIN_LVGL_ALIGN_TOP_MID,
                        0, 14);
    self->ui->style_set(self->notice_label,
                        GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->notice_label,
                        GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xFFU),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_add_flag(self->notice_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_set_size(self->exit_label, 180, 30);
    self->ui->obj_align(self->exit_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 34);
    self->ui->style_set(self->exit_label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
                        number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER),
                        GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->exit_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->obj_add_flag(self->exit_label, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->obj_set_size(self->exit_arc, 52, 52);
    self->ui->obj_align(self->exit_arc, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, -18);
    self->ui->arc_set_range(self->exit_arc, 0, EXIT_HOLD_MS);
    self->ui->arc_set_value(self->exit_arc, 0);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_WIDTH,
                        number(4), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_COLOR,
                        color(0x30U), GM_PLUGIN_LVGL_SELECTOR_MAIN);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_WIDTH,
                        number(4), GM_PLUGIN_LVGL_SELECTOR_INDICATOR);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_ARC_COLOR,
                        color(0xFFU), GM_PLUGIN_LVGL_SELECTOR_INDICATOR);
    self->ui->style_set(self->exit_arc, GM_PLUGIN_LVGL_STYLE_OPA,
                        number(0), GM_PLUGIN_LVGL_SELECTOR_KNOB);
    self->ui->obj_add_flag(self->exit_arc, GM_PLUGIN_LVGL_FLAG_HIDDEN);
    self->ui->label_set_text(self->chapter_label, "");
    self->ui->label_set_text(self->footer, "");
    return GM_PLUGIN_OK;
}

static gm_plugin_result_t on_start(void *opaque)
{
    novel_reader_t *self = opaque;
    self->font_mode = FONT_DEFAULT;
    self->speed = DEFAULT_SPEED;
    self->reading_mode = MODE_SCROLL;
    self->page_interval_seconds = DEFAULT_PAGE_INTERVAL_SECONDS;
    self->playing = 0U;
    self->connected = 1U;
    self->session = 0U;
    self->exit_sources = 0U;
    return create_ui(self);
}

static void on_loop(void *opaque, uint32_t elapsed_ms)
{
    novel_reader_t *self = opaque;
    if (self->notice_elapsed != 0U) {
        if (elapsed_ms >= self->notice_elapsed) {
            self->notice_elapsed = 0U;
            self->ui->obj_add_flag(self->notice_label,
                                   GM_PLUGIN_LVGL_FLAG_HIDDEN);
        } else {
            self->notice_elapsed -= elapsed_ms;
        }
    }
    if (self->exit_sources != 0U) {
        self->exit_elapsed += elapsed_ms;
        if (self->exit_elapsed >= EXIT_HOLD_MS) {
            reset_private_data(self);
            self->host->app_exit();
            return;
        }
        self->ui->arc_set_value(self->exit_arc, (int16_t)self->exit_elapsed);
        show_exit(self);
        return;
    }
    if (self->active != 0U && self->playing != 0U &&
        self->page_type[self->current_page] == PAGE_TEXT &&
        self->reading_mode == MODE_SCROLL) {
        uint32_t pixels;
        self->page_elapsed = 0U;
        self->scroll_elapsed += elapsed_ms;
        if (self->scroll_elapsed > 1000U) self->scroll_elapsed = 1000U;
        if (self->scroll_elapsed >= SCROLL_FRAME_MS) {
            self->scroll_fraction +=
                (uint32_t)self->speed * self->scroll_elapsed;
            self->scroll_elapsed = 0U;
            pixels = self->scroll_fraction / 1000U;
            self->scroll_fraction %= 1000U;
            if (pixels != 0U) advance_pixels(self, (int16_t)pixels);
        }
    } else if (self->active != 0U && self->playing != 0U &&
               (self->page_type[self->current_page] == PAGE_TEXT ||
                self->image_ready != 0U)) {
        uint32_t interval = (uint32_t)self->page_interval_seconds * 1000U;
        self->scroll_elapsed = 0U;
        self->scroll_fraction = 0U;
        if (self->page_elapsed < interval) self->page_elapsed += elapsed_ms;
        if (self->page_elapsed >= interval && self->next_ready != 0U) {
            self->page_elapsed = 0U;
            promote_page(self);
        }
    } else {
        self->scroll_elapsed = 0U;
        self->page_elapsed = 0U;
    }
    self->progress_elapsed += elapsed_ms;
    if (self->progress_elapsed >= PROGRESS_INTERVAL_MS) {
        self->progress_elapsed %= PROGRESS_INTERVAL_MS;
        send_progress(self);
    }
}

static bool handle_button(novel_reader_t *self,
                          const gm_plugin_event_t *event)
{
    gm_plugin_button_t button = event->data.button.button;
    gm_plugin_button_action_t action = event->data.button.action;
    if (button == GM_PLUGIN_BUTTON_PRIMARY) {
        if (action == GM_PLUGIN_BUTTON_ACTION_SINGLE) {
            self->playing = self->playing == 0U ? 1U : 0U;
            show_notice(self, self->playing != 0U ? "Playing" : "Paused");
            update_footer(self);
            send_progress(self);
        } else if (action == GM_PLUGIN_BUTTON_ACTION_DOUBLE) {
            if (send_action(self, ACTION_BOOKMARK))
                show_notice(self, "Bookmark Added");
        } else if (action == GM_PLUGIN_BUTTON_ACTION_LONG ||
                   action == GM_PLUGIN_BUTTON_ACTION_VERY_LONG) {
            set_exit_source(self, 1U, true);
        } else if (action == GM_PLUGIN_BUTTON_ACTION_RELEASE) {
            set_exit_source(self, 1U, false);
        }
        return true;
    }
    if (action != GM_PLUGIN_BUTTON_ACTION_TRIGGER) return false;
    self->page_elapsed = 0U;
    switch (button) {
    case GM_PLUGIN_BUTTON_UP:
    case GM_PLUGIN_BUTTON_SCROLL_UP:
        if (self->scroll_y == 0) previous_page(self);
        else advance_pixels(self, (int16_t)-self->line_height);
        break;
    case GM_PLUGIN_BUTTON_DOWN:
    case GM_PLUGIN_BUTTON_SCROLL_DOWN:
        advance_pixels(self, self->line_height); break;
    case GM_PLUGIN_BUTTON_PAGE_UP: previous_page(self); break;
    case GM_PLUGIN_BUTTON_PAGE_DOWN: advance_pixels(self, self->page_height); break;
    case GM_PLUGIN_BUTTON_LEFT: send_action(self, ACTION_PREVIOUS_CHAPTER); break;
    case GM_PLUGIN_BUTTON_RIGHT: send_action(self, ACTION_NEXT_CHAPTER); break;
    case GM_PLUGIN_BUTTON_BACK:
    case GM_PLUGIN_BUTTON_HOME:
        reset_private_data(self);
        self->host->app_exit();
        break;
    default: return false;
    }
    send_progress(self);
    return true;
}

static bool on_event(void *opaque, const gm_plugin_event_t *event)
{
    novel_reader_t *self = opaque;
    if (event == 0) return false;
    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != READER_CHANNEL) return false;
        return receive_message(self, event->data.bt.data,
                               event->data.bt.length);
    }
    if (event->type == GM_PLUGIN_EVENT_BUTTON)
        return handle_button(self, event);
    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        self->connected = event->data.connection.connected ? 1U : 0U;
        if (self->connected == 0U) {
            reset_private_data(self);
            self->session = 0U;
            self->total_bytes = 0U;
            self->ui->label_set_text(self->footer, "");
        }
        return true;
    }
    return false;
}

static void on_suspend(void *opaque)
{
    novel_reader_t *self = opaque;
    reset_private_data(self);
    self->session = 0U;
    self->total_bytes = 0U;
}

static void on_resume(void *opaque)
{
    novel_reader_t *self = opaque;
    self->ui->label_set_text(self->footer, "");
}

static void on_stop(void *opaque)
{
    novel_reader_t *self = opaque;
    reset_private_data(self);
    self->session = 0U;
    self->total_bytes = 0U;
    self->exit_sources = 0U;
    if (self->root != 0) self->ui->obj_clean(self->root);
    self->root = 0;
    self->viewport = 0;
    self->chapter_label = 0;
    self->footer = 0;
    self->page[0] = self->page[1] = 0;
    self->notice_label = 0;
    self->exit_label = 0;
    self->exit_arc = 0;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                   gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required = GM_PLUGIN_CAP_DISPLAY_BITMAP |
        GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_BLUETOOTH;
    if (host == 0 || plugin == 0 ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->display_get_info == 0 || host->graphics.lvgl == 0 ||
        host->graphics.framebuffer.lock == 0 ||
        host->graphics.framebuffer.unlock == 0 ||
        host->bt_send == 0 || host->app_exit == 0 ||
        (host->capabilities & required) != required ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE)
        return GM_PLUGIN_ENOTSUP;
    reader.host = host;
    reader.ui = host->graphics.lvgl;
    if (gm_plugin_libc_get(host, &reader.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    if (reader.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(reader.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_ENOTSUP;
    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &reader;
    plugin->on_start = on_start;
    plugin->on_resume = on_resume;
    plugin->on_loop = on_loop;
    plugin->on_event = on_event;
    plugin->on_suspend = on_suspend;
    plugin->on_stop = on_stop;
    return GM_PLUGIN_OK;
}
