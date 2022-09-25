#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#define SHEEP_LOG_IMPLEMENTATION
#include "log.h"
#define SHEEP_DYNARRAY_IMPLEMENTATION
#include "dynarray.h"
#include "config.h"
#include <SDL2/SDL.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"

#define unused(a) ((void)(a))

const int GUI_HEIGHT = 100;
const int MAX_BRUSHSIZE = 30;

enum Tool {
    BRUSH,
    BUCKET,
};

typedef struct {
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *tex;
    uint32_t *fb;
    uint32_t color;
    int tool;
    int brushsize;
    int w, h;
    bool isdrag;
    bool running;
    bool redraw;
    int lx, ly;
    struct nk_context *gui;
} app_t;

typedef struct {
    int x, y;
} vec2i_t;

void app_init(app_t *app, int w, int h) {
    app->w = w;
    app->h = h;
    app->fb = calloc(sizeof(*app->fb), w * h);
    if (app->fb == NULL) {
        panic("Failed to allocate memory for canvas, is canvas too big?");
    }
    memset(app->fb, 0xFF, w*h*sizeof(*app->fb));
    app->win = SDL_CreateWindow("sdraw", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h + GUI_HEIGHT, SDL_WINDOW_SHOWN);
    app->rend = SDL_CreateRenderer(app->win, -1, 0);
    app->tex = SDL_CreateTexture(app->rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    app->isdrag = false;
    app->color = colors[0];
    app->running = true;
    app->redraw = true;
    app->brushsize = 1;
    app->gui = nk_sdl_init(app->win, app->rend);
    {
        const float font_scale = 1;
        struct nk_font_atlas *atlas;
        struct nk_font_config config = nk_font_config(0);
        struct nk_font *font;
        nk_sdl_font_stash_begin(&atlas);
        font = nk_font_atlas_add_default(atlas, 13 * font_scale, &config);
        nk_sdl_font_stash_end();
        font->handle.height /= font_scale;
        nk_style_set_font(app->gui, &font->handle);
    }
}

void app_clean(app_t *app) {
    free(app->fb);
}


void flood_fill(int x, int y, app_t *app) {
    if (y >= app->h || y < 0 || x >= app->w || x < 0) {
        return;
    }
    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    const uint32_t oldcolor = app->fb[y*app->w+x];
    if (oldcolor == app->color) {
        return;
    }
    vec2i_t *stack = arrnew(vec2i_t);
    arrpush(stack, ((vec2i_t){x, y}));
    while (arrlen(stack) > 0) {
        vec2i_t v = arrpop(stack);
        app->fb[v.y*app->w + v.x] = app->color;
        for (int i = 0; i < 4; i++) {
            vec2i_t nv = {v.x + dx[i], v.y + dy[i]};
            if (nv.x >= app->w || nv.x < 0 || nv.y >= app->h || nv.y < 0) {
                continue;
            }
            if (app->fb[nv.y*app->w + nv.x] != oldcolor) {
                continue;
            }
            app->fb[v.y*app->w + v.x] = app->color;
            arrpush(stack, nv);
        }
    }
    arrfree(stack);
}

void draw_line(int x1, int y1, int x2, int y2, app_t *app) {
    // brehensam line drawing algorithm
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    int e2;
    while (true) {
        for (int i = -app->brushsize; i < app->brushsize; i++) {
            for (int j = -app->brushsize; j < app->brushsize; j++) {
                if (x1 + i < 0 || x1 + i >= app->w || y1 + j < 0 || y1 + j >= app->h) {
                    continue;
                }
                app->fb[(y1 + j) * app->w + (x1 + i)] = app->color;
            }
        }
        if (x1 == x2 && y1 == y2) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void app_event(app_t *app) {
    SDL_Event e;
    nk_input_begin(app->gui);
    while (SDL_PollEvent(&e)) {
        nk_sdl_handle_event(&e);
        switch (e.type) {
        case SDL_QUIT:
            app->running = false;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (app->tool == BRUSH) {
                app->isdrag = true;
                app->lx = e.button.x;
                app->ly = e.button.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            app->isdrag = false;
            if (app->tool == BUCKET) {
                flood_fill(e.button.x, e.button.y, app);
                app->redraw = true;
            }
            break;
        case SDL_MOUSEMOTION:
            if (app->isdrag) {
                int x = e.motion.x;
                int y = e.motion.y;
                if (x >= app->w || y >= app->h)
                    continue;
                draw_line(app->lx, app->ly, x, y, app);
                app->lx = x;
                app->ly = y;
                app->redraw = true;
            }
            break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
                case SDLK_1:
                    app->color = colors[0];
                    break;
                case SDLK_2:
                    app->color = colors[1];
                    break;
                case SDLK_3:
                    app->color = colors[2];
                    break;
                case SDLK_4:
                    app->color = colors[3];
                    break;
                case SDLK_5:
                    app->color = colors[4];
                    break;
                case SDLK_EQUALS:
                    app->brushsize++;
                    break;
                case SDLK_MINUS:
                    app->brushsize--;
                    break;
                case SDLK_n:
                    app->tool = BUCKET;
                    break;
                case SDLK_b:
                    app->tool = BRUSH;
                    break;
            }
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
                case SDL_WINDOWEVENT_EXPOSED:
                    app->redraw = true;
            }
            break;
        }
    }
    nk_input_end(app->gui);
}

#define ARGB_TO_NKCOLORF(argb) { \
    ((float)((argb >> 16) & 0xff) / 255.0f), \
    ((float)((argb >> 8) & 0xff) / 255.0f), \
    ((float)((argb >> 0) & 0xff) / 255.0f), \
    ((float)((argb >> 24) & 0xff) / 255.0f) \
}

#define NKCOLORF_TO_ARGB(colorf) \
    ((uint32_t)(colorf.r * 255.0f) << 16) | \
    ((uint32_t)(colorf.g * 255.0f) << 8) | \
    ((uint32_t)(colorf.b * 255.0f) << 0) | \
    ((uint32_t)(colorf.a * 255.0f) << 24) \

void app_draw_gui(app_t *app) {
    if (nk_begin(app->gui, "Settings", nk_rect(0, app->h, app->w, GUI_HEIGHT), NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(app->gui, GUI_HEIGHT, 3);
        if (nk_group_begin(app->gui, "col1", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(app->gui, 20, 1);
            nk_label(app->gui, "Brush size", NK_TEXT_LEFT);
            nk_layout_row_dynamic(app->gui, 20, 1);
            nk_slider_int(app->gui, 1, &app->brushsize, MAX_BRUSHSIZE, 1);
            nk_group_end(app->gui);
        }

        if (nk_group_begin(app->gui, "col2", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(app->gui, 80, 1);
            struct nk_colorf colorf = ARGB_TO_NKCOLORF(app->color);
            colorf = nk_color_picker(app->gui, colorf, NK_RGBA);
            app->color = NKCOLORF_TO_ARGB(colorf);
            nk_group_end(app->gui);
        }

        if (nk_group_begin(app->gui, "col3", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(app->gui, 30, 2);
            if (nk_option_label(app->gui, "Brush", app->tool == BRUSH)) app->tool = BRUSH;
            if (nk_option_label(app->gui, "Bucket", app->tool == BUCKET)) app->tool = BUCKET;
            nk_group_end(app->gui);
        }

        nk_end(app->gui);
    }
    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void app_draw(app_t *app) {
    app->redraw = false;
    SDL_UpdateTexture(app->tex, NULL, app->fb, app->w * sizeof(*app->fb));
    SDL_RenderClear(app->rend);
    const SDL_Rect dstrect = {
        .w = app->w,
        .h = app->h,
    };
    SDL_RenderCopy(app->rend, app->tex, NULL, &dstrect);
}

void app_run(app_t *app) {
    while (app->running) {
        app_event(app);
        if (app->redraw) {
            app_draw(app);
        }
        app_draw_gui(app);
        SDL_RenderPresent(app->rend);
        SDL_Delay(15);
    }
}

int main(int argc, char **argv) {
    int w = 800;
    int h = 600;

    if (argc >= 2) {
        if (sscanf(argv[1], "%dx%d", &w, &h) != 2) {
            w = 800;
            h = 600;
        }
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    app_t app;
    app_init(&app, w, h);
    app_run(&app);
    app_clean(&app);

    SDL_Quit();
    return EXIT_SUCCESS;
}
