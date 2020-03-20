#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

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
    uint8_t  introducer; // Always 0x21
    uint8_t  label;      // Always 0xF9
    uint8_t  block_size; // Always 0x04
    uint8_t  packed;
    uint16_t delay;
    uint8_t  color_index;
    uint8_t  terminator; // Always 0x00
} gif_ext_graphicsblock;

typedef struct {
    gif_ext_graphicsblock graphics;
    gif_imgdesc           imgdesc;
    gif_color             *colortable;
    uint64_t              colortable_length;
    SDL_Texture           *image;
} gif_imgblock;

typedef struct {
    gif_header    header;
    gif_color     *colortable;
    uint64_t      colortable_length;
    gif_imgblock  *blocks;
    uint64_t      blocks_count;
    uint64_t      blocks_reserved;
} gif_img;

// Helper structs
typedef struct {
    uint16_t code;
    uint8_t  *decomp;
    uint64_t decomp_size;
} gif_lzw_dict_entry;

void gif_read_header(FILE *file, gif_header *header);
void gif_read_global_colortable(FILE *file, gif_img *image);
// Assumes the label was already read
void gif_read_ext_graphicsblock(FILE *file, gif_ext_graphicsblock *graphics);

// src        should NOT be an invalid pointer
// dest       should be a pointer to a pointer of value NULL
// dict       should be an array of 4096 entries
// colortable should NOT be empty
void gif_decode(
    const uint8_t min_code_len,
    const uint8_t *src,         const uint64_t src_size,
    uint8_t **dest,             uint64_t *dest_size,
    gif_lzw_dict_entry *dict,   uint64_t *dict_size,
    gif_color *colortable,      uint64_t colortable_size);
