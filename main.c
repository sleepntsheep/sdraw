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

#define unused(a) ((void)(a))

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
} app_t;

typedef struct {
} menu_t;

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
    app->win = SDL_CreateWindow("sdraw", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_SHOWN);
    app->rend = SDL_CreateRenderer(app->win, -1, 0);
    app->tex = SDL_CreateTexture(app->rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    app->isdrag = false;
    app->color = colors[0];
    app->running = true;
    app->redraw = true;
    app->brushsize = 1;
}

void app_clean(app_t *app) {
    free(app->fb);
}

void flood_fill(int x, int y, app_t *app) {
    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    uint32_t oldcolor = app->fb[y*app->w+x];
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
    while (SDL_PollEvent(&e)) {
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
}

void app_draw(app_t *app) {
    app->redraw = false;
    SDL_UpdateTexture(app->tex, NULL, app->fb, app->w * sizeof(*app->fb));
    SDL_RenderClear(app->rend);
    SDL_RenderCopy(app->rend, app->tex, NULL, NULL);
    SDL_RenderPresent(app->rend);
}

void app_run(app_t *app) {
    while (app->running) {
        app_event(app);
        if (app->redraw) {
            app_draw(app);
        }
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
