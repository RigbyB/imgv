#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

struct PPMFile {
    const char* path;
    char magic_number[3];
    int width;
    int height;
    int max_color;
    uint8_t* data;
};

struct PPMFile* ppm_read_file(const char* path) {
    FILE* open_file = fopen(path, "r");
    
    if (open_file == NULL) {
        fprintf(stderr, "Failed to open file %s.\n", path);
        return NULL;
    }

    // 3 (include null terminator)
    char magic_number[3];

    if (fscanf(open_file, "%2s", magic_number) != 1 ||
        magic_number[0] != 'P' ||
        magic_number[1] != '3'
    ) {
        fprintf(stderr, "File is not in PPM P3 format.\n");
        return NULL;
    }

    int width = 0, height = 0;
    
    if (fscanf(open_file, "%i %i", &width, &height) != 2) {
        fprintf(stderr, "Error reading image dimensions.\n");
        return NULL;
    }

    int max_color = 0;

    if (fscanf(open_file, "%i", &max_color) != 1) {
        fprintf(stderr, "Error reading max color.\n");
        return NULL;
    }

    int num_pixels = width * height;

    // TODO: Don't use alpha channel
    uint8_t* data = malloc(num_pixels * 4);

    if (data == NULL) {
        fprintf(stderr, "Failed to allocate image pixel data array.\n");
        return NULL;
    }

    for (int i = 0; i < num_pixels; i++) {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        if (fscanf(open_file, "%hhu %hhu %hhu", &r, &g, &b) != 3) {
            fprintf(stderr, "Error reading pixel data.\n");
            return NULL;
        }

        // TODO: Check if greater than max color

        // TODO: This is the wrong way.
        data[i * 4] = 255;
        data[i * 4 + 1] = b;
        data[i * 4 + 2] = g;
        data[i * 4 + 3] = r;
    }    

    struct PPMFile* ppm_file = malloc(sizeof(struct PPMFile));

    if (ppm_file == NULL) {
        fprintf(stderr, "Failed to allocate PPMFile struct.\n");
        return NULL;
    }

    ppm_file->path = path;
    strncpy(ppm_file->magic_number, magic_number, 3);
    ppm_file->width = width;
    ppm_file->height = height;
    ppm_file->max_color = max_color;
    ppm_file->data = data;
    return ppm_file;
}

void ppm_destroy(struct PPMFile* file) {
    free(file->data);
    free(file);
}

void ppm_print_details(struct PPMFile* file) {
    printf("%s\n", file->path);
    printf("Magic number: %s\n", file->magic_number);
    printf("Size (WxH): %ix%i\n", file->width, file->height);
    printf("Max color: %i\n", file->max_color);
}

void sdl_render_once(SDL_Renderer* renderer, struct PPMFile* ppm_file) {
    if (ppm_file) {
        SDL_Texture* texture = SDL_CreateTexture(
            renderer, 
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            ppm_file->width, ppm_file->height     // Texture size fills window
        );

        if (texture == NULL) {
            fprintf(stderr, "Failed to create SDL texture: %s.\n", SDL_GetError()); 
            exit(EXIT_FAILURE);
        }

        void* texture_pixels = NULL;
        int texture_pitch = 0;

        if (SDL_LockTexture(
            texture, 
            NULL,               // Area (rect) to lock
            &texture_pixels,    
            &texture_pitch      // Pitch - length of one row in bytes
        ) != 0)
        {
            fprintf(stderr, "Failed to lock SDL texture: %s.\n", SDL_GetError()); 
            exit(EXIT_FAILURE);
        }

        memcpy(
            texture_pixels,
            ppm_file->data,
            // TODO: Don't like this hardcoding
            ppm_file->width * ppm_file->height * 4
        );

        SDL_UnlockTexture(texture);

        SDL_Rect texture_rect;
        texture_rect.x = 0; 
        texture_rect.y = 0; 
        texture_rect.w = ppm_file->width; 
        texture_rect.h = ppm_file->height;

        SDL_RenderCopy(
            renderer, 
            texture, 
            NULL,           // Source rect
            &texture_rect   // Target rect
        );

        SDL_DestroyTexture(texture);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);
    }

    SDL_RenderPresent(renderer);
}

bool update_state(const char* new_image_path, struct PPMFile** ppm_file, SDL_Window* window) {
    struct PPMFile* new_ppm_file = ppm_read_file(new_image_path);

    if (new_ppm_file == NULL) {
        return false;
    }

    if (*ppm_file) {
        ppm_destroy(*ppm_file);
    }

    *ppm_file = new_ppm_file;

    SDL_SetWindowTitle(window, new_ppm_file->path);
    SDL_SetWindowSize(window, new_ppm_file->width, new_ppm_file->height);

    ppm_print_details(new_ppm_file);

    return true;
}

int main(int argc, char *argv[]) {
    struct PPMFile* ppm_file = NULL;
    
    if (argc > 1) {
        ppm_file = ppm_read_file(argv[1]);
    
        if (ppm_file) {
            ppm_print_details(ppm_file);
        }
    }


    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "Failed to initialise SDL: %s\n.", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Defaults if no image is loaded
    const char* window_title = ppm_file ? ppm_file->path : "imgv";
    int window_width = ppm_file ? ppm_file->width : 500;
    int window_height = ppm_file ? ppm_file->height : 500;

    SDL_Window* window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_CENTERED,  SDL_WINDOWPOS_CENTERED, 
        window_width, window_height,
        0   // Flags
    );

    if (window == NULL) {
        fprintf(stderr, "Failed to create SDL window: %s\n.", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, 
        -1,     // Default renderering driver   
        0       // Flags
    );

    if (renderer == NULL) {
        fprintf(stderr, "Failed to create SDL renderer: %s\n.", SDL_GetError()); 
        exit(EXIT_FAILURE);
    }

    sdl_render_once(renderer, ppm_file);


    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                        running = false;
                    }

                    break;

                case SDL_DROPFILE: 
                    if (update_state(event.drop.file, &ppm_file, window)) {
                        sdl_render_once(renderer, ppm_file);
                    }

                    SDL_free(event.drop.file);   
                    break;

                default:
                    break;
            }
        }

        // May have updated after SDL event polling
        if (running == false) {
            break;
        }
    }

    return EXIT_SUCCESS;
}