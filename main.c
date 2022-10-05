/*  sdraw - simple paint program
 *
 *  TODO
 *   - rectangle tool
 *   - select & mvoe tool
 *   - lasso select tool
 *   - undo & redo
 *   - png export
 *   done - text tool
 *
 *  sleepntsheep 2022
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "tinyfiledialogs.h"
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define SHEEP_LOG_IMPLEMENTATION
#include "log.h"
#define SHEEP_DYNARRAY_IMPLEMENTATION
#include "dynarray.h"
/*#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"*/

#include "config.h"
#include "font.h"

#define NK_BUTTON_TRIGGER_ON_RELEASE
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

//#define STB_TRUETYPE_IMPLEMENTATION
//#include "stb_truetype.h"

#define UNUSED(a) ((void)(a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef uint32_t argb;

const int GUI_HEIGHT = 150;
const int MAX_BRUSHSIZE = 30;

enum Tool {
    BRUSH,
    BUCKET,
    LINE,
    TEXT,
    RECT,
    RECTFILL,
    TOOL_COUNT,
};

typedef struct {
    argb *fb;
    argb *tfb;
    argb fg;
    int w, h;
    int tool;
    int brushsize;
    bool isdrag;
    int lx, ly;
    bool use_tfb;
} canvas_t;

typedef struct {
    struct nk_context *ctx;
    struct {
        bool open_dialog;
        int quality;
        const char *file_path;
    } save;
    struct {
        bool open_dialog;
        int w, h;
        char wb[256], hb[256];
    } new;
    struct {
        bool open_dialog;
        int w, h;
        int channel;
        const char *file_path;
    } load;
    struct {
        bool open_dialog;
        char buf[1024];
        char size_buf[1024];
        int selidx;
        int size;
        int x, y;
    } text;
} gui_t;

typedef struct {
    canvas_t canvas;
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *tex;
    bool running;
    int w, h;
    gui_t gui;
    sdraw_font_t *font_arr;
    const char **font_name_arr;
    struct {
        argb *data;
        int x, y;
    } select;
} app_t;

typedef struct {
    int x, y;
} vec2i_t;

void canvas_init(canvas_t *canvas, int w, int h) {
    canvas->fb = malloc(sizeof(*canvas->fb) * w * h);
    if (canvas->fb == NULL)
        panic("Failed to allocate memory for canvas, is canvas too big?");
    canvas->tfb = calloc(sizeof(*canvas->tfb), w * h);
    if (canvas->tfb == NULL)
        panic("Failed to allocate memory for canvas, is canvas too big?");
    memset(canvas->fb, 0xFF, sizeof(*canvas->fb) * w * h);
    memset(canvas->tfb, 0x00, sizeof(*canvas->tfb) * w * h);
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
    if (canvas->use_tfb)
        canvas->tfb[y * canvas->w + x] = canvas->fg;
    else
        canvas->fb[y * canvas->w + x] = canvas->fg;
}

argb canvas_get_pixel(canvas_t *canvas, int x, int y) {
    if (canvas->use_tfb)
        return canvas->tfb[y * canvas->w + x];
    else
        return canvas->fb[y * canvas->w + x];
}

void app_init(app_t *app, int w, int h) {
    /* w, h params are canvas size, app->w and app->h is different */
    memset(app, 0, sizeof(app_t));
    app->font_arr = get_all_fonts();
    qsort(app->font_arr, dynarray_len(app->font_arr), sizeof(sdraw_font_t),
          sdraw_font_cmp);
    app->font_name_arr = NULL;
    for (size_t i = 0; i < dynarray_len(app->font_arr); i++) {
        arrpush(app->font_name_arr, app->font_arr[i].name);
        assert(app->font_arr[i].name == app->font_name_arr[i]);
    }
    app->w = MAX(w, 500);
    app->h = h;
    app->win = SDL_CreateWindow("sdraw", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, app->w, h + GUI_HEIGHT,
                                SDL_WINDOW_SHOWN);
    app->rend = SDL_CreateRenderer(app->win, -1, 0);
    app->tex = SDL_CreateTexture(app->rend, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_SetTextureBlendMode(app->tex, SDL_BLENDMODE_BLEND);
    app->running = true;
    canvas_init(&app->canvas, w, h);
    app->gui.ctx = nk_sdl_init(app->win, app->rend);
    app->gui.save.quality = 100;
    app->gui.text.selidx = 0;
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


void canvas_draw_text(canvas_t *canvas, int x, int y, const char *text,
        const char *font_path, int font_size) {
    /* TODO: make it less hacky (stbtt would be great); */
    TTF_Font *font;
    SDL_Surface *surf;
    font = TTF_OpenFont(font_path, font_size);
    if (font == NULL) {
        warn("Failed to load font: %s", font_path);
        return;
    }
    surf = TTF_RenderUTF8_Blended(font, text, (SDL_Color){0, 0, 0, 255});
    if (surf == NULL) {
        warn("Failed to render text");
        return;
    }
    // texture from surf
    SDL_Renderer *rend = SDL_CreateSoftwareRenderer(surf);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
    if (tex == NULL) {
        warn("Failed to create texture from surface");
        return;
    }
    if (rend == NULL) {
        warn("Failed to create software renderer");
        return;
    }
    argb tfg = canvas->fg;
    for (int i = 0; i < surf->w; i++) {
        for (int j = 0; j < surf->h; j++) {
            argb pixel;
            SDL_RenderReadPixels(rend, &(SDL_Rect){i, j, 1, 1},
                                 SDL_PIXELFORMAT_ARGB8888, &pixel,
                                 sizeof(pixel));
            if (pixel == 0)
                continue;
            canvas->fg = pixel;
            canvas_set_pixel(canvas, x + i, y + j);
        }
    }
    canvas->fg = tfg;

    SDL_UnlockSurface(surf);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
    SDL_DestroyRenderer(rend);
    TTF_CloseFont(font);
}

void app_clean(app_t *app) {
    SDL_DestroyTexture(app->tex);
    SDL_DestroyRenderer(app->rend);
    SDL_DestroyWindow(app->win);
    free(app->canvas.fb);
    free(app->canvas.tfb);
    for (size_t i = 0; i < dynarray_len(app->font_arr); i++)
        free(app->font_arr[i].name);
    arrfree(app->font_arr);
    arrfree(app->font_name_arr);
}

void canvas_save(canvas_t *canvas, const char *file_name, int quality) {
    char *in_rgba = malloc(3 * canvas->w * canvas->h);
    for (int i = 0; i < canvas->w * canvas->h; i++) {
        in_rgba[i * 3] = (canvas->fb[i] >> 16) & 0xFF;
        in_rgba[i * 3 + 1] = (canvas->fb[i] >> 8) & 0xFF;
        in_rgba[i * 3 + 2] = (canvas->fb[i]) & 0xFF;
    }
    stbi_write_jpg(file_name, canvas->w, canvas->h, 3, in_rgba, quality);
    free(in_rgba);
}

void canvas_flood_fill(canvas_t *canvas, int x, int y) {
    if (y >= canvas->h || y < 0 || x >= canvas->w || x < 0)
        return;
    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    const argb oldcolor = canvas_get_pixel(canvas, x, y);
    if (oldcolor == canvas->fg) {
        return;
    }
    vec2i_t *stack = NULL;
    arrpush(stack, ((vec2i_t){x, y}));
    canvas_set_pixel(canvas, x, y);
    while (arrlen(stack) > 0) {
        vec2i_t v = arrpop(stack);
        for (int i = 0; i < 4; i++) {
            vec2i_t nv = {v.x + dx[i], v.y + dy[i]};
            if (nv.x >= canvas->w || nv.x < 0 || nv.y >= canvas->h ||
                nv.y < 0)
                continue;
            if (canvas_get_pixel(canvas, nv.x, nv.y) != oldcolor)
                continue;
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
                if (x1 + i < 0 || x1 + i >= canvas->w || y1 + j < 0 ||
                    y1 + j >= canvas->h)
                    continue;
                canvas_set_pixel(canvas, x1 + i, y1 + j);
            }
        }
        if (x1 == x2 && y1 == y2)
            break;
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

void canvas_event(canvas_t *canvas, SDL_Event e, gui_t *gui) {
    switch (e.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.x < 0 || e.button.x >= canvas->w || e.button.y < 0 ||
                    e.button.y >= canvas->h)
                break;
            switch (canvas->tool) {
                case BRUSH: /* FALLTHROUGH */
                case LINE:
                case RECT:
                case RECTFILL:
                    canvas->isdrag = true;
                    canvas->lx = e.button.x;
                    canvas->ly = e.button.y;
                    break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            memset(canvas->tfb, 0, canvas->w * canvas->h * sizeof(argb));
            canvas->isdrag = false;
            switch (canvas->tool) {
                case BUCKET:
                    canvas_flood_fill(canvas, e.button.x, e.button.y);
                    break;
                case LINE:
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, e.motion.x,
                            e.motion.y);
                    break;
                case TEXT:
                    gui->text.open_dialog = true;
                    gui->text.x = e.button.x;
                    gui->text.y = e.button.y;
                    break;
                case RECT:
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, e.motion.x, canvas->ly);
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, canvas->lx, e.motion.y);
                    canvas_draw_line(canvas, e.motion.x, canvas->ly, e.motion.x, e.motion.y);
                    canvas_draw_line(canvas, canvas->lx, e.motion.y, e.motion.x, e.motion.y);
                    break;
                case RECTFILL: {
                    int sx = MIN(canvas->lx, e.motion.x);
                    int ex = MAX(canvas->lx, e.motion.x);
                    int sy = MIN(canvas->ly, e.motion.y);
                    int ey = MAX(canvas->ly, e.motion.y);
                    for (int i = sx; i <= ex; i++)
                        for (int j = sy; j <= ey; j++)
                            canvas_set_pixel(canvas, i, j);
                    break;
                               }
            }
            break;
        case SDL_MOUSEMOTION:
            if (!canvas->isdrag) break;
            switch (canvas->tool) {
                case BRUSH: {
                    int x = e.motion.x;
                    int y = e.motion.y;
                    if (x >= canvas->w || y >= canvas->h)
                        break;
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, x, y);
                    canvas->lx = x;
                    canvas->ly = y;
                    break;
                            }
                case LINE:
                    canvas->use_tfb = true;
                    memset(canvas->tfb, 0, canvas->w * canvas->h * sizeof(argb));
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, e.motion.x,
                            e.motion.y);
                    canvas->use_tfb = false;
                    break;
                case RECT: /* FALLTHROUGH */
                case RECTFILL:
                    canvas->use_tfb = true;
                    memset(canvas->tfb, 0, canvas->w * canvas->h * sizeof(argb));
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, e.motion.x, canvas->ly);
                    canvas_draw_line(canvas, canvas->lx, canvas->ly, canvas->lx, e.motion.y);
                    canvas_draw_line(canvas, e.motion.x, canvas->ly, e.motion.x, e.motion.y);
                    canvas_draw_line(canvas, canvas->lx, e.motion.y, e.motion.x, e.motion.y);
                    canvas->use_tfb = false;
                    break;
            }
        break;
    }
}

void app_event(app_t *app) {
    SDL_Event e;
    nk_input_begin(app->gui.ctx);
    while (SDL_PollEvent(&e)) {
        if (!nk_item_is_any_active(app->gui.ctx))
            canvas_event(&app->canvas, e, &app->gui);
        switch (e.type) {
        case SDL_QUIT:
            app->running = false;
            break;
        }
        nk_sdl_handle_event(&e);
    }
    nk_input_end(app->gui.ctx);
}

/* TODO use more robust way (maybe nk's function)
to prevent little-endian, big-endian problem
*/
#define ARGB_TO_NKCOLORF(argb)                                                 \
    {                                                                          \
        ((float)((argb >> 16) & 0xff) / 255.0f),                               \
            ((float)((argb >> 8) & 0xff) / 255.0f),                            \
            ((float)((argb >> 0) & 0xff) / 255.0f),                            \
            ((float)((argb >> 24) & 0xff) / 255.0f)                            \
    }

#define NKCOLORF_TO_ARGB(colorf)                                               \
    ((argb)(colorf.r * 255.0f) << 16) |                                    \
        ((argb)(colorf.g * 255.0f) << 8) |                                 \
        ((argb)(colorf.b * 255.0f) << 0) |                                 \
        ((argb)(colorf.a * 255.0f) << 24)

void app_draw_gui(app_t *app) {
    gui_t gui = app->gui;

    if (nk_begin(gui.ctx, "Settings", nk_rect(0, app->h, app->w, GUI_HEIGHT),
                 NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(gui.ctx, GUI_HEIGHT, 4);

        if (nk_group_begin(gui.ctx, "col0", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            if (nk_button_label(gui.ctx, "New"))
                gui.new.open_dialog = true;
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            if (nk_button_label(gui.ctx, "Save"))
                gui.save.open_dialog = true;
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            if (nk_button_label(gui.ctx, "Load"))
                gui.load.open_dialog = true;
            nk_group_end(gui.ctx);
        }

        if (nk_group_begin(gui.ctx, "col1", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(gui.ctx, 20, 1);
            nk_label(gui.ctx, "Tool size", NK_TEXT_LEFT);
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
            if (nk_option_label(gui.ctx, "Brush", app->canvas.tool == BRUSH))
                app->canvas.tool = BRUSH;
            if (nk_option_label(gui.ctx, "Bucket", app->canvas.tool == BUCKET))
                app->canvas.tool = BUCKET;
            if (nk_option_label(gui.ctx, "Line", app->canvas.tool == LINE))
                app->canvas.tool = LINE;
            if (nk_option_label(gui.ctx, "Text", app->canvas.tool == TEXT))
                app->canvas.tool = TEXT;
            if (nk_option_label(gui.ctx, "Rect(Line)", app->canvas.tool == RECT))
                app->canvas.tool = RECT;
            if (nk_option_label(gui.ctx, "Rect(Fill)", app->canvas.tool == RECTFILL))
                app->canvas.tool = RECTFILL;
            nk_group_end(gui.ctx);
        }
    }
    nk_end(gui.ctx);

    if (gui.save.open_dialog) {
        if (nk_begin(gui.ctx, "Save", nk_rect(50, 50, 200, 200),
                     NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
            nk_layout_row_dynamic(gui.ctx, 30, 1);
            if (nk_button_label(gui.ctx, gui.save.file_path
                                             ? gui.save.file_path
                                             : "Select file path")) {
                gui.save.file_path = tinyfd_saveFileDialog(
                    "Select where to save", "image.jpg", 2,
                    (const char *[]){"*.jpg", "*.jpeg"}, "jpg image");
            }
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Quality", NK_TEXT_LEFT);
            nk_slider_int(gui.ctx, 1, &gui.save.quality, 100, 1);
            nk_layout_row_dynamic(gui.ctx, 30, 1);
            if (gui.save.file_path != NULL) {
                if (nk_button_label(gui.ctx, "Save")) {
                    canvas_save(&app->canvas, gui.save.file_path,
                                gui.save.quality);
                    gui.save.open_dialog = false;
                    gui.save.file_path = NULL;
                }
            }
        } else {
            gui.save.open_dialog = false;
        }
        nk_end(gui.ctx);
    }

    if (gui.new.open_dialog) {
        if (nk_begin(gui.ctx, "New Canvas", nk_rect(50, 50, 200, 200),
                     NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Width", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_BOX, gui.new.wb,
                                           sizeof(gui.new.wb), nk_filter_ascii);
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Height", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_BOX, gui.new.hb,
                                           sizeof(gui.new.hb), nk_filter_ascii);
            if (nk_button_label(gui.ctx, "New")) {
                gui.new.w = 400;
                gui.new.h = 0;
                gui.new.w = strtol(gui.new.wb, NULL, 10);
                gui.new.h = strtol(gui.new.hb, NULL, 10);
                app_clean(app);
                app_init(app, gui.new.w, gui.new.h);
                return;
            }
        } else {
            gui.new.open_dialog = false;
        }
        nk_end(gui.ctx);
    }

    if (gui.load.open_dialog) {
        if (nk_begin(gui.ctx, "Load Image", nk_rect(50, 50, 200, 200),
                     NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
            nk_layout_row_dynamic(gui.ctx, 30, 1);
            if (nk_button_label(gui.ctx, gui.load.file_path
                                             ? gui.load.file_path
                                             : "Select file path")) {
                gui.load.file_path =
                    tinyfd_openFileDialog("Which file to load from", "", 2,
                                          (const char *[]){"*.jpg", "*.jpeg"},
                                          "JPG Image files", false);
                if (gui.load.file_path == NULL) {
                    gui.load.open_dialog = false;
                    warn("Failed getting file path");
                }
            }
            if (nk_button_label(gui.ctx, "Load")) {
                uint8_t *data = stbi_load(gui.load.file_path, &gui.load.w,
                                          &gui.load.h, &gui.load.channel, 4);
                if (data == NULL) {
                    gui.load.open_dialog = false;
                    warn("Failed opening image file %s", stbi_failure_reason());
                } else {
                    argb *in_rgba = malloc(gui.load.w * gui.load.h * 4);
                    for (int i = 0; i < gui.load.w * gui.load.h; i++) {
                        in_rgba[i] =
                            (data[i * 4 + 3] << 24) | (data[i * 4 + 0] << 16) |
                            (data[i * 4 + 1] << 8) | (data[i * 4 + 2] << 0);
                    }
                    app_clean(app);
                    app_init(app, gui.load.w, gui.load.h);
                    memcpy(app->canvas.fb, in_rgba,
                           gui.load.w * gui.load.h * 4);
                    free(in_rgba);
                    free(data);
                    return;
                }
            }
        } else {
            gui.load.open_dialog = false;
        }
        nk_end(gui.ctx);
    }

    if (gui.text.open_dialog) {
        if (nk_begin(gui.ctx, "Draw Text", nk_rect(50, 50, 200, 200),
                     NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
            nk_layout_row_dynamic(gui.ctx, 30, 1);
            nk_combobox(gui.ctx, (const char **)app->font_name_arr,
                        arrlen(app->font_name_arr), &gui.text.selidx, 30,
                        nk_vec2(350, 400));

            nk_layout_row_dynamic(gui.ctx, 30, 1);
            nk_edit_string_zero_terminated(gui.ctx, NK_EDIT_BOX, gui.text.buf,
                                           sizeof(gui.text.buf), 0);
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            nk_label(gui.ctx, "Size", NK_TEXT_LEFT);
            nk_slider_int(gui.ctx, 1, &gui.text.size, 72, 1);
            nk_layout_row_dynamic(gui.ctx, 30, 2);
            if (nk_button_label(gui.ctx, "Draw")) {
                canvas_draw_text(
                    &app->canvas, gui.text.x, gui.text.y, gui.text.buf,
                    app->font_arr[gui.text.selidx].path, gui.text.size);
                gui.text.open_dialog = false;
                gui.text.buf[0] = 0;
            }
        } else {
            gui.text.open_dialog = false;
        }
        nk_end(gui.ctx);
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
    app->gui = gui;
}

void app_draw_canvas(app_t *app) {
    SDL_SetRenderDrawColor(app->rend, 0, 0, 0, 255);
    SDL_RenderClear(app->rend);
    SDL_UpdateTexture(app->tex, NULL, app->canvas.fb,
                      app->canvas.w * sizeof(*app->canvas.fb));
    const SDL_Rect dstrect = {
        .w = app->canvas.w,
        .h = app->canvas.h,
    };
    SDL_RenderCopy(app->rend, app->tex, NULL, &dstrect);
    SDL_UpdateTexture(app->tex, NULL, app->canvas.tfb,
                      app->canvas.w * sizeof(*app->canvas.tfb));
    SDL_RenderCopy(app->rend, app->tex, NULL, &dstrect);
}

void app_run(app_t *app) {
    /* TODO better frame capping (time ms used for each frame)
    and subtract it from 16.6666 */
    while (app->running) {
        app_event(app);
        app_draw_canvas(app);
        app_draw_gui(app);
        SDL_RenderPresent(app->rend);
        SDL_Delay(15);
    }
}

int main(int argc, char **argv) {
    int w = 800;
    int h = 600;

    /* TODO - better argument parsing and more options */
    if (argc >= 2) {
        if (sscanf(argv[1], "%dx%d", &w, &h) != 2) {
            w = 800;
            h = 600;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
        warn("SDL_Init Failed %s", SDL_GetError());
    if (TTF_Init() < 0)
        warn("TTF_Init Failed %s", TTF_GetError());

    app_t app;
    app_init(&app, w, h);
    app_run(&app);
    app_clean(&app);

    TTF_Quit();
    SDL_Quit();
    return EXIT_SUCCESS;
}
