// Source: https://www.fileformat.info/format/gif/egff.htm

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

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
    gif_imgdesc imgdesc;
    gif_color*  colortable;
    uint64_t    colortable_length;
} gif_block;

typedef struct {
    gif_header header;
    gif_color* colortable;
    uint64_t   colortable_length;
    gif_block* blocks;
    uint64_t   block_count;
} gif_image;

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    gif_image image;
    image.colortable = NULL;
    image.colortable_length = 0;
    image.blocks = NULL;
    image.block_count = 0;

    // Will skip 0x1A (CTRL+Z byte) on Windows if b is not set
    FILE *file = fopen64("../epicpolarbear.gif", "r+b");

    if (file == NULL) {
        fprintf(stderr, "../epicpolarbear.gif could not be opened!\n");
        return EXIT_FAILURE;
    }

    fread(&image.header.signature,     1, 3, file);
    fread(&image.header.version,       1, 3, file);
    fread(&image.header.screen_width,  1, 2, file);
    fread(&image.header.screen_height, 1, 2, file);
    fread(&image.header.packed,        1, 1, file);
    fread(&image.header.background,    1, 1, file);
    fread(&image.header.aspect_ratio,  1, 1, file);

    printf("----- HEADER OF epicpolarbear.gif -----\n");
    printf("signature:     %c%c%c\n",    image.header.signature[0],
                                         image.header.signature[1],
                                         image.header.signature[2]);
    printf("version:       %c%c%c\n",    image.header.version[0],
                                         image.header.version[1],
                                         image.header.version[2]);
    printf("screen_width:  %"PRIu16"\n", image.header.screen_width);
    printf("screen_height: %"PRIu16"\n", image.header.screen_height);
    printf("\n");

    printf("----- LOGICAL SCREEN DESCRIPTORS -----\n");
    printf("packed:        %"PRIu8"\n",  image.header.packed);
    printf("background:    %"PRIu8"\n",  image.header.background);
    printf("aspect_ratio:  %"PRIu8"\n",  image.header.aspect_ratio);
    printf("\n");

    printf("--- PACKED INFORMATION ---\n");
    printf("colortable_size:  %"PRIu8"\n", image.header.packed & 0b00000111);
    printf("sort_flag:        %"PRIu8"\n", image.header.packed & 0b00001000 >> 3);
    printf("color_resolution: %"PRIu8"\n", image.header.packed & 0b01110000 >> 4);
    printf("colortable_flag:  %"PRIu8"\n", image.header.packed & 0b10000000 >> 7);
    printf("\n");

    uint64_t global_colortable_length = 1L << ((image.header.packed & 0b00000111) + 1);
    image.colortable = calloc(sizeof(gif_color), global_colortable_length);
    for (uint64_t i = 0; i < global_colortable_length; i++) {
        fread(&image.colortable[i].r, 1, 1, file);
        fread(&image.colortable[i].g, 1, 1, file);
        fread(&image.colortable[i].b, 1, 1, file);

        //printf("global_colortable[%3"PRIu64"]: %2X%2X%2X\n", i,
        //    image.colortable[i].r,
        //    image.colortable[i].g,
        //    image.colortable[i].b);
    }
    //printf("\n");

    printf("--- GLOBAL COLOR TABLE ---\n");
    printf("length: %"PRIu64"\n", global_colortable_length);
    printf("\n");

    // For a color table of size 256, a position of 781 in the file after initial setup would make sense:
    // 13 byte header + 3 r/g/b values * 256 colors = 13 + 768 = 781

    // Start reading image and extension blocks
    int parsing = 1;
    while (parsing) {
        uint8_t magic = 0;
        fread(&magic, 1, 1, file);

        if (magic == GIF_TRAILER) {
            printf("FOUND GIF TRAILER AT %ld\n", ftell(file) - 1);
            parsing = 0;
            continue;
        }
        // In case of a control block, just skip it for the moment
        else if (magic == GIF_CONTROL) {
            // Read label byte
            fread(&magic, 1, 1, file);
            // Skip accordingly
            switch (magic) {
                // Graphics Control Extension Block
                case 0xF9:
                    printf("FOUND GRAPHICS CONTROL EXTENSION BLOCK AT %ld\n", ftell(file) - 2);
                    fseek(file, 6, SEEK_CUR);
                    break;
                // Plain Text Extension Block
                case 0x1:
                    printf("FOUND PLAIN TEXT EXTENSION BLOCK AT %ld\n", ftell(file) - 2);
                    fseek(file, 13, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                    while (magic != 0) {
                        fseek(file, magic, SEEK_CUR);
                        fread(&magic, 1, 1, file);
                    }
                    break;
                // Application Extension Block
                case 0xFF:
                    printf("FOUND APPLICATION EXTENSION BLOCK AT %ld\n", ftell(file) - 2);
                    fseek(file, 12, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                    while (magic != 0) {
                        fseek(file, magic, SEEK_CUR);
                        fread(&magic, 1, 1, file);
                    }
                    break;
                // Comment Extension Block
                case 0xFE:
                    printf("FOUND COMMENT EXTENSION BLOCK AT %ld\n", ftell(file) - 2);
                    fread(&magic, 1, 1, file);
                    while (magic != 0) {
                        fseek(file, magic, SEEK_CUR);
                        fread(&magic, 1, 1, file);
                    }
                    break;
                default:
                    printf("FOUND UNKNOWN EXTENSION BLOCK AT %ld\n", ftell(file) - 2);
                    break;
            }
            
            printf("SKIPPED CONTROL BLOCK TO %ld\n", ftell(file) - 1);
        }
        else if (magic == GIF_SEPARATOR) {
            gif_block *new_blocks = realloc(image.blocks, sizeof(gif_block) * (image.block_count + 1));
            assert(new_blocks != NULL);
            image.blocks = new_blocks;
            image.block_count++;
            gif_block *new_block = &image.blocks[image.block_count - 1];
            new_block->colortable = NULL;
            new_block->colortable_length = 0;

            printf("IMAGE BLOCK BEGINNING AT %ld\n", ftell(file) - 1);
            new_block->imgdesc.separator = magic;
            fread(&new_block->imgdesc.left,   1, 2, file);
            fread(&new_block->imgdesc.top,    1, 2, file);
            fread(&new_block->imgdesc.width,  1, 2, file);
            fread(&new_block->imgdesc.height, 1, 2, file);
            fread(&new_block->imgdesc.packed, 1, 1, file);

            // Local color table
            if (new_block->imgdesc.packed & 1) {
                uint64_t table_size = (new_block->imgdesc.packed & 0b11100000) >> 5;
                new_block->colortable_length = 1L << (table_size + 1);
                new_block->colortable = calloc(new_block->colortable_length, sizeof(gif_color));

                for (uint64_t i = 0; i < new_block->colortable_length; i++) {
                    fread(&new_block->colortable[i].r, 1, 1, file);
                    fread(&new_block->colortable[i].g, 1, 1, file);
                    fread(&new_block->colortable[i].b, 1, 1, file);
                }

                printf("Read local color table of size %"PRIu64"\n", new_block->colortable_length);
            }

            fseek(file, 1, SEEK_CUR);
            fread(&magic, 1, 1, file);
            while (magic != 0) {
                fseek(file, magic, SEEK_CUR);
                fread(&magic, 1, 1, file);
            }

            printf("SKIPPED IMAGE DATA BLOCKS TO %ld\n", ftell(file) - 1);
        }
    }

    printf("Block count: %"PRIu64"\n", image.block_count);

    free(image.colortable);
    for (uint64_t i = 0; i < image.block_count; i++) {
        free(image.blocks[i].colortable);
    }
    free(image.blocks);

    fclose(file);

    return EXIT_SUCCESS;
}
