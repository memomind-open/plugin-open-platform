#ifndef SOKOBAN_SPRITES_H
#define SOKOBAN_SPRITES_H

#include <stdint.h>

#define SOKOBAN_SPRITE_SIZE 16U

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint8_t gray;
} sokoban_sprite_rect_t;

/* Shared 16 x 16 pixel-art resources. Rectangles are ordered back to front
 * and scaled once when the LVGL objects are created. */
static const sokoban_sprite_rect_t sokoban_box_sprite[] = {
    {1, 1, 14, 14, 0x38},
    {2, 2, 12, 12, 0x78},
    {3, 3, 10, 2, 0xD0},
    {3, 5, 2, 8, 0xB0},
    {11, 5, 2, 8, 0x28},
    {5, 5, 2, 2, 0xD0},
    {9, 5, 2, 2, 0xD0},
    {6, 7, 2, 2, 0xC0},
    {8, 7, 2, 2, 0xC0},
    {7, 9, 2, 2, 0xC0},
    {5, 11, 2, 2, 0xC0},
    {9, 11, 2, 2, 0xC0},
    {3, 13, 10, 1, 0x50},
};

static const sokoban_sprite_rect_t sokoban_player_sprite[] = {
    {5, 1, 6, 5, 0xD8},
    {5, 1, 6, 2, 0x38},
    {4, 2, 2, 3, 0x48},
    {10, 2, 2, 3, 0x48},
    {6, 3, 1, 1, 0x08},
    {9, 3, 1, 1, 0x08},
    {7, 5, 2, 1, 0x98},
    {5, 6, 6, 6, 0xA8},
    {6, 6, 4, 2, 0xE8},
    {3, 7, 2, 5, 0xD8},
    {11, 7, 2, 5, 0xD8},
    {2, 11, 3, 2, 0xD8},
    {11, 11, 3, 2, 0xD8},
    {5, 12, 3, 3, 0x68},
    {8, 12, 3, 3, 0x68},
    {4, 14, 4, 1, 0xE0},
    {8, 14, 4, 1, 0xE0},
};

#define SOKOBAN_BOX_SPRITE_RECT_COUNT \
    (sizeof(sokoban_box_sprite) / sizeof(sokoban_box_sprite[0]))
#define SOKOBAN_PLAYER_SPRITE_RECT_COUNT \
    (sizeof(sokoban_player_sprite) / sizeof(sokoban_player_sprite[0]))

#endif
