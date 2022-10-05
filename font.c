#include <fontconfig/fontconfig.h>
#ifdef _WIN32
#include "win_dirent.h"
#else
#include <dirent.h>
#endif
#include "log.h"
#include "dynarray.h"
#include "font.h"

//static font **add_fonts_from_dir(const char *dirname, sdraw_font_t **fonts) {
//    /* TODO - make this function add all subfolder fonts recursively (or not recursive?)*/
//    size_t dirnamelen = strlen(dirname);
//    DIR *dir = opendir(dirname);
//    if (dir != NULL) {
//        struct dirent *ent;
//        while ((ent = readdir(dir)) != NULL) {
//            if (ent->d_type == DT_REG) {
//                sdraw_font_t font;
//                // check if ent is font
//                if (strstr(ent->d_name, ".ttf") != NULL || strstr(ent->d_name, ".otf") != NULL) {
//                    size_t namelen = strlen(ent->d_name);
//                    // add font to fonts
//                    char *path = malloc(dirnamelen + namelen + 2);
//                    memcpy(path, dirname, dirnamelen);
//                    path[dirnamelen] = '/';
//                    memcpy(path + dirnamelen + 1, ent->d_name, namelen);
//                    font[dirnamelen + namelen + 1] = '\0';
//                    arrpush(fonts, path);
//                    info("Added font %s", path);
//                }
//                font.path = path;
//            }
//        }
//        closedir(dir);
//    } else {
//        //warnerr("Could not open font directory %s", dirname);
//    }
//    return fonts;
//}

sdraw_font_t *get_all_fonts() {
    sdraw_font_t *fonts = NULL;

    /*
    FcStrList *dirs = FcConfigGetFontDirs((void*)0);
    FcChar8 *dirname;
    while ((dirname = FcStrListNext(dirs))) {
        fonts = add_fonts_from_dir((char*)dirname, fonts);
    }
    */

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
