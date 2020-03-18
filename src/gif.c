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
    const uint8_t min_code_len,
    const uint8_t *src,         const uint64_t src_size,
    uint8_t **dest,             uint64_t *dest_size,
    gif_lzw_dict_entry *dict,   uint64_t *dict_size,
    gif_color *colortable,      uint64_t colortable_size)
{
    uint8_t cur_code_len = min_code_len;

    uint8_t *cols = calloc(src_size, 1);
    uint64_t cols_size = 0, cols_reserved = src_size;

    // Last two entries of initial dictionary
    uint16_t clear = 1 << cur_code_len,
             eoi = clear + 1;

    // Initialize single indicies, clear and termination code
    for (uint64_t i = 0; i < (uint64_t) (eoi + 1); i++) {
        dict[i].code = i;
        dict[i].decomp = malloc(1);
        dict[i].decomp[0] = i;
        dict[i].decomp_size = 1;
    }

    *dict_size = eoi + 1;

    // Decompress the image data
    uint64_t src_index = 0,
             max_entries = 0;
    uint32_t bytes = 0;
    uint8_t  cur_bit = 0,
             mask = 0,
             code = 0;
    // LZW
    gif_lzw_dict_entry *last = NULL;
    uint64_t dict_base = *dict_size,
             dict_base_offset = 0;

    #define DEBUG 1
    int initialized = 0, decompressing = 1;
    while (src_index < src_size && decompressing) {
        if (!initialized) {
            cur_code_len--;
            bytes = (src[src_index+2] << 16) | (src[src_index+1] << 8) | src[src_index];
            mask = (1 << cur_code_len) - 1;
            code = (bytes >> cur_bit) & mask;
            #if DEBUG
            printf("Code length: %"PRIu8"\n", cur_code_len);
            printf("Code read:   %"PRIu8"\n", code);
            #endif

            last = NULL;
            for (uint64_t i = 0; i < *dict_size; i++) {
                if (code == dict[i].code) {
                    last = &(dict[i]);
                    break;
                }
            }
            assert(last != NULL);

            cur_bit += cur_code_len;
            if (cur_bit >= 8) {
                cur_bit -= 8;
                src_index++;
            }

            // Only valid if 2^code length is equal to colortable length
            if (code != clear && code != eoi) {
                if (cols_size + 1 > cols_reserved) {
                    uint8_t *new_cols = NULL;
                    new_cols = realloc(cols, cols_size + src_size);
                    assert(new_cols != NULL);
                    cols = new_cols;
                    cols_reserved += src_size;
                }
                cols[cols_size] = last->decomp[0];
                cols_size++;
            }

            cur_code_len++;
            max_entries = 1 << (cur_code_len + 1);
            #if DEBUG
            printf("Code length: %"PRIu8"\n", cur_code_len);
            #endif

            initialized = 1;
        }

        bytes = (src[src_index+2] << 16) | (src[src_index+1] << 8) | src[src_index];
        mask = (1 << cur_code_len) - 1;
        code = (bytes >> cur_bit) & mask;
        cur_bit += cur_code_len;
        #if DEBUG
        printf("Code read:   %"PRIu8"\n", code);
        #endif

        if (cur_bit >= 8) {
            cur_bit -= 8;
            src_index++;
        }

        if (code == clear) {
            #if DEBUG
            printf("Clear code read!\n");
            #endif

            cur_code_len = min_code_len;

            for (uint64_t i = eoi + 1; i < *dict_size; i++) {
                free(dict[i].decomp);
            }

            dict_base_offset = 0;
            *dict_size = colortable_size;
            initialized = 0;
        }
        else if (code == eoi) {
            #if DEBUG
            printf("End of information code read!\n");
            #endif

            decompressing = 0;
        }
        else {
            gif_lzw_dict_entry *entry = NULL;

            for (uint64_t i = 0; i < *dict_size; i++) {
                if (dict[i].code == code) {
                    entry = &dict[i];
                    break;
                } 
            }

            gif_lzw_dict_entry *new = &dict[*dict_size];
            new->code        = dict_base + dict_base_offset;
            new->decomp      = malloc(last->decomp_size + 1);
            new->decomp_size = last->decomp_size + 1;
            dict_base_offset++;
            memcpy(new->decomp, last->decomp, last->decomp_size);
            if (entry != NULL) {
                new->decomp[last->decomp_size] = entry->decomp[0];
            }
            else {
                new->decomp[last->decomp_size] = last->decomp[0];
                entry = new;
            }

            (*dict_size)++;

            memcpy(cols + cols_size, entry->decomp, entry->decomp_size);
            cols_size += entry->decomp_size;

            last = entry;

            if (*dict_size >= max_entries) {
                cur_code_len++;
                max_entries = 1 << (cur_code_len + 1);
            }
        }
    }

    *dest = calloc(cols_size, 3);
    *dest_size = cols_size * 3;
    uint8_t *image_data = *dest;
    for (uint64_t i = 0; i < cols_size; i++) {
        gif_color *current_color = &(colortable[i]);
        image_data[i*3  ] = current_color->r;
        image_data[i*3+1] = current_color->g;
        image_data[i*3+2] = current_color->b;
    }

    free(cols);
}
