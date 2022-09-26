#pragma once

#ifndef SDRAW_FONT_H
#define SDRAW_FONT_H


typedef struct sdraw_font_s {
    char *name;
    char *path;
} sdraw_font_t;

sdraw_font_t *get_all_fonts();

int sdraw_font_cmp(const void *a, const void *b);

#endif
