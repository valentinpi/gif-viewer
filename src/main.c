#include "gif.h"

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    if (argc < 3) {
        printf("Usage: gif-viewer <file path> <scale>\n");
        return EXIT_FAILURE;
    }

    gif_img image;
    image.colortable = NULL;
    image.colortable_length = 0;
    image.blocks = calloc(4096, sizeof(gif_imgblock));
    image.blocks_count = 1;
    image.blocks_reserved = 4096;

    FILE *file = fopen(argv[1], "r+b");

    if (file == NULL) {
        fprintf(stderr, "%s could not be opened!\n", argv[1]);
        return EXIT_FAILURE;
    }

    gif_read_header(file, &image.header);
    gif_read_global_colortable(file, &image);

    // SDL Initialization and rendering
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    const double scale = strtod(argv[2], NULL);
    const int window_width  = image.header.screen_width  * scale,
              window_height = image.header.screen_height * scale;

    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO) != 0) {
        printf("%s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    window = SDL_CreateWindow(
        argv[1],
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width, window_height, 0);
    assert(window != NULL);
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer != NULL);

    uint64_t cur_imgblock = 0;
    while (1) {
        uint8_t magic = 0;
        fread(&magic, 1, 1, file);

        if (magic == GIF_TRAILER) {
            image.blocks_count--;
            break;
        }
        // In case of a control block, just skip any other than graphics extension blocks for the moment
        else if (magic == GIF_CONTROL) {
            // Read label byte
            fread(&magic, 1, 1, file);
            // Skip accordingly
            switch (magic) {
                // Graphics Control Extension Block
                case 0xF9:
                    gif_read_ext_graphicsblock(file, &image.blocks[cur_imgblock].graphics);
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
        }
        // Read and decompress image block
        else if (magic == GIF_SEPARATOR) {
            gif_imgblock *cur_block = &image.blocks[cur_imgblock];
            cur_block->imgdesc.separator = magic;
            fread(&cur_block->imgdesc.left,   1, 2, file);
            fread(&cur_block->imgdesc.top,    1, 2, file);
            fread(&cur_block->imgdesc.width,  1, 2, file);
            fread(&cur_block->imgdesc.height, 1, 2, file);
            fread(&cur_block->imgdesc.packed, 1, 1, file);

            // Local color table
            if (cur_block->imgdesc.packed & 1) {
                uint64_t table_size = (cur_block->imgdesc.packed & 0b11100000) >> 5;
                cur_block->colortable_length = 1 << (table_size + 1);
                cur_block->colortable = calloc(cur_block->colortable_length, sizeof(gif_color));

                for (uint64_t i = 0; i < cur_block->colortable_length; i++) {
                    fread(&cur_block->colortable[i].r, 1, 1, file);
                    fread(&cur_block->colortable[i].g, 1, 1, file);
                    fread(&cur_block->colortable[i].b, 1, 1, file);
                }

                printf("Read local color table of size %"PRIu64"\n", cur_block->colortable_length);
            }

            SDL_Surface *surface = NULL;
            const uint64_t pixels_size = cur_block->imgdesc.width * cur_block->imgdesc.height * 3;
            uint8_t *pixels = calloc(pixels_size, 1);

            /* DECOMPRESSION */
            uint8_t min_code_len = 0;
            fread(&min_code_len, 1, 1, file);

            uint8_t *compressed = NULL;
            uint64_t compressed_size = 0;
            fread(&magic, 1, 1, file);
            while (magic != 0) {
                compressed = realloc(compressed, compressed_size + magic);
                assert(compressed != NULL);
                fread(compressed + compressed_size, 1, magic, file);
                compressed_size += magic;
                fread(&magic, 1, 1, file);
            }

            uint8_t *image_data = NULL;
            uint64_t image_data_size = 0;
            if (cur_block->imgdesc.packed & 1) {
                gif_decode(
                    min_code_len,
                    compressed, compressed_size,
                    &image_data, &image_data_size,
                    cur_block->colortable, cur_block->colortable_length);
            }
            else {
                gif_decode(
                    min_code_len,
                    compressed, compressed_size,
                    &image_data, &image_data_size,
                    image.colortable, image.colortable_length);
            }
            assert(image_data != NULL);

            if (image_data_size >= pixels_size) {
                memcpy(pixels, image_data, pixels_size);
            }
            else {
                memcpy(pixels, image_data, image_data_size);
            }

            free(image_data);
            free(compressed);

            surface = SDL_CreateRGBSurfaceFrom(
                pixels,
                cur_block->imgdesc.width,
                cur_block->imgdesc.height,
                24,
                3 * cur_block->imgdesc.width,
            // See https://wiki.libsdl.org/SDL_CreateRGBSurfaceFrom for more details on endianess
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
            SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
            cur_block->image = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);

            free(pixels);

            cur_imgblock++;
            image.blocks_count++;
        }
    }

    printf("Block count: %"PRIu64"\n", image.blocks_count);

    fclose(file);

    uint64_t cur_image = 0;
    uint32_t begin = SDL_GetTicks();
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

        gif_imgblock *cur_block = &image.blocks[cur_image];
        if (SDL_GetTicks() - begin + 16 >= cur_block->graphics.delay * 10) {
            cur_image++;
            if (cur_image == image.blocks_count) {
                cur_image = 0;
            }
        }
        else {
            continue;
        }

        printf("Delay: %"PRIu16"\n", cur_block->graphics.delay);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        SDL_Rect dstrect = {
            cur_block->imgdesc.left   * scale,
            cur_block->imgdesc.top    * scale,
            cur_block->imgdesc.width  * scale,
            cur_block->imgdesc.height * scale
        };
        SDL_RenderCopy(renderer, cur_block->image, NULL, &dstrect);
        printf("Drawing texture %p to %d, %d, %d, %d\n", cur_block->image, dstrect.x, dstrect.y, dstrect.w, dstrect.h);

        SDL_RenderPresent(renderer);

        begin = SDL_GetTicks();
    }

    free(image.colortable);
    for (uint64_t i = 0; i < image.blocks_reserved; i++) {
        free(image.blocks[i].colortable);
        SDL_DestroyTexture(image.blocks[i].image);
    }
    free(image.blocks);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
