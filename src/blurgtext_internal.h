#ifndef _BLURGTEXT_INTERNAL_H_
#define _BLURGTEXT_INTERNAL_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <blurgtext.h>
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include "hashmap.h"
#include "list.h"

#define MAX_TEXTURES 16
#define BLURG_TEXTURE_SIZE 1024

#if (BT_ENABLE_FONTCONFIG || BT_ENABLE_DIRECTWRITE)
#define SYSFONTS
#endif

struct texturePacking {
    int curTex;
    int currentX;
    int currentY;
    int lineMax;
    blurg_texture_t* pages[MAX_TEXTURES];
};

typedef struct _allocated_font {
    char *data;
    size_t dataLen;
    int external;
} allocated_font;

struct _blurg_font {
    FT_Face face;
    int weight;
    int italic;
    int embolden;
    // hash of font properties that don't change
    uint32_t faceHash;
    // properties when FT_Face size is set
    // separate hash required for glyph caching
    uint32_t setSize;
    uint32_t hash;
    float ascender;
    float lineHeight;
    float scale;
    allocated_font backing;

    blurg_font_t *fallback;
};

typedef struct _font_manager font_manager_t;

struct _blurg {
    blurg_texture_allocate textureAllocate;
    blurg_texture_update textureUpdate;
    struct texturePacking packed;
    struct hashmap *glyphMap;
    font_manager_t *fontManager;
    FT_Library library;
    void *sysFontData;
};

typedef struct blurg_glyph {
    int texture;
    int srcX;
    int srcY;
    int srcW;
    int srcH;
    int offsetLeft;
    int offsetTop;
    int color;
} blurg_glyph;

void glyphatlas_init(blurg_t *blurg);
void glyphatlas_get(blurg_t *blurg, blurg_font_t *font, uint32_t index, blurg_glyph *glyph);
void glyphatlas_destroy(blurg_t *blurg);

blurg_font_t *blurg_from_freetype(FT_Face face);
blurg_font_t *blurg_font_create_internal(blurg_t *blurg, allocated_font *data);
void blurg_font_rehash(blurg_font_t *fnt);
blurg_font_t *blurg_font_fallback(blurg_t *blurg, blurg_font_t *font, uint32_t character);
blurg_font_t *blurg_sysfonts_query(blurg_t *blurg, const char *familyName, int weight, int italic, uint32_t character);
void font_use_size(blurg_font_t *fnt, float size);

void font_manager_init(blurg_t *blurg);
void font_manager_destroy(blurg_t *blurg);
void blurg_sysfonts_destroy(blurg_t *blurg);

#ifdef __cplusplus
}
#endif
#endif