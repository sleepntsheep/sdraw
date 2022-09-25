#include <fontconfig/fontconfig.h>
#ifdef _WIN32
#include "win_dirent.h"
#else
#include <dirent.h>
#endif
#include "log.h"
#include "stb_ds.h"

static char **add_fonts_from_dir(const char *dirname, char **fonts) {
    /* TODO - make this function add all subfolder fonts recursively (or not recursive?)*/
    size_t dirnamelen = strlen(dirname);
    DIR *dir = opendir(dirname);
    if (dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                // check if ent is font
                if (strstr(ent->d_name, ".ttf") != NULL || strstr(ent->d_name, ".otf") != NULL) {
                    size_t namelen = strlen(ent->d_name);
                    // add font to fonts
                    char *font = malloc(dirnamelen + namelen + 2);
                    memcpy(font, dirname, dirnamelen);
                    font[dirnamelen] = '/';
                    memcpy(font + dirnamelen + 1, ent->d_name, namelen);
                    font[dirnamelen + namelen + 1] = '\0';
                    arrpush(fonts, font);
                    info("Added font %s", font);
                }
            }
        }
        closedir(dir);
    } else {
        //warnerr("Could not open font directory %s", dirname);
    }
    return fonts;
}

char **get_all_fonts() {
    char **fonts = NULL;
    FcStrList *dirs = FcConfigGetFontDirs((void*)0);
    FcChar8 *dirname;
    while ((dirname = FcStrListNext(dirs))) {
        add_fonts_from_dir((char*)dirname, fonts);
    }
    return fonts;
}
