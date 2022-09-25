#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define SHEEP_LOG_IMPLEMENTATION
#include "log.h"
#define SHEEP_DYNARRAY_IMPLEMENTATION
#include "dynarray.h"
#include "config.h"
#include "tinyfiledialogs.h"
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
    uint32_t *fb;
    uint32_t fg;
    int w, h;
    int tool;
    int brushsize;
    bool isdrag;
    int lx, ly;
} canvas_t;

typedef struct {
    struct nk_context *ctx;
    struct {
        bool open_save_dialog;
        int quality;
        const char *file_path;
    } img;
    struct {
        bool open_dialog;
        int w, h;
        char wb[256], hb[256];
    } new;
} gui_t;

typedef struct {
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *tex;
    bool running;
    bool redraw;
    int w, h;
    canvas_t canvas;
    gui_t gui;
} app_t;

typedef struct {
    int x, y;
} vec2i_t;

void canvas_init(canvas_t *canvas, int w, int h) {
    canvas->fb = malloc(sizeof(*canvas->fb) * w * h);
    if (canvas->fb == NULL)
        panic("Failed to allocate memory for canvas, is canvas too big?");
    memset(canvas->fb, 0xFF, sizeof(*canvas->fb) * w * h);
    canvas->isdrag = false;
    canvas->brushsize = 1;
    canvas->tool = BRUSH;
    canvas->fg = colors[0];
    canvas->lx = canvas->ly = 0;
    canvas->w = w;
    canvas->h = h;
}

void canvas_set_pixel(canvas_t *canvas, int x, int y) {
    if (x < 0 || y < 0 || x >= canvas->w || y >= canvas->h)
        return;
    canvas->fb[y * canvas->w + x] = canvas->fg;
}

uint32_t canvas_get_pixel(canvas_t *canvas, int x, int y) {
    return canvas->fb[y * canvas->w + x];
}

void app_init(app_t *app, int w, int h) {
    memset(app, 0, sizeof(app_t));
    app->w = w;
    app->h = h;
    app->win = SDL_CreateWindow("sdraw", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h + GUI_HEIGHT, SDL_WINDOW_SHOWN);
    app->rend = SDL_CreateRenderer(app->win, -1, 0);
    app->tex = SDL_CreateTexture(app->rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    app->running = true;
    app->redraw = true;
    canvas_init(&app->canvas, w, h);
    app->gui.ctx = nk_sdl_init(app->win, app->rend);
    {
        const float font_scale = 1;
        struct nk_font_atlas *atlas;
        struct nk_font_config config = nk_font_config(0);
        struct nk_font *font;
        nk_sdl_font_stash_begin(&atlas);
        font = nk_font_atlas_add_default(atlas, 13 * font_scale, &config);
        nk_sdl_font_stash_end();
        font->handle.height /= font_scale;
        nk_style_set_font(app->gui.ctx, &font->handle);
    }
}

void app_clean(app_t *app) {
    SDL_DestroyTexture(app->tex);
    SDL_DestroyRenderer(app->rend);
    SDL_DestroyWindow(app->win);
    free(app->canvas.fb);
}

void canvas_save(canvas_t *canvas, const char *file_name, int quality) {
    char *in_rgba = malloc(3 * canvas->w * canvas->h);
    for (int i = 0; i < canvas->w * canvas->h; i++) {
        in_rgba[i*3] = (canvas->fb[i] >> 16) & 0xFF;
        in_rgba[i*3+1] = (canvas->fb[i] >> 8) & 0xFF;
        in_rgba[i*3+2] = (canvas->fb[i]) & 0xFF;
    }
    stbi_write_jpg(file_name, canvas->w, canvas->h, 3, in_rgba, quality);
    free(in_rgba);
}

void canvas_flood_fill(canvas_t *canvas, int x, int y) {
    if (y >= canvas->h || y < 0 || x >= canvas->w || x < 0) {
        return;
    }
    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    const uint32_t oldcolor = canvas_get_pixel(canvas, x, y);
    if (oldcolor == canvas->fg) {
        return;
    }
    vec2i_t *stack = arrnew(vec2i_t);
    arrpush(stack, ((vec2i_t){x, y}));
    while (arrlen(stack) > 0) {
        vec2i_t v = arrpop(stack);
        canvas_set_pixel(canvas, v.x, v.y);
        for (int i = 0; i < 4; i++) {
            vec2i_t nv = {v.x + dx[i], v.y + dy[i]};
            if (nv.x >= canvas->w || nv.x < 0 || nv.y >= canvas->h || nv.y < 0) {
                continue;
            }
            if (canvas_get_pixel(canvas, nv.x, nv.y) != oldcolor) {
                continue;
            }
            canvas_set_pixel(canvas, v.x, v.y);
            arrpush(stack, nv);
        }
    }
    arrfree(stack);
}

void canvas_draw_line(canvas_t *canvas, int x1, int y1, int x2, int y2) {
    // brehensam line drawing algorithm
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    int e2;
    while (true) {
        for (int i = -canvas->brushsize; i < canvas->brushsize; i++) {
            for (int j = -canvas->brushsize; j < canvas->brushsize; j++) {
                if (x1 + i < 0 || x1 + i >= canvas->w || y1 + j < 0 || y1 + j >= canvas->h) {
                    continue;
                }
                canvas_set_pixel(canvas, x1 + i, y1 + j);
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

bool canvas_event(canvas_t *canvas, SDL_Event e) {
    bool redraw = true;
    switch (e.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (canvas->tool == BRUSH) {
                canvas->isdrag = true;
                canvas->lx = e.button.x;
                canvas->ly = e.button.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            canvas->isdrag = false;
            if (canvas->tool == BUCKET) {
                canvas_flood_fill(canvas, e.button.x, e.button.y);
                redraw = true;
            }
            break;
        case SDL_MOUSEMOTION:
            if (canvas->isdrag) {
                int x = e.motion.x;
                int y = e.motion.y;
                if (x >= canvas->w || y >= canvas->h)
                    break;
                canvas_draw_line(canvas, canvas->lx, canvas->ly, x, y);
                canvas->lx = x;
                canvas->ly = y;
                redraw = true;
            }
            break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
                case SDLK_1:
                    canvas->fg = colors[0];
                    break;
                case SDLK_2:
                    canvas->fg = colors[1];
                    break;
                case SDLK_3:
                    canvas->fg = colors[2];
                    break;
                case SDLK_4:
                    canvas->fg = colors[3];
                    break;
                case SDLK_5:
                    canvas->fg = colors[4];
                    break;
                case SDLK_EQUALS:
                    canvas->brushsize++;
                    break;
                case SDLK_MINUS:
                    canvas->brushsize--;
                    break;
                case SDLK_n:
                    canvas->tool = BUCKET;
                    break;
                case SDLK_b:
                    canvas->tool = BRUSH;
                    break;
            }
            break;
    }
    return redraw;
}

void app_event(app_t *app) {
    SDL_Event e;
    nk_input_begin(app->gui.ctx);
    while (SDL_PollEvent(&e)) {
        nk_sdl_handle_event(&e);
        app->redraw |= canvas_event(&app->canvas, e);
        switch (e.type) {
        case SDL_QUIT:
            app->running = false;
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
                case SDL_WINDOWEVENT_EXPOSED:
                    app->redraw = true;
            }
            break;
        }
    }
    nk_input_end(app->gui.ctx);
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
    gui_t gui = app->gui;

    if (nk_begin(gui.ctx, "Settings", nk_rect(0, app->h, app->w, GUI_HEIGHT), NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(gui.ctx, GUI_HEIGHT, 4);

        if (nk_group_begin(gui.ctx, "col0", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            if (nk_button_label(gui.ctx, "New")) {
                gui.new.open_dialog = true;
            }
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            if (nk_button_label(gui.ctx, "Save")) {
                gui.img.open_save_dialog = true;
            }
            nk_group_end(gui.ctx);
        }
        
        if (nk_group_begin(gui.ctx, "col1", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            nk_label(gui.ctx, "Brush size", NK_TEXT_LEFT);
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            nk_slider_int(gui.ctx, 1, &app->canvas.brushsize, MAX_BRUSHSIZE, 1);
            nk_group_end(gui.ctx);
        }

        if (nk_group_begin(gui.ctx, "col2", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(gui.ctx, 80, 1);
            struct nk_colorf colorf = ARGB_TO_NKCOLORF(app->canvas.fg);
            colorf = nk_color_picker(gui.ctx, colorf, NK_RGBA);
            app->canvas.fg = NKCOLORF_TO_ARGB(colorf);
            nk_group_end(gui.ctx);
        }

        if (nk_group_begin(gui.ctx, "col3", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            if (nk_option_label(gui.ctx, "Brush", app->canvas.tool == BRUSH)) app->canvas.tool = BRUSH;
            if (nk_option_label(gui.ctx, "Bucket", app->canvas.tool == BUCKET)) app->canvas.tool = BUCKET;
            nk_group_end(gui.ctx);
        }


        nk_end(gui.ctx);
    }

    if (gui.img.open_save_dialog) {
        if (nk_begin(gui.ctx, "Save", nk_rect(50, 50, 200, 200), NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
            nk_layout_row_dynamic(gui.ctx, 30, 1);
            if (nk_button_label(gui.ctx, gui.img.file_path ? gui.img.file_path : "Select file path" )) {
                gui.img.file_path = tinyfd_saveFileDialog("Select where to save",
                        "image.jpg", 0, NULL, "jpg image");
            }
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Quality", NK_TEXT_LEFT);
            nk_slider_int(gui.ctx, 1, &gui.img.quality, 10, 1);
            nk_layout_row_dynamic(gui.ctx, 30, 1);
            if (gui.img.file_path != NULL) {
                if (nk_button_label(gui.ctx, "Save")) {
                    canvas_save(&app->canvas, gui.img.file_path, gui.img.quality);
                    gui.img.open_save_dialog = false;
                    gui.img.file_path = NULL;
                }
            }
            nk_end(gui.ctx);
        }
    }

    if (gui.new.open_dialog) {
        if (nk_begin(gui.ctx, "New Canvas", nk_rect(50, 50, 200, 200), NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Width", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(
                gui.ctx, NK_EDIT_BOX, gui.new.wb, sizeof(gui.new.wb), nk_filter_ascii
            );
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Height", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(
                gui.ctx, NK_EDIT_BOX, gui.new.hb, sizeof(gui.new.hb), nk_filter_ascii
            );
            if (nk_button_label(gui.ctx, "New")) {
                gui.new.w = 400;
                gui.new.h = 0;
                gui.new.w = strtol(gui.new.wb, NULL, 10);
                gui.new.h = strtol(gui.new.hb, NULL, 10);
                app_clean(app);
                return app_init(app, gui.new.w, gui.new.h);
            }
            nk_end(gui.ctx);
        }
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
    app->gui = gui;
}

void app_draw(app_t *app) {
    app->redraw = false;
    SDL_UpdateTexture(app->tex, NULL, app->canvas.fb, app->w * sizeof(*app->canvas.fb));
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
