#include <SDL2/SDL.h>

#include "gif.h"

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    gif_img image;
    image.colortable = NULL;
    image.colortable_length = 0;
    image.blocks = NULL;
    image.block_count = 0;

    //FILE *file = fopen("../epicpolarbear.gif", "r+b");
    FILE *file = fopen("../gif-test/oneblackpixel.gif", "r+b");

    if (file == NULL) {
        fprintf(stderr, "../epicpolarbear.gif could not be opened!\n");
        return EXIT_FAILURE;
    }

    gif_read_header(file, &image.header);
    gif_read_global_colortable(file, &image);

    // SDL Initialization and rendering
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    double scale = 3.0;
    int window_width  = image.header.screen_width  * scale,
        window_height = image.header.screen_height * scale;

    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0) {
        printf("%s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    window = SDL_CreateWindow(
        "epicpolarbearmeme.gif",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width, window_height, 0);
    assert(window != NULL);
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer != NULL);

    // For now, only render one texture
    SDL_Texture *texture = NULL;

    // Start reading image and extension blocks
    int parsing = 1;
    while (parsing) {
        uint8_t magic = 0;
        fread(&magic, 1, 1, file);

        if (magic == GIF_TRAILER) {
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
                    printf("FOUND GRAPHICS CONTROL EXTENSION BLOCK AT 0x%lX\n", ftell(file) - 2);
                    fseek(file, 6, SEEK_CUR);
                    break;
                // Plain Text Extension Block
                case 0x1:
                    printf("FOUND PLAIN TEXT EXTENSION BLOCK AT 0x%lX\n", ftell(file) - 2);
                    fseek(file, 13, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                    while (magic != 0) {
                        fseek(file, magic, SEEK_CUR);
                        fread(&magic, 1, 1, file);
                    }
                    break;
                // Application Extension Block
                case 0xFF:
                    printf("FOUND APPLICATION EXTENSION BLOCK AT 0x%lX\n", ftell(file) - 2);
                    fseek(file, 12, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                    while (magic != 0) {
                        fseek(file, magic, SEEK_CUR);
                        fread(&magic, 1, 1, file);
                    }
                    break;
                // Comment Extension Block
                case 0xFE:
                    printf("FOUND COMMENT EXTENSION BLOCK AT 0x%lX\n", ftell(file) - 2);
                    fread(&magic, 1, 1, file);
                    while (magic != 0) {
                        fseek(file, magic, SEEK_CUR);
                        fread(&magic, 1, 1, file);
                    }
                    break;
                default:
                    printf("FOUND UNKNOWN EXTENSION BLOCK AT 0x%lX\n", ftell(file) - 2);
                    break;
            }
            
            printf("SKIPPED CONTROL BLOCK TO 0x%lX\n", ftell(file) - 1);
        }
        // Read and decompress image block
        else if (magic == GIF_SEPARATOR) {
            gif_imgblock *new_blocks = realloc(image.blocks, sizeof(gif_imgblock) * (image.block_count + 1));
            assert(new_blocks != NULL);
            image.blocks = new_blocks;
            image.block_count++;
            
            gif_imgblock *new_block = &image.blocks[image.block_count - 1];
            new_block->colortable = NULL;
            new_block->colortable_length = 0;

            printf("IMAGE BLOCK BEGINNING AT 0x%lX\n", ftell(file) - 1);
            new_block->imgdesc.separator = magic;
            fread(&new_block->imgdesc.left,   1, 2, file);
            fread(&new_block->imgdesc.top,    1, 2, file);
            fread(&new_block->imgdesc.width,  1, 2, file);
            fread(&new_block->imgdesc.height, 1, 2, file);
            fread(&new_block->imgdesc.packed, 1, 1, file);
            printf("New block at %"PRIu16", %"PRIu16" of size %"PRIu16", %"PRIu16"\n",
                new_block->imgdesc.left,
                new_block->imgdesc.top,
                new_block->imgdesc.width,
                new_block->imgdesc.height);

            // Local color table
            if (new_block->imgdesc.packed & 1) {
                uint64_t table_size = (new_block->imgdesc.packed & 0b11100000) >> 5;
                new_block->colortable_length = 1 << (table_size + 1);
                new_block->colortable = calloc(new_block->colortable_length, sizeof(gif_color));

                for (uint64_t i = 0; i < new_block->colortable_length; i++) {
                    fread(&new_block->colortable[i].r, 1, 1, file);
                    fread(&new_block->colortable[i].g, 1, 1, file);
                    fread(&new_block->colortable[i].b, 1, 1, file);
                }

                printf("Read local color table of size %"PRIu64"\n", new_block->colortable_length);
            }

            if (texture == NULL) {
                SDL_Surface *surface = NULL;
                const uint64_t pixels_size = new_block->imgdesc.width * new_block->imgdesc.height * 3;
                uint8_t pixels[pixels_size];
                memset(pixels, 0, pixels_size);

                /* DECOMPRESSION */

                gif_lzw_dict_entry dict[4096];
                uint64_t dict_size = 0;

                uint8_t minimum_code_length = 0;
                fread(&minimum_code_length, 1, 1, file);

                uint64_t pixels_index = 0;
                while (magic != 0) {
                    fread(&magic, 1, 1, file);

                    uint8_t compressed[magic];
                    fread(compressed, 1, magic, file);

                    if (pixels_index == 0) {
                        for (uint64_t j = 0; j < magic; j++) {
                            printf("<%"PRIu8"> ", compressed[j]);
                        }
                        printf("\b\n");
                    }

                    uint8_t *image_data = NULL;
                    uint64_t image_data_size = 0;
                    gif_lzw_decode(
                        minimum_code_length,
                        compressed, magic,
                        &image_data, &image_data_size,
                        dict, &dict_size,
                        image.colortable, image.colortable_length);
                    assert(image_data != NULL);

                    memcpy(pixels + pixels_index * 3, image_data, image_data_size);
                    pixels_index += image_data_size;

                    free(image_data);
                }
                printf("Size: %"PRIu64", Index: %"PRIu64"\n", pixels_size, pixels_index);

                // See https://wiki.libsdl.org/SDL_CreateRGBSurfaceFrom for more details on endianess
                surface = SDL_CreateRGBSurfaceFrom(
                    pixels,
                    new_block->imgdesc.width,
                    new_block->imgdesc.height,
                    24,
                    3 * new_block->imgdesc.width,
                #if SDL_BYTEORDER == SDL_BIG_ENDIAN
                    0x00FF0000,
                    0x0000FF00,
                    0x000000FF,
                    0x00000000);
                #else
                    0x000000FF,
                    0x0000FF00,
                    0x00FF0000,
                    0x00000000);
                #endif
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);

                printf("Read image block and generated texture\n");

                for (uint64_t i = 0; i < dict_size; i++) {
                    free(dict[i].decomp);
                }
            }
            else {
                fseek(file, 1, SEEK_CUR);
                fread(&magic, 1, 1, file);
                while (magic != 0) {
                    fseek(file, magic, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                }

                printf("SKIPPED IMAGE DATA BLOCKS TO 0x%lX\n", ftell(file) - 1);
            }
        }

        printf("\n");
    }

    printf("Block count: %"PRIu64"\n", image.block_count);

    fclose(file);

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                default:
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        SDL_Rect dstrect = { 0, 0, window_width, window_height };
        SDL_RenderCopy(renderer, texture, NULL, &dstrect);

        SDL_RenderPresent(renderer);
    }

    free(image.colortable);
    for (uint64_t i = 0; i < image.block_count; i++) {
        free(image.blocks[i].colortable);
    }
    free(image.blocks);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
