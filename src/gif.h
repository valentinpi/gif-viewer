#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    uint16_t code;
    uint8_t  *decomp;
    uint64_t decomp_size;
} gif_lzw_dict_entry;

void gif_read_header(FILE *file, gif_header *header);
void gif_read_global_colortable(FILE *file, gif_img *image);
// Since you can't easily predict the final decompressed size, pass a pointer with value NULL for dest
void gif_lzw_decode(
    const uint8_t min_code_len,
    const uint8_t *src,         const uint64_t src_size,
    uint8_t **dest,             uint64_t *dest_size,
    gif_lzw_dict_entry *dict,   uint64_t *dict_size,
    gif_color *colortable,      uint64_t colortable_size);
