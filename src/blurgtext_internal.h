#ifndef _BLURGTEXT_INTERNAL_H_
#define _BLURGTEXT_INTERNAL_H_

#include <blurgtext.h>
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include "hashmap.h"

#define MAX_TEXTURES 16
#define BLURG_TEXTURE_SIZE 1024


struct texturePacking {
    int curTex;
    int currentX;
    int currentY;
    int lineMax;
    blurg_texture_t* pages[MAX_TEXTURES];
};

struct _blurg {
    blurg_texture_allocate textureAllocate;
    blurg_texture_update textureUpdate;
    struct texturePacking packed;
    struct hashmap *glyphMap;
    FT_Library library;
};

struct _blurg_font {
    uint32_t faceHash;
    uint32_t setSize;
    uint32_t hash;
    float ascender;
    float lineHeight;
    FT_Face face;
};

typedef struct blurg_glyph {
    int texture;
    int srcX;
    int srcY;
    int srcW;
    int srcH;
    int offsetLeft;
    int offsetTop;
} blurg_glyph;

void glyphatlas_init(blurg_t *blurg);
void glyphatlas_get(blurg_t *blurg, blurg_font_t *font, uint32_t index, blurg_glyph *glyph);
void glyphatlas_destroy(blurg_t *blurg);

blurg_font_t *blurg_from_freetype(FT_Face face);
void font_use_size(blurg_font_t *fnt, float size);

#endif
