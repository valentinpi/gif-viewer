#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "gif.h"

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0) {
        printf("%s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    gif_img image;
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
    int screen_width  = image.header.screen_width,
        screen_height = image.header.screen_height;

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

    double scale = 3.0;
    int window_width  = image.header.screen_width  * scale,
        window_height = image.header.screen_height * scale;
    SDL_Window *window = SDL_CreateWindow(
        "epicpolarbearmeme.gif",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width, window_height, 0);
    assert(window != NULL);
    SDL_Renderer *renderer = SDL_CreateRenderer(
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
            gif_imgblock *new_blocks = realloc(image.blocks, sizeof(gif_imgblock) * (image.block_count + 1));
            assert(new_blocks != NULL);
            image.blocks = new_blocks;
            image.block_count++;
            
            gif_imgblock *new_block = &image.blocks[image.block_count - 1];
            new_block->colortable = NULL;
            new_block->colortable_length = 0;

            printf("IMAGE BLOCK BEGINNING AT %ld\n", ftell(file) - 1);
            new_block->imgdesc.separator = magic;
            fread(&new_block->imgdesc.left,   1, 2, file);
            fread(&new_block->imgdesc.top,    1, 2, file);
            fread(&new_block->imgdesc.width,  1, 2, file);
            fread(&new_block->imgdesc.height, 1, 2, file);
            fread(&new_block->imgdesc.packed, 1, 1, file);
            printf("New block at %"PRIu16", %"PRIu16", %"PRIu16", %"PRIu16"\n",
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
                uint8_t *pixels = malloc(screen_width * screen_height * 3);

                // Clear
                for (int i = 0; i < screen_width * screen_height * 3; i++) {
                    pixels[i] = 0x00;
                }

                // Decode the image data and then create the surface
                fseek(file, 1, SEEK_CUR);
                fread(&magic, 1, 1, file);
                // Progress in the pixels buffer
                gif_imgdesc *desc = &new_block->imgdesc;
                int progress = desc->top * desc->width + desc->left;
                while (magic != 0) {
                    for (uint8_t i = 0; i < magic; i++) {
                        uint64_t color_index = 0;
                        fread(&color_index, 1, 1, file);
                        gif_color *color = &image.colortable[color_index];
                        pixels[progress  ] = color->r;
                        pixels[progress+1] = color->g;
                        pixels[progress+2] = color->b;
                        progress += 3;
                    }
                    fread(&magic, 1, 1, file);
                }

                // Little endian, in this case
                // TODO: We may want to make this portable
                // See https://wiki.libsdl.org/SDL_CreateRGBSurfaceFrom for more
                surface = SDL_CreateRGBSurfaceFrom(
                    pixels, screen_width, screen_height, 24, 3 * screen_width,
                    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
                // Remember to free the pixel data AFTER using it, since it does not get copied
                free(pixels);
            }
            else {
                fseek(file, 1, SEEK_CUR);
                fread(&magic, 1, 1, file);
                while (magic != 0) {
                    fseek(file, magic, SEEK_CUR);
                    fread(&magic, 1, 1, file);
                }
            }

            printf("SKIPPED IMAGE DATA BLOCKS TO %ld\n", ftell(file) - 1);
        }
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
