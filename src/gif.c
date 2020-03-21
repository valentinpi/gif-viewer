#include "gif.h"

void gif_read_header(FILE *file, gif_header *header)
{
    fread(&header->signature,     1, 3, file);
    fread(&header->version,       1, 3, file);
    fread(&header->screen_width,  1, 2, file);
    fread(&header->screen_height, 1, 2, file);
    fread(&header->packed,        1, 1, file);
    fread(&header->background,    1, 1, file);
    fread(&header->aspect_ratio,  1, 1, file);
}

void gif_read_global_colortable(FILE *file, gif_img *image)
{
    image->colortable_length = 1L << ((image->header.packed & 0b00000111) + 1);
    image->colortable = calloc(sizeof(gif_color), image->colortable_length);
    for (uint64_t i = 0; i < image->colortable_length; i++) {
        fread(&image->colortable[i].r, 1, 1, file);
        fread(&image->colortable[i].g, 1, 1, file);
        fread(&image->colortable[i].b, 1, 1, file);
    }
}

void gif_read_ext_graphicsblock(FILE *file, gif_ext_graphicsblock *graphics)
{
    graphics->introducer = 0x21;
    graphics->label      = 0xF9;
    graphics->block_size = 0x04;
    fseek(file, 1, SEEK_CUR);
    fread(&graphics->packed,      1, 1, file);
    fread(&graphics->delay,       1, 2, file);
    fread(&graphics->color_index, 1, 1, file);
    graphics->terminator = 0x00;
    fseek(file, 1, SEEK_CUR);
}

typedef struct {
    uint16_t code;
    uint8_t  *decomp;
    uint64_t decomp_size;
} gif_lzw_dict_entry;

uint16_t gif_read_code(
    const uint8_t *src,
    uint64_t *byte,
    uint8_t *bit,
    const uint8_t code_len)
{
    uint32_t bytes = src[(*byte) + 2] << 16 | src[(*byte) + 1] << 8 | src[*byte];
    uint16_t mask = (1 << code_len) - 1;
    uint16_t res = (bytes >> (*bit)) & mask;
    (*bit) += code_len;

    // Adjust bit and byte count
    uint8_t quot = (*bit) / 8, mod = (*bit) % 8;
    (*bit) = mod;
    (*byte) += quot;

    return res;
}

void gif_decode(
    const uint8_t min_code_len,
    const uint8_t *src,         const uint64_t src_size,
    uint8_t **dest,             uint64_t *dest_size,
    gif_color *colortable,      uint64_t colortable_size)
{
    gif_lzw_dict_entry *dict = calloc(4096, sizeof(gif_lzw_dict_entry));
    uint64_t dict_size = 0;

    uint8_t *cols = malloc(src_size);
    uint64_t cols_size = 0, cols_reserved = src_size;
    
    uint8_t cur_code_len = min_code_len + 1;

    // Last two entries of initial dictionary
    const uint16_t clear = 1 << min_code_len,
                   eoi   = clear + 1;

    int initialized = 0;
    if (dict_size == 0) {
        // Initialize single indicies, clear and termination code
        for (uint64_t i = 0; i < colortable_size; i++) {
            dict[i].code = i;
            dict[i].decomp = malloc(1);
            dict[i].decomp[0] = i;
            dict[i].decomp_size = 1;
        }
        dict_size = eoi + 1;
    }

    // Reading codes
    uint64_t src_index = 0;
    uint8_t  cur_bit = 0;
    uint16_t code = 0;

    // LZW and dictionary reset
    gif_lzw_dict_entry *last = NULL;
    uint64_t dict_base = eoi + 1,
             dict_base_offset = dict_size - dict_base;
    uint64_t max_entries = 0;

    while (src_index < src_size) {
        if (!initialized) {
            code = gif_read_code(src, &src_index, &cur_bit, cur_code_len);
            if (code == clear) {
                code = gif_read_code(src, &src_index, &cur_bit, cur_code_len);
            }
            if (code == eoi) {
                code = gif_read_code(src, &src_index, &cur_bit, cur_code_len);
            }

            last = NULL;
            for (uint64_t i = 0; i < dict_size; i++) {
                if (code == dict[i].code) {
                    last = &(dict[i]);
                    break;
                }
            }
            if (last == NULL) {
                last = &(dict[0]);
            }

            if (cols_size + last->decomp_size > cols_reserved) {
                cols = realloc(cols, cols_reserved + 4096);
                assert(cols != NULL);
                cols_reserved += 4096;
            }
            cols[cols_size] = last->decomp[0];
            cols_size++;

            max_entries = 1 << cur_code_len;

            initialized = 1;
        }

        code = gif_read_code(src, &src_index, &cur_bit, cur_code_len);
        if (code == clear) {
            cur_code_len = min_code_len + 1;

            for (uint64_t i = dict_base; i < dict_size; i++) {
                free(dict[i].decomp);
                dict[i].code = 0;
                dict[i].decomp = NULL;
                dict[i].decomp_size = 0;
            }

            dict_size = dict_base;
            dict_base_offset = 0;
            initialized = 0;
        }
        else if (code == eoi) {
            break;
        }
        else {
            if (dict_size >= 4096) {
                continue;
            }

            gif_lzw_dict_entry *entry = NULL;

            for (uint64_t i = 0; i < dict_size; i++) {
                if (dict[i].code == code) {
                    entry = &dict[i];
                    break;
                } 
            }

            gif_lzw_dict_entry *new = &dict[dict_base + dict_base_offset];
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
            
            dict_size++;

            if (cols_size + entry->decomp_size > cols_reserved) {
                cols = realloc(cols, cols_reserved + 4096);
                assert(cols != NULL);
                cols_reserved += 4096;
            }
            memcpy(cols + cols_size, entry->decomp, entry->decomp_size);
            cols_size += entry->decomp_size;

            last = entry;

            if (dict_size >= max_entries && dict_size < 4096) {
                cur_code_len++;
                max_entries = (1 << cur_code_len);
            }
        }
    }


    if (cols_size + last->decomp_size > cols_reserved) {
        cols = realloc(cols, cols_size + 4096);
        assert(cols != NULL);
        cols_reserved += 4096;
    }
    memcpy(cols + cols_size, last->decomp, last->decomp_size);
    cols_size += last->decomp_size;

    *dest = calloc(cols_size, 4);
    *dest_size = cols_size * 4;
    uint8_t *image_data = *dest;
    for (uint64_t i = 0; i < cols_size; i++) {
        uint8_t color_index = cols[i];
        gif_color *color = &(colortable[color_index]);
        image_data[i*4  ] = color->r;
        image_data[i*4+1] = color->g;
        image_data[i*4+2] = color->b;
        image_data[i*4+3] = 255;
    }

    free(cols);

    for (uint64_t i = 0; i < dict_size; i++) {
        free(dict[i].decomp);
    }
    free(dict);
}
