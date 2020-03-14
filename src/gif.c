#include "gif.h"

inline void gif_read_header(FILE *file, gif_header *header)
{
    fread(&header->signature,     1, 3, file);
    fread(&header->version,       1, 3, file);
    fread(&header->screen_width,  1, 2, file);
    fread(&header->screen_height, 1, 2, file);
    fread(&header->packed,        1, 1, file);
    fread(&header->background,    1, 1, file);
    fread(&header->aspect_ratio,  1, 1, file);
}

inline void gif_read_global_colortable(FILE *file, gif_img *image)
{
    image->colortable_length = 1L << ((image->header.packed & 0b00000111) + 1);
    image->colortable = calloc(sizeof(gif_color), image->colortable_length);
    for (uint64_t i = 0; i < image->colortable_length; i++) {
        fread(&image->colortable[i].r, 1, 1, file);
        fread(&image->colortable[i].g, 1, 1, file);
        fread(&image->colortable[i].b, 1, 1, file);
    }
}

void gif_lzw_decode(
    uint8_t minimum_code_length,
    const uint8_t *src, const uint64_t src_size,
    uint8_t **dest, uint64_t *dest_size,
    gif_lzw_dict_entry *dict, uint64_t *dict_size,
    gif_color *colortable, uint64_t colortable_size)
{
    (void) minimum_code_length;
    (void) src;
    (void) src_size;
    (void) dest;
    (void) dest_size;
    (void) dict;
    (void) dict_size;
    (void) colortable;
    (void) colortable_size;

    uint8_t cur_code_len = minimum_code_length;

    uint8_t *indices = NULL;
    uint64_t indices_size = 0;

    // Initialize single indicies, clear and termination code
    for (uint64_t i = 0; i < colortable_size; i++) {
        dict[i].code = i;
        dict[i].decomp = malloc(1);
        dict[i].decomp[0] = i;
        dict[i].decomp_size = 1;
    }
    dict[colortable_size].code = colortable_size;
    dict[colortable_size + 1].code = colortable_size + 1;
    *dict_size = colortable_size + 2;

    *dest = malloc(indices_size * 3);
    uint8_t *image_data = *dest;
    for (uint64_t i = 0; i < indices_size; i++) {
        gif_color *current_color = &(colortable[i]);
        image_data[i * 3    ] = current_color->r;
        image_data[i * 3 + 1] = current_color->g;
        image_data[i * 3 + 2] = current_color->b;
    }

    free(indices);
}
