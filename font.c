#include <fontconfig/fontconfig.h>
#ifdef _WIN32
#include "win_dirent.h"
#else
#include <dirent.h>
#endif
#include "log.h"
#include "dynarray.h"
#include "font.h"

sdraw_font_t *get_all_fonts() {
    sdraw_font_t *fonts = NULL;

    FcPattern *pat;
    FcFontSet *fs;
    FcObjectSet *os;
    FcConfig *config;

    if (!FcInit()) {
        warn("Could not initialize fontconfig");
        return NULL;
    }
    config = FcConfigGetCurrent();
    FcConfigSetRescanInterval(config, 0);

    pat = FcPatternCreate();
    os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, NULL);
    fs = FcFontList(config, pat, os);
    for (int i = 0; i < fs->nfont; i++) {
        FcChar8 *family, *style, *path;
        FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family);
        FcPatternGetString(fs->fonts[i], FC_STYLE, 0, &style);
        FcPatternGetString(fs->fonts[i], FC_FILE, 0, &path);

        /* SDL_ttf can only handle ttf */
        if (FcStrStr(path, (const FcChar8*)".ttf") == NULL)
            continue;

        // concat family and style
        size_t familylen = strlen((char*)family);
        size_t stylelen = strlen((char*)style);
        char *name = malloc(familylen + stylelen + 2);
        memcpy(name, family, familylen);
        name[familylen] = ' ';
        memcpy(name + familylen + 1, style, stylelen);
        name[familylen + stylelen + 1] = '\0';

        sdraw_font_t fnt;
        fnt.name = name;
        /* if we dont strdup this, FcFontSetDestroy will free it */
        fnt.path = strdup((char*)path);
        dynarray_push(fonts, fnt);
    }

    // cleanup
    if (fs) FcFontSetDestroy(fs);
    if (pat) FcPatternDestroy(pat);
    return fonts;
}

int sdraw_font_cmp(const void *a, const void *b) {
    sdraw_font_t *fnta = (sdraw_font_t*)a;
    sdraw_font_t *fntb = (sdraw_font_t*)b;
    return strcmp(fnta->name, fntb->name);
}
