#include <SDL2/SDL_keycode.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#define SHEEP_LOG_IMPLEMENTATION
#include "log.h"
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
            app->isdrag = true;
            app->lx = e.button.x;
            app->ly = e.button.y;
            break;
        case SDL_MOUSEBUTTONUP:
            app->isdrag = false;
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
