#pragma once

#include <inttypes.h>

// Not used at the moment
#define GIF_SEPARATOR  0x2C
#define GIF_CONTROL    0x21
#define GIF_TERMINATOR 0
#define GIF_TRAILER    0x3B

typedef struct {
    // ACTUAL HEADER (6 bytes)
    uint8_t  signature[3];
    uint8_t  version[3];
    uint16_t screen_width;
    uint16_t screen_height;
    // LOGICAL SCREEN DESCRIPTORS (3 bytes)
    uint8_t packed;
    uint8_t background;
    // We can calculate this using
    // (aspect_ratio + 15) / 64
    uint8_t aspect_ratio;
} gif_header;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} gif_color;

typedef struct {
    uint8_t  separator; // Always 0x2C
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint8_t  packed;
} gif_imgdesc;

typedef struct {
    gif_imgdesc imgdesc;
    gif_color*  colortable;
    uint64_t    colortable_length;
} gif_imgblock;

typedef struct {
    gif_header    header;
    gif_color*    colortable;
    uint64_t      colortable_length;
    gif_imgblock* blocks;
    uint64_t      block_count;
} gif_img;
