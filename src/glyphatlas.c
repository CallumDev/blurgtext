#include "blurgtext_internal.h"
#include FT_OUTLINE_H
#include FT_SYNTHESIS_H
static const uint8_t blurg_gamma[0x100] = {
    0x00, 0x0B, 0x11, 0x15, 0x19, 0x1C, 0x1F, 0x22, 0x25, 0x27, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34,
    0x36, 0x38, 0x3A, 0x3C, 0x3D, 0x3F, 0x41, 0x43, 0x44, 0x46, 0x47, 0x49, 0x4A, 0x4C, 0x4D, 0x4F,
    0x50, 0x51, 0x53, 0x54, 0x55, 0x57, 0x58, 0x59, 0x5B, 0x5C, 0x5D, 0x5E, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75,
    0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x84,
    0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93,
    0x94, 0x95, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x9F, 0xA0,
    0xA1, 0xA2, 0xA3, 0xA3, 0xA4, 0xA5, 0xA6, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAA, 0xAB, 0xAC, 0xAD,
    0xAD, 0xAE, 0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB6, 0xB6, 0xB7, 0xB8, 0xB8,
    0xB9, 0xBA, 0xBB, 0xBB, 0xBC, 0xBD, 0xBD, 0xBE, 0xBF, 0xBF, 0xC0, 0xC1, 0xC2, 0xC2, 0xC3, 0xC4,
    0xC4, 0xC5, 0xC6, 0xC6, 0xC7, 0xC8, 0xC8, 0xC9, 0xCA, 0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCE,
    0xCF, 0xD0, 0xD0, 0xD1, 0xD2, 0xD2, 0xD3, 0xD4, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD8, 0xD9,
    0xD9, 0xDA, 0xDB, 0xDB, 0xDC, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3,
    0xE3, 0xE4, 0xE4, 0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC, 0xEC,
    0xED, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF5, 0xF5,
    0xF6, 0xF7, 0xF7, 0xF8, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFB, 0xFC, 0xFC, 0xFD, 0xFD, 0xFE, 0xFF
};

typedef struct _glyph_entry {
    uint64_t key;
    blurg_glyph glyph;
} glyph_entry;

int glyph_compare(const void *a, const void* b, void *udata)
{
    const glyph_entry *ga = a;
    const glyph_entry *gb = b;
    return ga->key < gb->key ? -1 : ga->key > gb->key ? 1 : 0;
}

uint64_t glyph_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const glyph_entry *entry = item;
    return hashmap_sip(&entry->key, sizeof(uint64_t), seed0, seed1);
}

static void new_texture(blurg_t *blurg)
{
    blurg_texture_t *tex = malloc(sizeof(blurg_texture_t));
    blurg->textureAllocate(tex, BLURG_TEXTURE_SIZE, BLURG_TEXTURE_SIZE);
    //Set white pixel in top left corner
    uint32_t white = 0xFFFFFFFF;
    blurg->textureUpdate(tex, &white, 0, 0, 1, 1);
    blurg->packed.currentX = 2;
    blurg->packed.currentY = 0;
    blurg->packed.lineMax = 2;
    blurg->packed.pages[blurg->packed.curTex++] = tex;
}

void glyphatlas_init(blurg_t *blurg)
{
    blurg->glyphMap = hashmap_new(sizeof(glyph_entry), 0, 0, 0, glyph_hash, glyph_compare, NULL, NULL);
    new_texture(blurg);
}

void glyphatlas_destroy(blurg_t *blurg)
{
    hashmap_free(blurg->glyphMap);
}

void glyphatlas_get(blurg_t *blurg, blurg_font_t *font, uint32_t index, blurg_glyph *glyph)
{
    uint64_t key = ((uint64_t)font->hash);
    key = (key << 32) | index;

    const glyph_entry *result = hashmap_get(blurg->glyphMap, &(glyph_entry){ .key = key });
    if(result) {
        *glyph = result->glyph;
        return;
    }

    FT_Face face = font->face;
    int loadFlags = FT_LOAD_TARGET_LIGHT;
    if(FT_HAS_COLOR(face)) {
        loadFlags |= FT_LOAD_COLOR;
    }
    FT_Load_Glyph(face, index, loadFlags);
    if(font->embolden) {
        if(face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
            FT_Pos strength = FT_MulFix(face->units_per_EM, face->size->metrics.y_scale) / 48;
            FT_Outline_Embolden(&face->glyph->outline, strength);
        } else {
            FT_GlyphSlot_Embolden(face->glyph);
        }
    }
    FT_Error err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if(err != FT_Err_Ok) {
        printf("render error: %s\n", FT_Error_String(err));
    }
    FT_Bitmap rendered = face->glyph->bitmap;
    // find place to pack rendered glyph
    int packW = rendered.width + 1; // padding
    int packH = rendered.rows + 1;

    if(blurg->packed.currentX + packW > BLURG_TEXTURE_SIZE) {
        blurg->packed.currentX = 0;
        blurg->packed.currentY += blurg->packed.lineMax;
        blurg->packed.lineMax = 0;
    }
    if(blurg->packed.currentY + packH > BLURG_TEXTURE_SIZE) {
        new_texture(blurg);
    }
    if(packH > blurg->packed.lineMax)
        blurg->packed.lineMax = packH;
    // create and upload glyph data
    uint32_t* buf;
    if(rendered.pixel_mode == FT_PIXEL_MODE_BGRA) 
    {
        buf = (uint32_t*)rendered.buffer;
    }
    else 
    {
        buf = malloc(rendered.width * rendered.rows * sizeof(uint32_t));
        for(int i = 0; i < rendered.width * rendered.rows; i++) {
            buf[i] = 0x00FFFFFF | (blurg_gamma[rendered.buffer[i]] << 24);
        }
    }
    blurg->textureUpdate(
        blurg->packed.pages[blurg->packed.curTex - 1],
        (void*)buf,
        blurg->packed.currentX,
        blurg->packed.currentY,
        rendered.width,
        rendered.rows
    );
    if(rendered.pixel_mode != FT_PIXEL_MODE_BGRA) {
        free(buf);
    }
    *glyph = (blurg_glyph){
        .texture = blurg->packed.curTex - 1,
        .srcX = blurg->packed.currentX,
        .srcY = blurg->packed.currentY,
        .srcW = rendered.width,
        .srcH = rendered.rows,
        .offsetLeft = face->glyph->bitmap_left,
        .offsetTop = face->glyph->bitmap_top,
        .color = rendered.pixel_mode == FT_PIXEL_MODE_BGRA,
    };
    // update packing, set hashmap
    blurg->packed.currentX += packW;
    hashmap_set(blurg->glyphMap, &(glyph_entry){ .key = key, .glyph = *glyph });
}