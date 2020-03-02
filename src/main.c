#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

    FILE *file = fopen("../epicpolarbear.gif", "r+b");

    if (file == NULL) {
        fprintf(stderr, "../epicpolarbear.gif could not be opened!\n");
        return EXIT_FAILURE;
    }

    gif_read_header(file, &image.header);

    image.colortable_length = 1L << ((image.header.packed & 0b00000111) + 1);
    image.colortable = calloc(sizeof(gif_color), image.colortable_length);
    for (uint64_t i = 0; i < image.colortable_length; i++) {
        fread(&image.colortable[i].r, 1, 1, file);
        fread(&image.colortable[i].g, 1, 1, file);
        fread(&image.colortable[i].b, 1, 1, file);
    }

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
                new_block->colortable_length = 1L << (table_size + 1);
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
                uint8_t *pixels[pixels_size];
                memset(pixels, 0, pixels_size);

                // LZW Minimum length
                fseek(file, 1, SEEK_CUR);
                // Size of block
                fread(&magic, 1, 1, file);
                while (magic != 0) {
                    fseek(file, magic, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                }

                // Little endian, in this case
                // TODO: We may want to make this portable
                // See https://wiki.libsdl.org/SDL_CreateRGBSurfaceFrom for more
                surface = SDL_CreateRGBSurfaceFrom(
                    pixels,
                    new_block->imgdesc.width,
                    new_block->imgdesc.height,
                    24,
                    3 * new_block->imgdesc.width,
                    0x000000FF,
                    0x0000FF00,
                    0x00FF0000,
                    0xFF000000);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
                // Remember to free the pixel data AFTER using it, since it does not get copied

                printf("READ IMAGINE CONTROL BLOCK AND GENERATED TEXTURE\n");
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
