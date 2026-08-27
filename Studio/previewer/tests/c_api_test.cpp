#include "previewer_c_api.h"
#include "previewer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

void append_be16(std::vector<uint8_t> &bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value));
}

void append_be32(std::vector<uint8_t> &bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value >> 24));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value));
}

std::vector<uint8_t> lz4_literal_block(size_t size, uint8_t value)
{
    std::vector<uint8_t> output;
    output.push_back(static_cast<uint8_t>(std::min<size_t>(size, 15) << 4));
    if (size >= 15) {
        size_t remaining = size - 15;
        while (remaining >= 255) {
            output.push_back(255);
            remaining -= 255;
        }
        output.push_back(static_cast<uint8_t>(remaining));
    }
    output.insert(output.end(), size, value);
    return output;
}

std::vector<uint8_t> plugin_message_payload(size_t size)
{
    std::vector<uint8_t> payload(size);
    for (size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<uint8_t>((index * 37u + 11u) & 0xffu);
    }
    return payload;
}

int longest_bright_run_start(const std::vector<uint8_t> &frame, size_t row)
{
    int best_start = -1;
    int best_length = 0;
    int start = -1;
    for (int x = 0; x <= 600; ++x) {
        const bool bright = x < 600 && frame[row * 600u + static_cast<size_t>(x)] != 0;
        if (bright && start < 0) start = x;
        if (!bright && start >= 0) {
            const int length = x - start;
            if (length > best_length) {
                best_start = start;
                best_length = length;
            }
            start = -1;
        }
    }
    return best_length >= 70 ? best_start : -1;
}

} // namespace

int main(int argc, char **argv)
{
    gm_preview_handle *handle = gm_preview_create();
    assert(handle != nullptr);
    assert(gm_preview_is_loaded(handle) == 0);
    assert(gm_preview_is_running(handle) == 0);
    assert(gm_preview_frame_size() == 600u * 350u);

    std::vector<uint8_t> frame(gm_preview_frame_size());
    assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
    assert(gm_preview_load(handle, "/path/that/does/not/exist.gmp") == 0);
    assert(gm_preview_last_error(handle)[0] != '\0');

    if (argc > 1) {
        assert(gm_preview_load(handle, argv[1]) == 1);
        assert(gm_preview_start(handle) == 1);
        assert(gm_preview_tick(handle, 33) == 1);
        const uint8_t input[] = {2, 1, 0, 0};
        int handled = 0;
        assert(gm_preview_send_bluetooth(handle, 0x4647, input, sizeof(input), &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        assert(gm_preview_send_button(handle, 3, 1, &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_is_running(handle) == 0);
        assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_is_running(handle) == 0);
        assert(gm_preview_stop(handle) == 1);
    }

    if (argc > 2) {
        assert(gm_preview_load(handle, argv[2]) == 1);
        assert(gm_preview_start(handle) == 1);
        const uint8_t nonce[] = {1, 3, 3, 7};
        int handled = 0;
        assert(gm_preview_send_bluetooth(handle, 0x7ffe, nonce, sizeof(nonce), &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_outbox_count(handle) == 1);
        uint8_t service = 0;
        uint8_t command = 0;
        assert(gm_preview_outbox_service(handle, 0, &service) == 1);
        assert(gm_preview_outbox_command(handle, 0, &command) == 1);
        assert(service == 0x0f);
        assert(command == 0x29);
        uint16_t channel = 0;
        assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
        assert(channel == 0x7ffe);
        assert(gm_preview_outbox_payload_size(handle, 0) == sizeof(nonce));
        std::vector<uint8_t> echo(sizeof(nonce));
        assert(gm_preview_copy_outbox_payload(handle, 0, echo.data(), echo.size()) == 1);
        assert(echo == std::vector<uint8_t>(nonce, nonce + sizeof(nonce)));
        assert(gm_preview_clear_outbox(handle) == 1);

        constexpr std::array<size_t, 3> boundary_sizes = {65535u, 65536u, 81901u};
        for (const size_t payload_size : boundary_sizes) {
            const std::vector<uint8_t> payload = plugin_message_payload(payload_size);
            assert(gm_preview_send_bluetooth(handle, 0x7ffe, payload.data(), payload.size(), &handled) == 1);
            assert(handled == 1);
            assert(gm_preview_outbox_count(handle) == 1);
            assert(gm_preview_outbox_service(handle, 0, &service) == 1);
            assert(gm_preview_outbox_command(handle, 0, &command) == 1);
            assert(service == 0x0f);
            assert(command == 0x29);
            assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
            assert(channel == 0x7ffe);
            assert(gm_preview_outbox_payload_size(handle, 0) == payload.size());
            std::vector<uint8_t> echoed(payload.size());
            assert(gm_preview_copy_outbox_payload(handle, 0, echoed.data(), echoed.size()) == 1);
            assert(echoed == payload);
            assert(gm_preview_clear_outbox(handle) == 1);
        }

        const uint8_t text[] = {
            1,                   // element id
            0, 30, 0, 30,       // x, y
            2, 28, 0, 68,       // width, height
            2, 12,              // border, radius
            'M', 'O', 'M', 'O', ' ', 'R', 'E', 'A', 'D', 'Y',
        };
        assert(gm_preview_send_bluetooth(handle, 2, text, sizeof(text), &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        assert(std::any_of(frame.begin(), frame.end(), [](uint8_t value) { return value != 0; }));

        const uint32_t frame_id = 1;
        std::vector<uint8_t> begin;
        append_be32(begin, frame_id);
        append_be16(begin, 2);
        assert(gm_preview_send_bluetooth(handle, 8, begin.data(), begin.size(), &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_outbox_count(handle) == 1);
        assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
        assert(channel == 0x0104);
        assert(gm_preview_clear_outbox(handle) == 1);

        const uint8_t malformed_begin[] = {0, 0, 0, 2, 0};
        assert(gm_preview_send_bluetooth(handle, 8, malformed_begin,
                                         sizeof(malformed_begin), &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_outbox_count(handle) == 1);
        assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
        assert(channel == 0x0104);
        assert(gm_preview_clear_outbox(handle) == 1);

        constexpr uint16_t width = 200, height = 175, stride = width / 2;
        constexpr uint32_t decoded_size = stride * height;
        std::vector<uint8_t> tile;
        append_be32(tile, frame_id);
        append_be16(tile, 0);
        append_be16(tile, 0);
        append_be16(tile, 0);
        append_be16(tile, width);
        append_be16(tile, height);
        append_be16(tile, stride);
        append_be32(tile, decoded_size);
        const std::vector<uint8_t> compressed = lz4_literal_block(decoded_size, 0x77);
        tile.insert(tile.end(), compressed.begin(), compressed.end());
        if (gm_preview_send_bluetooth(handle, 9, tile.data(), tile.size(), &handled) != 1) {
            std::fprintf(stderr, "atomic tile failed: %s\n", gm_preview_last_error(handle));
            assert(false);
        }
        assert(handled == 1);
        assert(gm_preview_outbox_count(handle) == 1);
        assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
        assert(channel == 0x0104);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        assert(frame[0] == 0);
        assert(frame[(height - 1) * 600u + width - 1] == 0);
        assert(gm_preview_clear_outbox(handle) == 1);

        std::vector<uint8_t> final_tile;
        append_be32(final_tile, frame_id);
        append_be16(final_tile, 1);
        append_be16(final_tile, width);
        append_be16(final_tile, 0);
        append_be16(final_tile, width);
        append_be16(final_tile, height);
        append_be16(final_tile, stride);
        append_be32(final_tile, decoded_size);
        const std::vector<uint8_t> final_compressed = lz4_literal_block(decoded_size, 0x88);
        final_tile.insert(final_tile.end(), final_compressed.begin(), final_compressed.end());
        assert(gm_preview_send_bluetooth(handle, 9, final_tile.data(), final_tile.size(), &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_outbox_count(handle) == 1);
        assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
        assert(channel == 0x0104);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        assert(frame[0] == 0x77);
        assert(frame[(height - 1) * 600u + width - 1] == 0x77);
        assert(frame[width] == 0x88);
        assert(frame[(height - 1) * 600u + width * 2u - 1] == 0x88);
        assert(gm_preview_clear_outbox(handle) == 1);

        assert(gm_preview_send_button(handle, 1, 1, &handled) == 1);
        assert(handled == 1);
        assert(gm_preview_outbox_count(handle) == 1);
        assert(gm_preview_outbox_channel(handle, 0, &channel) == 1);
        assert(channel == 0x0100);
        assert(gm_preview_clear_outbox(handle) == 1);

        for (int index = 0; index < 4; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_set_direction_vector(handle, -1000, 0, 1, &handled) == 1);
        assert(handled == 1);
        for (int index = 0; index < 4; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_set_direction_vector(handle, 0, 0, 0, &handled) == 1);
        assert(handled == 1);
        for (int index = 0; index < 6; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        bool saw_direction_start = false;
        bool saw_direction_end = false;
        const size_t direction_message_count = gm_preview_outbox_count(handle);
        for (size_t index = 0; index < direction_message_count; ++index) {
            assert(gm_preview_outbox_channel(handle, index, &channel) == 1);
            if (channel != 0x0101) continue;
            assert(gm_preview_outbox_payload_size(handle, index) == 13);
            std::array<uint8_t, 13> gesture_payload{};
            assert(gm_preview_copy_outbox_payload(handle, index,
                                                   gesture_payload.data(),
                                                   gesture_payload.size()) == 1);
            const uint16_t gesture = static_cast<uint16_t>(
                (static_cast<uint16_t>(gesture_payload[10]) << 8) |
                gesture_payload[11]);
            if (gesture != 7) continue;
            if (gesture_payload[12] != 0) saw_direction_start = true;
            else if (saw_direction_start) saw_direction_end = true;
        }
        assert(saw_direction_start);
        assert(saw_direction_end);
        assert(gm_preview_clear_outbox(handle) == 1);
        assert(gm_preview_stop(handle) == 1);
    }

    if (argc > 3) {
        assert(gm_preview_load(handle, argv[3]) == 1);
        assert(gm_preview_start(handle) == 1);
        assert(gm_preview_stop(handle) == 1);
    }

    if (argc > 4) {
        assert(gm_preview_load(handle, argv[4]) == 1);
        assert(gm_preview_start(handle) == 1);
        assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        assert(gm_preview_text_overlay_count(handle) == 0);
        assert(std::any_of(frame.begin(), frame.end(), [](uint8_t pixel) {
            return pixel != 0;
        }));
        assert(gm_preview_stop(handle) == 1);
    }

    if (argc > 5) {
        int handled = 0;
        assert(gm_preview_load(handle, argv[5]) == 1);
        assert(gm_preview_start(handle) == 1);
        assert(gm_preview_tick(handle, 66) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        const int center_x = longest_bright_run_start(frame, 312);
        assert(center_x >= 0);

        assert(gm_preview_set_direction_vector(handle, -1000, 0, 1, &handled) == 1);
        assert(handled == 1);
        for (int index = 0; index < 6; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        const int held_left_x = longest_bright_run_start(frame, 312);
        assert(held_left_x >= 0 && held_left_x < center_x);

        assert(gm_preview_set_direction_vector(handle, 0, 0, 0, &handled) == 1);
        assert(handled == 1);
        for (int index = 0; index < 8; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        const int released_left_x = longest_bright_run_start(frame, 312);
        assert(released_left_x >= 0 && released_left_x <= held_left_x);
        assert(gm_preview_stop(handle) == 1);

        assert(gm_preview_load(handle, argv[5]) == 1);
        assert(gm_preview_start(handle) == 1);
        assert(gm_preview_tick(handle, 66) == 1);
        assert(gm_preview_set_direction_vector(handle, 1000, 0, 1, &handled) == 1);
        assert(handled == 1);
        for (int index = 0; index < 6; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_copy_frame(handle, frame.data(), frame.size()) == 1);
        const int held_right_x = longest_bright_run_start(frame, 312);
        assert(held_right_x > center_x);
        assert(gm_preview_set_direction_vector(handle, 0, 0, 0, &handled) == 1);
        assert(handled == 1);
        for (int index = 0; index < 8; ++index)
            assert(gm_preview_tick(handle, 33) == 1);
        assert(gm_preview_stop(handle) == 1);
    }

    if (argc > 6) {
        gmpreview::Previewer jet_runner;
        jet_runner.loadFile(argv[6]);
        jet_runner.start();
        jet_runner.tick(50);
        assert(jet_runner.device().pitch == 0);

        assert(jet_runner.setDirectionVector(0, -1000, true));
        for (int index = 0; index < 10; ++index) jet_runner.tick(50);
        assert(jet_runner.device().pitch == 18);
        assert(jet_runner.setDirectionVector(0, 0, false));
        for (int index = 0; index < 6; ++index) jet_runner.tick(33);
        assert(jet_runner.device().pitch == 18);

        assert(jet_runner.setDirectionVector(0, 1000, true));
        for (int index = 0; index < 10; ++index) jet_runner.tick(50);
        assert(jet_runner.device().pitch == 0);
        assert(jet_runner.setDirectionVector(0, 0, false));
        for (int index = 0; index < 6; ++index) jet_runner.tick(33);
        assert(jet_runner.device().pitch == 0);
        jet_runner.stop();
    }

    gm_preview_destroy(handle);
    return 0;
}
